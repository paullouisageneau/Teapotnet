/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/network.h"
#include "tpn/user.h"
#include "tpn/httptunnel.h"
#include "tpn/config.h"
#include "tpn/store.h"

#include "pla/binaryserializer.h"
#include "pla/jsonserializer.h"
#include "pla/object.h"
#include "pla/securetransport.h"
#include "pla/crypto.h"
#include "pla/random.h"
#include "pla/http.h"

namespace tpn
{

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
	
	Thread::start();
}

void Network::join(void)
{
	Thread::join();
	
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
	for(Map<Link, Handler*>::iterator it = mHandlers.lower_bound(link);
		it != mHandlers.end() && (it->first.local == local && it->first.remote == remote);
		++it)
	{
		listener->seen(it->first);	// so Listener::seen() is triggered even with incoming tunnels
		listener->connected(it->first, true);
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
						.insert("path", &it->first));
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
	
	LogDebug("Network::publish", "Publishing " + prefix);
	
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

	LogDebug("Network::subscribe", "Subscribing " + prefix);
	
	mSubscribers[prefix].insert(subscriber);
	
	// Local publishers
	matchPublishers(prefix, subscriber->link(), subscriber);
	
	// Remote publishers
	if(!subscriber->localOnly())
	{
		// Immediatly send subscribe message
		send(subscriber->link(), "subscribe", 
			 ConstObject()
				.insert("path", &prefix));
		
		// Retrieve from cache
		Set<BinaryString> targets;
		if(Store::Instance->retrieveValue(Store::Hash(prefix+"/"), targets))
			for(Set<BinaryString>::iterator it = targets.begin(); it != targets.end(); ++it)
				subscriber->incoming(Link::Null, prefix, "/", *it);
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
	
	LogDebug("Network::publish", "Advertising " + prefix + path);
	
	matchSubscribers(prefix, publisher->link(), publisher);
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
								LogDebug("Network::run", "Got candidate for " + message.source.toString());
								mOverlay.send(Overlay::Message(Overlay::Message::Call, message.source, message.content));	// TODO: tokens
							}
							
							// Or it can be about a contact
							Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.lower_bound(IdentifierPair(message.source, Identifier::Empty));	// pair is (remote, local)
							while(it != mListeners.end() && it->first.first == message.source)
							{
								for(Set<Listener*>::iterator jt = it->second.begin();
									jt != it->second.end();
									++jt)
								{
									(*jt)->seen(Link(it->first.second, it->first.first, message.content));
								}
								++it;
							}
						}
						
						break;
					}
					
				// Call
				case Overlay::Message::Call:
					{
						if(Store::Instance->hasBlock(message.content))
						{
							LogDebug("Network::run", "Called " + message.content.toString());

							class PushTask : public Task
							{
							public:
								PushTask(const BinaryString target, const Identifier &destination, unsigned count) 
								{ 
									this->target = target;
									this->destination = destination;
									this->count = count;
								}
								
								void run(void)
								{
									Fountain::Combination combination;
									for(unsigned i=0; i<count; ++i)
									{
										Store::Instance->pull(target, combination);
										
										Overlay::Message data(Overlay::Message::Data, "", destination, target);
										BinarySerializer serializer(&data.content);
										serializer.write(combination);
										data.content.writeBinary(combination.data(), combination.codedSize());
										
										Network::Instance->overlay()->send(data);
										
										if(i >= combination.componentsCount())
											break;
									}
									
									delete this;	// autodelete
								}
								
							private:
								Overlay *overlay;
								BinaryString target;
								Identifier destination;
								unsigned count;
							};
							
							const unsigned count = 1024;	// TODO
							mThreadPool.launch(new PushTask(message.content, message.source, count));
						}
						else {
							LogDebug("Network::run", "Called (unknown) " + message.content.toString());
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
							unregisterAllCallers(message.source);
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
					Desynchronize(this);
					mOverlay.send(Overlay::Message(Overlay::Message::Retrieve, "", it->first));
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
						Desynchronize(this);
						storeValue(*it, node);
					}
					
					for(Set<Identifier>::iterator it = remoteIds.begin();
						it != remoteIds.end();
						++it)
					{
						Desynchronize(this);
						mOverlay.send(Overlay::Message(Overlay::Message::Retrieve, "", *it));
					}
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

bool Network::registerHandler(const Link &link, Handler *handler)
{
	Synchronize(this);
	Assert(!link.local.empty());
	Assert(!link.remote.empty());
	Assert(!link.node.empty());
	
	if(!handler)
		return false;
	
	Handler *l = NULL;
	if(mHandlers.get(link, l))
	{
		if(l == handler) 
			return true;
		
		if(Random().uniform(0, 1) == 0)
			return false;
		
		LogDebug("Network::registerHandler", "Replacing current handler");
	}
	
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
						.insert("path", &it->first));
				break;
			}
		}
	}
	
	return true;
}

bool Network::unregisterHandler(const Link &link, Handler *handler)
{
	Synchronize(this);
	Assert(!link.node.empty());
	
	if(!handler)
		return false;
	
	Handler *l = NULL;
	if(!mHandlers.get(link, l) || l != handler)
		return false;
	
	mRemoteSubscribers.erase(link);
	mHandlers.erase(link);
	return true;	
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
	
	if(success) LogDebug("Network::outgoing", "Sending command (type=\"" + type + "\")");
	return success;
}

bool Network::incoming(const Link &link, const String &type, Serializer &serializer)
{
	Synchronize(this);
	LogDebug("Network::incoming", "Incoming command (type=\"" + type + "\")");
	
	bool hasListener = mListeners.contains(IdentifierPair(link.remote, link.local));
	
	if(type == "publish")
	{
		// If link is not trusted, ignore publications
		// Subscriptions are filtered in outgoing()
		if(!hasListener) return false;
		
		String path;
		SerializableList<BinaryString> targets;
		serializer.read(Object()
				.insert("path", &path)
				.insert("targets", &targets));
		
		bool hasNew = false;
		BinaryString key = Store::Hash(path + "/");
		for(SerializableList<BinaryString>::iterator it = targets.begin();
			it != targets.end();
			++it)
		{
			hasNew|= !Store::Instance->hasValue(key, *it);
			Store::Instance->storeValue(key, *it, Store::Temporary);	// cache
		}
		
		if(hasNew)	// prevents publishing loops
		{
			RemotePublisher publisher(targets, link);
			matchSubscribers(path, link, &publisher);
		}
	}
	else if(type == "subscribe")
	{
		String path;
		serializer.read(Object()
				.insert("path", &path));
		
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
				LogDebug("Network::Handler::incoming", "Anouncing " + path);
	
				send(link, "publish", ConstObject()
						.insert("path", &path)
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

void Network::onConnected(const Link &link, bool status)
{
	Synchronize(this);
	
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it != mListeners.end())
	{
		for(Set<Listener*>::iterator jt = it->second.begin();
			jt != it->second.end();
			++jt)
		{
			if(status) (*jt)->seen(link);	// so Listener::seen() is triggered even with incoming tunnels
			(*jt)->connected(link, status);
		}
	}
}

bool Network::onRecv(const Link &link, const String &type, Serializer &serializer)
{
	Synchronize(this);
	
	bool ret = false;
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it != mListeners.end())
	{
		for(Set<Listener*>::iterator jt = it->second.begin();
			jt != it->second.end();
			++jt)
		{
			ret|= (*jt)->recv(link, type, serializer); 
		}
	}
	
	return ret;
}

bool Network::onAuth(const Link &link, const Rsa::PublicKey &pubKey)
{
	Synchronize(this);
	
	Map<IdentifierPair, Set<Listener*> >::iterator it = mListeners.find(IdentifierPair(link.remote, link.local));
	if(it != mListeners.end())
	{
		for(Set<Listener*>::iterator jt = it->second.begin();
			jt != it->second.end();
			++jt)
		{
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

bool Network::Subscriber::fetch(const Link &link, const String &prefix, const String &path, const BinaryString &target)
{
	// Test local availability
	if(Store::Instance->hasBlock(target))
	{
		Resource resource(target, true);	// local only
		if(resource.isLocallyAvailable())
			return true;
	}
	
	class PrefetchTask : public Task
	{
	public:
		PrefetchTask(Network::Subscriber *subscriber, const Link &link, const String &prefix, const String &path, const BinaryString &target)
		{
			this->subscriber = subscriber;
			this->link = link;
			this->target = target;
			this->prefix = prefix;
			this->path = path;
		}
		
		void run(void)
		{
			try {
				Resource resource(target);
				Resource::Reader reader(&resource, "", true);	// empty password + no check
				reader.discard();				// read everything
				
				subscriber->incoming(link, prefix, path, target);
			}
			catch(const Exception &e)
			{
				LogWarn("Network::Subscriber::fetch", "Fetching failed for " + target.toString() + ": " + e.what());
			}
			
			delete this;	// autodelete
		}
	
	private:
		Network::Subscriber *subscriber;
		Link link;
		BinaryString target;
		String prefix;
		String path;
	};
	
	PrefetchTask *task = new PrefetchTask(this, link, prefix, path, target);
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
				
		Network::Instance->send(link, "publish",
			ConstObject()
				.insert("path", &prefix)
				.insert("targets", &targets));
	}
}

bool Network::RemoteSubscriber::localOnly(void) const
{
	return true;
}

Network::Caller::Caller(void)
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

Network::Listener::Listener(void)
{
	
}

Network::Listener::~Listener(void)
{
	for(Set<IdentifierPair>::iterator it = mPairs.begin();
		it != mPairs.end();
		++it)
	{
		Network::Instance->unregisterListener(it->second, it->first, this);
	}
}

void Network::Listener::listen(const Identifier &local, const Identifier &remote)
{
	mPairs.insert(IdentifierPair(remote, local));
	Network::Instance->registerListener(local, remote, this);
}

Network::Tunneler::Tunneler(void)
{

}

Network::Tunneler::~Tunneler(void)
{
	
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
	
	LogDebug("Network::Tunneler::open", "Opening tunnel to " + remote.toString());
	
	uint64_t tunnelId;
	Random().readBinary(tunnelId);	// Generate random tunnel ID
	BinaryString local = user->identifier();
	
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
	LogDebug("Network::Tunneler::open", "Setting certificate credentials: " + user->name());
	transport->addCredentials(user->certificate(), false);
	
	return handshake(transport, Link(local, remote, node), async);
}

SecureTransport *Network::Tunneler::listen(BinaryString *source)
{
	Synchronize(&mQueueSync);
	
	while(true)
	{
		while(mQueue.empty()) mQueueSync.wait();
		
		Overlay::Message &datagram = mQueue.front();

		// Read tunnel ID
		uint64_t tunnelId;
		datagram.content.readBinary(tunnelId);

		Map<uint64_t, Tunnel*>::iterator it = mTunnels.find(tunnelId);
		if(it == mTunnels.end())
		{
			if(source) *source = datagram.source;
			
			Tunneler::Tunnel *tunnel = NULL;
			SecureTransport *transport = NULL;
			try {
				tunnel = new Tunneler::Tunnel(this, tunnelId, datagram.source);
				transport = new SecureTransportServer(tunnel, NULL, true);	// ask for certificate
			}
			catch(...)
			{
				delete tunnel;
				mQueue.pop();
				throw;
			}
			
			tunnel->incoming(datagram);
			mQueue.pop();
			return transport;
		}
		
		it->second->incoming(datagram);
		mQueue.pop();
	}
}

bool Network::Tunneler::incoming(const Overlay::Message &datagram)
{
	Synchronize(&mQueueSync);
	mQueue.push(datagram);
	mQueueSync.notifyAll();
	return true;
}

bool Network::Tunneler::registerTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	
	if(!tunnel)
		return false;
	
	Tunneler::Tunnel *t = NULL;
	if(mTunnels.get(tunnel->id(), t))
		return (t == tunnel);
	
	mTunnels.insert(tunnel->id(), tunnel);
	return true;
}

bool Network::Tunneler::unregisterTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	
	if(!tunnel)
		return false;
	
	Tunneler::Tunnel *t = NULL;
	if(!mTunnels.get(tunnel->id(), t) || t != tunnel)
		return false;
	
	mTunnels.erase(tunnel->id());
		return true;	
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
			catch(const std::exception &e)
			{
				LogInfo("Network::Tunneler::handshake", String("Handshake failed: ") + e.what());
				delete transport;
				return false;
			}
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
	try {
		while(true)
		{
			Identifier node;
			SecureTransport *transport = listen(&node);
			if(!transport) break;
			
			LogDebug("Network::Tunneler::run", "Incoming tunnel from " + node.toString());
			handshake(transport, Link(Identifier::Empty, Identifier::Empty, node), true); // async
		}
	}
	catch(const std::exception &e)
	{
		LogError("Network::Tunneler::run", e.what());
	}
	
	LogWarn("Network::Backend::run", "Closing tunneler");
}

Network::Tunneler::Tunnel::Tunnel(Tunneler *tunneler, uint64_t id, const BinaryString &node) :
	mTunneler(tunneler),
	mId(id),
	mNode(node),
	mOffset(0),
	mTimeout(milliseconds(Config::Get("idle_timeout").toInt()))
{
	LogDebug("Network::Tunneler::Tunnel", "Registering tunnel " + String::hexa(mId) + " to " + mNode.toString());
	if(!mTunneler->registerTunnel(this))
		throw Exception("Tunnel " + String::hexa(mId) + " is already registered");

	mBuffer.writeBinary(mId);
}

Network::Tunneler::Tunnel::~Tunnel(void)
{
	LogDebug("Network::Tunneler::Tunnel", "Unregistering tunnel " + String::hexa(mId) + " to " + mNode.toString());
	mTunneler->unregisterTunnel(this);
}

uint64_t Network::Tunneler::Tunnel::id(void) const
{
	return mId;
}

void Network::Tunneler::Tunnel::setTimeout(double timeout)
{
	mTimeout = timeout;
}

size_t Network::Tunneler::Tunnel::readData(char *buffer, size_t size)
{
	Synchronize(&mQueueSync);
	
	double timeout = mTimeout;
        while(mQueue.empty())
		if(!mQueueSync.wait(timeout))
			throw Timeout();
	
	const Overlay::Message &message = mQueue.front();
	Assert(mOffset <= message.content.size());
	size = std::min(size, size_t(message.content.size() - mOffset));
	std::copy(message.content.data() + mOffset, message.content.data() + mOffset + size, buffer);
        return size;
}

void Network::Tunneler::Tunnel::writeData(const char *data, size_t size)
{
	mBuffer.writeBinary(data, size);
}

bool Network::Tunneler::Tunnel::waitData(double &timeout)
{
	Synchronize(&mQueueSync);
	
	while(mQueue.empty())
	{
		if(timeout < 0.)
			return false;
		
		if(!mQueueSync.wait(timeout))
			return false;
	}
	
	return true;
}

bool Network::Tunneler::Tunnel::nextRead(void)
{
	if(!mQueue.empty()) mQueue.pop();
	mOffset = 0;
	return true;
}

bool Network::Tunneler::Tunnel::nextWrite(void)
{
	Network::Instance->overlay()->send(Overlay::Message(Overlay::Message::Tunnel, mBuffer, mNode));
	mBuffer.clear();
	mBuffer.writeBinary(mId);
	return true;
}

bool Network::Tunneler::Tunnel::isDatagram(void) const
{
	return true; 
}

bool Network::Tunneler::Tunnel::incoming(const Overlay::Message &datagram)
{
	Synchronize(&mQueueSync);
	
	if(datagram.type != Overlay::Message::Tunnel)
		return false;
	
	mQueue.push(datagram);
	mQueueSync.notifyAll();
	return true;
}

Network::Handler::Handler(Stream *stream, const Link &link) :
	mStream(stream),
	mLink(link),
	mTokens(10.),
	mRank(0.),
	mRedundancy(1.25),	// TODO
	mTimeout(milliseconds(Config::Get("retransmit_timeout").toInt())),
	mTimeoutTask(this)
{
	if(!Network::Instance->registerHandler(mLink, this))
		throw Exception("A handler already exists for the same link");
}

Network::Handler::~Handler(void)
{
	Network::Instance->unregisterHandler(mLink, this);	// should be done already
	Scheduler::Global->cancel(&mTimeoutTask);
	
	delete mStream;
}

void Network::Handler::write(const String &type, const String &content)
{
	Synchronize(this);
	
	BinaryString buffer;
	buffer.writeBinary(type.c_str(), type.size()+1);
	buffer.writeBinary(content.c_str(), content.size()+1);
	mSource.write(buffer.data(), buffer.size());
	mRank+= mRedundancy;
	
	send();
}

int Network::Handler::send(bool force)
{
	Synchronize(this);
	
	int count = 0;
	while(force || (mSource.count() > 0 && mRank >= 1. && mTokens >= 1.))
	{
		//LogDebug("Network::Handler::send", "Sending combination (count=" + String::number(mSource.count()) + ", rank=" + String::number(mRank)+", tokens=" + String::number(mTokens) + ")");
		
		try {
			Fountain::Combination combination;
			mSource.generate(combination);
			
			BinarySerializer serializer(mStream);
			serializer.write(combination);
			serializer.write(uint32_t(mSink.nextSeen()));
			
			mStream->writeBinary(combination.data(), combination.codedSize());
			mStream->nextWrite();
			++count;
			
			if(!combination.isNull())
			{
				mTokens = std::max(0., mTokens - 1.);
				mRank   = std::max(0., mRank - 1.);
			}
			
			force = false;
		}
		catch(std::exception &e)
		{
			LogWarn("Network::Handler::send", String("Sending failed: ") + e.what());
			mStream->close();
			break;
		}
	}
	
	double idleTimeout = milliseconds(Config::Get("idle_timeout").toInt())/2;	// so the tunnel should not time out
	if(mSource.count() > 0) Scheduler::Global->schedule(&mTimeoutTask, mTimeout);
	else Scheduler::Global->schedule(&mTimeoutTask, idleTimeout);
	return count;
}

void Network::Handler::timeout(void)
{
	Synchronize(this);

	// Inactivity timeout is handled by Tunnel
	if(mTokens < 1.) mTokens = 1.;
	send(true);
}

bool Network::Handler::read(String &type, String &content)
{
	Synchronize(this);
	
	if(!readString(type))
		return false;
	
	if(!readString(content))
		throw Exception("Connexion unexpectedly closed");
	
	return true;
}

bool Network::Handler::readString(String &str)
{
	Synchronize(this);
	
	str.clear();
	while(true)
	{
		// Try to read
		char chr;
		while(mSink.read(&chr, 1))
		{
			if(chr == '\0')
			{
				// Finished
				return true;
			}
			
			str+= chr;
		}
		
		// We need more combinations
		Fountain::Combination combination;
		BinarySerializer serializer(mStream);
		DesynchronizeStatement(this, if(!serializer.read(combination)) break);
		
		uint32_t nextSeen = 0;
		serializer.read(nextSeen);
		
		BinaryString data;
		mStream->readBinary(data);
		combination.setCodedData(data);
		mStream->nextRead();
		
		//LogDebug("Network::Handler::readString", "Received combination (size=" + String::number(combination.size()) + ", nextSeen=" + String::number(nextSeen) + ")");
		
		mTokens+= mSource.drop(nextSeen)*(1.+1./(mTokens+1.));
		if(!combination.isNull())
		{
			mSink.solve(combination);
		
			if(!send(false))
				Scheduler::Global->schedule(&mTimeoutTask, mTimeout/10);
		}
	}
	
	return false;
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
	
	Network::Instance->onConnected(mLink, true);
	
	try {
		process();
		
		LogDebug("Network::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogWarn("Network::Handler", String("Closing handler: ") + e.what());
	}
	
	Network::Instance->onConnected(mLink, false);
	Network::Instance->unregisterHandler(mLink, this);
	Scheduler::Global->cancel(&mTimeoutTask);
	
	notifyAll();
	Thread::Sleep(10.);	// TODO
	delete this;		// autodelete
}

}
