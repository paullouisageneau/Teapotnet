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
		// TODO
		//mBackends.push_back(mTunnelBackend);
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
	
	BinaryString target;
        if(publisher->anounce(Identifier::Null, prefix, "/", target))
	{
		SerializableList<BinaryString> array;
		array.push_back(target);
		
		// Local
		matchSubscribers(prefix, Identifier::Null, array);
		
		// Broadcast
		BinaryString payload;
		BinarySerializer serializer(&payload);
		serializer.write(prefix);
		serializer.write(array);
		outgoing(Message::Broadcast, Message::Publish, payload);
	}
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
	matchPublishers(prefix, Identifier::Null);
	
	// Immediatly send subscribe message
	BinaryString payload;
	BinarySerializer serializer(&payload);
	serializer.write(prefix);
	outgoing(Message::Lookup, Message::Subscribe, payload);
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

void Core::broadcast(const Notification &notification)
{
	Synchronize(this);
	
	String payload;
	JsonSerializer serializer(&payload);
	serializer.write(notification);
	
	Array<Identifier> identifiers;
	mHandlers.getKeys(identifiers);
	
	for(int i=0; i<identifiers.size(); ++i)
	{
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			String tmp(payload);
			handler->outgoing(handler->remote(), Message::Broadcast, Message::Notify, tmp);
		}
	}
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
		
		handler->outgoing(peer, Message::Forward, Message::Notify, payload);
		return true;
	}
	
	return false;
}

void Core::route(Message &message, const Identifier &from)
{
	Synchronize(this);
	
	// Drop if too many hops
	if(message.hops >= 8)
		return;
	
	if(message.destination != Identifier::Null)
	{
		// 1st case: neighbour
		if(send(message, message.destination))
			return;

		// 2nd case: routing table entry exists
		Identifier route;
		if(mRoutes.get(message.destination, route))
			if(send(message, route))
				return;
	}
	
	// 3rd case: no routing table entry
	broadcast(message, from);
}

void Core::broadcast(Message &message, const Identifier &from)
{
	Synchronize(this);
	
	message.payload.reset();
	
	Array<Identifier> identifiers;
	mHandlers.getKeys(identifiers);
	
	for(int i=0; i<identifiers.size(); ++i)
	{
		if(identifiers[i] == from) continue;
		
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			handler->send(message);
		}
	}
}

bool Core::send(Message &message, const Identifier &to)
{
	Synchronize(this);
	
	message.payload.reset();
	
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
	Handler *handler = new Handler(this, stream, local, remote);
	mThreadPool.launch(handler);
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
	Assert(handler != NULL);
	if(peer == Identifier::Null) return false;
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
	Assert(handler != NULL);
	Synchronize(this);
  
	Handler *h = NULL;
	if(!mHandlers.get(peer, h) || h != handler)
		return false;
	
	mHandlers.erase(peer);
	return true;
}

void Core::outgoing(uint8_t type, uint8_t content, Stream &payload)
{
	outgoing(Identifier::Null, type, content, payload);
}

void Core::outgoing(const Identifier &dest, uint8_t type, uint8_t content, Stream &payload)
{
	//LogDebug("Core::Handler::outgoing", "Outgoing message (type=" + String::number(unsigned(type)) + ", content=" + String::number(unsigned(content)) + ")");
	
	Message message;
	message.prepare(Identifier::Null, dest, type, content);
	message.payload.write(payload);
	route(message);
}

bool Core::matchPublishers(const String &path, const Identifier &source)
{
	Synchronize(this);
	
	List<String> list;
	path.explode(list,'/');
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
				BinaryString target;
				if((*jt)->anounce(source, prefix, truncatedPath, target))
				{
					Assert(!target.empty());
					LogDebug("Core::Handler::incoming", "Anouncing " + target.toString() + " for " + path);
					targets.push_back(target);
				}
			}
			
			if(!targets.empty()) 
			{
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

bool Core::matchSubscribers(const String &path, const Identifier &source, const List<BinaryString> &targets)
{
	Synchronize(this);
	
	List<String> list;
	path.explode(list,'/');
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
		
		// Pass to local subscribers
		Map<String, Set<Subscriber*> >::iterator it = mSubscribers.find(prefix);
		if(it != mSubscribers.end())
		{
			Set<Subscriber*> set = it->second;
			Desynchronize(this);
			
			for(Set<Subscriber*>::iterator jt = set.begin();
				jt != set.end();
				++jt)
			{
				for(List<BinaryString>::const_iterator kt = targets.begin();
					kt != targets.end();
					++kt)
				{
					// TODO: should prevent forwarding in case we want to republish another content
					(*jt)->incoming(source, prefix, truncatedPath, *kt);
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
	hops(0),
	payload(1300)
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

Core::Publisher::Publisher(void)
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

void Core::Publisher::publish(const String &prefix)
{
	Core::Instance->publish(prefix, this);
	mPublishedPrefixes.insert(prefix);
	
	// Call anounce and trigger broadcast if necessary
	BinaryString target;
	if(anounce(Identifier::Null, prefix, "/", target))
		publish(prefix, "/", target);
}

void Core::Publisher::publish(const String &prefix, const String &path, const BinaryString &target)
{
	Core::Instance->publish(prefix, this);
	mPublishedPrefixes.insert(prefix);
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
	for(StringSet::iterator it = mSubscribedPrefixes.begin();
		it != mSubscribedPrefixes.end();
		++it)
	{
		Core::Instance->unsubscribe(*it, this);
	}
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
	mCore(core)
{
	Assert(mCore);
}

Core::Backend::~Backend(void)
{
	
}

void Core::Backend::process(SecureTransport *transport, const Locator &locator)
{
	if(!locator.name.empty())
		transport->setHostname(locator.name);
  
	if(!locator.peering.empty())
	{
		LogDebug("Core::Backend::process", "Setting PSK credentials: " + locator.peering.toString());
	  
		// Add contact private shared key
		SecureTransportClient::Credentials *creds = new SecureTransportClient::PrivateSharedKey(locator.peering.toString(), locator.secret);
		transport->addCredentials(creds, true);	// must delete
		
		doHandshake(transport, locator.peering, locator.peering);
	}
	else if(locator.user)
	{
		LogDebug("Core::Backend::process", "Setting certificate credentials: " + locator.user->name());
		
		if(locator.name.empty())
			LogWarn("Core::Backend::process", "Remote name is not set in locator for certificate authentication");
		
		// Add user certificate
		SecureTransportClient::Certificate *cert = locator.user->certificate();
		if(cert) transport->addCredentials(cert, false);
		
		doHandshake(transport, locator.user->identifier(), locator.identifier);
	}
	else {
		LogDebug("Core::Backend::process", "Setting anonymous credentials");

		// Add anonymous credentials
		transport->addCredentials(&mAnonymousClientCreds);
		
		doHandshake(transport, Identifier::Null, Identifier::Null);
	}
}

void Core::Backend::doHandshake(SecureTransport *transport, const Identifier &local, const Identifier &remote)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Identifier local, remote;
		Rsa::PublicKey publicKey;
		
		MyVerifier(Core *core) { this->core = core; }
		
		bool verifyName(const String &name, SecureTransport *transport)
		{
			LogDebug("Core::Backend::doHandshake", String("Verifying name: ") + name);
			
			User *user = User::Get(name);
			if(user)
			{
				SecureTransport::Credentials *creds = user->certificate();
				if(creds) transport->addCredentials(creds);
				local = user->identifier();
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
				local = remote;
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
			remote = publicKey.digest();
			
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
	  
		void run(void)
		{
			LogDebug("Core::Backend::doHandshake", "HandshakeTask starting...");
			
			try {
				// Set verifier
				MyVerifier verifier(core);
				transport->setVerifier(&verifier);
				
				// Do handshake
				transport->handshake();
				
				if(!transport->hasCertificate())
				{
					// Assign identifiers if client
					if(transport->isClient())
					{
						verifier.local = local;
						verifier.remote = remote;
					}
				}
				else {
					// Assign local if client
					if(transport->isClient())
						verifier.local = local;
						
					// Check remote identifier
					if(remote != Identifier::Null && verifier.remote != remote)
						throw Exception("Invalid identifier: " + verifier.remote.toString());
				}
				
				// Handshake succeeded, add peer
				LogDebug("Core::Backend::doHandshake", "Handshake succeeded");
				
				// Sanity checks
				if(transport->hasCertificate()) 
				{
					Assert(verifier.local != Identifier::Null);
					Assert(verifier.remote != Identifier::Null);
				}
				else if(transport->hasPrivateSharedKey())
				{
					Assert(verifier.local == verifier.remote);
				}
				else {
					Assert(verifier.local == Identifier::Null);
					Assert(verifier.remote == Identifier::Null);
				}
				
				core->addPeer(transport, verifier.local, verifier.remote);
			}
			catch(const std::exception &e)
			{
				LogInfo("Core::Backend::doHandshake", String("Handshake failed: ") + e.what());
				delete transport;
			}

			delete this;	// autodelete
		}
		
	private:
		Core *core;
		SecureTransport *transport;
		Identifier local, remote;
	};
	
	HandshakeTask *task = NULL;
	try {
		task = new HandshakeTask(mCore, transport, local, remote);
		mThreadPool.launch(task);
	}
	catch(const std::exception &e)
	{
		LogError("Core::Backend::doHandshake", e.what());
		delete task;
		delete transport;
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
				try {
					// Add server credentials (certificate added on name verification)
					transport->addCredentials(&mAnonymousServerCreds, false);
					transport->addCredentials(&mPrivateSharedKeyServerCreds, false);
				
					// No remote identifier specified, accept any identifier
					doHandshake(transport, Identifier::Null, Identifier::Null);	// async
				}
				catch(...)	// should not happen
				{
					delete transport;
					throw;
				}
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

SecureTransport *Core::StreamBackend::connect(const Locator &locator)
{
	for(Set<Address>::const_reverse_iterator it = locator.addresses.rbegin();
		it != locator.addresses.rend();
		++it)
	{
		try {
			return connect(*it, locator);
		}
		catch(const NetException &e)
		{
			LogDebug("Core::StreamBackend::connect", e.what());
		}
	}
	
	return NULL;
}

SecureTransport *Core::StreamBackend::connect(const Address &addr, const Locator &locator)
{
	Socket *sock = NULL;
	SecureTransport *transport = NULL;
	
	LogDebug("Core::StreamBackend::connect", "Trying address " + addr.toString() + " (TCP)");
	
	try {
		const double timeout = 5.;		// TODO
		
		sock = new Socket;
		sock->setConnectTimeout(timeout);
		sock->connect(addr);
		
		transport = new SecureTransportClient(sock, NULL, "", false);	// stream mode
	}
	catch(...)
	{
		delete sock;
		throw;
	}
	
	try {
		process(transport, locator);
	}
	catch(...)
	{
		delete transport;
		throw;
	}
	
	return transport;
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

SecureTransport *Core::DatagramBackend::connect(const Locator &locator)
{
	for(Set<Address>::const_reverse_iterator it = locator.addresses.rbegin();
		it != locator.addresses.rend();
		++it)
	{
		try {
			return connect(*it, locator);
		}
		catch(const NetException &e)
		{
			LogDebug("Core::DatagramBackend::connect", e.what());
		}
	}
	
	return NULL;
}

SecureTransport *Core::DatagramBackend::connect(const Address &addr, const Locator &locator)
{
	DatagramStream *stream = NULL;
	SecureTransport *transport = NULL;
	
	LogDebug("Core::DatagramBackend::connect", "Trying address " + addr.toString() + " (UDP)");
	
	try {
		stream = new DatagramStream(&mSock, addr);
		transport = new SecureTransportClient(stream, NULL, "", true);	// datagram mode
	}
	catch(...)
	{
		delete stream;
		throw;
	}
	
	try {
		process(transport, locator);
	}
	catch(...)
	{
		delete transport;
		throw;
	}
	
	return transport;
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

SecureTransport *Core::TunnelBackend::connect(const Locator &locator)
{
	Assert(locator.user);
	
	if(locator.identifier.empty())
		return NULL;
	
	LogDebug("Core::TunnelBackend::connect", "Trying tunnel for " + locator.identifier.toString());
	
	Identifier remote = locator.identifier;
	Identifier local = locator.user->identifier();

	TunnelWrapper *wrapper = NULL;
	SecureTransport *transport = NULL;
	try {
		wrapper = new TunnelWrapper(mCore, local, remote);
		transport = new SecureTransportServer(wrapper, NULL, true, true);	// ask for certificate, datagram mode
	}
	catch(...)
	{
		delete wrapper;
		throw;
	}
	
	try {
		process(transport, locator);
	}
	catch(...)
	{
		delete transport;
		throw;
	}
	
	mWrappers.insert(IdentifierPair(local, remote), wrapper);
	return transport;
}

SecureTransport *Core::TunnelBackend::listen(void)
{
	Synchronize(&mQueueSync);
	while(mQueue.empty()) mQueueSync.wait();
	
	Message &message = mQueue.front();
	Assert(message.type == Message::Tunnel);
	
	TunnelWrapper *wrapper = NULL;
	SecureTransport *transport = NULL;
	try {
		wrapper = new TunnelWrapper(mCore, message.destination, message.source);
		transport = new SecureTransportServer(wrapper, NULL, true, true);	// ask for certificate, datagram mode
	}
	catch(...)
	{
		delete wrapper;
		mQueue.pop();
		throw;
	}
	
	mQueue.pop();
	mWrappers.insert(IdentifierPair(message.destination, message.source), wrapper);
	return transport;
}

bool Core::TunnelBackend::incoming(Message &message)
{
	if(message.type != Message::Tunnel)
		return false;
	
	Map<IdentifierPair, TunnelWrapper*>::iterator it = mWrappers.find(IdentifierPair(message.destination, message.source));	
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
	mRemote(remote)
{

}

Core::TunnelBackend::TunnelWrapper::~TunnelWrapper(void)
{

}                        

size_t Core::TunnelBackend::TunnelWrapper::readData(char *buffer, size_t size)
{
	// TODO: timeout

	Synchronize(&mQueueSync);
        while(mQueue.empty()) mQueueSync.wait();

        Message &message = mQueue.front();
	size = std::min(size, size_t(message.payload.size()));
        std::copy(message.payload.data(), message.payload.data() + size, buffer);
        mQueue.pop();
        return size;
}

void Core::TunnelBackend::TunnelWrapper::writeData(const char *data, size_t size)
{
	Message message;
	message.prepare(mLocal, mRemote);
	message.payload.writeBinary(data, size);
	mCore->route(message);
}

bool Core::TunnelBackend::TunnelWrapper::incoming(Message &message)
{
	Synchronize(&mQueueSync);
	mQueue.push(message);
	mQueueSync.notifyAll();
	return true;
}

Core::Handler::Handler(Core *core, Stream *stream, const Identifier &local, const Identifier &remote) :
	mCore(core),
	mStream(stream),
	mLocal(local),
	mRemote(remote),
	mIsIncoming(true),
	mStopping(false)
{

}

Core::Handler::~Handler(void)
{
	mRunner.clear();
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

void Core::Handler::notify(const Identifier &id, Stream &payload, bool ack)
{
	Synchronize(this);
  
	if(!mSenders.contains(id)) mSenders[id] = new Sender(this, id);
	mSenders[id]->notify(payload, ack); 
}

bool Core::Handler::recv(Message &message)
{
	Synchronize(this);
	
	{
		Desynchronize(this);
		MutexLocker lock(&mStreamReadMutex);
		
		uint16_t size = 0;
		
		if(!mStream->readBinary(message.version)) return false;
		AssertIO(mStream->readBinary(message.flags));
		AssertIO(mStream->readBinary(message.type));
		AssertIO(mStream->readBinary(message.content));
		AssertIO(mStream->readBinary(message.hops));
		AssertIO(mStream->readBinary(size));
		
		BinarySerializer serializer(mStream);
		AssertIO(static_cast<Serializer*>(&serializer)->input(message.source));
		AssertIO(static_cast<Serializer*>(&serializer)->input(message.destination));
		
		message.payload.clear();
		if(size > message.payload.length())
			throw Exception("Message payload too big");
		
		if(mStream->readBinary(message.payload, size) != size)
			throw Exception("Incomplete message (size should be " + String::number(unsigned(size))+")");
		
		++message.hops;
	}
	
	return true;
}

void Core::Handler::send(Message &message)
{
	Synchronize(this);
	
	uint16_t size = message.payload.size();
	
	ByteArray buffer(1400);
	buffer.writeBinary(message.version);
	buffer.writeBinary(message.flags);
	buffer.writeBinary(message.type);
	buffer.writeBinary(message.content);
	buffer.writeBinary(message.hops);
	buffer.writeBinary(size);
	
	BinarySerializer serializer(&buffer);
	if(message.source != Identifier::Null) serializer.write(message.source);
	else serializer.write(mLocal);
	serializer.write(message.destination);
	
	buffer.writeBinary(message.payload.data(), size);
	
	{
		Desynchronize(this);
		MutexLocker lock(&mStreamWriteMutex);
		mStream->writeBinary(buffer.data(), buffer.size());
	}
}

void Core::Handler::route(Message &message)
{
	Synchronize(this);
	DesynchronizeStatement(this, mCore->route(message, mRemote));
}

bool Core::Handler::incoming(Message &message)
{
	Synchronize(this);
	
	const Identifier &source = message.source;
	Stream &payload = message.payload;
	
	if(message.content != Message::Data)
		LogDebug("Core::Handler", "Incoming message (content=" + String::number(unsigned(message.content)) + ", size=" + String::number(unsigned(message.payload.size())) + ")");
	
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
			// TODO
			//if(!mSenders.contains(source)) mSenders[source] = new Sender(this, source);
			//mSenders[source]->ack(payload);
			
			Notification notification;
			JsonSerializer json(&payload);
			json.read(notification);
			
			// TODO: correct sync
			Desynchronize(this);
			Synchronize(mCore);
			Map<Identifier, Set<Listener*> >::iterator it = mCore->mListeners.find(source);
			while(it != mCore->mListeners.end() && it->first == source)
			{
				for(Set<Listener*>::iterator jt = it->second.begin();
					jt != it->second.end();
					++jt)
				{
					(*jt)->recv(source, notification);
				}
				
				++it;
			}
			
			break;
		}
		
		case Message::Ack:
		{
			Sender *sender;
			if(mSenders.get(source, sender))
				sender->acked(payload);
			break;
		}
		
		case Message::Call:
		{
			BinarySerializer serializer(&payload);
			
			BinaryString target;
			uint16_t tokens;
			AssertIO(serializer.read(target));
			AssertIO(serializer.read(tokens));
			
			if(!mSenders.contains(source)) mSenders[source] = new Sender(this, source);
			mSenders[source]->addTarget(target, tokens);
			
			// TODO: return false is content is not stored locally (for correct handling of lookups)
			break;
		}
		
		case Message::Cancel:
		{
			BinarySerializer serializer(&payload);
			
			BinaryString target;
			AssertIO(serializer.read(target));
			
			Map<BinaryString, Sender*>::iterator it = mSenders.find(source);
			if(it != mSenders.end())
			{
				it->second->removeTarget(target);
				
				// TODO
				/*if(it->second->empty())
				{
					delete it->second;
					mSenders.erase(it);
				}*/
			}
			break;
		}
		
		case Message::Data:
		{
			Desynchronize(this);
			
			BinarySerializer serializer(&payload);
			
			BinaryString target;
			AssertIO(serializer.read(target));
			
			if(Store::Instance->push(target, payload))
			{
				mCore->unregisterAllCallers(target);
				
				BinaryString response;
				BinarySerializer serializer(&response);
				serializer.write(target);
				outgoing(source, Message::Forward, Message::Cancel, response);
			}
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
			
			return mCore->matchSubscribers(path, source, targets);
		}
		
		case Message::Subscribe:
		{
			Desynchronize(this);
			
			BinarySerializer serializer(&payload);
			
			String path;
			AssertIO(serializer.read(path));
			
			return mCore->matchPublishers(path, source);
		}
		
		default:
			return false;
	}
	
	return true;
}

void Core::Handler::outgoing(const Identifier &dest, uint8_t type, uint8_t content, Stream &payload)
{
	//LogDebug("Core::Handler::outgoing", "Outgoing message (type=" + String::number(unsigned(type)) + ", content=" + String::number(unsigned(content)) + ")");
	
	Message message;
	message.prepare(mLocal, dest, type, content);
	message.payload.write(payload);
	send(message);
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
	
	try {
		mCore->removeHandler(mRemote, this);
	}
	catch(...)
	{
	  
	}
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}

Core::Handler::Sender::Sender(Handler *handler, const BinaryString &destination) :
	mHandler(handler),
	mDestination(destination),
	mCurrentSequence(0)
{
	
}

Core::Handler::Sender::~Sender(void)
{
	mHandler->mRunner.cancel(this);
}

void Core::Handler::Sender::addTarget(const BinaryString &target, unsigned tokens)
{
	Synchronize(this);
	
	LogDebug("Core::Handler::Sender", "Adding target " + target.toString() + " (" + String::number(tokens) + " tokens)");
	mTargets.insert(target, tokens);
	mHandler->mRunner.schedule(this);
}

void Core::Handler::Sender::removeTarget(const BinaryString &target)
{
	Synchronize(this);
	mTargets.erase(target);
}

void Core::Handler::Sender::addTokens(unsigned tokens)
{
	Synchronize(this);
	mTokens+= tokens;
	mHandler->mRunner.schedule(this);
}

void Core::Handler::Sender::removeTokens(unsigned tokens)
{
	Synchronize(this);
	if(mTokens > tokens) mTokens-= tokens;
	else mTokens = 0;
}

bool Core::Handler::Sender::empty(void) const
{
	Synchronize(this);
	return mTargets.empty() && mUnacked.empty();
}

void Core::Handler::Sender::notify(Stream &payload, bool ack)
{
	uint32_t sequence = 0;
	if(ack)
	{
		++mCurrentSequence;
		if(!mCurrentSequence) ++mCurrentSequence;
		sequence = mCurrentSequence;
	}
	
	// TODO: this is deprecated
	Message message;
	message.prepare(mHandler->mLocal, mDestination, Message::Forward, Message::Notify);
	message.payload.writeBinary(uint32_t(sequence));
	message.payload.write(payload);
	
	const double delay = 0.5;	// TODO
	const int count = 5;		// TODO
	mUnacked.insert(sequence, SendTask(this, sequence, message, delay, count + 1));
}

void Core::Handler::Sender::ack(Stream &payload)
{
	// TODO: this is deprecated
	uint32_t sequence;
	AssertIO(payload.readBinary(sequence));
	
	BinaryString ack;
	ack.writeBinary(sequence);
	
	mHandler->outgoing(mDestination, Message::Forward, Message::Ack, payload);
}

void Core::Handler::Sender::acked(Stream &payload)
{
	uint32_t sequence;
	AssertIO(payload.readBinary(sequence));
	mUnacked.erase(sequence);
}

void Core::Handler::Sender::run(void)
{
	Synchronize(this);
	
	// TODO: tokens
	
	if(/*!mTokens ||*/ mTargets.empty()) 
		return;
	
	//LogDebug("Core::Handler::Sender", "Running sender (" + String::number(unsigned(mTargets.size())) + " targets)");
	
	Map<BinaryString, unsigned>::iterator it;
	if(!mNextTarget.empty()) 
	{
		it = mTargets.find(mNextTarget);
		if(it == mTargets.end()) it = mTargets.begin();
	}
	else {
		 it = mTargets.begin();
	}
	
	mNextTarget.clear();
	
	if(it->second)
	{
		//LogDebug("Core::Handler::Sender", "Sending target " + it->first.toString() + " (" + String::number(it->second) + " tokens left)");
		
		BinaryString payload;
		BinarySerializer serializer(&payload);
		serializer.write(it->first);	// target
		
		unsigned chunks = 0;
		if(Store::Instance->pull(it->first, payload, &it->second))
		{
			//--mTokens;
			if(it->second) ++it;
			else mTargets.erase(it++);
			if(it != mTargets.end()) mNextTarget = it->first;
			
			BinaryString dest(mDestination);
			DesynchronizeStatement(this, mHandler->outgoing(dest, Message::Forward, Message::Data, payload));
		}
		else {
			LogWarn("Core::Handler::Sender", "Unknown target: " + it->first.toString());
			
			mTargets.erase(it++);
			if(it != mTargets.end()) mNextTarget = it->first;
		}
		
		// Warning: iterator is not valid anymore here
	}
	else {
		mTargets.erase(it++);
		if(it != mTargets.end()) mNextTarget = it->first;
	}
	
	mHandler->mRunner.schedule(this);
}

Core::Handler::Sender::SendTask::SendTask(Sender *sender, uint32_t sequence, const Message &message, double delay, int count) :
	mSender(sender),
	mMessage(message),
	mLeft(count),
	mSequence(sequence)
{
	if(mLeft > 0)
	{
		Synchronize(mSender);
		mSender->mScheduler.schedule(this);
		mSender->mScheduler.repeat(this, delay);
	}
}

Core::Handler::Sender::SendTask::~SendTask(void)
{
	Synchronize(mSender);
	mSender->mScheduler.cancel(this);
}

void Core::Handler::Sender::SendTask::run(void)
{  
	mSender->mHandler->send(mMessage);
	
	--mLeft;
	if(mLeft <= 0)
	{
		Synchronize(mSender);
		mSender->mScheduler.cancel(this);
		mSender->mUnacked.erase(mSequence);
	}
}

}
