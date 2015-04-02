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

#include "tpn/core.h"
#include "tpn/user.h"
#include "tpn/store.h"
#include "tpn/httptunnel.h"
#include "tpn/config.h"

#include "pla/binaryserializer.h"
#include "pla/jsonserializer.h"
#include "pla/securetransport.h"
#include "pla/crypto.h"
#include "pla/random.h"
#include "pla/http.h"

namespace tpn
{

Core *Core::Instance = NULL;


Core::Core(int port) :
		mThreadPool(4, 16, Config::Get("max_connections").toInt()),
		mLastPublicIncomingTime(0)
{
	bool configChanged = false;
	String tmp;
	
	// Define instance number
	mNumber = 0;
	tmp = Config::Get("instance_number");
	tmp.hexaMode(true);
	tmp.read(mNumber);
	if(!mNumber)
	{
		Set<BinaryString> hardwareAddrs;
		
		try {
			DatagramSocket dummy;
			dummy.getHardwareAddresses(hardwareAddrs);
		}
		catch(...)
		{

		}
		
		if(!hardwareAddrs.empty())
		{
			BinaryString tmp = *hardwareAddrs.rbegin();
			BinaryString digest;
			Sha256().compute(tmp, digest);
			digest.readBinary(mNumber);
		}
		else {
			LogWarn("Core", "Unable to get a hardware address, using a random instance number");
		}
		
		if(mNumber == 0)
		{
			Random rnd(Random::Nonce);
			while(mNumber == 0) 
				rnd.read(mNumber);
		}
		
		Config::Put("instance_number", String::hexa(mNumber));
		configChanged = true;
	}
	
	// Define instance name
	mName = Config::Get("instance_name");
	if(mName.empty())
	{
		char hostname[HOST_NAME_MAX];
		if(!gethostname(hostname,HOST_NAME_MAX)) 
			mName = hostname;
		
		if(mName.empty() || mName == "localhost")
		{
		#ifdef ANDROID
			mName = String("Android");
		#else
			mName = String::hexa(mNumber);
		#endif
			
			Config::Put("instance_name", mName);
			configChanged = true;
		}
	}
	
	LogInfo("Core", "Instance name is \"" + mName + "\", unique number is " + String::hexa(mNumber));
	
	if(configChanged)
	{
		const String configFileName = "config.txt";
		Config::Save(configFileName);
	}
	
	mTunneler = NULL;
	
	try {
		// Create tunneller 
		mTunneler = new Tunneler;
	
		// Create backends
		mBackends.push_back(new StreamBackend(this, port));
		mBackends.push_back(new DatagramBackend(this, port));
	}
	catch(...)
	{
		// Delete tunneller
		delete mTunneler;
		
		// Delete created backends
		for(List<Backend*>::iterator it = mBackends.begin();
			it != mBackends.end();
			++it)
		{
			Backend *backend = *it;
			delete backend;
		}
		
		throw;
	}
}

Core::~Core(void)
{
	join();
	
	// Delete tunneller
	delete mTunneler;
	
	// Delete backends
	for(List<Backend*>::iterator it = mBackends.begin();
		it != mBackends.end();
		++it)
	{
		Backend *backend = *it;
		delete backend;
	}
}

void Core::start(void)
{
	// Start tunneller
	mTunneler->join();
	
	// Start backends
	for(List<Backend*>::iterator it = mBackends.begin();
		it != mBackends.end();
		++it)
	{
		Backend *backend = *it;
		backend->start();
	}
}

void Core::join(void)
{
	// Join tunneller
	mTunneler->join();
	
	// Join backends
	for(List<Backend*>::iterator it = mBackends.begin();
		it != mBackends.end();
		++it)
	{
		Backend *backend = *it;
		backend->join();
	}
}

uint64_t Core::getNumber(void) const
{
	Synchronize(this);
	Assert(mNumber != 0);
	return mNumber;
}

String Core::getName(void) const
{
	Synchronize(this);
	Assert(!mName.empty());
	return mName;
}

void Core::getAddresses(Set<Address> &set) const
{
	Synchronize(this);
	
	set.clear();
	for(List<Backend*>::const_iterator it = mBackends.begin();
		it != mBackends.end();
		++it)
	{
		const Backend *backend = *it;
		Set<Address> backendSet;
		backend->getAddresses(backendSet);
		set.insertAll(backendSet);
	}
}

void Core::getKnownPublicAdresses(Set<Address> &set) const
{
	Synchronize(this);
	mKnownPublicAddresses.getKeys(set);
}

bool Core::isPublicConnectable(void) const
{
	return (Time::Now()-mLastPublicIncomingTime <= 3600.); 
}

bool Core::connect(const Set<Address> &addrs)
{
	List<Backend*> backends;
	SynchronizeStatement(this, backends = mBackends);
	
	for(List<Backend*>::iterator it = backends.begin();
		it != backends.end();
		++it)
	{
		Backend *backend = *it;
		if(backend->connect(addrs))
			return true;
	}
	
	return false;
}

int Core::connectionsCount(void) const
{
	Synchronize(this);
	return mHandlers.size();
}

void Core::registerCaller(const BinaryString &target, Caller *caller)
{
	Synchronize(this);
	mCallers[target].insert(caller);
	
	LogDebug("Core::registerCaller", "Calling " + target.toString());
	
	StringMap content;
	content["target"] = target.toString();
	outgoing("call", content);
	
	// TODO: beacons
}

void Core::unregisterCaller(const BinaryString &target, Caller *caller)
{
	Synchronize(this);
	
	Map<BinaryString, Set<Caller*> >::iterator it = mCallers.find(target);
	if(it != mCallers.end())
	{
		it->second.erase(caller);
		if(it->second.empty())   
			mCallers.erase(it);
	}
}

void Core::unregisterAllCallers(const BinaryString &target)
{
	Synchronize(this);
	mCallers.erase(target);
}

void Core::registerListener(const Identifier &id, Listener *listener)
{
	Synchronize(this);
	mListeners[id].insert(listener);
	
	//LogDebug("Core::registerListener", "Registered listener: " + id.toString());
	
	// TODO: tunnels
	
	/*List<Identifier> connectedIdentifiers;
	Map<Address, Handler*>::iterator it = mHandlers.find(id);
	while(it != mHandlers.end() && it->first == id)
	{
		connectedIdentifiers.push_back(it->first);
		++it;
	}
	
	if(!connectedIdentifiers.empty())
	{
		Desynchronize(this);
		for(List<Identifier>::iterator it = connectedIdentifiers.begin();
			it != connectedIdentifiers.end();
			++it)
		{
			listener->connected(*it); 
		}
	}*/
}

void Core::unregisterListener(const Identifier &id, Listener *listener)
{
	Synchronize(this);
	
	Map<Identifier, Set<Listener*> >::iterator it = mListeners.find(id);
	while(it != mListeners.end() && it->first == id)
	{
		it->second.erase(listener);
		//if(it->second.erase(listener))
		//	LogDebug("Core::unregisterListener", "Unregistered listener: " + id.toString());
		
		if(it->second.empty())   
			mListeners.erase(it);
			
		++it;
	}
}

void Core::publish(String prefix, Publisher *publisher)
{
	Synchronize(this);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Core::publish", "Publishing " + prefix);
	
	mPublishers[prefix].insert(publisher);
}

void Core::unpublish(String prefix, Publisher *publisher)
{
	Synchronize(this);
	
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

void Core::subscribe(String prefix, Subscriber *subscriber)
{
	Synchronize(this);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);

	LogDebug("Core::subscribe", "Subscribing " + prefix);
	
	mSubscribers[prefix].insert(subscriber);
	
	// Local publishers
	matchPublishers(prefix, Identifier::Null, subscriber);
	
	if(!subscriber->localOnly())
	{
		// Immediatly send subscribe message
		BinaryString payload;
		BinarySerializer serializer(&payload);
		serializer.write(prefix);
		
		StringMap content;
		content["prefix"] = prefix;
		outgoing("subscribe", content);
	}
}

void Core::unsubscribe(String prefix, Subscriber *subscriber)
{
	Synchronize(this);
	
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

void Core::advertise(String prefix, const String &path, const Identifier &source, Publisher *publisher)
{
	Synchronize(this);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Core::publish", "Advertising " + prefix + path);
	
	matchSubscribers(prefix, source, publisher); 
}

void Core::addRemoteSubscriber(const Identifier &peer, const String &path, bool publicOnly)
{
	Synchronize(this);
	
	mRemoteSubscribers.push_front(RemoteSubscriber(peer, publicOnly));
	mRemoteSubscribers.begin()->subscribe(path);
}

bool Core::broadcast(const Identifier &local, const Notification &notification)
{
	Synchronize(this);
	
	return outgoing(local, Identifier::Null, "notif", notification);
}

bool Core::send(const Identifier &local, const Identifier &remote, const Notification &notification)
{
	Synchronize(this);
	
	return outgoing(local, remote, "notif", notification);
}

bool Core::route(const Message &message, const Identifier &from)
{
	Synchronize(this);
	
	// Drop if too many hops
	if(message.hops >= 8)
		return false;
	
	if(message.destination != Identifier::Null)
	{
		// Tunneler::Tunnel messages must not be routed through the same tunnel
		if(message.content != Message::Tunneler::Tunnel || from != Identifier::Null)
		{
			// 1st case: neighbour
			if(mHandlers.contains(message.destination))
				return send(message, message.destination);
		}
		
		// 2nd case: routing table entry exists
		Identifier route;
		if(mRoutes.get(message.destination, route))
			return send(message, route);
	}
	
	// 3rd case: no routing table entry
	return broadcast(message, from);
}

bool Core::broadcast(const Message &message, const Identifier &from)
{
	Synchronize(this);
	
	Array<Identifier> identifiers;
	mHandlers.getKeys(identifiers);
	
	bool success = false;
	for(int i=0; i<identifiers.size(); ++i)
	{
		if(identifiers[i] == from) continue;
		
		// Tunneler::Tunnel messages must not be routed through the same tunnel
		if(message.content != Message::Tunneler::Tunnel || from != Identifier::Null || identifiers[i] != message.destination)
		{
			Handler *handler;
			if(mHandlers.get(identifiers[i], handler))
			{
				Desynchronize(this);
				success|= handler->send(message);
			}
		}
	}
	
	return success;
}

bool Core::send(const Message &message, const Identifier &to)
{
	Synchronize(this);
	
	if(to == Identifier::Null)
	{
		broadcast(message);
		return true;
	}
	
	Handler *handler;
	if(mHandlers.get(to, handler))
	{
		Desynchronize(this);
		handler->send(message);
		return true;
	}
	
	return false;
}

void Core::addRoute(const Identifier &id, const Identifier &route)
{
	Synchronize(this);
	
	if(id == Identifier::Null || route == Identifier::Null)
		return;
	
	if(id == route)
		return;
	
	bool isNew = !mRoutes.contains(id);
	mRoutes.insert(id, route);
	
	if(isNew)
	{
		// New node is seen
		Map<Identifier, Set<Listener*> >::iterator it = mListeners.find(id);
		while(it != mListeners.end() && it->first == id)
		{
			for(Set<Listener*>::iterator jt = it->second.begin();
				jt != it->second.end();
				++jt)
			{
				(*jt)->seen(id); 
			}
			
			++it;
		}
	}
}

bool Core::getRoute(const Identifier &id, Identifier &route)
{
	Synchronize(this);
	
	Map<Identifier, Identifier>::iterator it = mRoutes.find(id);
	if(it == mRoutes.end()) return false;
	route = it->second;
	return true;
}

bool Core::addLink(Stream *stream, const Identifier &local, const Identifier &remote)
{
	// Not synchronized
	Assert(stream);
	
	LogDebug("Core", "New link");
	Link *link = new Link(stream, local, remote);
	mThreadPool.launch(link);
	return true;
}

bool Core::hasLink(const Identifier &local, const Identifier &remote)
{
	Synchronize(this);
	return mLinks.contains(IdentifierPair(local, remote));
}

bool Core::registerHandler(const Address &addr, Core::Handler *handler)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	Handler *h = NULL;
	if(mHandlers.get(addr, h))
		return (h == handler);
	
	mHandlers.insert(host, handler);
	return true;
}

bool Core::unregisterHandler(const String &host, Core::Handler *handler)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	Handler *h = NULL;
	if(!mHandlers.get(addr, h) || h != handler)
		return false;
		
	mHandlers.erase();
		return true;
}

bool Core::registerLink(const Identifier &local, const Identifier &remote, Link *link)
{
	Synchronize(this);
	
	if(!link)
		return false;
	
	IdentifierPair pair(local, remote);
	
	Linker::Link *l = NULL;
	if(mLinker::Links.get(pair, l))
		return (l == link);
	
	mLinker::Links.insert(pair, link);
	return true;
}

bool Core::unregisterLink(const Identifier &local, const Identifier &remote, Link *link)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	IdentifierPair pair(local, remote);
	
	Linker::Link *l = NULL;
	if(!mLinker::Links.get(pair, l) || l != link)
		return false;
		
	mLinker::Links.erase();
		return true;	
}

bool Core::outgoing(const Identifier &local, const Identifier &remote, const String &type, const Serializable &content)
{
	Synchronize(this);
	
	local.setNumber(getNumber());
	
	if(remote != Identifier::Null)
	{
		Map<IdentifierPair, Link*>::iterator it = mLinks.find(IdentifierPair(local, remote));
		if(it != mLinks.end())
		{
			return it->second->outgoing(type, content);
		}
		
		return false;
	}
	else {
		bool success = false;
		for(Map<IdentifierPair, Link*>::iterator it = mLinks.lower_bound(IdentifierPair(local, Identifier::Null));
			it->first == local;
			++it)
		{
			success|= it->second->outgoing(type, content);
		}
		
		return success;
	}
}

bool incoming(const Identifier &local, const Identifier &remote, const String &type, Serializer &serializer)
{
	
}

bool incoming(const Message &message)
{
	Synchronize(this);
	
	if(message.destination.getNumber() != getNumber())
		return false;
	
	Map<IdentifierPair, Tunneler::Tunnel*>::iterator it = mTunneler::Tunnels.find(IdentifierPair(message.destination, message.source));	
	if(it != mTunneler::Tunnels.end()) return it->second->incoming(message);
	else return Core::Instance->mTunneler->incoming(message);
}

bool Core::matchPublishers(const String &path, const Identifier &source, Subscriber *subscriber)
{
	Synchronize(this);
	
	List<String> list;
	path.before('?').explode(list,'/');
	if(list.empty()) return false;
	
	// First item should be empty because path begins with /
	if(list.front().empty()) 
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
				List<BinaryString> result;
				if((*jt)->anounce(source, prefix, truncatedPath, result))
				{
					Assert(!result.empty());
					if(subscriber) 	// local
					{
						for(List<BinaryString>::iterator it = result.begin(); it != result.end(); ++it)
							subscriber->incoming(Identifier::Null, path, "/", *it);
					}
					else targets.splice(targets.end(), result);	// remote
				}
			}
			
			if(!targets.empty()) 
			{
				LogDebug("Core::Handler::incoming", "Anouncing " + path);
				
				// TODO: limit size
				BinaryString response;
				BinarySerializer serializer(&response);
				serializer.write(path);
				serializer.write(targets);
				outgoing(source, Message::Broadcast, Message::Publish, response);
			}
		}
		
		if(list.empty()) break;
		list.pop_back();
	}
	
	return true;
}

bool Core::matchSubscribers(const String &path, const Identifier &source, Publisher *publisher)
{
	Synchronize(this);
	
	List<String> list;
	path.before('?').explode(list,'/');
	if(list.empty()) return false;
	
	// First item should be empty because path begins with /
	if(list.front().empty()) 
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
				List<BinaryString> targets;
				if(publisher->anounce(subscriber->remote(), prefix, truncatedPath, targets))
				{
					for(List<BinaryString>::const_iterator kt = targets.begin();
						kt != targets.end();
						++kt)
					{
						// TODO: should prevent forwarding in case we want to republish another content
						subscriber->incoming(source, prefix, truncatedPath, *kt);
					}
				}
			}
		}
	
		if(list.empty()) break;
		list.pop_back();
	}
	
	return true;
}

bool Core::track(const String &tracker, Set<Address> &result)
{
	LogDebug("Core::track", "Contacting tracker " + tracker);
	
	try {
		String url("http://" + tracker + "/teapotnet/");
		
		// Dirty hack to test if tracker is private or public
		bool trackerIsPrivate = false;
		List<Address> trackerAddresses;
		Resolve(tracker, trackerAddresses);
		for(	List<Address>::iterator it = trackerAddresses.begin();
			it != trackerAddresses.end();
			++it)
		{
			if(it->isPrivate())
			{
				trackerIsPrivate = true;
				break;
			}
		}
		
		Set<Address> addresses, tmp;
		Config::GetExternalAddresses(addresses); 
		
		getKnownPublicAdresses(tmp);	// Our own addresses are mixed with known public addresses
		addresses.insertAll(tmp);
		
		String strAddresses;
		for(	Set<Address>::iterator it = addresses.begin();
			it != addresses.end();
			++it)
		{
			if(!it->isLocal() && (trackerIsPrivate || it->isPublic()))	// We publish only public addresses if tracker is not private
			{
				if(!strAddresses.empty()) strAddresses+= ',';
				strAddresses+= it->toString();
			}
		}
		
		StringMap post;
		if(!strAddresses.empty())
			post["addresses"] = strAddresses;
		
		const String externalPort = Config::Get("external_port");
		if(!externalPort.empty() && externalPort != "auto")
		{
			post["port"] = externalPort;
		}
                else if(!PortMapping::Instance->isAvailable()
			|| !PortMapping::Instance->getExternalAddress(PortMapping::TCP, Config::Get("port").toInt()).isPublic())	// Cascading NATs
		{
			post["port"] = Config::Get("port");
		}
		
		String json;
		int code = Http::Post(url, post, &json);
		if(code == 200)
		{
			JsonSerializer serializer(&json);
			return serializer.input(result);
		}
		
		LogWarn("Core::track", "Tracker HTTP error: " + String::number(code)); 
	}
	catch(const std::exception &e)
	{
		LogWarn("Core::track", e.what()); 
	}
	
	return false;
}

Core::Message::Message(void) :
	version(0),
	flags(0),
	type(Forward),
	content(Empty),
	hops(0)
{
	
}

Core::Message::~Message(void)
{
	
}

void Core::Message::prepare(const Identifier &source, const Identifier &destination, uint8_t type, uint8_t content)
{
	this->source = source;
	this->destination = destination;
	this->type = type;
	this->content = content;
	payload.clear();
}

void Core::Message::clear(void)
{
	source.clear();
	destination.clear();
	payload.clear();
}

void Core::Message::serialize(Serializer &s) const
{
	// TODO
	s.write(source);
	s.write(destination);
	s.write(payload);
}

bool Core::Message::deserialize(Serializer &s)
{
	// TODO
	if(!s.read(source)) return false;
	AssertIO(s.read(destination));
	AssertIO(s.read(payload));
}

Core::Locator::Locator(User *user, const Identifier &id)
{
	this->user = user;
	this->identifier = id;
}

Core::Locator::Locator(User *user, const Address &addr)
{
	this->user = user;
	this->addresses.insert(addr);
}

Core::Locator::Locator(User *user, const Set<Address> &addrs)
{
	this->user = user;
	this->addresses = addrs;
}

Core::Locator::~Locator(void)
{

}

Core::Publisher::Publisher(const Identifier &peer) :
	mPeer(peer)
{

}

Core::Publisher::~Publisher(void)
{
	for(StringSet::iterator it = mPublishedPrefixes.begin();
		it != mPublishedPrefixes.end();
		++it)
	{
		Core::Instance->unpublish(*it, this);
	}
}

void Core::Publisher::publish(const String &prefix, const String &path)
{
	if(!mPublishedPrefixes.contains(prefix))
	{
		Core::Instance->publish(prefix, this);
		mPublishedPrefixes.insert(prefix);
	}
	
	Core::Instance->advertise(prefix, path, mPeer, this);
}

void Core::Publisher::unpublish(const String &prefix)
{
	if(mPublishedPrefixes.contains(prefix))
	{
		Core::Instance->unpublish(prefix, this);
		mPublishedPrefixes.erase(prefix);
	}
}

Core::Subscriber::Subscriber(const Identifier &peer) :
	mPeer(peer),
	mThreadPool(0, 1, 8)
{
	
}

Core::Subscriber::~Subscriber(void)
{
	unsubscribeAll();
}

void Core::Subscriber::subscribe(const String &prefix)
{
	if(!mSubscribedPrefixes.contains(prefix))
	{
		Core::Instance->subscribe(prefix, this);
		mSubscribedPrefixes.insert(prefix);
	}
}

void Core::Subscriber::unsubscribe(const String &prefix)
{
	if(mSubscribedPrefixes.contains(prefix))
	{
		Core::Instance->unsubscribe(prefix, this);
		mSubscribedPrefixes.erase(prefix);
	}
}

void Core::Subscriber::unsubscribeAll(void)
{
	for(StringSet::iterator it = mSubscribedPrefixes.begin();
		it != mSubscribedPrefixes.end();
		++it)
	{
		Core::Instance->unsubscribe(*it, this);
	}
}

Identifier Core::Subscriber::remote(void) const
{
	return Identifier::Null;
}

bool Core::Subscriber::localOnly(void) const
{
	return false;
}

bool Core::Subscriber::fetch(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target)
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
		PrefetchTask(Core::Subscriber *subscriber, const Identifier &peer, const String &prefix, const String &path, const BinaryString &target)
		{
			this->subscriber = subscriber;
			this->peer = peer;
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
				
				subscriber->incoming(peer, prefix, path, target);
			}
			catch(const Exception &e)
			{
				LogWarn("Core::Subscriber::fetch", "Fetching failed for " + target.toString() + ": " + e.what());
			}
			
			delete this;	// autodelete
		}
	
	private:
		Core::Subscriber *subscriber;
		Identifier peer;
		BinaryString target;
		String prefix;
		String path;
	};
	
	PrefetchTask *task = new PrefetchTask(this, peer, prefix, path, target);
	mThreadPool.launch(task);
	return false;
}

Core::RemotePublisher::RemotePublisher(const List<BinaryString> targets):
	mTargets(targets)
{
  
}

Core::RemotePublisher::~RemotePublisher(void)
{
  
}

bool Core::RemotePublisher::anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets)
{
	targets = mTargets;
	return !targets.empty();
}

Core::RemoteSubscriber::RemoteSubscriber(const Identifier &remote, bool publicOnly) :
	mRemote(remote),
	mPublicOnly(publicOnly)
{

}

Core::RemoteSubscriber::~RemoteSubscriber(void)
{

}

bool Core::RemoteSubscriber::incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target)
{
	if(mRemote != Identifier::Null)
	{
		SerializableArray<BinaryString> array;
		array.append(target);
		
		BinaryString payload;
		BinarySerializer serializer(&payload);
		serializer.write(prefix);
		serializer.write(array);
		Core::Instance->outgoing(mRemote, Message::Forward, Message::Publish, payload); 
	}
}

Identifier Core::RemoteSubscriber::remote(void) const
{
	if(!mPublicOnly) return mRemote;
	else return Identifier::Null;
}

bool Core::RemoteSubscriber::localOnly(void) const
{
	return true;
}

Core::Caller::Caller(void)
{
	
}

Core::Caller::Caller(const BinaryString &target)
{
	Assert(!target.empty());
	startCalling(target);
}

Core::Caller::~Caller(void)
{
	stopCalling();
}
	
void Core::Caller::startCalling(const BinaryString &target)
{
	if(target != mTarget)
	{
		stopCalling();
		
		mTarget = target;
		if(!mTarget.empty()) Core::Instance->registerCaller(mTarget, this);
	}
}

void Core::Caller::stopCalling(void)
{
	if(!mTarget.empty())
	{
		Core::Instance->unregisterCaller(mTarget, this);
		mTarget.clear();
	}
}

Core::Listener::Listener(void)
{
	
}

Core::Listener::~Listener(void)
{
	for(Set<Identifier>::iterator it = mPeers.begin();
		it != mPeers.end();
		++it)
	{
		Core::Instance->unregisterListener(*it, this);
	}
}

void Core::Listener::listen(const Identifier &peer)
{
	mPeers.insert(peer);
	Core::Instance->registerListener(peer, this);
}

Core::Backend::Backend(Core *core) :
	mCore(core),
	mAnonymousClientCreds(),
	mPrivateSharedKeyServerCreds(String::hexa(core->getNumber()))
{
	Assert(mCore);
}

Core::Backend::~Backend(void)
{
	
}

bool Core::Backend::process(SecureTransport *transport, const Set<Address> &addrs)
{
	LogDebug("Core::Backend::process", "Setting anonymous credentials");
	
	// Add anonymous credentials
	transport->addCredentials(&mAnonymousClientCreds);
	
	return handshake(transport, Identifier::Null, Identifier::Null, false);
}

bool Core::Backend::handshake(SecureTransport *transport, bool async)
{
	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(SecureTransport *transport) { this->transport = transport; }
		
		bool handshake(void)
		{
			LogDebug("Core::Backend::handshake", "HandshakeTask starting...");
			
			try {
				// Do handshake
				transport->handshake();
				
				// Handshake succeeded, add peer
				LogDebug("Core::Backend::handshake", "Success, spawning new handler");
				
				Handler *handler = new Handler(this, stream,  &mThreadPool, Identifier(local, Instance::Core->getNumber()), remote);
				return true;
			}
			catch(const std::exception &e)
			{
				LogInfo("Core::Backend::handshake", String("Handshake failed: ") + e.what());
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
	};
	
	HandshakeTask *task = NULL;
	try {
		if(async)
		{
			task = new HandshakeTask(mCore, transport, local, remote);
			mThreadPool.launch(task);
			return true;
		}
		else {
			HandshakeTask stask(mCore, transport, local, remote);
			return stask.handshake();
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Backend::handshake", e.what());
		delete task;
		delete transport;
		return false;
	}
}

void Core::Backend::run(void)
{
	try {
		while(true)
		{
			SecureTransport *transport = listen();
			if(!transport) break;
			
			LogDebug("Core::Backend::run", "Incoming connection");
			
			// Add server credentials
			transport->addCredentials(&mAnonymousServerCreds, false);
			
			// No remote identifier specified, accept any identifier
			handshake(transport, true);	// async
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Backend::run", e.what());
	}
	
	LogWarn("Core::Backend::run", "Closing backend");
}

Core::StreamBackend::StreamBackend(Core *core, int port) :
	Backend(core),
	mSock(port)
{

}

Core::StreamBackend::~StreamBackend(void)
{
	
}

bool Core::StreamBackend::connect(const Set<Address> &addrs)
{
	for(Set<Address>::const_reverse_iterator it = addrs.rbegin();
		it != addrs.rend();
		++it)
	{
		try {
			if(connect(*it))
				return true;
		}
		catch(const NetException &e)
		{
			LogDebug("Core::StreamBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Core::StreamBackend::connect(const Address &addr)
{
	Socket *sock = NULL;
	SecureTransport *transport = NULL;
	
	LogDebug("Core::StreamBackend::connect", "Trying address " + addr.toString() + " (TCP)");
	
	try {
		const double timeout = 5.;		// TODO
		
		sock = new Socket;
		sock->setConnectTimeout(timeout);
		sock->connect(addr);
		
		transport = new SecureTransportClient(sock, NULL, "");
	}
	catch(...)
	{
		delete sock;
		throw;
	}
	
	return process(transport, locator);
}

SecureTransport *Core::StreamBackend::listen(void)
{
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, true);	// ask for certificate
		if(transport) return transport;
	}
	
	return NULL;
}

void Core::StreamBackend::getAddresses(Set<Address> &set) const
{
	mSock.getLocalAddresses(set);
}

Core::DatagramBackend::DatagramBackend(Core *core, int port) :
	Backend(core),
	mSock(port)
{
	
}

Core::DatagramBackend::~DatagramBackend(void)
{
	
}

bool Core::DatagramBackend::connect(const Set<Address> &addrs)
{
	for(Set<Address>::const_reverse_iterator it = addrs.rbegin();
		it != addrs.rend();
		++it)
	{
		try {
			if(connect(*it))
				return true;
		}
		catch(const NetException &e)
		{
			LogDebug("Core::DatagramBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Core::DatagramBackend::connect(const Address &addr)
{
	DatagramStream *stream = NULL;
	SecureTransport *transport = NULL;
	
	LogDebug("Core::DatagramBackend::connect", "Trying address " + addr.toString() + " (UDP)");
	
	try {
		stream = new DatagramStream(&mSock, addr);
		transport = new SecureTransportClient(stream, NULL);
	}
	catch(...)
	{
		delete stream;
		throw;
	}
	
	return process(transport, locator);
}

SecureTransport *Core::DatagramBackend::listen(void)
{
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, true);	// ask for certificate
		if(transport) return transport;
	}
	
	return NULL;
}

void Core::DatagramBackend::getAddresses(Set<Address> &set) const
{
	mSock.getLocalAddresses(set);
}

Core::Tunneler(Core *core) :
	Backend(core)
{

}

Core::Tunneler::~Tunneler(void)
{
	
}

bool Core::Tunneler::open(const Identifier &identifier, User *user)
{
	Assert(locator.user);
	
	if(identifier.empty())
		return false;
	
	if(mCore->connectionsCount() == 0)
		return false;
	
	LogDebug("Core::Tunneler::open", "Trying tunnel for " + locator.identifier.toString());
	
	Identifier remote(locator.identifier);
	Identifier local(locator.user->identifier(), mCore->getNumber());
	
	Tunneler::Tunnel *tunnel = NULL;
	SecureTransport *transport = NULL;
	try {
		tunnel = new Tunneler::Tunnel(local, remote);
		transport = new SecureTransportClient(tunnel, NULL);
	}
	catch(...)
	{
		delete tunnel;
		throw;
	}
	
	LogDebug("Core::Tunneler::open", "Setting certificate credentials: " + user->name());
		
	// Set remote name and local instance hint
	String name = identifier.digest().toString() + "#" + String::hexa(Core::Instance->getNumber());
	transport->setHostname(name);
	
	Identifier local(user->identifier(), Core::Instance->getNumber());
	
	// Add user certificate
	SecureTransportClient::Certificate *cert = user->certificate();
	if(cert) transport->addCredentials(cert, false);
	
	mThreadPool.run(tunnel);
	
	return handshake(transport, local, identifier, false);	// sync
}

SecureTransport *Core::Tunneler::listen(void)
{
	Synchronize(&mQueueSync);
	
	while(mQueue.empty()) mQueueSync.wait();
	
	const Message &datagram = mQueue.front();
	
	LogDebug("Core::Tunneler::listen", "Incoming tunnel from " + datagram.source.toString());

	Identifier local(datagram.destination, mCore->getNumber());
	Identifier remote(datagram.source);

	Tunneler::Tunnel *tunnel = NULL;
	SecureTransport *transport = NULL;
	try {
		tunnel = new Tunneler::Tunnel(mCore, local, remote);
		transport = new SecureTransportServer(tunnel, NULL, true);	// ask for certificate
	}
	catch(...)
	{
		delete tunnel;
		mQueue.pop();
		throw;
	}
	
	mTunneler::Tunnels.insert(IdentifierPair(local, remote), tunnel);
	tunnel->incoming(datagram);
	
	mThreadPool.run(tunnel);
	
	mQueue.pop();
	return transport;
}

bool Core::Tunneler::incoming(const Message &datagram)
{
	Synchronize(&mQueueSync);
	mQueue.push(datagram);
	mQueueSync.notifyAll();
	return true;
}

bool registerTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	
	if(!tunnel)
		return false;
	
	IdentifierPair pair(tunnel->local(), tunnel->remote());
	
	Tunneler::Tunnel *t = NULL;
	if(mTunneler::Tunnels.get(pair, t))
		return (t == tunnel);
	
	mTunneler::Tunnels.insert(pair, tunnel);
	return true;
}

bool unregisterTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	IdentifierPair pair(tunnel->local(), tunnel->remote());
	
	Tunneler::Tunnel *t = NULL;
	if(!mTunneler::Tunnels.get(pair, t) || t != tunnel)
		return false;
		
	mTunneler::Tunnels.erase();
		return true;	
}

bool Core::Tunneler::handshake(SecureTransport *transport, const Identifier &local, const Identifier &remote, bool async)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Identifier local, remote;
		Rsa::PublicKey publicKey;
		uint64_t instance;
		
		MyVerifier(void) { this->instance = 0; }
		
		bool verifyName(const String &name, SecureTransport *transport)
		{
			String digest = name;			// local digest
			String number = digest.cut('#');	// remote instance
			if(!number.empty())
			{
				number.hexaMode(true);
				number.read(instance);
			}
			
			LogDebug("Core::Tunneler::handshake", String("Verifying user: ") + digest);
			
			Identifier id;
			try {
				id.fromString(digest);
			}
			catch(...)
			{
				LogDebug("Core::Tunneler::handshake", String("Invalid identifier: ") + digest);
				return false;
			}
			
			User *user = User::GetByIdentifier(id);
			if(user)
			{
				SecureTransport::Credentials *creds = user->certificate();
				if(creds) transport->addCredentials(creds);
				local = Identifier(user->identifier(), core->getNumber());
			}
			else {
				 LogDebug("Core::Tunneler::handshake", String("User does not exist: ") + name);
			}
			
			return true;	// continue handshake anyway
		}
		
		bool verifyCertificate(const Rsa::PublicKey &pub)
		{
			publicKey = pub;
			remote = Identifier(publicKey.digest(), instance);
			
			LogDebug("Core::Tunneler::handshake", String("Verifying remote certificate: ") + remote.toString());
			
			Synchronize(core);
			
			Map<Identifier, Set<Listener*> >::iterator it = core->mListeners.find(remote);
			while(it != core->mListeners.end() && it->first == remote)
			{
				for(Set<Listener*>::iterator jt = it->second.begin();
					jt != it->second.end();
					++jt)
				{
					if((*jt)->auth(remote, publicKey))
						return true;
				}
				
				++it;
			}
			
			LogDebug("Core::Tunneler::handshake", "Certificate verification failed");
			return false;
		}
	};
	
	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(SecureTransport *transport, const Identifier &local, const Identifier &remote)
		{ 
			this->transport = transport;
			this->local = local;
			this->remote = remote;
		}
		
		bool handshake(void)
		{
			LogDebug("Core::Tunneler::handshake", "HandshakeTask starting...");
			
			try {
				// Set verifier
				MyVerifier verifier;
				transport->setVerifier(&verifier);
				
				// Do handshake
				transport->handshake();
				Assert(transport->hasCertificate());
				
				// Assign local if client
				if(transport->isClient())
				{
					verifier.local = local;
					verifier.remote.setNumber(remote.number());	// pass instance number
				}
						
				// Check remote identifier
				if(remote != Identifier::Null && verifier.remote != remote)
					throw Exception("Invalid identifier: " + verifier.remote.toString());
				
				// Handshake succeeded
				LogDebug("Core::Tunneler::handshake", "Success");
				
				// TODO: addLink in Core
				
				return true;
			}
			catch(const std::exception &e)
			{
				LogInfo("Core::Tunneler::handshake", String("Handshake failed: ") + e.what());
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
		Identifier local, remote;
	};
	
	HandshakeTask *task = NULL;
	try {
		if(async)
		{
			task = new HandshakeTask(mCore, transport, local, remote);
			mThreadPool.launch(task);
			return true;
		}
		else {
			HandshakeTask stask(mCore, transport, local, remote);
			return stask.handshake();
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Tunneler::handshake", e.what());
		delete task;
		delete transport;
		return false;
	}
}

void Core::Tunneler::run(void)
{
	try {
		while(true)
		{
			SecureTransport *transport = listen();
			if(!transport) break;
			
			LogDebug("Core::Backend::run", "Incoming tunnel");
			
			handshake(transport, Identifier::Null, Identifier::Null, true); // async
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Tunneler::run", e.what());
	}
	
	LogWarn("Core::Backend::run", "Closing tunneler");
}

Core::Tunneler::Tunnel::Tunneler::Tunnel(Tunneler *tunneler, const Identifier &local, const Identifier &remote) :
	mTunneler(tunneler),
	mLocal(local),
	mRemote(remote),
	mTimeout(DefaultTimeout)
{
	mTunneler->registerTunnel(this);
}

Core::Tunneler::Tunnel::~Tunneler::Tunnel(void)
{
	mTunneler->unregisterTunnel(this);
}

Identifier Core::Tunneler::Tunnel::local(void) const
{
	return mLocal;
}

Identifier Core::Tunneler::Tunnel::remote(void) const
{
	return mRemote;
}

void Core::Tunneler::Tunnel::setTimeout(double timeout)
{
	mTimeout = timeout;
}

size_t Core::Tunneler::Tunnel::readData(char *buffer, size_t size)
{
	Synchronize(&mQueueSync);
	
	double timeout = mTimeout;
        while(mQueue.empty())
		if(!mQueueSync.wait(timeout))
			throw Timeout();
	
	const Message &message = mQueue.front();
	size = std::min(size, size_t(message.payload.size()));
	std::copy(message.payload.data(), message.payload.data() + size, buffer);
        mQueue.pop();
        return size;
}

void Core::Tunneler::Tunnel::writeData(const char *data, size_t size)
{
	Message message;
	message.prepare(mLocal, mRemote, Message::Forward, Message::Tunneler::Tunnel);
	message.payload.writeBinary(data, size);
	mCore->route(message);
}

bool Core::Tunneler::Tunnel::waitData(double &timeout)
{
	Synchronize(&mQueueSync);
	
	while(mQueue.empty())
	{
		if(timeout == 0.)
			return false;
		
		if(!mQueueSync.wait(timeout))
			return false;
	}
	
	return true;
}

bool Core::Tunneler::Tunnel::waitData(const double &timeout)
{
	double dummy = timeout;
	return waitData(dummy);
}

bool Core::Tunneler::Tunnel::isDatagram(void) const
{
	return true; 
}

bool Core::Tunneler::Tunnel::incoming(const Message &datagram)
{
	Synchronize(&mQueueSync);
	mQueue.push(datagram);
	mQueueSync.notifyAll();
	return true;
}

Core::Handler::Handler(Core *core, Stream *stream, ThreadPool *pool, const Address &addr) :
	mCore(core),
	mStream(stream),
	mAddress(addr)
{
	Core::Instance->registerHandler(mAddress, this);
}

Core::Handler::~Handler(void)
{
	Core::Instance->unregisterHandler(mAddress, this);	// should be done already
	
	delete mSender;
	delete mStream;
}

bool Core::Handler::recv(Message &datagram)
{
	Synchronize(this);
	
	const size_t MaxSize = 1500;	// TODO
	char buffer[MaxSize];
	size_t size = 0;
	
	if(mStream->isDatagram())
	{
		Desynchronize(this);
		
		size = mStream->readBinary(buffer, MaxSize);
	}
	else {
		Desynchronize(this);
		
		uint16_t datagramSize = 0;
		if(!mStream->readBinary(datagramSize))
			return false;
		
		size = datagramSize;
		if(mStream->readBinary(buffer, size) != size)
			throw Exception("Connection unexpectedly closed (size should be " + String::number(unsigned(size))+")");
	}
	
	if(size)
	{
		ByteArray s(buffer, size);
		BinarySerializer serializer(&s);
		AssertIO(serializer.read(datagram.source));
		AssertIO(serializer.read(datagram.destination));
		
		datagram.payload.clear();
		s.readBinary(datagram.payload);
		return true;
	}
	
	return false;
}

bool Core::Handler::send(const Message &datagram)
{
	Synchronize(this);
	
	ByteArray buffer(1500);
	
	BinarySerializer serializer(&buffer);
	serializer.write(datagram.source);
	serializer.write(datagram.destination);
	
	buffer.writeBinary(datagram.payload);
	
	if(!mStream->isDatagram())
		mStream->writeBinary(uint16_t(buffer.size()));
	
	DesynchronizeStatement(this, mStream->writeBinary(buffer.data(), buffer.size()));
}

/*bool Core::Handler::incoming(const Message &message)
{
	Synchronize(this);
	
	if(message.content != Message::Tunneler::Tunnel && message.content != Message::Data)
		LogDebug("Core::Handler", "Incoming message (content=" + String::number(unsigned(message.content)) + ", size=" + String::number(unsigned(message.payload.size())) + ")");
	
	const Identifier &source = message.source;
	BinaryString payload = message.payload;		// copy
	
	switch(message.content)
	{
		case Message::Tunneler::Tunnel:
		{
			Core::instance->dispatchTunneler::Tunnel(message);
			break; 
		}
		
		case Message::Notify:
		{
			Notification notification;
			JsonSerializer json(&payload);
			json.read(notification);
			
			// TODO: correct sync
			// TODO: getListeners function in Core
			Desynchronize(this);
			Synchronize(Core::Instance);
			Map<Identifier, Set<Listener*> >::iterator it = mCore->mListeners.find(source);
			while(it != mCore->mListeners.end() && it->first == source)
			{
				Set<Listener*> set = (it++)->second;
				Set<Listener*>::iterator jt = set.begin();
				while(jt != set.end())
					(*jt++)->recv(source, notification);
			}
			
			break;
		}
		
		case Message::Ack:
		{
			// TODO
			break;
		}
		
		case Message::Call:
		{
			Desynchronize(this);
			BinarySerializer serializer(&payload);
			
			BinaryString target;
			uint16_t tokens;
			AssertIO(serializer.read(target));
			AssertIO(serializer.read(tokens));
			
			// TODO: function
			unsigned left = tokens;
			while(left)
			{
				BinaryString payload;
				BinarySerializer serializer(&payload);
				serializer.write(target);
				
				if(!Store::Instance->pull(target, payload, &left))
					break;
				
				if(!mCore->outgoing(source, Message::Forward, Message::Data, payload))
					break;
			}
			
			if(left == tokens)
				return false;
			break;
		}
		
		case Message::Data:
		{
			Desynchronize(this);
			
			BinarySerializer serializer(&payload);
			
			BinaryString target;
			AssertIO(serializer.read(target));
			
			if(Store::Instance->push(target, payload))
				mCore->unregisterAllCallers(target);
			break;
		}
		
		case Message::Publish:
		{
			Desynchronize(this);
			
			BinarySerializer serializer(&payload);
			
			String path;
			AssertIO(serializer.read(path));
			
			SerializableList<BinaryString> targets;
			AssertIO(serializer.read(targets));
			
			RemotePublisher publisher(targets);
			return mCore->matchSubscribers(path, (source == mRemote ? source : Identifier::Null), &publisher);
		}
		
		case Message::Subscribe:
		{
			Desynchronize(this);
			
			BinarySerializer serializer(&payload);
			
			String path;
			AssertIO(serializer.read(path));
		
			mCore->addRemoteSubscriber(source, path, (source != mRemote));
			return true;
		}
		
		default:
			return false;
	}
	
	return true;
}*/

void Core::Handler::process(void)
{
	Synchronize(this);
	
	Message datagram;
	while(recv(datagram))
	{
		Desynchronize(this);
		//LogDebug("Core::Handler", "Received datagram");
		Core::Instance->incoming(datagram);
	}
}

void Core::Handler::run(void)
{
	try {
		LogDebug("Core::Handler", "Starting handler");
	
		process();
		
		LogDebug("Core::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogDebug("Core::Handler", String("Closing handler: ") + e.what());
	}
	
	mCore->unregisterHandler(mAddress, this);
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}

Core::Link::Link(Stream *stream) :
	mStream(stream),
	mTokens(0.),
	mRedundancy(1.1)	// TODO
{
	Core::Instance->registerLink(this);
}

Core::Link::~Link(void)
{
	Core::Instance->unregisterLink(this);	// should be done already
	
	delete mStream;
}

bool read(String &type, String &content)
{
	Synchronize(this);
	
	content.clear();
	while(true)
	{
		// Try to read some content
		char chr;
		while(mSink.read(&chr, 1))
		{
			if(chr == '\0')
			{
				// Finished
				// TODO: type
				return true;
			}
			
			content+= chr;
		}
		
		// We need more combinations to get more content
		BinaryString temp;
		DesynchronizeStatement(this, if(!mStream->readBinary(temp)) return false);
		
		Fountain::Combination combination;
		BinarySerializer serializer(&temp);
		serializer.input(combination);
		combination.setData(temp);
		mSink.solve(combination);
	}
}

void wirte(const String &type, const String &content)
{
	Synchronize(this);
	mSource.write(data, size);
	
	// TODO: tokens
}

void Core::Link::process(void)
{
	Synchronize(this);
/*
	// TODO: correct sync
	Map<Identifier, Set<Listener*> >::iterator it = mCore->mListeners.find(mRemote);
	while(it != mCore->mListeners.end() && it->first == mRemote)
	{
		for(Set<Listener*>::iterator jt = it->second.begin();
			jt != it->second.end();
			++jt)
		{
			(*jt)->connected(mRemote); 
		}
		
		++it;
	}
*/	

}

void Core::Link::run(void)
{
	try {
		LogDebug("Core::Link", "Starting link");
	
		process();
		
		LogDebug("Core::Link", "Closing link");
	}
	catch(const std::exception &e)
	{
		LogDebug("Core::Link", String("Closing link: ") + e.what());
	}
	
	mCore->unregisterLink(mAddress, this);
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}


}
