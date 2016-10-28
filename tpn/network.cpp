/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/network.hpp"
#include "tpn/user.hpp"
#include "tpn/httptunnel.hpp"
#include "tpn/config.hpp"
#include "tpn/store.hpp"
#include "tpn/block.hpp"

#include "pla/binaryserializer.hpp"
#include "pla/jsonserializer.hpp"
#include "pla/object.hpp"
#include "pla/securetransport.hpp"
#include "pla/crypto.hpp"
#include "pla/random.hpp"
#include "pla/http.hpp"

namespace tpn
{

const duration Network::CallerFallbackTimeout = seconds(10.);
const unsigned Network::DefaultTokens = 8;
const unsigned Network::DefaultThreshold = Network::DefaultTokens*16;

Network *Network::Instance = NULL;
const Network::Link Network::Link::Null;

Network::Network(int port) :
		mOverlay(port)
{
	// Start network thread
	mThread = std::thread([this]()
	{
		run();
	});
}

Network::~Network(void)
{

}

void Network::join(void)
{
	mThread.join();
	mOverlay.join();
}

Overlay *Network::overlay(void)
{
	return &mOverlay;
}

void Network::connect(const Identifier &node, const Identifier &remote, User *user)
{
	if(!hasLink(Link(user->identifier(), remote, node)))
		mTunneler.open(node, remote, user, true);	// async
}

void Network::registerCaller(const BinaryString &target, Caller *caller)
{
	Assert(caller);
	
	LogDebug("Network::registerCaller", "Calling " + target.toString());

	{
		std::unique_lock<std::mutex> lock(mCallersMutex);
		mCallers[target].insert(caller);
	}
	
	Set<BinaryString> hints;
	hints.insert(caller->hint());
	call(target, hints);
}

void Network::unregisterCaller(const BinaryString &target, Caller *caller)
{
	Assert(caller);
	
	{
		std::unique_lock<std::mutex> lock(mCallersMutex);
		
		auto it = mCallers.find(target);
		if(it != mCallers.end())
		{
			it->second.erase(caller);
			if(it->second.empty())   
				mCallers.erase(it);
		}
	}
}

void Network::unregisterAllCallers(const BinaryString &target)
{
	std::unique_lock<std::mutex> lock(mCallersMutex);
	mCallers.erase(target);
}

void Network::registerListener(const Identifier &local, const Identifier &remote, Listener *listener)
{
	Assert(listener);
	
	{
		std::unique_lock<std::recursive_mutex> lock(mListenersMutex);
		mListeners[IdentifierPair(remote, local)].insert(listener);
	}
	
	Link link(local, remote);

	{
		std::unique_lock<std::recursive_mutex> lock(mHandlersMutex);
		
		for(auto it = mHandlers.lower_bound(link);
			it != mHandlers.end() && (it->first.local == local && it->first.remote == remote);
			++it)
		{
			try {
				listener->seen(it->first);      // so Listener::seen() is triggered even with incoming tunnels
				listener->connected(it->first, true);
			}
			catch(const Exception &e)
			{
				LogWarn("Network::registerListener", e.what());
			}
		}
	}
	
	{
		std::unique_lock<std::mutex> lock(mSubscribersMutex);
		
		for(auto it = mSubscribers.begin(); it != mSubscribers.end(); ++it)
		{
			for(auto jt = it->second.begin(); jt != it->second.end(); ++jt)
			{
				if((*jt)->link() == link)
				{
					send(link, "subscribe", 
						Object()
							.insert("path", it->first));
					break;
				}
			}
		}
	}
}

void Network::unregisterListener(const Identifier &local, const Identifier &remote, Listener *listener)
{
	Assert(listener);
	
	{
		std::unique_lock<std::recursive_mutex> lock(mListenersMutex);
		
		auto it = mListeners.find(IdentifierPair(remote, local));
		if(it != mListeners.end())
		{
			it->second.erase(listener);
			if(it->second.empty())   
				mListeners.erase(it);
		}
	}
}

void Network::publish(String prefix, Publisher *publisher)
{
	Assert(publisher);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Network::publish", "Publishing " + prefix);
	
	{
		std::unique_lock<std::mutex> lock(mPublishersMutex);
		mPublishers[prefix].insert(publisher);
	}
}

void Network::unpublish(String prefix, Publisher *publisher)
{
	Assert(publisher);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	{
		std::unique_lock<std::mutex> lock(mPublishersMutex);
		
		auto it = mPublishers.find(prefix);
		if(it != mPublishers.end())
		{
			it->second.erase(publisher);
			if(it->second.empty()) 
				mPublishers.erase(it);
		}
	}
}

void Network::subscribe(String prefix, Subscriber *subscriber)
{
	Assert(subscriber);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Network::subscribe", "Subscribing " + prefix);
	
	{
		std::unique_lock<std::mutex> lock(mSubscribersMutex);
		mSubscribers[prefix].insert(subscriber);
	}
	
	// Local publishers
	matchPublishers(prefix, subscriber->link(), subscriber);
	
	// Remote publishers
	if(!subscriber->localOnly())
	{
		// Immediatly send subscribe message
		send(subscriber->link(), "subscribe", 
			Object()
				.insert("path", prefix));
		
		// Retrieve from cache
		Set<BinaryString> targets;
		if(Store::Instance->retrieveValue(Store::Hash(prefix), targets))
			for(auto it = targets.begin(); it != targets.end(); ++it)
				subscriber->incoming(Link::Null, prefix, "", *it);
	}
}

void Network::unsubscribe(String prefix, Subscriber *subscriber)
{
	Assert(subscriber);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	{
		std::unique_lock<std::mutex> lock(mSubscribersMutex);
		
		auto it = mSubscribers.find(prefix);
		if(it != mSubscribers.end())
		{
			it->second.erase(subscriber);
			if(it->second.empty())
				mSubscribers.erase(it);
		}
	}
}

void Network::advertise(String prefix, const String &path, Publisher *publisher)
{
	Assert(publisher);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Network::publish", "Advertising " + prefix + path);

	matchSubscribers(prefix, publisher->link(), publisher);
}

void Network::issue(String prefix, const String &path, Publisher *publisher, const Mail &mail)
{
	if(mail.empty()) return;
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Network::issue", "Issuing " + mail.digest().toString());
	
	matchSubscribers(prefix, publisher->link(), mail);
}

void Network::addRemoteSubscriber(const Link &link, const String &path)
{
	sptr<RemoteSubscriber> subscriber;
	{
		std::unique_lock<std::mutex> lock(mRemoteSubscribersMutex);
		
		if(!mRemoteSubscribers[link].get(path, subscriber))
		{
			subscriber = std::make_shared<RemoteSubscriber>(link);
			mRemoteSubscribers[link].insert(path, subscriber);
		}
	}
	
	subscriber->subscribe(path);
}

bool Network::broadcast(const Identifier &local, const String &type, const Serializable &content)
{	
	// Alias
	return send(Link(local, Identifier::Empty), type, content);
}

bool Network::send(const Identifier &local, const Identifier &remote, const String &type, const Serializable &content)
{
	// Alias
	return send(Link(local, remote), type, content);
}

bool Network::send(const Link &link, const String &type, const Serializable &content)
{
	// Alias
	return outgoing(link, type, content);
}

void Network::storeValue(const BinaryString &key, const BinaryString &value)
{
	mOverlay.store(key, value);
}

bool Network::retrieveValue(const BinaryString &key, Set<BinaryString> &values)
{
	return mOverlay.retrieve(key, values);
}

bool Network::hasLink(const Identifier &local, const Identifier &remote)
{
	// Alias
	return hasLink(Link(local, remote));
}

bool Network::hasLink(const Link &link)
{
	std::unique_lock<std::recursive_mutex> lock(mHandlersMutex);
	return mHandlers.contains(link);
}

void Network::run(void)
{
	const duration period = seconds(1.);	// TODO
	
	unsigned loops = 0;
	while(true)
	try {
		++loops;
		
		using clock = std::chrono::steady_clock;
		std::chrono::time_point<clock> end;
		end = clock::now() + std::chrono::duration_cast<clock::duration>(period);

		// Receive messages
		while(std::chrono::steady_clock::now() <= end)
		{
			duration left = end - std::chrono::steady_clock::now();
			
			Overlay::Message message;
			if(!mOverlay.recv(message, left))
				break;
			
			//LogDebug("Network::incoming", "Processing message, type: " + String::hexa(unsigned(message.type)));
			
			switch(message.type)
			{
			// Value
			case Overlay::Message::Value:
				{
					if(!message.content.empty())
					{
						// It can be about a block
						matchCallers(message.source, message.content);
						
						// Or it can be about a contact
						matchListeners(message.source, message.content);
					}
					
					break;
				}
				
			// Call
			case Overlay::Message::Call:
				{
					uint16_t tokens;
					BinaryString target;
					message.content.readBinary(tokens);
					message.content.readBinary(target);
					
					if(Store::Instance->hasBlock(target))
					{
						if(tokens) LogDebug("Network::run", "Called " + target.toString() + " (" + String::number(tokens) + " tokens)");
						mPusher.push(target, message.source, tokens);
					}
					else {
						//LogDebug("Network::run", "Called (unknown) " + target.toString());
					}
					break;
				}
				
			// Data
			case Overlay::Message::Data:
				{
					const BinaryString &target = message.source;
					Fountain::Combination combination;
					BinarySerializer(&message.content) >> combination;
					combination.setCodedData(message.content);
					
					//LogDebug("Network::run", "Data for " + target.toString() + " (" + combination.toString() + ")");
					
					if(Store::Instance->push(target, combination))
					{
						unregisterAllCallers(target);
						
						Set<BinaryString> nodes;
						if(Store::Instance->retrieveValue(target, nodes))
						{
							BinaryString call;
							call.writeBinary(uint16_t(0));
							call.writeBinary(target);
							
							for(auto kt = nodes.begin(); kt != nodes.end(); ++kt)
								mOverlay.send(Overlay::Message(Overlay::Message::Call, call, *kt));
						}
					}
					break;
				}
				
			// Tunnel
			case Overlay::Message::Tunnel:
				{
					mTunneler.incoming(message);
					break;
				}
			}
		}
		
		// Send calls
		if(loops % 2 == 0)
		{
			std::unique_lock<std::mutex> lock(mCallersMutex);
			
			for(auto it = mCallers.begin(); it != mCallers.end(); ++it)
			{
				const BinaryString &target = it->first;
				Set<BinaryString> hints;
				
				bool fallback = false;
				for(const Caller *caller : it->second)
				{
					fallback|= (caller->elapsed() >= CallerFallbackTimeout);
					hints.insert(caller->hint());
				}
				
				call(target, hints, fallback);
			}
		}
		
		// Send beacons
		if(loops % 10 == 0)
		{
			Identifier node(mOverlay.localNode());
			Set<Identifier> localIds;
			Set<Identifier> remoteIds;
			
			for(auto &p : mListeners)
			{
				localIds.insert(p.first.second);
				remoteIds.insert(p.first.first);
			}
			
			for(auto &id : localIds)
				storeValue(id, node);
			
			for(auto &id : remoteIds)
				mOverlay.retrieve(id);
			
			//LogDebug("Network::run", "Identifiers: stored " + String::number(localIds.size()) + ", queried " + String::number(remoteIds.size()));
			
			{
				std::unique_lock<std::mutex> lock(mCallersMutex);
				mCallCandidates.clear();
			}
		}
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::run", e.what());
	}
}

void Network::registerHandler(const Link &link, sptr<Handler> handler)
{	
	Assert(!link.local.empty());
	Assert(!link.remote.empty());
	Assert(!link.node.empty());
	Assert(handler);

	sptr<Handler> currentHandler;

	{
		std::unique_lock<std::recursive_mutex> lock(mHandlersMutex);
		
		mHandlers.get(link, currentHandler); 
		mHandlers.insert(link, handler);
	}
	
	{
		std::unique_lock<std::mutex> lock(mSubscribersMutex);
		
		for(auto it = mSubscribers.begin(); it != mSubscribers.end(); ++it)
		{
			for(Subscriber *subscriber : it->second)
			{
				if(subscriber->link() == link)
				{
					send(link, "subscribe", 
						Object()
							.insert("path", it->first));
					break;
				}
			}
		}
	}
	
	onConnected(link, true);
}

void Network::unregisterHandler(const Link &link, Handler *handler)
{
	Assert(!link.local.empty());
	Assert(!link.remote.empty());
	Assert(!link.node.empty());
	
	sptr<Handler> currentHandler;	// prevent handler deletion on erase (we need reference on link)
	
	{
		std::unique_lock<std::recursive_mutex> lock(mHandlersMutex);
		
		if(!mHandlers.get(link, currentHandler) || currentHandler.get() != handler)
			return;
		
		mHandlers.erase(link);
	}
	
	{
		std::unique_lock<std::mutex> lock(mRemoteSubscribersMutex);
		mRemoteSubscribers.erase(link);
	}
	
	onConnected(link, false);
}

bool Network::outgoing(const String &type, const Serializable &content)
{
	// Alias
	return outgoing(Link::Null, type, content);
}

bool Network::outgoing(const Link &link, const String &type, const Serializable &content)
{
	String serialized;
	JsonSerializer(&serialized) << content;

	Set<sptr<Handler> > handlers;
	{
		std::unique_lock<std::recursive_mutex> lock1(mHandlersMutex,  std::defer_lock);
		std::unique_lock<std::recursive_mutex> lock2(mListenersMutex, std::defer_lock);
		std::lock(lock1, lock2);
		
		for(auto it = mHandlers.lower_bound(link);
			it != mHandlers.end() && it->first == link;
			++it)
		{
			if(type == "subscribe")
			{
				// If link is not trusted, do not send subscriptions
				if(!mListeners.contains(IdentifierPair(it->first.remote, it->first.local)))
					continue;
			}
			
			handlers.insert(it->second);
		}
	}
	
	for(auto h : handlers)
		h->write(type, serialized);
	
	//LogDebug("Network::outgoing", "Sending command (type=\"" + type + "\") on " + String::number(handlers.size()) + " links");
	return !handlers.empty();
}

bool Network::push(const BinaryString &target, unsigned tokens)
{
	// Alias
	return push(Link::Null, target, tokens);
}

bool Network::push(const Link &link, const BinaryString &target, unsigned tokens)
{
	if(!Store::Instance->hasBlock(target))
		return false;
	
	Set<sptr<Handler> > handlers;
	{
		std::unique_lock<std::recursive_mutex> lock1(mHandlersMutex,  std::defer_lock);
		std::unique_lock<std::recursive_mutex> lock2(mListenersMutex, std::defer_lock);
		std::lock(lock1, lock2);
		
		for(auto it = mHandlers.lower_bound(link);
			it != mHandlers.end() && it->first == link;
			++it)
		{
			handlers.insert(it->second);
		}
	}
	
	tokens = (tokens + (handlers.size()-1))/handlers.size();
	for(auto h : handlers)
		h->push(target, tokens);
	
	//LogDebug("Network::push", "Pushing " + target.toString() + " on " + String::number(handlers.size()) + " links");
	return !handlers.empty();
}

bool Network::incoming(const Link &link, const String &type, Serializer &serializer)
{
	LogDebug("Network::incoming", "Incoming command (type=\"" + type + "\")");
	
	if(type == "pull")	// equivalent to call between users
	{
		BinaryString target;
		unsigned tokens = 0;
		serializer >> Object()
				.insert("target", target)
				.insert("tokens", tokens);
		
		if(tokens) LogDebug("Network::run", "Pulled " + target.toString() + " (" + String::number(tokens) + " tokens)");
		
		push(link, target, tokens);
	}
	else if(type == "publish")
	{
		// If link is not trusted, ignore publications
		// Subscriptions are filtered in outgoing()
		{
			std::unique_lock<std::recursive_mutex> lock(mListenersMutex);
			if(!mListeners.contains(IdentifierPair(link.remote, link.local)))
				return false;
		}
		
		String path;
		Mail mail;
		List<BinaryString> targets;
		serializer >> Object()
				.insert("path", path)
				.insert("message", mail)
				.insert("targets", targets);
		
		// We check in cache to prevent publishing loops
		BinaryString key = Store::Hash(path);
		
		// Message
		if(!mail.empty() && !Store::Instance->hasValue(key, mail.digest()))
		{
			Store::Instance->storeValue(key, mail.digest(), Store::Temporary);
			matchSubscribers(path, link, mail);
		}
		
		// Targets
		bool hasNew = false;
		for(auto target : targets)
		{
			hasNew|= !Store::Instance->hasValue(key, target);
			Store::Instance->storeValue(key, target, Store::Temporary);	// cache
			
			{
				std::unique_lock<std::mutex> lock(mTargetsMutex);
				mTargets[target].insert(link);
			}
		}
		
		if(hasNew)
		{
			RemotePublisher publisher(targets, link);
			matchSubscribers(path, link, &publisher);
		}
	}
	else if(type == "subscribe")
	{
		String path;
		serializer >> Object()
				.insert("path", path);
		
		addRemoteSubscriber(link, path);
	}
	else if(type == "invite")
	{
		String name;
		serializer >> Object()
				.insert("name", name);
		
		User *user = User::GetByIdentifier(link.local);
		if(user && !name.empty()) user->invite(link.remote, name);
	}
	else {
		return onRecv(link, type, serializer);
	}
	
	return true;
}

bool Network::call(const BinaryString &target, Set<BinaryString> hints, bool fallback)
{
	unsigned tokens = Store::Instance->missing(target);
	
	// Consider target as a hint
	hints.insert(target);
	
	// Get other hints from Store
	Set<BinaryString> otherHints;
	Store::Instance->getBlockHints(target, otherHints);
	hints.insertAll(otherHints);
	
	Set<Link> links;
	if(!fallback)
	{
		std::unique_lock<std::mutex> lock(mTargetsMutex);
		
		// Retrieve candidate links for pulling
		for(const BinaryString &hint : hints)
		{
			auto it = mTargets.find(hint);
			if(it != mTargets.end())
			{
				auto jt = it->second.begin();
				while(jt != it->second.end())
				{
					if(hasLink(*jt)) links.insert(*jt++);
					else jt = it->second.erase(jt);	
				}
				
				if(it->second.empty())
					mTargets.erase(it);
			}
		}
	}
	else {
		std::unique_lock<std::mutex> lock(mTargetsMutex);
		
		for(const BinaryString &hint : hints)
			mTargets.erase(hint);
	}
	
	// If we have candidate links
	if(!links.empty())
	{
		LogDebug("Network::run", "Pulling " + target.toString() + " from " + String::number(links.size()) + " users");
		
		// Immediately send pull
		tokens = (tokens + (links.size()-1))/links.size();
		for(auto link : links)
		{
			send(link, "pull", Object()
				.insert("target", target)
				.insert("tokens", uint16_t(tokens)));
		}
		
		return true;
	}
	
	// Fallback: Immediately call nodes providing hinted blocks and target block
	for(const BinaryString &hint : hints)
	{
		Set<BinaryString> nodes;
		if(Store::Instance->retrieveValue(hint, nodes))
		{
			BinaryString call;
			call.writeBinary(uint16_t(tokens));
			call.writeBinary(target);
			
			for(auto n : nodes)
				mOverlay.send(Overlay::Message(Overlay::Message::Call, call, n));
		}
		
		// Send retrieve for node
		mOverlay.retrieve(hint);
	}
	
	return false;
}

bool Network::matchPublishers(const String &path, const Link &link, Subscriber *subscriber)
{
	if(path.empty() || path[0] != '/') return false;
	
	List<String> list;
	path.before('?').explode(list,'/');
	if(list.empty()) return false;
	list.pop_front();
	
	// Match prefixes, longest first
	while(true)
	{
		std::unique_lock<std::mutex> lock(mPublishersMutex);
		
		String prefix;
		prefix.implode(list, '/');
		prefix = "/" + prefix;
		
		String truncatedPath(path.substr(prefix.size()));
		if(truncatedPath.empty()) truncatedPath = "/";

		List<BinaryString> targets;
		auto it = mPublishers.find(prefix);
		if(it != mPublishers.end())
		{
			for(Publisher *publisher : it->second)
			{
				if(publisher->link() != link)
					continue;
				
				List<BinaryString> result;
				if(publisher->anounce(link, prefix, truncatedPath, result))
				{
					Assert(!result.empty());
					if(subscriber) 	// local
					{
						for(auto it = result.begin(); it != result.end(); ++it)
							subscriber->incoming(publisher->link(), path, "/", *it);
					}
					else targets.splice(targets.end(), result);	// remote
				}
			}
			
			if(!targets.empty())	// remote
			{
				LogDebug("Network::matchPublishers", "Anouncing " + path);
	
				send(link, "publish",
					Object()
						.insert("path", path)
						.insert("targets", targets));
			}
		}
		
		if(list.empty()) break;
		list.pop_back();
	}
	
	return true;
}

bool Network::matchSubscribers(const String &path, const Link &link, Publisher *publisher)
{
	if(path.empty() || path[0] != '/') return false;
	
	List<String> list;
	path.explode(list,'/');
	if(list.empty()) return false;
	list.pop_front();
	
	// Match prefixes, longest first
	while(true)
	{
		std::unique_lock<std::mutex> lock(mSubscribersMutex);
		
		String prefix;
		prefix.implode(list, '/');
		prefix = "/" + prefix;
		
		String truncatedPath(path.substr(prefix.size()));
		if(truncatedPath.empty()) truncatedPath = "/";
		
		// Pass to subscribers
		auto it = mSubscribers.find(prefix);
		if(it != mSubscribers.end())
		{
			for(Subscriber *subscriber : it->second)
			{
				if(subscriber->link() != link)
					continue;
				
				List<BinaryString> targets;
				if(publisher->anounce(link, prefix, truncatedPath, targets))
				{
					for(List<BinaryString>::const_iterator kt = targets.begin();
						kt != targets.end();
						++kt)
					{
						// TODO: should prevent forwarding in case we want to republish another content
						subscriber->incoming(publisher->link(), prefix, truncatedPath, *kt);
					}
				}
			}
		}
	
		if(list.empty()) break;
		list.pop_back();
	}
	
	return true;
}

bool Network::matchSubscribers(const String &path, const Link &link, const Mail &mail)
{
	if(path.empty() || path[0] != '/') return false;
	
	List<String> list;
	path.explode(list,'/');
	if(list.empty()) return false;
	list.pop_front();
	
	// Match prefixes, longest first
	while(true)
	{
		std::unique_lock<std::mutex> lock(mSubscribersMutex);
		
		String prefix;
		prefix.implode(list, '/');
		prefix = "/" + prefix;
		
		String truncatedPath(path.substr(prefix.size()));
		if(truncatedPath.empty()) truncatedPath = "/";
		
		// Pass to subscribers
		auto it = mSubscribers.find(prefix);
		if(it != mSubscribers.end())
		{
			for(Subscriber *subscriber : it->second)
			{
				if(subscriber->link() != link)
					continue;
				
				subscriber->incoming(link, prefix, truncatedPath, mail);
			}
		}
	
		if(list.empty()) break;
		list.pop_back();
	}
	
	return true;
}

bool Network::matchCallers(const Identifier &target, const Identifier &node)
{
	if(node == mOverlay.localNode())
		return false;
	
	std::unique_lock<std::mutex> lock(mCallersMutex);
	
	if(!mCallers.contains(target))
		return false;
	
	if(mCallCandidates[target].contains(node))
		return true;
	
	mCallCandidates[target].insert(node);
	
	LogDebug("Network::run", "Got candidate for " + target.toString());
	
	unsigned tokens = Store::Instance->missing(target);
	BinaryString call;
	call.writeBinary(uint16_t(tokens));
	call.writeBinary(target);
	
	mOverlay.send(Overlay::Message(Overlay::Message::Call, call, node));
	return true;
}

bool Network::matchListeners(const Identifier &identifier, const Identifier &node)
{
	if(node == mOverlay.localNode())
		return false;
	
	std::unique_lock<std::recursive_mutex> lock(mListenersMutex);
	
	auto it = mListeners.lower_bound(IdentifierPair(identifier, Identifier::Empty));	// pair is (remote, local)
	if(it == mListeners.end())
		return false;
	
	//LogDebug("Network::run", "Got instance for " + identifier.toString());
	
	while(it != mListeners.end() && it->first.first == identifier)
	{
		for(auto listener : it->second)
			listener->seen(Link(it->first.second, it->first.first, node));
		++it;
	}
	
	return true;
}

void Network::onConnected(const Link &link, bool status) const
{
	std::unique_lock<std::recursive_mutex> lock(mListenersMutex);

	auto it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it == mListeners.end()) return;
	
	while(it != mListeners.end() && it->first.first == link.remote && it->first.second == link.local)
	{
		for(auto listener : it->second)
		{
			if(status) listener->seen(link);	// so Listener::seen() is triggered even with incoming tunnels
			listener->connected(link, status);
		}
		++it;
	}
}

bool Network::onRecv(const Link &link, const String &type, Serializer &serializer) const
{
	std::unique_lock<std::recursive_mutex> lock(mListenersMutex);
	
	auto it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it == mListeners.end()) return false;
	
	bool ret = false;
	while(it != mListeners.end() && it->first.first == link.remote && it->first.second == link.local)
	{
		for(auto listener : it->second)
			ret|= listener->recv(link, type, serializer);
		++it;
	}
	
	return ret;
}

bool Network::onAuth(const Link &link, const Rsa::PublicKey &pubKey) const
{
	std::unique_lock<std::recursive_mutex> lock(mListenersMutex);
	
	auto it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it == mListeners.end()) return false;
	
	while(it != mListeners.end() && it->first.first == link.remote && it->first.second == link.local)
	{
		for(auto listener : it->second)
			if(!listener->auth(link, pubKey))
				return false;
		++it;
	}
	
	return true;
}

Network::Link::Link(void)
{
	
}

Network::Link::Link(const Identifier &local, const Identifier &remote, const Identifier &node)
{
	this->local = local;
	this->remote = remote;
	this->node = node;
}

Network::Link::~Link(void)
{
	
}

void Network::Link::setNull(void)
{
	local.clear();
	remote.clear();
	node.clear();
}

bool Network::Link::isNull(void) const
{
	return (local.empty() && remote.empty() && node.empty());
}

bool Network::Link::operator < (const Network::Link &l) const
{
	return ((local < l.local && !local.empty() && !l.local.empty())
		|| (local == l.local && ((remote < l.remote && !remote.empty() && !l.remote.empty()) 
		|| (remote == l.remote && node < l.node && !node.empty() && !l.node.empty()))));
}

bool Network::Link::operator > (const Network::Link &l) const
{
	return ((local > l.local && !local.empty() && !l.local.empty())
		|| (local == l.local && ((remote > l.remote && !remote.empty() && !l.remote.empty())
		|| (remote == l.remote && node > l.node && !node.empty() && !l.node.empty()))));	
}

bool Network::Link::operator == (const Network::Link &l) const
{
	return ((local == l.local || local.empty() || l.local.empty())
		&& (remote == l.remote || remote.empty() || l.remote.empty())
		&& (node == l.node || node.empty() || l.node.empty()));
}

bool Network::Link::operator != (const Network::Link &l) const
{
	return !(*this == l);
}

Network::Publisher::Publisher(const Link &link) :
	mLink(link)
{

}

Network::Publisher::~Publisher(void)
{
	for(auto it = mPublishedPrefixes.begin(); it != mPublishedPrefixes.end(); ++it)
	{
		Network::Instance->unpublish(*it, this);
	}
}

const Network::Link &Network::Publisher::link(void) const
{
	return mLink;
}

void Network::Publisher::publish(const String &prefix, const String &path)
{
	if(!mPublishedPrefixes.contains(prefix))
	{
		Network::Instance->publish(prefix, this);
		mPublishedPrefixes.insert(prefix);
	}
	
	Network::Instance->advertise(prefix, path, this);
}

void Network::Publisher::unpublish(const String &prefix)
{
	if(mPublishedPrefixes.contains(prefix))
	{
		Network::Instance->unpublish(prefix, this);
		mPublishedPrefixes.erase(prefix);
	}
}

void Network::Publisher::issue(const String &prefix, const Mail &mail, const String &path)
{
	Network::Instance->issue(prefix, path, this, mail);
}

Network::Subscriber::Subscriber(const Link &link) :
	mLink(link),
	mPool(2)	// TODO
{
	
}

Network::Subscriber::~Subscriber(void)
{
	unsubscribeAll();
}

const Network::Link &Network::Subscriber::link(void) const
{
	return mLink;
}

void Network::Subscriber::subscribe(const String &prefix)
{
	if(!mSubscribedPrefixes.contains(prefix))
	{
		Network::Instance->subscribe(prefix, this);
		mSubscribedPrefixes.insert(prefix);
	}
}

void Network::Subscriber::unsubscribe(const String &prefix)
{
	if(mSubscribedPrefixes.contains(prefix))
	{
		Network::Instance->unsubscribe(prefix, this);
		mSubscribedPrefixes.erase(prefix);
	}
}

void Network::Subscriber::unsubscribeAll(void)
{
	for(auto it = mSubscribedPrefixes.begin(); it != mSubscribedPrefixes.end(); ++it)
	{
		Network::Instance->unsubscribe(*it, this);
	}
}

bool Network::Subscriber::localOnly(void) const
{
	return false;
}

bool Network::Subscriber::fetch(const Link &link, const String &prefix, const String &path, const BinaryString &target, bool fetchContent)
{
	// Test local availability
	if(Store::Instance->hasBlock(target))
	{
		Resource resource(target, true);	// local only
		if(!fetchContent || resource.isLocallyAvailable())
			return true;
	}
	
	// Enqueue fetch task
	mPool.enqueue([this, link, prefix, path, target, fetchContent]()
	{
		try {
			Resource resource(target);
			
			if(fetchContent)
			{
				Resource::Reader reader(&resource, "", true);	// empty password + no check
				reader.discard();				// read everything
			}

			incoming(link, prefix, path, target);
		}
		catch(const Exception &e)
		{
			LogWarn("Network::Subscriber::fetch", "Fetching failed for " + target.toString() + ": " + e.what());
		}
	});
	
	return false;
}

Network::RemotePublisher::RemotePublisher(const List<BinaryString> targets, const Link &link) :
	Publisher(link),
	mTargets(targets)
{

}

Network::RemotePublisher::~RemotePublisher(void)
{
  
}

bool Network::RemotePublisher::anounce(const Link &link, const String &prefix, const String &path, List<BinaryString> &targets)
{
	targets = mTargets;
	return !targets.empty();
}

Network::RemoteSubscriber::RemoteSubscriber(const Link &link) :
	Subscriber(link)
{

}

Network::RemoteSubscriber::~RemoteSubscriber(void)
{

}

bool Network::RemoteSubscriber::incoming(const Link &link, const String &prefix, const String &path, const BinaryString &target)
{
	if(link.remote.empty() || link != this->link())
	{
		Array<BinaryString> targets;
		targets.append(target);
		
		Network::Instance->send(this->link(), "publish",
			Object()
				.insert("path", prefix)
				.insert("targets", targets));
		
		return true;
	}
	
	return false;
}

bool Network::RemoteSubscriber::incoming(const Link &link, const String &prefix, const String &path, const Mail &mail)
{
	if(link.remote.empty() || link != this->link())
	{
		Network::Instance->send(this->link(), "publish",
			Object()
				.insert("path", prefix)
				.insert("message", mail));
		
		return true;
	}
	
	return false;
}

bool Network::RemoteSubscriber::localOnly(void) const
{
	return true;
}

Network::Caller::Caller(void) :
	mStartTime(std::chrono::steady_clock::time_point::min())
{

}

Network::Caller::Caller(const BinaryString &target, const BinaryString &hint)
{
	Assert(!target.empty());
	startCalling(target, hint);
}

Network::Caller::~Caller(void)
{
	stopCalling();
}
	
void Network::Caller::startCalling(const BinaryString &target, const BinaryString &hint)
{
	if(target != mTarget)
	{
		stopCalling();
		
		mTarget = target;
		mHint = hint;
		mStartTime = std::chrono::steady_clock::now();
		if(!mTarget.empty()) Network::Instance->registerCaller(mTarget, this);
	}
}

void Network::Caller::stopCalling(void)
{
	if(!mTarget.empty())
	{
		Network::Instance->unregisterCaller(mTarget, this);
		mTarget.clear();
		mHint.clear();
	}
}

BinaryString Network::Caller::target(void) const
{
	return mTarget;
}

BinaryString Network::Caller::hint(void) const
{
	return mHint;
}

duration Network::Caller::elapsed(void) const
{
	return std::chrono::steady_clock::now() - mStartTime;
}

Network::Listener::Listener(void)
{
	
}

Network::Listener::~Listener(void)
{
	ignore();
}

void Network::Listener::listen(const Identifier &local, const Identifier &remote)
{
	mPairs.insert(IdentifierPair(remote, local));
	Network::Instance->registerListener(local, remote, this);
}

void Network::Listener::ignore(const Identifier &local, const Identifier &remote)
{
	mPairs.erase(IdentifierPair(remote, local));
	Network::Instance->unregisterListener(local, remote, this);
}

void Network::Listener::ignore(void)
{
	for(auto it = mPairs.begin(); it != mPairs.end(); ++it)
        {
                Network::Instance->unregisterListener(it->second, it->first, this);
        }

	mPairs.clear();
}

Network::Tunneler::Tunneler(void) : 
	mPool(3),	// TODO 
	mStop(false)
{
	mThread = std::thread([this]()
	{
		run();
	});
}

Network::Tunneler::~Tunneler(void)
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mStop = true;
	}
	
	mCondition.notify_all();
	mThread.join();
}

bool Network::Tunneler::open(const BinaryString &node, const Identifier &remote, User *user, bool async)
{
	Assert(!node.empty());
	Assert(!remote.empty());
	Assert(user);
	
	if(node == Network::Instance->overlay()->localNode())
		return false;
	
	if(Network::Instance->overlay()->connectionsCount() == 0)
		return false;
	
	if(Network::Instance->hasLink(Link(user->identifier(), remote, node)))
		return false;
	
	uint64_t tunnelId = 0;
	Random().readBinary(tunnelId);	// Generate random tunnel ID
	BinaryString local = user->identifier();
	
	SecureTransport *transport = NULL;
	{
		std::unique_lock<std::mutex> lock(mTunnelsMutex);
		
		if(mPending.contains(node))
			return false;
		
		mPending.insert(node);
	
		//LogDebug("Network::Tunneler::open", "Opening tunnel to " + node.toString() + " (id " + String::hexa(tunnelId) + ")");
		
		Tunneler::Tunnel *tunnel = NULL;
		try {
			tunnel = new Tunneler::Tunnel(this, tunnelId, node);
			transport = new SecureTransportClient(tunnel, NULL);
			mTunnels.insert(tunnel->id(), tunnel);
			
			// Set remote name
			transport->setHostname(remote.toString());
			
			// Add certificates
			//LogDebug("Network::Tunneler::open", "Setting certificate credentials: " + user->name());
			transport->addCredentials(user->certificate().get(), false);
		}
		catch(...)
		{
			delete tunnel;
			delete transport;
			mPending.erase(node);
			throw;
		}
	}
	
	return handshake(transport, Link(local, remote, node), async);
}

SecureTransport *Network::Tunneler::listen(BinaryString *source)
{
	while(true)
	{
		Overlay::Message message;
		{
			std::unique_lock<std::mutex> lock(mMutex);
			
			if(mQueue.empty() && !mStop)
			{
				mCondition.wait(lock, [this]() {
					return !mQueue.empty() || mStop;
				});
			}
			
			if(mStop) break;
			
			message = mQueue.front();
			mQueue.pop();
		}
		
		// Read tunnel ID
		uint64_t tunnelId = 0;
		if(!message.content.readBinary(tunnelId))
			continue;
		
		{
			std::unique_lock<std::mutex> lock(mTunnelsMutex);
			
			// Find tunnel
			Tunneler::Tunnel *tunnel = NULL;
			auto it = mTunnels.find(tunnelId);
			if(it != mTunnels.end())
				tunnel = it->second;

			if(tunnel)
			{
				//LogDebug("Network::Tunneler::listen", "Message tunnel from " + message.source.toString() + " (id " + String::hexa(tunnelId) + ")");
				tunnel->incoming(message);
			}
			else {
				LogDebug("Network::Tunneler::listen", "Incoming tunnel from " + message.source.toString() /*+ " (id " + String::hexa(tunnelId) + ")"*/);
				
				SecureTransport *transport = NULL;
				try {
					tunnel = new Tunneler::Tunnel(this, tunnelId, message.source);
					transport = new SecureTransportServer(tunnel, NULL, true);	// ask for certificate
					mTunnels.insert(tunnel->id(), tunnel);
				}
				catch(...)
				{
					delete tunnel;
					delete transport;
					throw;
				}
				
				tunnel->incoming(message);
				
				if(source) *source = message.source;
				return transport;
			}
		}
	}
	
	return NULL;
}

bool Network::Tunneler::incoming(const Overlay::Message &message)
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mQueue.push(message);
	}
	
	mCondition.notify_all();
	return true;
}

void Network::Tunneler::registerTunnel(uint64_t id, Tunnel *tunnel)
{
	std::unique_lock<std::mutex> lock(mTunnelsMutex);
	Assert(tunnel);
	mTunnels.insert(tunnel->id(), tunnel);
}

void Network::Tunneler::unregisterTunnel(uint64_t id)
{
	std::unique_lock<std::mutex> lock(mTunnelsMutex);
	auto it = mTunnels.find(id);
	if(it != mTunnels.end())
	{
		mPending.erase(it->second->node());
		mTunnels.erase(it);
	}
}

bool Network::Tunneler::handshake(SecureTransport *transport, const Link &link, bool async)
{
	Assert(!link.node.empty());
	
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Identifier local, remote, node;
		Rsa::PublicKey publicKey;
		
		bool verifyName(const String &name, SecureTransport *transport)
		{
			LogDebug("Network::Tunneler::handshake", String("Verifying user: ") + name);
			
			try {
				local.fromString(name);
			}
			catch(...)
			{
				LogDebug("Network::Tunneler::handshake", String("Invalid identifier: ") + name);
				return false;
			}
			
			User *user = User::GetByIdentifier(local);
			if(user)
			{
				transport->addCredentials(user->certificate().get(), false);
			}
			else {
				 LogDebug("Network::Tunneler::handshake", String("User does not exist: ") + name);
			}
			
			return true;	// continue handshake anyway
		}
		
		bool verifyPublicKey(const std::vector<Rsa::PublicKey> &chain)
		{
			if(chain.empty()) return false;
			publicKey = chain[0];
			remote = Identifier(publicKey.digest());
			
			LogDebug("Network::Tunneler::handshake", String("Verifying remote certificate: ") + remote.toString());
			
			/*if(Network::Instance->onAuth(Link(local, remote, node), publicKey)) return true;
			LogDebug("Network::Tunneler::handshake", "Certificate verification failed");
			return false;*/
			
			// Always accept
			Network::Instance->onAuth(Link(local, remote, node), publicKey);
			return true;
		}
	};
	
	try {
		auto handshakeTask = [transport, link]()
		{
			//LogDebug("Network::Tunneler::handshake", "HandshakeTask starting...");
			
			try {
				// Set verifier
				MyVerifier verifier;
				verifier.local = link.local;
				verifier.node  = link.node;
				transport->setVerifier(&verifier);
				
				// Set MTU
				transport->setDatagramMtu(1200);	// TODO
				
				// Set timeout
				duration timeout = milliseconds(Config::Get("request_timeout").toDouble());
				transport->setHandshakeTimeout(timeout);
				
				// Do handshake
				transport->handshake();
				Assert(transport->hasCertificate());
				
				if(!link.local.empty() && link.local != verifier.local)
					return false;
				
				if(!link.remote.empty() && link.remote != verifier.remote)
					return false;
				
				// Handshake succeeded
				LogDebug("Network::Tunneler::handshake", "Handshake succeeded");
				
				Link link(verifier.local, verifier.remote, verifier.node);
				sptr<Handler> handler = std::make_shared<Handler>(transport, link);
				Network::Instance->registerHandler(link, handler);
				return true;
			}
			catch(const Timeout &e)
			{
				//LogDebug("Network::Tunneler::handshake", String("Handshake failed: ") + e.what());
			}
			catch(const std::exception &e)
			{
				LogInfo("Network::Tunneler::handshake", String("Handshake failed: ") + e.what());
			}
			
			delete transport;
			return false;
		};
		
		if(async)
		{
			mPool.enqueue(handshakeTask);
			return true;
		}
		else {
			return handshakeTask();
		}
	}
	catch(const std::exception &e)
	{
		LogError("Network::Tunneler::handshake", e.what());
		delete transport;
		return false;
	}
}

void Network::Tunneler::run(void)
{
	LogDebug("Network::Tunneler::run", "Starting tunneler");
	
	while(true)
	{
		try {
			Identifier node;
			SecureTransport *transport = listen(&node);
			if(!transport) break;
			
			handshake(transport, Link(Identifier::Empty, Identifier::Empty, node), true); // async
		}
		catch(const std::exception &e)
		{
			LogError("Network::Tunneler::run", e.what());
		}
	}
	
	LogWarn("Network::Tunneler::run", "Closing tunneler");
}

Network::Tunneler::Tunnel::Tunnel(Tunneler *tunneler, uint64_t id, const BinaryString &node) :
	mTunneler(tunneler),
	mId(id),
	mNode(node),
	mOffset(0),
	mTimeout(milliseconds(Config::Get("idle_timeout").toDouble())),
	mClosed(false)
{
	mBuffer.writeBinary(mId);
}

Network::Tunneler::Tunnel::~Tunnel(void)
{
	//LogDebug("Network::Tunneler::Tunnel", "Unregistering tunnel " + String::hexa(mId) + " to " + mNode.toString());
	mTunneler->unregisterTunnel(mId);
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mClosed = true;
	}
	
	mCondition.notify_all();
	std::this_thread::sleep_for(seconds(1.));	// TODO
	std::unique_lock<std::mutex> lock(mMutex);
}

uint64_t Network::Tunneler::Tunnel::id(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mId;
}

BinaryString Network::Tunneler::Tunnel::node(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mNode;
}

void Network::Tunneler::Tunnel::setTimeout(duration timeout)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mTimeout = timeout;
}

size_t Network::Tunneler::Tunnel::readData(char *buffer, size_t size)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	if(mClosed) return 0;
	mCondition.wait_for(lock, mTimeout, [this]() {
		return mClosed || !mQueue.empty();
	});
	
	if(mClosed) return 0;
	if(mQueue.empty()) throw Timeout();
	
	const Overlay::Message &message = mQueue.front();
	Assert(mOffset <= message.content.size());
	size = std::min(size, size_t(message.content.size() - mOffset));
	std::copy(message.content.data() + mOffset, message.content.data() + mOffset + size, buffer);
        return size;
}

void Network::Tunneler::Tunnel::writeData(const char *data, size_t size)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mBuffer.writeBinary(data, size);
}

bool Network::Tunneler::Tunnel::waitData(duration timeout)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	if(mClosed) return true;
	mCondition.wait_for(lock, timeout, [this]() {
		return mClosed || !mQueue.empty();
	});
	
	return (mClosed || !mQueue.empty());
}

bool Network::Tunneler::Tunnel::nextRead(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	if(!mQueue.empty()) mQueue.pop();
	mOffset = 0;
	return true;
}

bool Network::Tunneler::Tunnel::nextWrite(void)
{
	Network::Instance->overlay()->send(Overlay::Message(Overlay::Message::Tunnel, mBuffer, mNode));
	
	std::unique_lock<std::mutex> lock(mMutex);
	mBuffer.clear();
	mBuffer.writeBinary(mId);
	return true;
}

bool Network::Tunneler::Tunnel::isDatagram(void) const
{
	return true; 
}

bool Network::Tunneler::Tunnel::incoming(const Overlay::Message &message)
{
	if(message.type != Overlay::Message::Tunnel)
		return false;
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if(mClosed) return false;
		mQueue.push(message);
	}
	
	mCondition.notify_all();
	return true;
}

Network::Handler::Handler(Stream *stream, const Link &link) :
	mStream(stream),
	mLink(link),
	mTokens(DefaultTokens),
	mAvailableTokens(DefaultTokens),
	mThreshold(DefaultTokens/2),
	mAccumulator(0.),
	mLocalCap(0.),
	mLocalSequence(0),
	mLocalSideSeen(0),
	mLocalSideCount(0),
	mSideCount(0), 
	mCap(0),
	mRedundancy(1.25),	// TODO
	mTimeout(milliseconds(Config::Get("retransmit_timeout").toInt())),
	mClosed(false)
{
	// Set timeout alarm
	mTimeoutAlarm.set([this]()
	{
		timeout();
	});
	
	// Start handler thread
	mThread = std::thread([this]()
	{
		run();
	});
}

Network::Handler::~Handler(void)
{
	mTimeoutAlarm.join();
	mStream->close();
	
	if(mThread.get_id() == std::this_thread::get_id()) mThread.detach();
	else if(mThread.joinable()) mThread.join();
	
	delete mStream;
}

void Network::Handler::write(const String &type, const Serializable &content)
{
	String serialized;
	JsonSerializer(&serialized) << content;
	write(type, serialized);
}

void Network::Handler::write(const String &type, const String &record)
{
	std::unique_lock<std::mutex> lock(mMutex);
	writeRecord(type, record);
}

void Network::Handler::push(const BinaryString &target, unsigned tokens)
{
	std::unique_lock<std::mutex> lock(mMutex);
	if(mClosed) return;
	
	if(!tokens) mTargets.erase(target);
	{
		tokens = unsigned(double(tokens)*mRedundancy + 0.5);
		mTargets[target] = tokens;
	}
	
	send(false);
}

void Network::Handler::timeout(void)
{
	std::unique_lock<std::mutex> lock(mMutex);

	// Inactivity timeout is handled by Tunnel
	if(mAvailableTokens < 1.) mAvailableTokens = 1.;
	
	send(true);
}


bool Network::Handler::readRecord(String &type, String &record)
{
	if(mClosed) return false;
	
	try {
		if(readString(type))
		{
			AssertIO(readString(record));
			return true;
		}
	}
	catch(std::exception &e)
	{
		//LogDebug("Network::Handler::read", e.what());
		mClosed = true;
		throw Exception("Connection lost");
	}
	
	mClosed = true;
	return false;
}

void Network::Handler::writeRecord(const String &type, const String &record)
{
	writeString(type);
	writeString(record);
	flush();
}

bool Network::Handler::readString(String &str)
{
	str.clear();
	
	char chr;
	while(readBinary(&chr, 1))
	{
		if(chr == '\0') return true;
		str+= chr;
	}
	
	return false;
}

void Network::Handler::writeString(const String &str)
{
	char zero = '\0';
	writeBinary(str.data(), str.size());
	writeBinary(&zero, 1);
}

size_t Network::Handler::readData(char *buffer, size_t size)
{
	if(!size) return 0;
	
	size_t count = 0;
	while(true)
	{
		// Try to read
		size_t r;
		while(size && (r = mSink.read(buffer, size)))
		{
			buffer+= r;
			count+= r;
			size-= r;
		}
		
		if(count) break;
		
		// We need more combinations
		BinaryString target;
		Fountain::Combination combination;
		if(!recvCombination(target, combination))
			break;
		
		//LogDebug("Network::Handler::readString", "Received combination");
		
		if(target.empty())
		{
			if(!combination.isNull())
			{
				mSink.drop(combination.firstComponent());
				mSink.solve(combination);
			
				// We need to lock since we are calling send()
				std::unique_lock<std::mutex> lock(mMutex);

				if(!send(false))
					mTimeoutAlarm.schedule(mTimeout*0.1);
			}
		}
		else {
			if(Store::Instance->push(target, combination))
			{
				write("pull", Object()
					.insert("target", target)
					.insert("tokens", uint16_t(0)));
			}
		}
	}
	
	return count;
}

void Network::Handler::writeData(const char *data, size_t size)
{
	if(mClosed) return;
	
	mSourceBuffer.writeData(data, size);
}

void Network::Handler::flush(void)
{
	if(!mSourceBuffer.empty())
	{
		unsigned count = mSource.write(mSourceBuffer.data(), mSourceBuffer.size());
		mAccumulator+= mRedundancy*count;
		mSourceBuffer.clear();
	}
	
	// Already locked, we can call send() safely
	send(false);
}

bool Network::Handler::recvCombination(BinaryString &target, Fountain::Combination &combination)
{
	BinarySerializer s(mStream);
	
	// 32-bit header
	uint8_t  version    = 0;
	uint8_t  targetSize = 0;
	uint16_t dataSize   = 0;
	if(!(s >> version)) return false;
	AssertIO(s >> targetSize);
	AssertIO(s >> dataSize);
	
	// 64-bit flow acknowledgement
	uint32_t nextSeen = 0;
	uint32_t nextDecoded = 0;
	AssertIO(s >> nextSeen);	// 32-bit next seen
	AssertIO(s >> nextDecoded);	// 32-bit next decoded

	uint32_t sideSeen = 0;
	uint32_t sideCount = 0;
	uint32_t sequence = 0;
	uint32_t cap = 0;
	if(version & 0x01)	// side channel bit
	{
		// 64-bit side acknowledgement
		AssertIO(s >> sideSeen);	// 32-bit side seen
		AssertIO(s >> sideCount); 	// 32-bit side count
		
		// 64-bit side sequencing	
		AssertIO(s >> sequence);	// 32-bit sequence
		AssertIO(s >> cap);		// 32-bit count cap
	}

	// 64-bit combination descriptor
	AssertIO(s >> combination);
	
	// Target
	AssertIO(mStream->readBinary(target, targetSize) == targetSize);
	
	// Data
	BinaryString data;
	AssertIO(mStream->readBinary(data, dataSize) == dataSize); 
	combination.setCodedData(data);
	
	mStream->nextRead();

	VAR(target);
	
	{
		std::unique_lock<std::mutex> lock(mMutex);

		mCap = std::max(mCap, cap);	// Count cap, prevent counting redundant packets
		
		if(!target.empty())
		{
			mLocalSideSeen = std::max(mLocalSideSeen, sequence);	// update local side seen
			mLocalSideCount = std::min(mLocalSideCount+1, mCap);	// increment and cap local count
		}
		
		unsigned flowBacklog  = nextSeen - std::min(nextDecoded, nextSeen);	// packets seen but not decoded on remote side
		unsigned flowReceived = mSource.drop(nextSeen);				// packets newly decoded on remote side
		
		unsigned sideBacklog  = sideSeen - std::min(sideCount, sideSeen);	// packets seen but not counted on remote side
		unsigned sideReceived = sideCount - std::min(mSideCount, sideCount);	// packets newly counted on remote side
		
		unsigned backlog  = flowBacklog + sideBacklog;		// total backlog
		unsigned received = flowReceived + sideReceived;	// total received
		
		mSideCount = std::max(mSideCount, sideCount);	// update remote side count

		if(backlog < mThreshold || backlog > mThreshold*2.)
		{
			double tokens;
			if(mTokens < mThreshold)
			{
				// Slow start
				tokens = received*2.;
			}
			else {
				// Additive increase
				tokens = received*1./mTokens;
			}
			
			mTokens+= tokens;
			mAvailableTokens+= tokens;
		}
		else {
			// Congestion: Multiplicative decrease
			mThreshold = mTokens/2.;
			mTokens = 1.;
			mAvailableTokens = mTokens;	
		}
	}

	return true;
}

void Network::Handler::sendCombination(const BinaryString &target, const Fountain::Combination &combination)
{
	uint32_t nextSeen = mSink.nextSeen();
	uint32_t nextDecoded = mSink.nextDecoded();
	mAvailableTokens = std::max(0., mAvailableTokens - 1.);
	
	BinarySerializer s(mStream);
	
	uint8_t version = 0;
	if(!target.empty() || mLocalSideSeen)
		version|= 0x01; // side channel bit

	// 32-bit header
	s << uint8_t(version);
	s << uint8_t(target.size());
	s << uint16_t(combination.codedSize());
	
	// 64-bit flow acknowledgement
	s << uint32_t(nextSeen);	// 32-bit next seen
	s << uint32_t(nextDecoded);	// 32-bit next decoded

	if(version & 0x01)	// side channel bit
	{
		// 64-bit side acknowledgement
		s << uint32_t(mLocalSideSeen);		// 32-bit side seen
		s << uint32_t(mLocalSideCount);		// 32-bit side count
	
		// 64-bit side sequencing
		s << uint32_t(mLocalSequence);		// 32-bit sequence
		s << uint32_t(std::ceil(mLocalCap));	// 32-bit count cap
	}

	// 64-bit combination descriptor
	s << combination;
	
	// Target
	mStream->writeBinary(target.data(), target.size());
	
	// Data
	mStream->writeBinary(combination.data(), combination.codedSize());
	
	mStream->nextWrite();
}

int Network::Handler::send(bool force)
{
	if(mClosed) return 0;
	
	int count = 0;
	while(force || (mAvailableTokens >= 1. && (!mTargets.empty() || (mSource.rank() >= 1 && mAccumulator >= 1.))))
	{
		try {
			BinaryString target;
			Fountain::Combination combination;
			mSource.generate(combination);
			
			if(!combination.isNull())
			{
				//LogDebug("Network::Handler::send", "Sending combination (rank=" + String::number(mSource.rank()) + ", accumulator=" + String::number(mAccumulator)+", tokens=" + String::number(mAvailableTokens) + ")");
				
				mAccumulator = std::max(0., mAccumulator - 1.);
			}
			else if(!mTargets.empty())
			{
				// Pick at random
				int r = Random().uniform(0, int(mTargets.size()));
				auto it = mTargets.begin();
				while(r--) ++it;
				Assert(it != mTargets.end());
				
				target = it->first;
				unsigned &tokens = it->second;
				
				unsigned rank = 0;
				Store::Instance->pull(target, combination, &rank);
				
				tokens = std::min(tokens, unsigned(double(rank)*mRedundancy + 0.5));
				if(tokens) --tokens;
				
				++mLocalSequence;
				mLocalCap+= 1/mRedundancy;
				
				if(!tokens) mTargets.erase(it);
			}
			
			if(!combination.isNull())
				--mAvailableTokens;
			
			sendCombination(target, combination);
			++count;	
			force = false;
		}
		catch(const std::exception &e)
		{
			LogWarn("Network::Handler::send", String("Sending failed: ") + e.what());
			//mStream->close();
			mClosed = true;
			break;
		}
	}
	
	duration idleTimeout = milliseconds(Config::Get("idle_timeout").toDouble())*0.1;	// so the tunnel should not time out
	if(mSource.rank() == 0) mTimeoutAlarm.schedule(idleTimeout); 
	else mTimeoutAlarm.schedule(mTimeout);
	return count;
}

void Network::Handler::process(void)
{
	String type, record;
	while(readRecord(type, record))
	{
		try {
			JsonSerializer serializer(&record);
			Network::Instance->incoming(mLink, type, serializer);
		}
		catch(const std::exception &e)
		{
			LogWarn("Network::Handler::process", "Unable to process command (type=\"" + type + "\"): " + e.what());
		}
	}
}

void Network::Handler::run(void)
{
	LogDebug("Network::Handler", "Starting handler");
	
	try {
		process();
		
		LogDebug("Network::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::Handler", String("Closing handler: ") + e.what());
	}
	
	Network::Instance->unregisterHandler(mLink, this);
}

Network::Pusher::Pusher(void) :
	mRedundant(16)	// TODO
{
	mThread = std::thread([this]()
	{
		run();
	});
}

Network::Pusher::~Pusher(void)
{
	
}

void Network::Pusher::push(const BinaryString &target, const Identifier &destination, unsigned tokens)
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		
		if(!tokens) 
		{
			mTargets[target].erase(destination);
			if(mTargets[target].empty())
				mTargets.erase(target);
			
		}
		else {
			if(tokens < mRedundant) tokens*=2;
			else tokens+= mRedundant;
			mTargets[target][destination] = tokens;
		}
	}
	
	mCondition.notify_all();
}

void Network::Pusher::run(void)
{
	while(true)
	try {
		std::unique_lock<std::mutex> lock(mMutex);
		
		if(mTargets.empty())
		{
			mCondition.wait(lock, [this]() {
				return !mTargets.empty();
			});
		}
		
		bool congestion = false;	// local congestion
		auto it = mTargets.begin();
		while(it != mTargets.end())
		{
			const BinaryString &target = it->first;
			
			auto jt = it->second.begin();
			while(jt != it->second.end())
			{
				const Identifier &destination = jt->first;
				unsigned &tokens = jt->second;
				
				if(tokens)
				{
					unsigned rank = 0;
					Fountain::Combination combination;
					Store::Instance->pull(target, combination, &rank);
					
					tokens = std::min(tokens, rank + mRedundant);
					--tokens;
					
					Overlay::Message data(Overlay::Message::Data, "", destination, target);
					BinarySerializer(&data.content) << combination;
					data.content.writeBinary(combination.data(), combination.codedSize());
					
					congestion|= !Network::Instance->overlay()->send(data);
				}
				
				if(!tokens) it->second.erase(jt++);
				else ++jt;
			}
			
			if(it->second.empty()) mTargets.erase(it++);
			else ++it;
		}
		
		if(congestion)
		{
			lock.unlock();
			std::this_thread::yield();
		}
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::Pusher::run", e.what());
	}
}

}
