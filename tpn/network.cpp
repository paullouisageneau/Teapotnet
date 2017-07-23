/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

const unsigned Network::DefaultTokens = 4;
const unsigned Network::DefaultThreshold = Network::DefaultTokens*64;
const unsigned Network::TunnelMtu = 1200;
const unsigned Network::DefaultRedundantCount = 32;
const double   Network::DefaultRedundancy = 1.20;
const double   Network::DefaultPacketRate = 1000.;	// Packets/second
const duration Network::CallPeriod = seconds(1.);
const duration Network::CallFallbackTimeout = seconds(10.);

Network *Network::Instance = NULL;
const Network::Link Network::Link::Null;

Network::Network(int port) :
	mOverlay(port),
	mPool(4)
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
		mTunneler.open(node, remote, user);
}

void Network::registerCaller(const BinaryString &target, Caller *caller)
{
	Assert(caller);

	LogDebug("Network::registerCaller", "Calling " + target.toString());

	bool first = false;
	{
		std::unique_lock<std::mutex> lock(mCallersMutex);
		first = !mCallers.contains(target);
		mCallers[target].insert(caller);
	}

	if(first) directCall(target, Store::Instance->missing(target));
}

void Network::unregisterCaller(const BinaryString &target, Caller *caller)
{
	Assert(caller);

	bool last = false;
	{
		std::unique_lock<std::mutex> lock(mCallersMutex);

		auto it = mCallers.find(target);
		if(it != mCallers.end())
		{
			it->second.erase(caller);
			if(it->second.empty())
			{
				mCallers.erase(it);
				last = true;
			}
		}
	}

	if(last) directCall(target, 0);
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
		std::unique_lock<std::recursive_mutex> lock(mSubscribersMutex);

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
		std::unique_lock<std::recursive_mutex> lock(mPublishersMutex);
		mPublishers[prefix].insert(publisher);
	}
}

void Network::unpublish(String prefix, Publisher *publisher)
{
	Assert(publisher);

	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);

	{
		std::unique_lock<std::recursive_mutex> lock(mPublishersMutex);

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
		std::unique_lock<std::recursive_mutex> lock(mSubscribersMutex);
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
			for(auto t : targets)
			{
				subscriber->incoming(Locator(prefix, "/"), t);
			}
	}
}

void Network::unsubscribe(String prefix, Subscriber *subscriber)
{
	Assert(subscriber);

	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);

	{
		std::unique_lock<std::recursive_mutex> lock(mSubscribersMutex);

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
	Locator locator(prefix, path);
	LogDebug("Network::publish", "Advertising " + locator.fullPath());
	matchSubscribers(locator.fullPath(), publisher->link(), publisher);
}

void Network::issue(String prefix, const String &path, Publisher *publisher, const Mail &mail)
{
	if(mail.empty()) return;
	Locator locator(prefix, path);
	LogDebug("Network::issue", "Issuing " + mail.digest().toString());
	matchSubscribers(locator.fullPath(), publisher->link(), mail);
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

bool Network::retrieveValue(const BinaryString &key, Set<BinaryString> &values, duration timeout)
{
	return mOverlay.retrieve(key, values, timeout);
}

bool Network::hasLink(const Identifier &local, const Identifier &remote) const
{
	// Alias
	return hasLink(Link(local, remote));
}

bool Network::hasLink(const Link &link) const
{
	std::unique_lock<std::recursive_mutex> lock(mHandlersMutex);
	return mHandlers.contains(link);
}

bool Network::getLinkFromNode(const BinaryString &node, Link &link) const
{
	std::unique_lock<std::mutex> lock(mLinksFromNodesMutex);

	auto it = mLinksFromNodes.find(node);
	if(it != mLinksFromNodes.end() && !it->second.empty())
	{
		link = it->second.back();
		return true;
	}
	else return false;
}

void Network::run(void)
{
	const duration period = CallPeriod;

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
					const BinaryString &key = message.source;

					uint64_t ts = 0;
					BinaryString value = message.content;
					Assert(value.readBinary(ts) && !value.empty());

					// It can be about a block
					matchCallers(key, value);

					// Or it can be about a contact
					matchListeners(key, value);

					break;
				}

			// Call
			case Overlay::Message::Call:
				{
					uint16_t tokens = 0;
					BinaryString target;
					message.content.readBinary(tokens);
					message.content.readBinary(target);

					if(Store::Instance->hasBlock(target))
					{
						if(tokens) LogDebug("Network::run", "Called " + target.toString() + " (" + String::number(tokens) + " tokens)");
						pushRaw(message.source, target, tokens);
					}
					else {
						LogDebug("Network::run", "Called (unknown) " + target.toString());
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
		sendCalls();

		// Send beacons
		if(loops % 10 == 0) sendBeacons();
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::run", e.what());
	}
}

void Network::sendCalls(void)
{
	Set<BinaryString> targets;
	{
		std::unique_lock<std::mutex> lock(mCallersMutex);

		for(auto it = mCallers.begin(); it != mCallers.end(); ++it)
		{
			const BinaryString &target = it->first;

			for(const Caller *caller : it->second)
			{
				if(caller->elapsed() >= CallFallbackTimeout)
				{
					targets.insert(target);
					break;
				}
			}
		}
	}

	for(auto target : targets)
		fallbackCall(target, Store::Instance->missing(target));
}

void Network::sendBeacons(void)
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

void Network::registerHandler(const Link &link, sptr<Handler> handler)
{
	Assert(!link.local.empty());
	Assert(!link.remote.empty());
	Assert(!link.node.empty());
	Assert(handler);

	sptr<Handler> currentHandler;
	{
		std::unique_lock<std::recursive_mutex> lock(mHandlersMutex);

		if(mHandlers.get(link, currentHandler))
			currentHandler->stop();

		mHandlers.insert(link, handler);
		handler->start();
	}

	{
		std::unique_lock<std::mutex> lock(mLinksFromNodesMutex);
		mLinksFromNodes[link.node].push_back(link);
	}

	{
		std::unique_lock<std::recursive_mutex> lock(mSubscribersMutex);

		for(auto it = mSubscribers.begin(); it != mSubscribers.end(); ++it)
			for(Subscriber *subscriber : it->second)
				if(subscriber->link() == link)
				{
					send(link, "subscribe",
						Object()
							.insert("path", it->first));
					break;
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
		std::unique_lock<std::mutex> lock(mLinksFromNodesMutex);

		auto it = mLinksFromNodes.find(link.node);
		if(it != mLinksFromNodes.end())
		{
			it->second.remove(link);
			if(it->second.empty())
				mLinksFromNodes.erase(it);
		}
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

	LogDebug("Network::outgoing", "Sending command (type=\"" + type + "\") on " + String::number(handlers.size()) + " links");
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

	if(tokens) LogDebug("Network::push", "Pushing " + target.toString() + " on " + String::number(handlers.size()) + " links");

	for(auto h : handlers)
		h->push(target, tokens);

	return !handlers.empty();
}

bool Network::pushRaw(const BinaryString &node, const BinaryString &target, unsigned tokens)
{
	mPusher.push(target, node, tokens);	// Warning: order is different
	return true;
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

		if(tokens) LogDebug("Network::incoming", "Pulled " + target.toString() + " (" + String::number(tokens) + " tokens)");

		if(!push(link, target, tokens))
			LogWarn("Network::incoming", "Failed to push " + target.toString());
	}
	else if(type == "push")
	{
		BinaryString target;
		serializer >> Object()
				.insert("target", target);

		unsigned tokens = Store::Instance->missing(target);
		if(tokens) directCall(target, tokens);
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

		if(!path.empty() && path[path.size()-1] == '/')
			path.resize(path.size()-1);

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
			// We check in cache to prevent publishing loops
			hasNew|= !Store::Instance->hasValue(key, target);

			Store::Instance->storeValue(key, target, Store::Temporary);		// cache path resolution
			Store::Instance->storeValue(target, link.node, Store::Temporary);	// cache candidate node
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

		if(!path.empty() && path[path.size()-1] == '/')
		path.resize(path.size()-1);

		addRemoteSubscriber(link, path);
	}
	else if(type == "invite")
	{
		String name;
		serializer >> Object()
				.insert("name", name);

		sptr<User> user = User::GetByIdentifier(link.local);
		if(user && !name.empty()) user->invite(link.remote, name);
	}
	else {
		return onRecv(link, type, serializer);
	}

	return true;
}

bool Network::directCall(const BinaryString &target, unsigned tokens)
{
	// Get hints from Store
	Set<BinaryString> hints;
	Store::Instance->getBlockHints(target, hints);
	hints.insert(target);

	// Retrieve candidate links for pulling
	Set<Link> links;
	for(const BinaryString &hint : hints)
	{
		Set<BinaryString> nodes;
		Store::Instance->retrieveValue(hint, nodes);
		for(auto n : nodes)
		{
			Link l;
			if(getLinkFromNode(n, l))
				links.insert(l);
		}
	}

	if(links.empty()) return false;

	LogDebug("Network::run", "Pulling " + target.toString() + " from " + String::number(links.size()) + " users");

	// Immediately send pull
	if(tokens) tokens = (tokens + (links.size()-1))/links.size();
	for(auto link : links)
	{
		send(link, "pull", Object()
			.insert("target", target)
			.insert("tokens", uint16_t(tokens)));
	}

	return true;
}

bool Network::fallbackCall(const BinaryString &target, unsigned tokens)
{
	// Get hints from Store
	Set<BinaryString> hints;
	Store::Instance->getBlockHints(target, hints);
	hints.insert(target);

	// Call nodes providing hinted blocks and target block
	bool success = false;
	for(const BinaryString &hint : hints)
	{
		Set<BinaryString> nodes;
		if(Store::Instance->retrieveValue(hint, nodes))
		{
			BinaryString call;
			call.writeBinary(uint16_t(tokens));
			call.writeBinary(target);

			for(auto n : nodes)
				success|= mOverlay.send(Overlay::Message(Overlay::Message::Call, call, n));
		}

		// Send retrieve for node
		mOverlay.retrieve(hint);
	}

	return success;
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
		std::unique_lock<std::recursive_mutex> lock(mPublishersMutex);

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
				if(publisher->anounce(Locator(prefix, truncatedPath, link), result))
				{
					Assert(!result.empty());
					if(subscriber) 	// local
					{
						for(const auto &t : result)
							subscriber->incoming(Locator(prefix, truncatedPath, publisher->link()), t);
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
		std::unique_lock<std::recursive_mutex> lock(mSubscribersMutex);

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
				if(publisher->anounce(Locator(prefix, truncatedPath, link), targets))
				{
					for(const auto &t : targets)
					{
						// TODO: should prevent forwarding in case we want to republish another content
						subscriber->incoming(Locator(prefix, truncatedPath, publisher->link()), t);
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
		std::unique_lock<std::recursive_mutex> lock(mSubscribersMutex);

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

				subscriber->incoming(Locator(prefix, truncatedPath, link), mail);
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

Network::Link::Link(Identifier _local, Identifier _remote, Identifier _node) :
	local(std::move(_local)),
	remote(std::move(_remote)),
	node(std::move(_node))
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

Network::Locator::Locator(void)
{

}

Network::Locator::Locator(String _prefix, String _path, Link _link) :
	link(std::move(_link)),
	prefix(std::move(_prefix)),
	path(std::move(_path))
{
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
}

String Network::Locator::fullPath(void) const
{
	String fullPath(prefix + path);
	if(fullPath[fullPath.size()-1] == '/')
		fullPath.resize(fullPath.size()-1);
	return fullPath;
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
	mLink(link)
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

bool Network::Subscriber::fetch(const Locator &locator, const BinaryString &target, bool fetchContent)
{
	// Test local availability
	if(Store::Instance->hasBlock(target))
	{
		Resource resource(target, true);	// local only
		if(!fetchContent || resource.isLocallyAvailable())
			return true;
	}

	// Schedule fetch task
	Network::Instance->mPool.enqueue([this, locator, target, fetchContent]()
	{
		try {
			Resource resource(target);

			if(fetchContent)
			{
				Resource::Reader reader(&resource, "", true);	// empty password + no check
				reader.discard();				// read everything
			}

			incoming(locator, target);
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

bool Network::RemotePublisher::anounce(const Locator &locator, List<BinaryString> &targets)
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

bool Network::RemoteSubscriber::incoming(const Locator &locator, const BinaryString &target)
{
	if(!locator.link.remote.empty() && locator.link == this->link())
		return false;

	//LogDebug("Network::RemoteSubscriber::incoming", "Publishing " + target.toString() + " for " + fullPath);

	Array<BinaryString> targets;
	targets.append(target);

	Network::Instance->send(this->link(), "publish",
		Object()
			.insert("path", locator.fullPath())
			.insert("targets", targets));

	return true;
}

bool Network::RemoteSubscriber::incoming(const Locator &locator, const Mail &mail)
{
	if(!locator.link.remote.empty() && locator.link == this->link())
		return false;

	Network::Instance->send(this->link(), "publish",
		Object()
			.insert("path", locator.fullPath())
			.insert("message", mail));

	return true;
}

bool Network::RemoteSubscriber::localOnly(void) const
{
	return true;
}

Network::Caller::Caller(void) :
	mStartTime(std::chrono::steady_clock::time_point::min())
{

}

Network::Caller::Caller(const BinaryString &target)
{
	Assert(!target.empty());
	startCalling(target);
}

Network::Caller::~Caller(void)
{
	stopCalling();
}

void Network::Caller::startCalling(const BinaryString &target)
{
	if(target != mTarget)
	{
		stopCalling();

		mTarget = target;
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
	}
}

BinaryString Network::Caller::target(void) const
{
	return mTarget;
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
	mPool(8),
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

bool Network::Tunneler::open(const BinaryString &node, const Identifier &remote, User *user)
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

	{
		std::unique_lock<std::mutex> lock(mTunnelsMutex);
		if(mPending.contains(node))
			return false;
	}

	mPool.enqueue([this, node, remote, user]()
	{
		uint64_t tunnelId = 0;
		Random().readBinary(tunnelId);	// Generate random tunnel ID

		BinaryString local(user->identifier());
		Link link(local, remote, node);

		if(Network::Instance->hasLink(link))
			return false;

		SecureTransport *transport = NULL;
		{
			std::unique_lock<std::mutex> lock(mTunnelsMutex);
			if(mPending.contains(node))
				return false;

			mPending.insert(node, tunnelId);

			//LogDebug("Network::Tunneler::open", "Opening tunnel to " + node.toString() + ": " + String::hexa(tunnelId));

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
				lock.unlock();	// tunnel will unregister
				delete tunnel;
				delete transport;
				mPending.erase(node);
				throw;
			}
		}

		return handshake(transport, link);
	});

	return true;
}

SecureTransport *Network::Tunneler::listen(BinaryString *source)
{
	while(true)
	{
		Overlay::Message message;
		{
			std::unique_lock<std::mutex> lock(mMutex);

			mCondition.wait(lock, [this]() {
				return !mQueue.empty() || mStop;
			});

			if(mStop) break;

			message = mQueue.front();
			mQueue.pop();
		}

		const BinaryString &node = message.source;

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
				//LogDebug("Network::Tunneler::listen", "Message tunnel from " + message.source.toString() + ": " + String::hexa(tunnelId));
				tunnel->incoming(message);
			}
			else {
				auto it = mPending.find(node);
				if(it != mPending.end() && it->second >= tunnelId)
					continue;

				mPending.insert(node, tunnelId);

				LogDebug("Network::Tunneler::listen", "Incoming tunnel from " + node.toString() + ": " + String::hexa(tunnelId));

				SecureTransport *transport = NULL;
				try {
					tunnel = new Tunneler::Tunnel(this, tunnelId, node);
					transport = new SecureTransportServer(tunnel, NULL, true);	// ask for certificate
					mTunnels.insert(tunnel->id(), tunnel);
				}
				catch(...)
				{
					lock.unlock();	// tunnel will unregister
					delete tunnel;
					delete transport;
					mPending.erase(node);
					throw;
				}

				tunnel->incoming(message);

				if(source) *source = node;
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
	mTunnels.erase(id);
}

void Network::Tunneler::removePending(const BinaryString &node)
{
	std::unique_lock<std::mutex> lock(mTunnelsMutex);
	mPending.erase(node);
}

bool Network::Tunneler::handshake(SecureTransport *transport, const Link &link)
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

			sptr<User> user = User::GetByIdentifier(local);
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
			remote = publicKey.fingerprint<Sha3_256>();

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
		mPool.enqueue([transport, link, this /* for removePending() */]()
		{
			//LogDebug("Network::Tunneler::handshake", "HandshakeTask starting...");

			try {
				// Set verifier
				MyVerifier verifier;
				verifier.local = link.local;
				verifier.node  = link.node;
				transport->setVerifier(&verifier);

				// Set MTU
				transport->setDatagramMtu(TunnelMtu);

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

				removePending(link.node);
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

			removePending(link.node);
			delete transport;
			return false;
		});

		return true;
	}
	catch(const std::exception &e)
	{
		LogError("Network::Tunneler::handshake", e.what());
		removePending(link.node);
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

			handshake(transport, Link(Identifier::Empty, Identifier::Empty, node));
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
	std::this_thread::sleep_for(seconds(1.));
	std::unique_lock<std::mutex> lock(mMutex);
}

uint64_t Network::Tunneler::Tunnel::id(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
	return mId;
}

BinaryString Network::Tunneler::Tunnel::node(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
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

	mCondition.wait_for(lock, mTimeout, [this]() {
		return mClosed || !mQueue.empty();
	});

	if(mClosed) return 0;
	if(mQueue.empty()) throw Timeout();

	const Overlay::Message &message = mQueue.front();
	Assert(mOffset <= message.content.size());

	size = std::min(size, size_t(message.content.size() - mOffset));
	std::copy(message.content.data() + mOffset, message.content.data() + mOffset + size, buffer);
	mOffset+= size;
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

	return mCondition.wait_for(lock, timeout, [this]() {
		return mClosed || !mQueue.empty();
	});
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
	std::unique_lock<std::mutex> lock(mMutex);

	Network::Instance->overlay()->send(Overlay::Message(Overlay::Message::Tunnel, mBuffer, mNode));

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
	mThreshold(DefaultThreshold),
	mAccumulator(0.),
	mLocalSideSequence(0.),
	mRedundancy(DefaultRedundancy),
	mLocalSideSeen(0),
	mLocalSideCount(0),
	mSideSeen(0),
	mSideCount(0),
	mCongestion(false),
	mTimeout(milliseconds(Config::Get("retransmit_timeout").toDouble())),
	mKeepaliveTimeout(milliseconds(Config::Get("keepalive_timeout").toDouble())),	// so the tunnel should not time out
	mClosed(false)
{
	Assert(mStream);

	// Set timeout alarm
	mTimeoutAlarm.set([this]()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		LogDebug("Network::Handler", "Triggered timeout");
		send(true);
	});

	// Set acknowledge alarm
	mAcknowledgeAlarm.set([this]()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		LogDebug("Network::Handler", "Triggered acknowledge");
		send(true);
	});
}

Network::Handler::~Handler(void)
{
	// Close stream
	stop();

	// Cancel alarms
	mTimeoutAlarm.cancel();
	mAcknowledgeAlarm.cancel();

	// Join thread
	if(mThread.get_id() == std::this_thread::get_id()) mThread.detach();
	else if(mThread.joinable()) mThread.join();

	// Delete stream
	std::unique_lock<std::mutex> lock(mMutex);
	delete mStream;
}

void Network::Handler::start(void)
{
	// Start handler thread
	mThread = std::thread([this]()
	{
		run();

		Network::Instance->unregisterHandler(mLink, this);
	});
}

void Network::Handler::stop(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mStream->close();
	mClosed = true;
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

	if(!tokens)
	{
		mTargets.remove_if([target](const Target &t)
		{
			return t.digest == target;
		});
	}
	else {
		tokens = unsigned(std::ceil(double(tokens)*mRedundancy));

		auto it = mTargets.begin();
		while(it != mTargets.end())
		{
			if(it->digest == target)
				break;
			++it;
		}

		if(it != mTargets.end())
		{
			it->tokens = std::min(it->tokens + DefaultRedundantCount, tokens);
		}
		else {
			Target t;
			t.digest = target;
			t.tokens = tokens;
			mTargets.push_back(t);
		}
	}

	send(false);
}

Network::Link Network::Handler::link(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mLink;
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
		LogDebug("Network::Handler::read", e.what());
		throw Exception("Connection lost");
	}

	return false;
}

void Network::Handler::writeRecord(const String &type, const Serializable &content, bool dontsend)
{
	String serialized;
	JsonSerializer(&serialized) << content;
	writeRecord(type, serialized, dontsend);
}

void Network::Handler::writeRecord(const String &type, const String &record, bool dontsend)
{
	writeString(type);
	writeString(record);
	flush(dontsend);
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
	if(mClosed || !size) return 0;

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

		if(!combination.isNull() || !target.empty())
		{
			if(target.empty())
			{
				LogDebug("Network::Handler::recvCombination", "Received flow combination (first=" + String::number(combination.firstComponent()) + ")");

				mSink.drop(combination.firstComponent());
				mSink.solve(combination);
			}
			else {
				//LogDebug("Network::Handler::recvCombination", "Received side combination (target=" + target.toString() + ")");

				if(Store::Instance->push(target, combination))
					Network::Instance->unregisterAllCallers(target);
			}

			// We need to lock since we are calling send()
			std::unique_lock<std::mutex> lock(mMutex);

			if(!send(false))
			{
				if(!mClosed && !mAcknowledgeAlarm.isScheduled())
					mAcknowledgeAlarm.schedule(mTimeout*0.1);
			}
		}
		else {
			//LogDebug("Network::Handler::recvCombination", "Received null combination");
		}
	}

	return count;
}

void Network::Handler::writeData(const char *data, size_t size)
{
	if(mClosed) return;

	mSourceBuffer.writeData(data, size);
}

void Network::Handler::flush(bool dontsend)
{
	if(mClosed) return;

	if(!mSourceBuffer.empty())
	{
		unsigned count = mSource.write(mSourceBuffer.data(), mSourceBuffer.size());
		mAccumulator+= mRedundancy*count;
		mSourceBuffer.clear();
	}

	// Already locked, we can call send() safely
	if(!dontsend) send(false);
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

	// 32-bit sequence
	uint32_t sequence = 0;
	AssertIO(s >> sequence);	// 32-bit sequence

	// 64-bit flow acknowledgement
	uint32_t nextSeen = 0;
	uint32_t nextDecoded = 0;
	AssertIO(s >> nextSeen);	// 32-bit next seen
	AssertIO(s >> nextDecoded);	// 32-bit next decoded

	uint32_t sideSeen = 0;
	uint32_t sideCount = 0;

	if(version & 0x01)	// side channel bit
	{
		// 64-bit side acknowledgement
		AssertIO(s >> sideSeen);	// 32-bit side seen
		AssertIO(s >> sideCount); 	// 32-bit side count
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

	{
		std::unique_lock<std::mutex> lock(mMutex);

		if(!target.empty())
		{
			mLocalSideSeen  = std::max(mLocalSideSeen, sequence);		// update local side seen
			mLocalSideCount = std::min(mLocalSideCount+1, mLocalSideSeen);	// increment and cap local count
		}

		// Compute received
		unsigned flowReceived = mSource.drop(nextSeen);
		unsigned sideReceived = sideSeen - std::min(mSideSeen, sideSeen);
		unsigned received = flowReceived + sideReceived;

		const double alpha = 2.;	// Slow start factor
		const double beta  = 1.;	// Additive increase factor
		const double gamma = 0.5;	// Multiplicative decrease factor
		const unsigned trigger = 16;	// Congestion trigger

		double delta;
		if(mTokens < mThreshold)
		{
			// Slow start
			delta = alpha;
		}
		else {
			// Additive increase
			delta = beta/std::max(mTokens, 1.);
			mCongestion = false;
		}

		mTokens+= delta*received;
		mAvailableTokens+= (mRedundancy + delta)*received;
		mAvailableTokens = std::min(mAvailableTokens, mTokens);

		unsigned backlog = nextSeen - nextDecoded;
		mAccumulator+= mRedundancy*backlog;
		mAccumulator = std::min(mAccumulator, mRedundancy*mSource.rank());

		if(backlog > mSource.rank() + trigger || sideSeen > sideCount + trigger)
		{
			if(!mCongestion)
			{
				// Congestion: Multiplicative decrease
				mCongestion = true;
				mThreshold = mTokens*gamma;
				mTokens = double(DefaultTokens);
				mAvailableTokens = mTokens;
			}
		}
		else mCongestion = false;

		mSideSeen  = std::max(mSideSeen,  sideSeen);	// update remote side seen
		mSideCount = std::max(mSideCount, sideCount);	// update remote side count

		if(received) LogDebug("Network::Handler::recvCombination", "Acknowledged: flow="+String::number(nextSeen)+", side="+String::number(sideSeen)+" (received=" + String::number(flowReceived) + "+" + String::number(sideReceived) + ", tokens=" + String::number(unsigned(mTokens)) + ", available=" + String::number(unsigned(mAvailableTokens)) +", threshold=" + String::number(unsigned(mThreshold)) + ")");
	}

	return true;
}

void Network::Handler::sendCombination(const BinaryString &target, const Fountain::Combination &combination)
{
	uint32_t nextSeen = mSink.nextSeen();
	uint32_t nextDecoded = mSink.nextDecoded();

	uint32_t sequence;
	if(target.empty()) sequence = combination.lastComponent();
	else sequence = std::ceil(mLocalSideSequence);

	BinarySerializer s(mStream);

	uint8_t version = 0;
	if(mLocalSideSeen) version|= 0x01; // side channel bit

	// 32-bit header
	s << uint8_t(version);
	s << uint8_t(target.size());
	s << uint16_t(combination.codedSize());

	// 32-bit sequencing
	s << uint32_t(sequence);

	// 64-bit flow acknowledgement
	s << uint32_t(nextSeen);	// 32-bit next seen
	s << uint32_t(nextDecoded);	// 32-bit next decoded

	if(version & 0x01)	// side channel bit
	{
		// 64-bit side acknowledgement
		s << uint32_t(mLocalSideSeen);		// 32-bit side seen
		s << uint32_t(mLocalSideCount);		// 32-bit side count
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

			if(mSource.rank() >= 1 && mAccumulator >= 1.)
				mSource.generate(combination);

			if(!combination.isNull())
			{
				LogDebug("Network::Handler::send", "Sending flow combination (rank=" + String::number(mSource.rank()) + ", accumulator=" + String::number(mAccumulator) + ", tokens=" + String::number(unsigned(mTokens)) + ", available=" + String::number(unsigned(mAvailableTokens)) + ")");

				mAccumulator = std::max(0., mAccumulator - 1.);
			}
			else if(!mTargets.empty())
			{
				//LogDebug("Network::Handler::send", "Sending side combination (tokens=" + String::number(mTokens) + ", available=" + String::number(mAvailableTokens) + ")");

				target = mTargets.begin()->digest;
				Assert(!target.empty());

				unsigned &tokens = mTargets.begin()->tokens;
				unsigned rank = 0;

				Store::Instance->pull(target, combination, &rank);

				mLocalSideSequence+= 1/mRedundancy;

				tokens = std::min(tokens, unsigned(double(rank)*mRedundancy + 0.5));
				if(tokens) --tokens;

				if(!tokens)
				{
					writeRecord("push",
						Object()
							.insert("target", target),
						true);

					mTargets.pop_front();
				}
			}

			if(mAvailableTokens >= 1. && !combination.isNull())
				mAvailableTokens-= 1.;

			sendCombination(target, combination);

			++count;
			force = false;
		}
		catch(const std::exception &e)
		{
			LogWarn("Network::Handler::send", String("Sending failed: ") + e.what());
			mStream->close();
			mClosed = true;
			break;
		}
	}

	// Reset timeout
	if(!mClosed)
	{
		duration timeout = mKeepaliveTimeout;
		if(mSource.rank() >= 1 || !mTargets.empty())
			timeout = std::min(timeout, mTimeout);
		mTimeoutAlarm.schedule(timeout);
	}

	return count;
}

void Network::Handler::run(void)
{
	LogDebug("Network::Handler", "Starting handler");

	try {
		String type, record;
		while(readRecord(type, record))
		{
			try {
				JsonSerializer serializer(&record);
				Network::Instance->incoming(mLink, type, serializer);
			}
			catch(const std::exception &e)
			{
				LogWarn("Network::Handler", "Unable to process command (type=\"" + type + "\"): " + e.what());
			}
		}

		LogDebug("Network::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::Handler", String("Closing handler: ") + e.what());
	}
}

Network::Pusher::Pusher(void) :
	mRedundant(DefaultRedundantCount),
	mPeriod(seconds(1.)/DefaultPacketRate)
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
			auto it = mTargets.find(destination);
			if(it != mTargets.end())
			{
				List<Target> &list = it->second;

				list.remove_if([target](const Target &t) {
					return t.digest == target;
				});

				if(list.empty())
					mTargets.erase(it);
			}
		}
		else {
			if(tokens < mRedundant) tokens*=2;
			else tokens+= mRedundant;

			List<Target> &list = mTargets[destination];

			auto jt = find_if(list.begin(), list.end(), [target](const Target &t) {
				return (t.digest == target);
			});

			if(jt != list.end())
			{
				jt->tokens = std::min(jt->tokens + mRedundant, tokens);
			}
			else {
				Target t;
				t.digest = target;
				t.tokens = tokens;
				list.push_back(t);
			}
		}
	}

	mCondition.notify_all();
}

void Network::Pusher::run(void)
{
	using clock = std::chrono::steady_clock;
	auto currentTime = clock::now();
	duration accumulator = seconds(0.);

	while(true)
	try {
		std::unique_lock<std::mutex> lock(mMutex);

		if(!mTargets.empty())
		{
			mCondition.wait_for(lock, mPeriod);
			if(mTargets.empty()) continue;
		}
		else {
			mCondition.wait(lock, [this]() {
					return !mTargets.empty();
			});
			currentTime = clock::now();
			accumulator = mPeriod;
		}

		duration elapsed = clock::now() - currentTime;
		accumulator+= elapsed;
		currentTime = clock::now();

		auto it = mTargets.begin();
		std::advance(it, Random().uniform(0, int(mTargets.size())));	// first element is random
		while(accumulator >= mPeriod && !mTargets.empty())
		{
			accumulator-= mPeriod;

			List<Target> &list = it->second;
			if(!list.empty())
			{
				const BinaryString &destination = it->first;
				const Identifier &target = list.begin()->digest;
				unsigned &tokens = list.begin()->tokens;

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

					Network::Instance->overlay()->send(data);
				}

				if(!tokens) list.pop_front();
			}

			if(list.empty()) mTargets.erase(it++);
			else ++it;
			if(it == mTargets.end()) it = mTargets.begin();
		}
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::Pusher::run", e.what());
	}
}

}
