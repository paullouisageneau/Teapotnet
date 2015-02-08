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
	
	mTunnelBackend = NULL;

	try {
		// Create backends
		mTunnelBackend = new TunnelBackend(this);
		mBackends.push_back(mTunnelBackend);
		mBackends.push_back(new StreamBackend(this, port));
		mBackends.push_back(new DatagramBackend(this, port));
	}
	catch(...)
	{
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
	// Join backends
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

bool Core::connect(const Locator &locator)
{
	List<Backend*> backends;
	SynchronizeStatement(this, backends = mBackends);
	
	for(List<Backend*>::iterator it = backends.begin();
		it != backends.end();
		++it)
	{
		Backend *backend = *it;
		if(backend->connect(locator))
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
	
	uint16_t tokens = 1024;	// TODO
	BinaryString payload;
	BinarySerializer serializer(&payload);
	serializer.write(target);
	serializer.write(tokens);
	outgoing(Message::Lookup, Message::Call, payload);
	
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
	
	List<Identifier> connectedIdentifiers;
	Map<Identifier, Handler*>::iterator it = mHandlers.find(id);
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
	}
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
		outgoing(Message::Lookup, Message::Subscribe, payload);
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

bool Core::broadcast(const Notification &notification)
{
	Synchronize(this);
	
	String payload;
	JsonSerializer serializer(&payload);
	serializer.write(notification);
	
	Array<Identifier> identifiers;
	mHandlers.getKeys(identifiers);
	
	bool success = false;
	for(int i=0; i<identifiers.size(); ++i)
	{
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			String tmp(payload);
			success|= handler->outgoing(handler->remote(), Message::Broadcast, Message::Notify, tmp);
		}
	}
	
	return success;
}

bool Core::send(const Identifier &peer, const Notification &notification)
{
	Synchronize(this);
	
	Handler *handler;
	if(mHandlers.get(peer, handler))
	{
		Desynchronize(this);
		
		String payload;
		JsonSerializer serializer(&payload);
		serializer.write(notification);
		
		return handler->outgoing(peer, Message::Forward, Message::Notify, payload);
	}
	
	return false;
}

bool Core::route(const Message &message, const Identifier &from)
{
	Synchronize(this);
	
	// Drop if too many hops
	if(message.hops >= 8)
		return false;
	
	if(message.destination != Identifier::Null)
	{
		// Tunnel messages must not be routed through the same tunnel
		if(message.content != Message::Tunnel || from != Identifier::Null)
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
		
		// Tunnel messages must not be routed through the same tunnel
		if(message.content != Message::Tunnel || from != Identifier::Null || identifiers[i] != message.destination)
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

bool Core::addPeer(Stream *stream, const Identifier &local, const Identifier &remote)
{
	// Not synchronized
	Assert(stream);
	
	LogDebug("Core", "Spawning new handler");
	Handler *handler = new Handler(this, stream,  &mThreadPool, Identifier(local, getNumber()), remote);
	return true;
}

bool Core::hasPeer(const Identifier &remote)
{
	Synchronize(this);
	return mHandlers.contains(remote);
}

/*
void Core::run(void)
{
	LogDebug("Core", "Starting...");
	
	try {
		while(true)
		{
			Thread::Sleep(0.01);

			Socket *sock = new Socket;
			mSock.accept(*sock);
			
			try {
				Address addr;
				const size_t peekSize = 5;	
				char peekData[peekSize];
				std::memset(peekData, 0, peekSize);
				
				try {
					addr = sock->getRemoteAddress();
					LogDebug("Core::run", "Incoming connection from " + addr.toString());
					
					if(addr.isPublic() && addr.isIpv4()) // TODO: isPublicConnectable() currently reports state for ipv4 only
						mLastPublicIncomingTime = Time::Now();
					
					sock->setTimeout(milliseconds(Config::Get("tpot_timeout").toInt()));
					sock->peekData(peekData, peekSize);
					
					sock->setTimeout(milliseconds(Config::Get("tpot_read_timeout").toInt()));
				}
				catch(const std::exception &e)
				{
					delete sock;
					throw;
				}
			
				Stream *bs = NULL;
				
				if(std::memcmp(peekData, "GET ", 4) == 0
					|| std::memcmp(peekData, "POST ", 5) == 0)
				{
					// This is HTTP, forward connection to HttpTunnel
					bs = HttpTunnel::Incoming(sock);
					if(!bs) continue;
				}
				else {
					bs = sock;
				}
				
				LogInfo("Core", "Incoming peer from " + addr.toString() + " (tunnel=" + (bs != sock ? "true" : "false") + ")");
				addPeer(bs, addr, Identifier::Null, true);	// async
			}
			catch(const std::exception &e)
			{
				LogDebug("Core::run", String("Processing failed: ") + e.what());
			}
		}
	}
	catch(const NetException &e)
	{
		LogDebug("Core::run", e.what());
	}
	
	LogDebug("Core", "Finished");
}
*/

bool Core::addHandler(const Identifier &peer, Core::Handler *handler)
{
	Assert(handler);

	if(peer == Identifier::Null) 
	{
		LogWarn("Core::addHandler", "Remote identifier is null");
		return false;
	}
	else if(!peer.number())
	{
		LogWarn("Core::addHandler", "Remote identifier has no instance number");
		return false;
	}
	else {
		Synchronize(this);
		
		Handler *h = NULL;
		if(mHandlers.get(peer, h))
			return (h == handler);
		
		mHandlers.insert(peer, handler);
		return true;
	}
}

bool Core::removeHandler(const Identifier &peer, Core::Handler *handler)
{
	Assert(handler);
	
	if(peer != Identifier::Null && peer.number()) 
	{
		Synchronize(this);
	
		Handler *h = NULL;
		if(!mHandlers.get(peer, h) || h != handler)
			return false;
		
		mHandlers.erase(peer);
		return true;
	}
}

bool Core::outgoing(uint8_t type, uint8_t content, Stream &payload)
{
	return outgoing(Identifier::Null, type, content, payload);
}

bool Core::outgoing(const Identifier &dest, uint8_t type, uint8_t content, Stream &payload)
{
	//LogDebug("Core::Handler::outgoing", "Outgoing message (type=" + String::number(unsigned(type)) + ", content=" + String::number(unsigned(content)) + ")");
	
	Message message;
	message.prepare(Identifier::Null, dest, type, content);
	message.payload.write(payload);
	return route(message);
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

bool Core::Backend::process(SecureTransport *transport, const Locator &locator)
{
	if(!locator.peering.empty())
	{
		Identifier local(locator.peering, mCore->getNumber());
		LogDebug("Core::Backend::process", "Setting PSK credentials: " + local.toString());
		
		// Add contact private shared key
		SecureTransportClient::Credentials *creds = new SecureTransportClient::PrivateSharedKey(local.toString(), locator.secret);
		transport->addCredentials(creds, true);	// must delete
		
		return handshake(transport, local, locator.peering, false);
	}
	else if(locator.user)
	{
		LogDebug("Core::Backend::process", "Setting certificate credentials: " + locator.user->name());
		
		// Set remote name and local instance hint
		String name = locator.identifier.digest().toString() + "#" + String::hexa(mCore->getNumber());
		transport->setHostname(name);
		
		Identifier local(locator.user->identifier(), mCore->getNumber());
		
		// Add user certificate
		SecureTransportClient::Certificate *cert = locator.user->certificate();
		if(cert) transport->addCredentials(cert, false);
		
		return handshake(transport, local, locator.identifier, false);
	}
	else {
		LogDebug("Core::Backend::process", "Setting anonymous credentials");
		
		// Add anonymous credentials
		transport->addCredentials(&mAnonymousClientCreds);
		
		return handshake(transport, Identifier::Null, Identifier::Null, false);
	}
}

bool Core::Backend::handshake(SecureTransport *transport, const Identifier &local, const Identifier &remote, bool async)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Identifier local, remote;
		Rsa::PublicKey publicKey;
		uint64_t instance;
		
		MyVerifier(Core *core) { this->core = core; this->instance = 0; }
		
		bool verifyName(const String &name, SecureTransport *transport)
		{
			String digest = name;			// local digest
			String number = digest.cut('#');	// remote instance
			if(!number.empty())
			{
				number.hexaMode(true);
				number.read(instance);
			}
			
			LogDebug("Core::Backend::doHandshake", String("Verifying user: ") + digest);
			
			Identifier id;
			try {
				id.fromString(digest);
			}
			catch(...)
			{
				LogDebug("Core::Backend::doHandshake", String("Invalid identifier: ") + digest);
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
				 LogDebug("Core::Backend::doHandshake", String("User does not exist: ") + name);
			}
			
			return true;	// continue handshake anyway
		}
		
		bool verifyPrivateSharedKey(const String &name, BinaryString &key)
		{
			LogDebug("Core::Backend::doHandshake", String("Verifying PSK: ") + name);
			
			try {
				remote.fromString(name);
				local = Identifier(remote, core->getNumber());
			}
			catch(...)
			{
				LogDebug("Core::Backend::doHandshake", String("Invalid peering: ") + name);
				return false;
			}
			
			if(!remote.empty())
			{
				Synchronize(core);
				
				Map<Identifier, Set<Listener*> >::iterator it = core->mListeners.find(remote);
				while(it != core->mListeners.end() && it->first == remote)
				{
					for(Set<Listener*>::iterator jt = it->second.begin();
						jt != it->second.end();
						++jt)
					{
						if((*jt)->auth(remote, key))
							return true;
					}
					
					++it;
				}
			}

			LogDebug("Core::Backend::doHandshake", String("Peering not found: ") + remote.toString());
			return false;
		}
		
		bool verifyCertificate(const Rsa::PublicKey &pub)
		{
			publicKey = pub;
			remote = Identifier(publicKey.digest(), instance);
			
			LogDebug("Core::Backend::doHandshake", String("Verifying remote certificate: ") + remote.toString());
			
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
			
			LogDebug("Core::Backend::doHandshake", "Certificate verification failed");
			return false;
		}
		
	private:
		Core *core;
	};
	
	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(Core *core, SecureTransport *transport, const Identifier &local, const Identifier &remote)
		{ 
			this->core = core;
			this->transport = transport;
			this->local = local;
			this->remote = remote;
		}
		
		bool handshake(void)
		{
			LogDebug("Core::Backend::doHandshake", "HandshakeTask starting...");
			
			try {
				// Set verifier
				MyVerifier verifier(core);
				transport->setVerifier(&verifier);
				
				// Do handshake
				transport->handshake();

				if(transport->hasCertificate())
				{
					// Assign local if client
					if(transport->isClient())
					{
						verifier.local = local;
						verifier.remote.setNumber(remote.number());	// pass instance number
					}
						
					// Check remote identifier
					if(remote != Identifier::Null && verifier.remote != remote)
						throw Exception("Invalid identifier: " + verifier.remote.toString());
					
					// Sanity checks
					Assert(verifier.local != Identifier::Null);
					Assert(verifier.remote != Identifier::Null);
				}
				else if(transport->hasPrivateSharedKey())
				{
					// Assign identifiers if client
					if(transport->isClient())
					{
						verifier.local = local;
						verifier.remote = remote;
						
						// Remote instance is transmitted in PSK hint
						String hint = transport->getPrivateSharedKeyHint();
						if(hint.empty())
							throw Exception("Missing PSK hint");
						
						uint64_t number = 0;
						hint.hexaMode(true);
						hint.read(number);
						verifier.remote.setNumber(number);
					}
					
					// Sanity checks
					Assert(verifier.local.digest() == verifier.remote.digest());
				}
				else {
					// Sanity checks
					Assert(verifier.local == Identifier::Null);
					Assert(verifier.remote == Identifier::Null);
				}
				
				// Handshake succeeded, add peer
				LogDebug("Core::Backend::doHandshake", "Handshake succeeded");
				core->addPeer(transport, verifier.local, verifier.remote);
				return true;
			}
			catch(const std::exception &e)
			{
				LogInfo("Core::Backend::doHandshake", String("Handshake failed: ") + e.what());
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
		Core *core;
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
			HandshakeTask task(mCore, transport, local, remote);
			return task.handshake();
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Backend::doHandshake", e.what());
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
			
			if(!transport->isHandshakeDone())
			{
				// Add server credentials (certificate added on name verification)
				transport->addCredentials(&mAnonymousServerCreds, false);
				transport->addCredentials(&mPrivateSharedKeyServerCreds, false);
				
				// No remote identifier specified, accept any identifier
				handshake(transport, Identifier::Null, Identifier::Null, true); // async
			}
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

bool Core::StreamBackend::connect(const Locator &locator)
{
	for(Set<Address>::const_reverse_iterator it = locator.addresses.rbegin();
		it != locator.addresses.rend();
		++it)
	{
		try {
			if(connect(*it, locator))
				return true;
		}
		catch(const NetException &e)
		{
			LogDebug("Core::StreamBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Core::StreamBackend::connect(const Address &addr, const Locator &locator)
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

bool Core::DatagramBackend::connect(const Locator &locator)
{
	for(Set<Address>::const_reverse_iterator it = locator.addresses.rbegin();
		it != locator.addresses.rend();
		++it)
	{
		try {
			if(connect(*it, locator))
				return true;
		}
		catch(const NetException &e)
		{
			LogDebug("Core::DatagramBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Core::DatagramBackend::connect(const Address &addr, const Locator &locator)
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

Core::TunnelBackend::TunnelBackend(Core *core) :
	Backend(core)
{

}

Core::TunnelBackend::~TunnelBackend(void)
{
	
}

bool Core::TunnelBackend::connect(const Locator &locator)
{
	Assert(locator.user);
	
	if(locator.identifier.empty())
		return NULL;
	
	if(mCore->connectionsCount() == 0)
		return NULL;
	
	LogDebug("Core::TunnelBackend::connect", "Trying tunnel for " + locator.identifier.toString());
	
	Identifier remote(locator.identifier);
	Identifier local(locator.user->identifier(), mCore->getNumber());

	TunnelWrapper *wrapper = NULL;
	SecureTransport *transport = NULL;
	try {
		wrapper = new TunnelWrapper(mCore, local, remote);
		transport = new SecureTransportClient(wrapper, NULL);
	}
	catch(...)
	{
		delete wrapper;
		throw;
	}
	
	mWrappers.insert(IdentifierPair(local, remote), wrapper);
	return process(transport, locator);
}

SecureTransport *Core::TunnelBackend::listen(void)
{
	Synchronize(&mQueueSync);

	while(mQueue.empty()) mQueueSync.wait();
	
	const Message &message = mQueue.front();
	Assert(message.content == Message::Tunnel);
	
	LogDebug("Core::TunnelBackend::listen", "Incoming tunnel from " + message.source.toString());

	Identifier local(message.destination, mCore->getNumber());
	Identifier remote(message.source);

	TunnelWrapper *wrapper = NULL;
	SecureTransport *transport = NULL;
	try {
		wrapper = new TunnelWrapper(mCore, local, remote);
		transport = new SecureTransportServer(wrapper, NULL, true);	// ask for certificate
	}
	catch(...)
	{
		delete wrapper;
		mQueue.pop();
		throw;
	}
	
	mWrappers.insert(IdentifierPair(local, remote), wrapper);
	wrapper->incoming(message);
	
	mQueue.pop();
	return transport;
}

bool Core::TunnelBackend::incoming(const Message &message)
{
	if(message.content != Message::Tunnel)
		return false;
	
	//LogDebug("Core::TunnelBackend::incoming", "Received tunnel message");
	
	Identifier local(message.destination, mCore->getNumber());
	Identifier remote(message.source);
	
	Map<IdentifierPair, TunnelWrapper*>::iterator it = mWrappers.find(IdentifierPair(local, remote));	
	if(it != mWrappers.end())
	{
		return it->second->incoming(message);
	}
	else {
		Synchronize(&mQueueSync);
		mQueue.push(message);
		mQueueSync.notifyAll();
	}
	
	return true;
}

Core::TunnelBackend::TunnelWrapper::TunnelWrapper(Core *core, const Identifier &local, const Identifier &remote) :
	mCore(core),
	mLocal(local),
	mRemote(remote),
	mTimeout(DefaultTimeout)
{

}

Core::TunnelBackend::TunnelWrapper::~TunnelWrapper(void)
{
	// TODO: synchro
	mCore->mTunnelBackend->mWrappers.erase(IdentifierPair(mLocal, mRemote));
}

void Core::TunnelBackend::TunnelWrapper::setTimeout(double timeout)
{
	mTimeout = timeout;
}

size_t Core::TunnelBackend::TunnelWrapper::readData(char *buffer, size_t size)
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

void Core::TunnelBackend::TunnelWrapper::writeData(const char *data, size_t size)
{
	Message message;
	message.prepare(mLocal, mRemote, Message::Forward, Message::Tunnel);
	message.payload.writeBinary(data, size);
	mCore->route(message);
}

bool Core::TunnelBackend::TunnelWrapper::waitData(double &timeout)
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

bool Core::TunnelBackend::TunnelWrapper::waitData(const double &timeout)
{
	double dummy = timeout;
	return waitData(dummy);
}

bool Core::TunnelBackend::TunnelWrapper::isDatagram(void) const
{
	return true; 
}

bool Core::TunnelBackend::TunnelWrapper::incoming(const Message &message)
{
	Synchronize(&mQueueSync);
	mQueue.push(message);
	mQueueSync.notifyAll();
	return true;
}

Core::Handler::Handler(Core *core, Stream *stream, ThreadPool *pool, const Identifier &local, const Identifier &remote) :
	mCore(core),
	mStream(stream),
	mLocal(local),
	mRemote(remote),
	mIsIncoming(true),
	mStopping(false)
{
	mSender = new Sender(stream);
	pool->launch(this);
	pool->launch(mSender);
}

Core::Handler::~Handler(void)
{
	delete mSender;
	delete mStream;
}

Identifier Core::Handler::local(void) const
{
	Synchronize(this);
	return mLocal;
}

Identifier Core::Handler::remote(void) const
{
	Synchronize(this);
	return mRemote;
}

bool Core::Handler::recv(Message &message)
{
	Synchronize(this);
	
	{
		Desynchronize(this);
		
		uint16_t size = 0;
		
		if(mStream->isDatagram())
		{
			char buffer[1500];
			size_t size = mStream->readBinary(buffer, 1500);
			if(!size) return false;
			
			ByteArray s(buffer, size);
			
			AssertIO(s.readBinary(message.version));
			AssertIO(s.readBinary(message.flags));
			AssertIO(s.readBinary(message.type));
			AssertIO(s.readBinary(message.content));
			AssertIO(s.readBinary(message.hops));
			AssertIO(s.readBinary(size));
			
			BinarySerializer serializer(mStream);
			AssertIO(s.read(message.source));
			AssertIO(s.read(message.destination));
			
			message.payload.clear();
			if(s.readBinary(message.payload, size) != size)
				throw Exception("Incomplete message (size should be " + String::number(unsigned(size))+")");
		}
		else {
			if(!mStream->readBinary(message.version)) return false;
			AssertIO(mStream->readBinary(message.flags));
			AssertIO(mStream->readBinary(message.type));
			AssertIO(mStream->readBinary(message.content));
			AssertIO(mStream->readBinary(message.hops));
			AssertIO(mStream->readBinary(size));
			
			BinarySerializer serializer(mStream);
			AssertIO(serializer.read(message.source));
			AssertIO(serializer.read(message.destination));
			
			message.payload.clear();
			if(mStream->readBinary(message.payload, size) != size)
				throw Exception("Incomplete message (size should be " + String::number(unsigned(size))+")");
		}
		
		++message.hops;
	}
	
	return true;
}

bool Core::Handler::send(const Message &message)
{
	Synchronize(this);
	return mSender->push(message);
}

void Core::Handler::route(const Message &message)
{
	Synchronize(this);
	DesynchronizeStatement(this, mCore->route(message, mRemote));
}

bool Core::Handler::incoming(const Message &message)
{
	Synchronize(this);
	
	if(message.content != Message::Tunnel && message.content != Message::Data)
		LogDebug("Core::Handler", "Incoming message (content=" + String::number(unsigned(message.content)) + ", size=" + String::number(unsigned(message.payload.size())) + ")");
	
	const Identifier &source = message.source;
	BinaryString payload = message.payload;		// copy
	
	switch(message.content)
	{
		case Message::Tunnel:
		{
			Desynchronize(this);
			if(mCore->mTunnelBackend) mCore->mTunnelBackend->incoming(message);
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
			Synchronize(mCore);
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
}

bool Core::Handler::outgoing(const Identifier &dest, uint8_t type, uint8_t content, Stream &payload)
{
	//LogDebug("Core::Handler::outgoing", "Outgoing message (type=" + String::number(unsigned(type)) + ", content=" + String::number(unsigned(content)) + ")");
	
	Message message;
	message.prepare(mLocal, dest, type, content);
	message.payload.write(payload);
	return send(message);
}

void Core::Handler::process(void)
{
	Synchronize(this);

	String command, args;
	StringMap parameters;
	
	// New node is connected
	if(mRemote != Identifier::Null)
	{
		Desynchronize(this);
		Synchronize(mCore);
		
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
	}
	
	Message message;
	while(recv(message))
	{
		try {
			//LogDebug("Core::Handler", "Received message (type=" + String::number(unsigned(message.type)) + ")");
			
			if(message.source == Identifier::Null || message.source == mLocal)
				continue;
			
			if(mRemote != Identifier::Null && message.source != mRemote)
			{
				Desynchronize(this);
				mCore->addRoute(message.source, mRemote);
			}
			
			switch(message.type)
			{
				case Message::Forward:
					if(message.destination == mLocal) incoming(message);
					else route(message);
					break;
					
				case Message::Broadcast:
					incoming(message);
					route(message);
					break;
					
				case Message::Lookup:
					if(message.destination == mLocal) incoming(message);
					else if(!incoming(message))
						route(message);
					break;
					
				default:
					LogDebug("Core::Handler", "Unknwon message type " + String::number(unsigned(message.type)) + ", dropping");
					break;
			}
		}
		catch(const std::exception &e)
		{
			LogWarn("Core::Handler", String("Unable to process message: ") + e.what()); 
		}
	}
}

void Core::Handler::run(void)
{
	mCore->addHandler(mRemote, this);
	
	try {
		LogDebug("Core::Handler", "Starting link handler");
	
		process();
		
		LogDebug("Core::Handler", "Closing link handler");
	}
	catch(const std::exception &e)
	{
		LogDebug("Core::Handler", String("Closing link handler: ") + e.what());
	}
	
	if(mRemote.number())
		mCore->removeHandler(mRemote, this);
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}

Core::Handler::Sender::Sender(Stream *stream) :
	mStream(stream)
{
	
}

Core::Handler::Sender::~Sender(void)
{

}

bool Core::Handler::Sender::push(const Message &message)
{
	Synchronize(this);
	
	if(mQueue.size() + mSecondaryQueue.size() > 1024)
	{
		if(!mSecondaryQueue.empty()) mSecondaryQueue.pop();
		else mQueue.pop();
	}
	
	if(message.content == Message::Data || message.content == Message::Ack) mSecondaryQueue.push(message);
	else mQueue.push(message);
	
	notifyAll();
	return true;
}

void Core::Handler::Sender::run(void)
{
	while(true)
	{
		Message message;
		
		{
			Synchronize(this);
			
			while(mQueue.empty() && mSecondaryQueue.empty())
				wait();
			
			if(!mQueue.empty()) 
			{
				message = mQueue.front();
				mQueue.pop();
			}
			else {
				message = mSecondaryQueue.front();
				mSecondaryQueue.pop();
			}
		}
		
		uint16_t size = message.payload.size();
		
		ByteArray buffer(1500);
		buffer.writeBinary(message.version);
		buffer.writeBinary(message.flags);
		buffer.writeBinary(message.type);
		buffer.writeBinary(message.content);
		buffer.writeBinary(message.hops);
		buffer.writeBinary(size);
		
		BinarySerializer serializer(&buffer);
		serializer.write(message.source);
		serializer.write(message.destination);
		
		buffer.writeBinary(message.payload.data(), size);
		mStream->writeBinary(buffer.data(), buffer.size());
	}
}

}
