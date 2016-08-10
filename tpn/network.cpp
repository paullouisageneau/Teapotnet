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

const unsigned Network::DefaultTokens = 8;
const unsigned Network::DefaultThreshold = Network::DefaultTokens*16;

Network *Network::Instance = NULL;
const Network::Link Network::Link::Null;

Network::Network(int port) :
		mOverlay(port),
		mThreadPool(1, 8, Config::Get("max_connections").toInt())
{

}

Network::~Network(void)
{
	join();
}

void Network::start(void)
{
	mOverlay.start();
	mTunneler.start();
	mPusher.start();
	Thread::start();
}

void Network::join(void)
{
	Thread::join();
	mPusher.join();
	mTunneler.join();
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
	Synchronize(this);
	Assert(caller);
	
	LogDebug("Network::registerCaller", "Calling " + target.toString());

	// Immediately send to node providing hinted block
	if(!caller->hint().empty())
	{
		Set<BinaryString> nodes;
		if(Store::Instance->retrieveValue(caller->hint(), nodes))
		{
			uint16_t tokens = uint16_t(Store::Instance->missing(target));
			BinaryString call;
			call.writeBinary(tokens);
			call.writeBinary(target);
			
			for(Set<BinaryString>::iterator kt = nodes.begin(); kt != nodes.end(); ++kt)
				mOverlay.send(Overlay::Message(Overlay::Message::Call, call, *kt));
		}
		
		// Send retrieve for node
		mOverlay.retrieve(caller->hint());
	}
	
	// Immediately send retrieve for target
	mOverlay.retrieve(target);
	
	mCallers[target].insert(caller);
}

void Network::unregisterCaller(const BinaryString &target, Caller *caller)
{
	Synchronize(this);
	Assert(caller);
	
	Map<BinaryString, Set<Caller*> >::iterator it = mCallers.find(target);
	if(it != mCallers.end())
	{
		it->second.erase(caller);
		if(it->second.empty())   
			mCallers.erase(it);
	}
}

void Network::unregisterAllCallers(const BinaryString &target)
{
	Synchronize(this);
	mCallers.erase(target);
}

void Network::registerListener(const Identifier &local, const Identifier &remote, Listener *listener)
{
	Synchronize(this);
	Assert(listener);
	
	mListeners[IdentifierPair(remote, local)].insert(listener);
	
	Link link(local, remote);

	Set<Link> temp;		// We need to copy the links since we are going to desynchronize
	for(Map<Link, Handler*>::iterator it = mHandlers.lower_bound(link);
		it != mHandlers.end() && (it->first.local == local && it->first.remote == remote);
		++it)
	{
		temp.insert(it->first);
	}
	
	for(Set<Link>::iterator it = temp.begin(); it != temp.end(); ++it)
	{
		Desynchronize(this);
		listener->seen(*it);      // so Listener::seen() is triggered even with incoming tunnels
                listener->connected(*it, true);
	}

	for(Map<String, Set<Subscriber*> >::iterator it = mSubscribers.begin();
		it != mSubscribers.end();
		++it)
	{
		for(Set<Subscriber*>::iterator jt = it->second.begin();
			jt != it->second.end();
			++jt)
		{
			if((*jt)->link() == link)
			{
				send(link, "subscribe", 
					ConstObject()
						.insert("pa.hpp", &it->first));
				break;
			}
		}
	}
}

void Network::unregisterListener(const Identifier &local, const Identifier &remote, Listener *listener)
{
	Synchronize(this);
	Assert(listener);
	
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(remote, local));
	if(it != mListeners.end())
	{
		it->second.erase(listener);
		if(it->second.empty())   
			mListeners.erase(it);
	}
}

void Network::publish(String prefix, Publisher *publisher)
{
	Synchronize(this);
	Assert(publisher);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	//LogDebug("Network::publi.hpp", "Publishing " + prefix);
	
	mPublishers[prefix].insert(publisher);
}

void Network::unpublish(String prefix, Publisher *publisher)
{
	Synchronize(this);
	Assert(publisher);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	Map<String, Set<Publisher*> >::iterator it = mPublishers.find(prefix);
	if(it != mPublishers.end())
	{
		it->second.erase(publisher);
		if(it->second.empty()) 
			mPublishers.erase(it);
	}
}

void Network::subscribe(String prefix, Subscriber *subscriber)
{
	Synchronize(this);
	Assert(subscriber);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	//LogDebug("Network::subscribe", "Subscribing " + prefix);
	
	mSubscribers[prefix].insert(subscriber);
	
	// Local publishers
	matchPublishers(prefix, subscriber->link(), subscriber);
	
	// Remote publishers
	if(!subscriber->localOnly())
	{
		// Immediatly send subscribe message
		send(subscriber->link(), "subscribe", 
			 ConstObject()
				.insert("pa.hpp", &prefix));
		
		// Retrieve from cache
		Set<BinaryString> targets;
		if(Store::Instance->retrieveValue(Store::Hash(prefix), targets))
			for(Set<BinaryString>::iterator it = targets.begin(); it != targets.end(); ++it)
				subscriber->incoming(Link::Null, prefix, "", *it);
	}
}

void Network::unsubscribe(String prefix, Subscriber *subscriber)
{
	Synchronize(this);
	Assert(subscriber);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	Map<String, Set<Subscriber*> >::iterator it = mSubscribers.find(prefix);
	if(it != mSubscribers.end())
	{
		it->second.erase(subscriber);
		if(it->second.empty())
			mSubscribers.erase(it);
	}
}

void Network::advertise(String prefix, const String &path, Publisher *publisher)
{
	Synchronize(this);
	Assert(publisher);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	//LogDebug("Network::publi.hpp", "Advertising " + prefix + path);
	
	matchSubscribers(prefix, publisher->link(), publisher);
}

void Network::issue(String prefix, const String &path, Publisher *publisher, const Mail &mail)
{
	Synchronize(this);
	if(mail.empty()) return;
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	//LogDebug("Network::issue", "Issuing " + mail.digest().toString());
	
	matchSubscribers(prefix, publisher->link(), mail);
}

void Network::addRemoteSubscriber(const Link &link, const String &path)
{
	Synchronize(this);
	
	if(!mRemoteSubscribers[link].contains(path))
		mRemoteSubscribers[link].insert(path, RemoteSubscriber(link));
	
	mRemoteSubscribers[link][path].subscribe(path);
}

bool Network::broadcast(const Identifier &local, const String &type, const Serializable &object)
{	
	// Alias
	return send(Link(local, Identifier::Empty), type, object);
}

bool Network::send(const Identifier &local, const Identifier &remote, const String &type, const Serializable &object)
{
	// Alias
	return send(Link(local, remote), type, object);
}

bool Network::send(const Link &link, const String &type, const Serializable &object)
{
	Synchronize(this);
	return outgoing(link, type, object);
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
	Synchronize(this);
	return mHandlers.contains(link);
}

void Network::run(void)
{
	const double period = 1.;	// 1 sec
	
	unsigned loops = 0;
	while(true)
	{
		try {
			double timeout = period;
			
			// Receive messages
			Overlay::Message message;
			while(mOverlay.recv(message, timeout))
			{
				//LogDebug("Network::incoming", "Processing message, type: " + String::hexa(unsigned(message.type)));
				
				switch(message.type)
				{
				// Value
				case Overlay::Message::Value:
					{
						Synchronize(this);
						
						if(!message.content.empty() && message.content != mOverlay.localNode())
						{
							// It can be about a block
							if(mCallers.contains(message.source))
							{
								//LogDebug("Network::run", "Got candidate for " + message.source.toString());
								
								uint16_t tokens = uint16_t(Store::Instance->missing(message.source));
								BinaryString call;
								call.writeBinary(tokens);
								call.writeBinary(message.source);
								
								mOverlay.send(Overlay::Message(Overlay::Message::Call, call, message.content));
							}
							
							// Or it can be about a contact
							Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.lower_bound(IdentifierPair(message.source, Identifier::Empty));	// pair is (remote, local)
							
							// We have to copy the sets since we are going to desynchronize
							Map<IdentifierPair, Set<Listener*> > temp;
							while(it != mListeners.end() && it->first.first == message.source)
							{
								temp.insert(it->first, it->second);
								++it;
							}
							
							if(!temp.empty())
							{
								//LogDebug("Network::run", "Got instance for " + message.source.toString());
								
								for(Map<IdentifierPair, Set<Listener*> >::iterator kt = temp.begin(); kt != temp.end(); ++kt)
								{
									for(Set<Listener*>::iterator jt = kt->second.begin();
											jt != kt->second.end();
											++jt)
									{
										it = mListeners.find(kt->first);
										if(it == mListeners.end()) break;
										if(it->second.contains(*jt))
										{
											Desynchronize(this);
											(*jt)->seen(Link(kt->first.second, kt->first.first, message.content));
										}
									}
								}
							}
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
							if(tokens)
							{
								if(tokens < uint16_t(-1)) LogDebug("Network::run", "Called " + target.toString() + " (" + String::number(tokens) + " tokens)");
								else LogDebug("Network::run", "Called " + target.toString());
							}
							
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
						BinarySerializer serializer(&message.content);
						Fountain::Combination combination;
						serializer.read(combination);
						combination.setCodedData(message.content);
						if(Store::Instance->push(message.source, combination))
						{
							unregisterAllCallers(message.source);
							
							Set<BinaryString> nodes;
							if(Store::Instance->retrieveValue(message.source, nodes))
							{
								BinaryString call;
								call.writeBinary(uint16_t(0));
								call.writeBinary(message.source);
								
								for(Set<BinaryString>::iterator kt = nodes.begin(); kt != nodes.end(); ++kt)
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
			
			// Send beacons
			{
				Synchronize(this);
				
				for(Map<BinaryString, Set<Caller*> >::iterator it = mCallers.begin();
					it != mCallers.end();
					++it)
				{
					for(Set<Caller*>::iterator jt = it->second.begin();
						jt != it->second.end();
						++jt)
					{
						if(!(*jt)->hint().empty())
						{
							Set<BinaryString> nodes;
							if(Store::Instance->retrieveValue((*jt)->hint(), nodes))
							{
								uint16_t tokens = uint16_t(Store::Instance->missing(it->first));
								BinaryString call;
								call.writeBinary(tokens);
								call.writeBinary(it->first);
								
								for(Set<BinaryString>::iterator kt = nodes.begin(); kt != nodes.end(); ++kt)
									mOverlay.send(Overlay::Message(Overlay::Message::Call, call, *kt));
							}
							else mOverlay.retrieve((*jt)->hint());
						}
					}

					mOverlay.retrieve(it->first);
				}
				
				if(loops % 10 == 0)
				{
					Identifier node(mOverlay.localNode());
					Set<Identifier> localIds;
					Set<Identifier> remoteIds;
					
					for(Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.begin();
						it != mListeners.end();
						++it)
					{
						localIds.insert(it->first.second);
						remoteIds.insert(it->first.first);
					}
					
					for(Set<Identifier>::iterator it = localIds.begin();
						it != localIds.end();
						++it)
					{
						storeValue(*it, node);
					}
					
					for(Set<Identifier>::iterator it = remoteIds.begin();
						it != remoteIds.end();
						++it)
					{
						mOverlay.retrieve(*it);
					}
					
					//LogDebug("Network::run", "Identifiers: stored " + String::number(localIds.size()) + ", queried " + String::number(remoteIds.size()));
				}
			}
		}
		catch(const std::exception &e)
		{
			LogWarn("Network::run", e.what());
		}
		
		++loops;
	}
}

void Network::registerHandler(const Link &link, Handler *handler)
{
	Synchronize(this);
	Assert(!link.local.empty());
	Assert(!link.remote.empty());
	Assert(!link.node.empty());
	Assert(handler);
	
	Handler *h = NULL;
	if(mHandlers.get(link, h))
		mOtherHandlers.insert(h);
	
	mHandlers.insert(link, handler);
	mThreadPool.launch(handler);
	
	for(Map<String, Set<Subscriber*> >::iterator it = mSubscribers.begin();
		it != mSubscribers.end();
		++it)
	{
		for(Set<Subscriber*>::iterator jt = it->second.begin();
			jt != it->second.end();
			++jt)
		{
			if((*jt)->link() == link)
			{
				send(link, "subscribe", 
					ConstObject()
						.insert("pa.hpp", &it->first));
				break;
			}
		}
	}
	
	onConnected(link, true);
}

void Network::unregisterHandler(const Link &link, Handler *handler)
{
	Synchronize(this);
	Assert(!link.local.empty());
	Assert(!link.remote.empty());
	Assert(!link.node.empty());
	Assert(handler);
	
	mOtherHandlers.erase(handler);
	
	Handler *h = NULL;
	if(mHandlers.get(link, h) && h == handler)
	{
		mRemoteSubscribers.erase(link);
		mHandlers.erase(link);
		onConnected(link, false);
	}
}

bool Network::outgoing(const String &type, const Serializable &content)
{
	// Alias
	return outgoing(Link::Null, type, content);
}

bool Network::outgoing(const Link &link, const String &type, const Serializable &content)
{
	Synchronize(this);
	
	String serialized;
	JsonSerializer(&serialized).write(content);

	bool success = false;
	for(Map<Link, Handler*>::iterator it = mHandlers.lower_bound(link);
		it != mHandlers.end() && it->first == link;
		++it)
	{
		if(type == "subscribe")
		{
			// If link is not trusted, do not send subscriptions
			if(!mListeners.contains(IdentifierPair(it->first.remote, it->first.local)))
				continue;
		}
		
		it->second->write(type, serialized);
		success = true;
	}
	
	//if(success) LogDebug("Network::outgoing", "Sending command (type=\"" + type + "\")");
	return success;
}

bool Network::incoming(const Link &link, const String &type, Serializer &serializer)
{
	Desynchronize(this);
	//LogDebug("Network::incoming", "Incoming command (type=\"" + type + "\")");
	
	bool hasListener = mListeners.contains(IdentifierPair(link.remote, link.local));
	
	if(type == "publi.hpp")
	{
		// If link is not trusted, ignore publications
		// Subscriptions are filtered in outgoing()
		if(!hasListener) return false;
		
		String path;
		Mail mail;
		SerializableList<BinaryString> targets;
		serializer.read(Object()
				.insert("pa.hpp", &path)
				.insert("message", &mail)
				.insert("targets", &targets));
		
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
		for(SerializableList<BinaryString>::iterator it = targets.begin();
			it != targets.end();
			++it)
		{
			hasNew|= !Store::Instance->hasValue(key, *it);
			Store::Instance->storeValue(key, *it, Store::Temporary);	// cache
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
		serializer.read(Object()
				.insert("pa.hpp", &path));
		
		addRemoteSubscriber(link, path);
	}
	else if(type == "invite")
	{
		String name;
		serializer.read(Object()
				.insert("name", &name));
		
		User *user = User::GetByIdentifier(link.local);
		if(user && !name.empty()) user->invite(link.remote, name);
	}
	else {
		return onRecv(link, type, serializer);
	}
	
	return true;
}

bool Network::matchPublishers(const String &path, const Link &link, Subscriber *subscriber)
{
	Synchronize(this);
	
	if(path.empty() || path[0] != '/') return false;
	
	List<String> list;
	path.before('?').explode(list,'/');
	if(list.empty()) return false;
	list.pop_front();
	
	// Match prefixes, longest first
	while(true)
	{
		String prefix;
		prefix.implode(list, '/');
		prefix = "/" + prefix;
		
		String truncatedPath(path.substr(prefix.size()));
		if(truncatedPath.empty()) truncatedPath = "/";

		SerializableList<BinaryString> targets;
		Map<String, Set<Publisher*> >::iterator it = mPublishers.find(prefix);
		if(it != mPublishers.end())
		{
			Set<Publisher*> set = it->second;
			Desynchronize(this);
			
			for(Set<Publisher*>::iterator jt = set.begin();
				jt != set.end();
				++jt)
			{
				Publisher *publisher = *jt;
				if(publisher->link() != link)
					continue;
				
				List<BinaryString> result;
				if(publisher->anounce(link, prefix, truncatedPath, result))
				{
					Assert(!result.empty());
					if(subscriber) 	// local
					{
						for(List<BinaryString>::iterator it = result.begin(); it != result.end(); ++it)
							subscriber->incoming(publisher->link(), path, "/", *it);
					}
					else targets.splice(targets.end(), result);	// remote
				}
			}
			
			if(!targets.empty())	// remote
			{
				//LogDebug("Network::Handler::incoming", "Anouncing " + path);
	
				send(link, "publi.hpp", ConstObject()
						.insert("pa.hpp", &path)
						.insert("targets", &targets));
			}
		}
		
		if(list.empty()) break;
		list.pop_back();
	}
	
	return true;
}

bool Network::matchSubscribers(const String &path, const Link &link, Publisher *publisher)
{
	Synchronize(this);
	
	if(path.empty() || path[0] != '/') return false;
	
	List<String> list;
	path.explode(list,'/');
	if(list.empty()) return false;
	list.pop_front();
	
	// Match prefixes, longest first
	while(true)
	{
		String prefix;
		prefix.implode(list, '/');
		prefix = "/" + prefix;
		
		String truncatedPath(path.substr(prefix.size()));
		if(truncatedPath.empty()) truncatedPath = "/";
		
		// Pass to subscribers
		Map<String, Set<Subscriber*> >::iterator it = mSubscribers.find(prefix);
		if(it != mSubscribers.end())
		{
			Set<Subscriber*> set = it->second;
			Desynchronize(this);
			
			for(Set<Subscriber*>::iterator jt = set.begin();
				jt != set.end();
				++jt)
			{
				Subscriber *subscriber = *jt;
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
	Synchronize(this);
	
	if(path.empty() || path[0] != '/') return false;
	
	List<String> list;
	path.explode(list,'/');
	if(list.empty()) return false;
	list.pop_front();
	
	// Match prefixes, longest first
	while(true)
	{
		String prefix;
		prefix.implode(list, '/');
		prefix = "/" + prefix;
		
		String truncatedPath(path.substr(prefix.size()));
		if(truncatedPath.empty()) truncatedPath = "/";
		
		// Pass to subscribers
		Map<String, Set<Subscriber*> >::iterator it = mSubscribers.find(prefix);
		if(it != mSubscribers.end())
		{
			Set<Subscriber*> set = it->second;
			Desynchronize(this);
			
			for(Set<Subscriber*>::iterator jt = set.begin();
				jt != set.end();
				++jt)
			{
				Subscriber *subscriber = *jt;
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

void Network::onConnected(const Link &link, bool status)
{
	Synchronize(this);
	
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it == mListeners.end()) return;
	
	Set<Listener*> set(it->second);	// we have to copy the set since we are going to desynchronize
	for(Set<Listener*>::iterator jt = set.begin();
		jt != set.end();
		++jt)
	{
                it = mListeners.find(IdentifierPair(link.remote, link.local));
		if(it == mListeners.end()) break;
		if(it->second.contains(*jt))
		{
 			Desynchronize(this);
			if(status) (*jt)->seen(link);	// so Listener::seen() is triggered even with incoming tunnels
			(*jt)->connected(link, status);
		}
	}
}

bool Network::onRecv(const Link &link, const String &type, Serializer &serializer)
{
	Synchronize(this);
	
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it == mListeners.end()) return false;

	bool ret = false;
	Set<Listener*> set(it->second);	// we have to copy the set since we are going to desynchronize
	for(Set<Listener*>::iterator jt = set.begin();
		jt != set.end();
		++jt)
	{
		it = mListeners.find(IdentifierPair(link.remote, link.local));
		if(it == mListeners.end()) break;
		if(it->second.contains(*jt))
		{
			Desynchronize(this);
			if((*jt)->recv(link, type, serializer))
				return true;
		}
	}
	
	return false;
}

bool Network::onAuth(const Link &link, const Rsa::PublicKey &pubKey)
{
	Synchronize(this);
	
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it == mListeners.end()) return true;
	
	Set<Listener*> set(it->second);	// we have to copy the set since we are going to desynchronize
	for(Set<Listener*>::iterator jt = set.begin();
		jt != set.end();
		++jt)
	{
		it = mListeners.find(IdentifierPair(link.remote, link.local));
		if(it == mListeners.end()) break;
		if(it->second.contains(*jt))
		{
			Desynchronize(this);
			if(!(*jt)->auth(link, pubKey))
				return false;
		}
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
	for(StringSet::iterator it = mPublishedPrefixes.begin();
		it != mPublishedPrefixes.end();
		++it)
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
	mThreadPool(0, 1, 8)
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
	for(StringSet::iterator it = mSubscribedPrefixes.begin();
		it != mSubscribedPrefixes.end();
		++it)
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
	
	class PrefetchTask : public Task
	{
	public:
		PrefetchTask(Network::Subscriber *subscriber, const Link &link, const String &prefix, const String &path, const BinaryString &target, bool fetchContent)
		{
			this->subscriber = subscriber;
			this->link = link;
			this->target = target;
			this->prefix = prefix;
			this->path = path;
			this->fetchContent = fetchContent;
		}
		
		void run(void)
		{
			try {
				Resource resource(target);
				
				if(fetchContent)
				{
					Resource::Reader reader(&resource, "", true);	// empty password + no check
					reader.discard();				// read everything
				}

				subscriber->incoming(link, prefix, path, target);
			}
			catch(const Exception &e)
			{
				LogWarn("Network::Subscriber::fet.hpp", "Fetching failed for " + target.toString() + ": " + e.what());
			}
			
			delete this;	// autodelete
		}
	
	private:
		Network::Subscriber *subscriber;
		Link link;
		BinaryString target;
		String prefix;
		String path;
		bool fetchContent;
	};
	
	PrefetchTask *task = new PrefetchTask(this, link, prefix, path, target, fetchContent);
	mThreadPool.launch(task);
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
		SerializableArray<BinaryString> targets;
		targets.append(target);
		
		Network::Instance->send(this->link(), "publi.hpp",
			ConstObject()
				.insert("pa.hpp", &prefix)
				.insert("targets", &targets));
	}
}

bool Network::RemoteSubscriber::incoming(const Link &link, const String &prefix, const String &path, const Mail &mail)
{
	if(link.remote.empty() || link != this->link())
	{
		Network::Instance->send(this->link(), "publi.hpp",
			ConstObject()
				.insert("pa.hpp", &prefix)
				.insert("message", &mail));
	}
}

bool Network::RemoteSubscriber::localOnly(void) const
{
	return true;
}

Network::Caller::Caller(void)
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
	for(Set<IdentifierPair>::iterator it = mPairs.begin();
                it != mPairs.end();
                ++it)
        {
                Network::Instance->unregisterListener(it->second, it->first, this);
        }

	mPairs.clear();
}

Network::Tunneler::Tunneler(void)
{

}

Network::Tunneler::~Tunneler(void)
{
	
}

bool Network::Tunneler::open(const BinaryString &node, const Identifier &remote, User *user, bool async)
{
	Synchronize(this);
	Assert(!node.empty());
	Assert(!remote.empty());
	Assert(user);
	
	if(mPending.contains(node))
		return false;
	
	if(node == Network::Instance->overlay()->localNode())
		return false;
	
	if(Network::Instance->overlay()->connectionsCount() == 0)
		return false;
	
	if(Network::Instance->hasLink(Link(user->identifier(), remote, node)))
		return false;
	
	uint64_t tunnelId = 0;
	Random().readBinary(tunnelId);	// Generate random tunnel ID
	BinaryString local = user->identifier();
	
	//LogDebug("Network::Tunneler::open", "Opening tunnel to " + node.toString() + " (id " + String::hexa(tunnelId) + ")");
	
	Tunneler::Tunnel *tunnel = NULL;
	SecureTransport *transport = NULL;
	try {
		tunnel = new Tunneler::Tunnel(this, tunnelId, node);
		transport = new SecureTransportClient(tunnel, NULL);
	}
	catch(...)
	{
		delete tunnel;
		throw;
	}
	
	// Set remote name
	transport->setHostname(remote.toString());
	
	// Add certificates
	//LogDebug("Network::Tunneler::open", "Setting certificate credentials: " + user->name());
	transport->addCredentials(user->certificate(), false);
	
	mPending.insert(node);
	
	return handshake(transport, Link(local, remote, node), async);
}

SecureTransport *Network::Tunneler::listen(BinaryString *source)
{
	Synchronize(this);
	
	while(true)
	{
		while(mQueue.empty()) wait();
		
		try {
			Overlay::Message &message = mQueue.front();

			// Read tunnel ID
			uint64_t tunnelId = 0;
			if(!message.content.readBinary(tunnelId))
				continue;

			Map<uint64_t, Tunnel*>::iterator it = mTunnels.find(tunnelId);
			if(it == mTunnels.end())
			{
				LogDebug("Network::Tunneler::listen", "Incoming tunnel from " + message.source.toString() /*+ " (id " + String::hexa(tunnelId) + ")"*/);
				
				if(source) *source = message.source;
				
				Tunneler::Tunnel *tunnel = NULL;
				SecureTransport *transport = NULL;
				try {
					tunnel = new Tunneler::Tunnel(this, tunnelId, message.source);
					transport = new SecureTransportServer(tunnel, NULL, true);	// ask for certificate
				}
				catch(...)
				{
					delete tunnel;
					throw;
				}
				
				tunnel->incoming(message);
				mQueue.pop();
				return transport;
			}
			
			//LogDebug("Network::Tunneler::listen", "Message tunnel from " + message.source.toString() + " (id " + String::hexa(tunnelId) + ")");
			it->second->incoming(message);
			mQueue.pop();
		}
		catch(...)
		{
			mQueue.pop();
			throw;
		}
	}
	
	return NULL;
}

bool Network::Tunneler::incoming(const Overlay::Message &message)
{
	Synchronize(this);
	
	mQueue.push(message);
	notifyAll();
	return true;
}

bool Network::Tunneler::registerTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	Assert(tunnel);
	
	Tunneler::Tunnel *t = NULL;
	if(mTunnels.get(tunnel->id(), t))
		return (t == tunnel);
	
	mTunnels.insert(tunnel->id(), tunnel);
	return true;
}

bool Network::Tunneler::unregisterTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	Assert(tunnel);
	
	mPending.erase(tunnel->node());
	
	Tunneler::Tunnel *t = NULL;
	if(!mTunnels.get(tunnel->id(), t) || t != tunnel)
		return false;
	
	mTunnels.erase(tunnel->id());
	return true;
}

bool Network::Tunneler::handshake(SecureTransport *transport, const Link &link, bool async)
{
	Synchronize(this);
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
				transport->addCredentials(user->certificate(), false);
			}
			else {
				 LogDebug("Network::Tunneler::handshake", String("User does not exist: ") + name);
			}
			
			return true;	// continue handshake anyway
		}
		
		bool verifyPublicKey(const Array<Rsa::PublicKey> &chain)
		{
			if(chain.empty()) return false;
			publicKey = chain[0];
			remote = Identifier(publicKey.digest());
			
			LogDebug("Network::Tunneler::handshake", String("Verifying remote certificate: ") + remote.toString());
			if(Network::Instance->onAuth(Link(local, remote, node), publicKey))
				return true;
			
			LogDebug("Network::Tunneler::handshake", "Certificate verification failed");
			return false;
		}
	};
	
	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(SecureTransport *transport, const Link &link)
		{ 
			this->transport = transport;
			this->link = link;
		}
		
		bool handshake(void)
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
				const double timeout = milliseconds(Config::Get("request_timeout").toInt());
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
				Handler *handler = new Handler(transport, link);
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
		}
		
		void run(void)
		{
			handshake();
			delete this;	// autodelete
		}
		
	private:
		SecureTransport *transport;
		Link link;
	};
	
	HandshakeTask *task = NULL;
	try {
		if(async)
		{
			task = new HandshakeTask(transport, link);
			mThreadPool.launch(task);
			return true;
		}
		else {
			Desynchronize(this);
			HandshakeTask stask(transport, link);
			return stask.handshake();
		}
	}
	catch(const std::exception &e)
	{
		LogError("Network::Tunneler::handshake", e.what());
		delete task;
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
	mTimeout(milliseconds(Config::Get("idle_timeout").toInt()))
{
	//LogDebug("Network::Tunneler::Tunnel", "Registering tunnel " + String::hexa(mId) + " to " + mNode.toString());
	if(!mTunneler->registerTunnel(this))
		throw Exception("Tunnel " + String::hexa(mId) + " is already registered");

	mBuffer.writeBinary(mId);
}

Network::Tunneler::Tunnel::~Tunnel(void)
{
	//LogDebug("Network::Tunneler::Tunnel", "Unregistering tunnel " + String::hexa(mId) + " to " + mNode.toString());
	mTunneler->unregisterTunnel(this);
}

uint64_t Network::Tunneler::Tunnel::id(void) const
{
	Synchronize(this);
	return mId;
}

BinaryString Network::Tunneler::Tunnel::node(void) const
{
	Synchronize(this);
	return mNode;
}

void Network::Tunneler::Tunnel::setTimeout(double timeout)
{
	Synchronize(this);
	mTimeout = timeout;
}

size_t Network::Tunneler::Tunnel::readData(char *buffer, size_t size)
{
	Synchronize(this);
	
	double timeout = mTimeout;
        while(mQueue.empty())
		if(!wait(timeout))
			throw Timeout();
	
	const Overlay::Message &message = mQueue.front();
	Assert(mOffset <= message.content.size());
	size = std::min(size, size_t(message.content.size() - mOffset));
	std::copy(message.content.data() + mOffset, message.content.data() + mOffset + size, buffer);
        return size;
}

void Network::Tunneler::Tunnel::writeData(const char *data, size_t size)
{
	Synchronize(this);
	
	mBuffer.writeBinary(data, size);
}

bool Network::Tunneler::Tunnel::waitData(double &timeout)
{
	Synchronize(this);
	
	while(mQueue.empty())
	{
		if(timeout <= 0.)
			return false;
		
		if(!wait(timeout))
			return false;
	}
	
	return true;
}

bool Network::Tunneler::Tunnel::nextRead(void)
{
	Synchronize(this);
	if(!mQueue.empty()) mQueue.pop();
	mOffset = 0;
	return true;
}

bool Network::Tunneler::Tunnel::nextWrite(void)
{
	Network::Instance->overlay()->send(Overlay::Message(Overlay::Message::Tunnel, mBuffer, mNode));
	
	Synchronize(this);
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
	Synchronize(this);
	
	if(message.type != Overlay::Message::Tunnel)
		return false;
	
	mQueue.push(message);
	notifyAll();
	return true;
}

Network::Handler::Handler(Stream *stream, const Link &link) :
	mStream(stream),
	mLink(link),
	mTokens(DefaultTokens),
	mThreshold(DefaultTokens/2),
	mAvailableTokens(DefaultTokens),
	mAccumulator(0.),
	mRedundancy(1.25),	// TODO
	mTimeout(milliseconds(Config::Get("retransmit_timeout").toInt())),
	mClosed(false),
	mTimeoutTask(this)
{
	Network::Instance->registerHandler(mLink, this);
}

Network::Handler::~Handler(void)
{
	Network::Instance->unregisterHandler(mLink, this);	// should be done already
	Scheduler::Global->cancel(&mTimeoutTask);
	
	delete mStream;
}

void Network::Handler::write(uint8_t type, const BinaryString &record)
{
	writeRecord(type, record);
}

void Network::Handler::push(const BinaryString &target, unsigned tokens)
{
	Synchronize(this);
	if(mClosed) return;
	
	if(tokens < uint16_t(-1))
	{
		tokens = unsigned(double(tokens)*mRedundancy + 0.5);
	}
	
	if(tokens) mTargets[target] = tokens;
	else mTargets.erase(target);
	
	send(false);
}

void Network::Handler::timeout(void)
{
	Synchronize(this);

	// Inactivity timeout is handled by Tunnel
	if(mAvailableTokens < 1.) mAvailableTokens = 1.;
	send(true);
}

bool Network::Handler::readRecord(uint8_t &type, BinaryString &record)
{
	Synchronize(this);
	if(mClosed) return false;
	
	try {
		uint8_t version;
		uint16_t size;
		
		if(readBinary(version))
		{
			AssertIO(readBinary(type));
			AssertIO(readBinary(size));
			AssertIO(readBinary(content, size) == size);
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

Network::Handler::writeRecord(uint8_t type, const BinaryString &record)
{
	BinaryString buffer;
	buffer.writeBinary(uint8_t(0));		// version
	buffer.writeBinary(uint8_t(type));
	buffer.writeBinary(uint16_t(record.size()));
	buffer.writeBinary(content.c_str(), content.size());
	writeData(buffer.data(), buffer.size());
	flush();
}

size_t Network::Handler::readData(char *buffer, size_t size)
{
	Synchronize(this);
	
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
		
		if(!size)
			break;
		
		// We need more combinations
		Fountain::Combination combination;
		if(!recvCombination(combination))
			break;
		
		//LogDebug("Network::Handler::readString", "Received combination");
		
		if(!combination.isNull())
		{
			mSink.drop(combination.firstComponent());
			mSink.solve(combination);
				
			if(!send(false))
				Scheduler::Global->schedule(&mTimeoutTask, mTimeout/10);
		}
	}
	
	return count;
}

void Network::Handler::writeData(const char *data, size_t size)
{
	Synchronize(this);
	if(mClosed) return;
	
	unsigned count = mSource.write(data, size);
	mAccumulator+= mRedundancy*count;
}

void Network::Handler::flush(void)
{
	send(false);
}

int Network::Handler::send(bool force)
{
	Synchronize(this);
	if(mClosed) return 0;
	
	int count = 0;
	while(force || (mSource.rank() >= 1 && mAccumulator >= 1. && mAvailableTokens >= 1.))
	{
		//LogDebug("Network::Handler::send", "Sending combination (rank=" + String::number(mSource.rank()) + ", accumulator=" + String::number(mAccumulator)+", tokens=" + String::number(mAvailableTokens) + ")");
		
		try {
			Fountain::Combination combination;
			mSource.generate(combination);
			
			if(!combination.isNull())
			{
				mAccumulator = std::max(0., mAccumulator - 1.);
			}
			/*else {
				if(!mTargets.empty())
				{
					// Pick at random
					int r = Random().uniform(0, int(mTargets.size()));
					Map<BinaryString, unsigned>::iterator it = mTargets.begin();
					while(r--) ++it;
					Assert(it != mTargets.end());
					
					BinaryString target = it->first;
                        	        unsigned &tokens = it->second;
					
					unsigned rank = 0;
					Store::Instance->pull(target, combination, &rank);
					
					tokens = std::min(tokens, unsigned(double(rank)*mRedundancy + 0.5));
					--tokens;
				}
			}*/
			
			sendCombination(combination);
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
	
	double idleTimeout = milliseconds(Config::Get("idle_timeout").toInt())/5;	// so the tunnel should not time out
	if(mSource.rank() > 0) Scheduler::Global->schedule(&mTimeoutTask, mTimeout);
	else Scheduler::Global->schedule(&mTimeoutTask, idleTimeout);
	return count;
}

bool Network::Handler::recvCombination(Fountain::Combination &combination)
{
	Desynchronize(this);
	BinarySerializer s(mStream);
	
	// 32-bit header
	uint16_t  version    = 0;
	uint16_t dataSize    = 0;
	if(!s.read(version)) return false;
	AssertIO(s.read(dataSize));
	
	uint32_t nextSeen = 0;
	uint32_t nextDecoded = 0;
	AssertIO(s.read(nextSeen));	// 32-bit next seen
	AssertIO(s.read(nextDecoded));	// 32-bit next decoded
	
	// 64-bit combination descriptor
	AssertIO(s.read(combination));
	
	// Data
	BinaryString data;
	mStream->readBinary(data, dataSize); 
	combination.setCodedData(data);
	
	mStream->nextRead();
	
	Synchronize(this);
	unsigned backlog = nextSeen - std::min(nextDecoded, nextSeen);
	unsigned dropped = mSource.drop(nextSeen);
	
	if(backlog < mThreshold || backlog > mThreshold*2.)
	{
		double tokens;
		if(mTokens < mThreshold)
		{
			// Slow start
			tokens = dropped*2.;
		}
		else {
			// Additive increase
			tokens = dropped*1./mTokens;
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
	return true;
}

void Network::Handler::sendCombination(const Fountain::Combination &combination)
{
	Synchronize(this);
	uint32_t nextSeen = mSink.nextSeen();
	uint32_t nextDecoded = mSink.nextDecoded();
	mAvailableTokens = std::max(0., mAvailableTokens - 1.);
	
	Desynchronize(this);
	MutexLocker lock(&mWriteMutex);
	BinarySerializer s(mStream);
	
	// 32-bit header
	s.write(uint16_t(0));	// version
	s.write(uint16_t(combination.codedSize()));
	
	s.write(uint32_t(nextSeen));		// 32-bit next seen
	s.write(uint32_t(nextDecoded));		// 32-bit next decoded
	
	// 64-bit combination descriptor
	s.write(combination);
	
	// Data
	mStream->writeBinary(combination.data(), combination.codedSize());
	
	mStream->nextWrite();
}

void Network::Handler::process(void)
{
	Synchronize(this);
	
	String type, content;
	while(read(type, content))
	{
		try {
			Desynchronize(this);
			JsonSerializer serializer(&content);
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
	Scheduler::Global->cancel(&mTimeoutTask);
	
	notifyAll();
	Thread::Sleep(10.);	// TODO
	delete this;		// autodelete
}

Network::Pusher::Pusher(void) :
	mRedundant(16)	// TODO
{
	
}

Network::Pusher::~Pusher(void)
{
	
}

void Network::Pusher::push(const BinaryString &target, const Identifier &destination, unsigned tokens)
{
	Synchronize(this);
	
	if(tokens < uint16_t(-1))
	{
		if(tokens < mRedundant) tokens*=2;
		else tokens+= mRedundant;
	}
	
	if(tokens) mTargets[target][destination] = tokens;
	else {
		mTargets[target].erase(destination);
		if(mTargets[target].empty())
			mTargets.erase(target);
	}
	
	notifyAll();
}

void Network::Pusher::run(void)
{
	while(true)
	{
		try {
			Synchronize(this);
			
			while(mTargets.empty()) wait();
			
			bool congestion = false;	// local congestion
			Map<BinaryString, Map<Identifier, unsigned> >::iterator it = mTargets.begin();
			while(it != mTargets.end())
			{
				const BinaryString &target = it->first;
				
				Map<Identifier, unsigned>::iterator jt = it->second.begin();
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
						BinarySerializer serializer(&data.content);
						serializer.write(combination);
						data.content.writeBinary(combination.data(), combination.codedSize());
						
						congestion|= Network::Instance->overlay()->send(data);
					}
					
					if(!tokens) it->second.erase(jt++);
					else ++jt;
				}
				
				if(it->second.empty()) mTargets.erase(it++);
				else ++it;
			}
			
			if(congestion)
			{
				Desynchronize(this);
				yield();
			}
		}
		catch(const std::exception &e)
		{
			LogWarn("Network::Pusher::run", e.what());
		}
	}
}

}
