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

#include "tpn/overlay.h"
#include "tpn/httptunnel.h"
#include "tpn/portmapping.h"
#include "tpn/config.h"
#include "tpn/cache.h"
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

Overlay::Overlay(int port) :
		mThreadPool(4, 16, Config::Get("max_connections").toInt())
{
	mFileName = "keys";
	load();
	
	// Generate RSA key if necessary
	if(mPublicKey.isNull())
	{
		Random rnd(Random::Key);
		Rsa rsa(4096);
		rsa.generate(mPublicKey, mPrivateKey);
	}
	
	// Create certificate
	mCertificate = new SecureTransport::RsaCertificate(mPublicKey, mPrivateKey, localNode().toString());
	
	// Define node name
	mName = Config::Get("node_name");
	if(mName.empty())
	{
		char hostname[HOST_NAME_MAX];
		if(gethostname(hostname,HOST_NAME_MAX) == 0)
			mName = hostname;

		if(mName.empty() || mName == "localhost")
			mName = localNode().toString();
	}
	
	// Launch
	try {
		// Create backends
		mBackends.push_back(new DatagramBackend(this, port));
		mBackends.push_back(new StreamBackend(this, port));
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
		
		// Delete certificate
		delete mCertificate;
		
		throw;
	}
	
	LogDebug("Overlay", "Instance name is \"" + localName() + "\"");
	LogDebug("Overlay", "Local node is " + localNode().toString());
	
	save();
}

Overlay::~Overlay(void)
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
	
	delete mCertificate;
}

void Overlay::load()
{
	Synchronize(this);
	
	if(!File::Exist(mFileName)) return;
	File file(mFileName, File::Read);
	JsonSerializer serializer(&file);
	serializer.read(*this);
	file.close();
}

void Overlay::save() const
{
	Synchronize(this);
	
	SafeWriteFile file(mFileName);
	JsonSerializer serializer(&file);
	serializer.write(*this);
	file.close();
}

void Overlay::start(void)
{
	// Start backends
	for(List<Backend*>::iterator it = mBackends.begin();
		it != mBackends.end();
		++it)
	{
		Backend *backend = *it;
		backend->start();
	}
	
	Scheduler::Global->schedule(this);
	Scheduler::Global->repeat(this, 60.);	// 1 min
}

void Overlay::join(void)
{
	// Join backends
	for(List<Backend*>::iterator it = mBackends.begin();
		it != mBackends.end();
		++it)
	{
		Backend *backend = *it;
		backend->join();
	}

	Scheduler::Global->cancel(this);
}

String Overlay::localName(void) const
{
	Synchronize(this);
	Assert(!mName.empty());
	return mName;
}

BinaryString Overlay::localNode(void) const
{
	Synchronize(this);
	return mPublicKey.digest();
}

const Rsa::PublicKey &Overlay::publicKey(void) const
{
	Synchronize(this);
	return mPublicKey; 
}

const Rsa::PrivateKey &Overlay::privateKey(void) const
{
	Synchronize(this);
	return mPrivateKey; 
}

SecureTransport::Certificate *Overlay::certificate(void) const
{
	Synchronize(this);
	return mCertificate;
}

void Overlay::getAddresses(Set<Address> &set) const
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

bool Overlay::connect(const Set<Address> &addrs, const BinaryString &remote, bool async)
{
	class ConnectTask : public Task
	{
	public:
		ConnectTask(const List<Backend*> &backends, const Set<Address> &addrs, const BinaryString &remote)
		{
			this->backends = backends;
			this->addrs = addrs;
			this->remote = remote;
		}
		
		bool connect(void)
		{
			for(List<Backend*>::iterator it = backends.begin();
				it != backends.end();
				++it)
			{
				try {
					Backend *backend = *it;
					if(backend->connect(addrs, remote))
						return true;
				}
				catch(const std::exception &e)
				{
					LogWarn("Overlay::connect", e.what());
				}
			}
			
			return false;
		}
		
		void run(void)
		{
			connect();
			delete this;	// autodelete
		}
		
	private:
		List<Backend*> backends;
		Set<Address> addrs;
		BinaryString remote;
	};
	
	ConnectTask *task = NULL;
	try {
		Synchronize(this);
		
		Set<Address> filteredAddrs;
		for(Set<Address>::iterator it = addrs.begin();
			it != addrs.end();
			++it)
		{
			Address tmp(*it);
			tmp.setPort(0);	// so it matches any port
			if(!mRemoteAddresses.contains(tmp))
				filteredAddrs.insert(*it);
		}
		
		if(!filteredAddrs.empty())
		{
			if(async)
			{
				task = new ConnectTask(mBackends, filteredAddrs, remote);
				mThreadPool.launch(task);
				return true;
			}
			else {
				ConnectTask stask(mBackends, filteredAddrs, remote);
				DesynchronizeStatement(this, stask.connect());
			}
		}
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::connect", e.what());
		delete task;
		return false;
	}
	
	return false;
}

int Overlay::connectionsCount(void) const
{
	Synchronize(this);
	return mHandlers.size();
}

bool Overlay::recv(Message &message, double &timeout)
{
	Synchronize(&mIncomingSync);

	if(timeout >= 0.)
	{
		while(mIncoming.empty())
			if(!mIncomingSync.wait(timeout))
				return false;
	}
	else {
		while(mIncoming.empty())
			mIncomingSync.wait();
	}
	
	message = mIncoming.front();
	mIncoming.pop();
	return true;
}

bool Overlay::recv(Message &message, const double &timeout)
{
	double tmp = timeout;
	return recv(message, tmp);
}

bool Overlay::send(const Message &message)
{
	Synchronize(this);
	
	//LogDebug("Overlay::send", "Sending message (type=" + String::hexa(unsigned(message.type)) + ")");
	
	if(mHandlers.empty())
		return false;
	
	if(message.destination.empty())
		return broadcast(message);
	
	if(mHandlers.contains(message.destination))
		return sendTo(message, message.destination);
	
	// Don't use route since we want to force the message to be sent to the best route
	Map<BinaryString, BinaryString> sorted;
	Array<BinaryString> neighbors;
	mHandlers.getKeys(neighbors);
	for(int i=0; i<neighbors.size(); ++i)
		sorted.insert(message.destination ^ neighbors[i], neighbors[i]);
	
	return sendTo(message, sorted.begin()->second);
}

void Overlay::store(const BinaryString &key, const BinaryString &value)
{
	Store::Instance->storeValue(key, value, false);	// not permanent

	Message message(Message::Store, value, key);
	Array<BinaryString> routes;
	getRoutes(key, 0, routes);
	for(int i=0; i<routes.size(); ++i)
		sendTo(message, routes[i]);
}

bool Overlay::retrieve(const BinaryString &key, Set<BinaryString> &values)
{
	Synchronize(&mRetrieveSync);
	
	bool sent = false;
	BinaryString route = getRoute(key);
	if(route != localNode() && !mRetrievePending.contains(key))
	{
		mRetrievePending.insert(key);
		sent = sendTo(Message(Message::Retrieve, "", key), route);
	}
	
	if(sent)
	{
		double timeout = milliseconds(Config::Get("request_timeout").toInt());
		while(mRetrieveSync.wait(timeout))
			if(!mRetrievePending.contains(key))
				break;
	}
	
	mRetrievePending.erase(key);
	Store::Instance->retrieveValue(key, values);
	return !values.empty();
}

void Overlay::run(void)
{
	SerializableSet<Address> addrs;
	Config::GetExternalAddresses(addrs);	// TODO: external address discovery by other nodes
	if(!addrs.empty())
	{
		BinaryString content;
		BinarySerializer(&content).write(addrs);
		broadcast(Message(Message::Offer, content));
	}
	
	if(connectionsCount() < 8)	// TODO
	{
		Set<Address> addrs;
		if(track(Config::Get("tracker"), addrs))
			connect(addrs);
	}
}

bool Overlay::incoming(Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	// Route if necessary
	if((message.type & 0x80) && !message.destination.empty() && message.destination != localNode())
	{
		route(message, from);
		return false;
	}

	//LogDebug("Overlay::incoming", "Incoming message (type=" + String::hexa(unsigned(message.type)) + ")");
	
	// Message is for us
	switch(message.type)
	{
	case Message::Dummy:
		{
			// Nothing to do
			break;
		}
		
	// Path-folding offer
	case Message::Offer:
		{
			message.type = Message::Suggest;	// message modified in place
			
			BinaryString distance = message.source ^ localNode();			
			Array<BinaryString> neighbors;
			mHandlers.getKeys(neighbors);
			for(int i=0; i<neighbors.size(); ++i)
			{
				if(message.source != neighbors[i] 
					&& (message.source ^ neighbors[i]) <= distance)
				{
					message.destination = neighbors[i];
					send(message);
				}
			}
			
			break;
		}
	
	// Path-folding suggestion (relayed offer)
	case Message::Suggest:
		{
			SerializableSet<Address> addrs;
			BinarySerializer serializer(&message.content);
			serializer.read(addrs);
			connect(addrs, message.source);
			break;
		}

	// Retrieve value from DHT
	case Message::Retrieve:
		{
			//LogDebug("Overlay::Incoming", "Retrieve " + message.destination.toString());
			
			BinaryString route = getRoute(message.destination);
			if(route != localNode()) sendTo(message, route);
			else {
				Set<BinaryString> values;
				Store::Instance->retrieveValue(message.destination, values);
				for(Set<BinaryString>::iterator it = values.begin();
					it != values.end();
					++it)
				{
					send(Message(Message::Value, *it, message.source, message.destination));
				}
			}
			
			//push(message);	// useless
			break;
		}
		
	// Store value in DHT
	case Message::Store:
		{
			//LogDebug("Overlay::Incoming", "Store " + message.destination.toString());
			
			store(message.destination, message.content);	// routing is done in storeValue()
			
			Synchronize(&mRetrieveSync);
			if(mRetrievePending.contains(message.content))
			{
				mRetrievePending.erase(message.content);
				mRetrieveSync.notifyAll();
			}
			
			//push(message);	// useless
			break;
		}
		
	// Response to retrieve from DHT
	case Message::Value:
		{
			//LogDebug("Overlay::Incoming", "Value " + message.source.toString());
			
			// Value messages differ from Store messages because key is in the source field
			store(message.source, message.content);
			route(message);
			
			Synchronize(&mRetrieveSync);
			if(mRetrievePending.contains(message.content))
			{
				mRetrievePending.erase(message.content);
				mRetrieveSync.notifyAll();
			}
			
			push(message);
			break;
		}
		
	// Ping
	case Message::Ping:
		{
			LogDebug("Overlay::incoming", "Ping from " + message.source.toString());
			send(Message(Message::Pong, message.content, message.source));
			break;
		}
	
	// Pong
	case Message::Pong:
		{
			LogDebug("Overlay::incoming", "Pong from " + message.source.toString());
			break;
		}
		
	// Higher-level messages are pushed to queue
	case Message::Call:
	case Message::Data:
	case Message::Tunnel:
		{
			push(message);
			break;
		}
		
	default:
		{
			LogDebug("Overlay::incoming", "Unknown message type: " + String::number(message.type));
			return false;
		}
	}
	
	return true;
}

bool Overlay::push(Message &message)
{
	Synchronize(&mIncomingSync);
	mIncoming.push(message);
	mIncomingSync.notifyAll();
	return true;
}

bool Overlay::route(const Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	// Drop if TTL is zero
	if(message.ttl == 0) return false;
	
	// Drop if not connected
	if(mHandlers.empty()) return false;
	
	// Drop if self
	if(message.destination == localNode()) return false;
	
	// Best guess for source
	if(!from.empty() && !mRoutes.contains(message.source))
		mRoutes.insert(message.source, from);
	
	// Get route
	BinaryString route;
	mRoutes.get(message.destination, route);
	
	// If no route or dead end
	if(route.empty() || route == from)
	{
		Array<BinaryString> routes;
		getRoutes(message.destination, 0, routes);
		
		int index = 0;
		if(route.empty())
		{
			while(index < routes.size() && routes[index] == from)
				++index;
		}
		else {	// route == from
			while(index < routes.size() && routes[index] != from)
				++index;
			if(index < routes.size())
				++index;
		}
		
		if(index == routes.size())
		{
			mRoutes.erase(message.destination);
			return false;
		}
		
		route = routes[index];
		mRoutes.insert(message.destination, route);
	}
	
	return sendTo(message, route);
}

bool Overlay::broadcast(const Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	Array<BinaryString> neighbors;
	mHandlers.getKeys(neighbors);
	
	bool success = false;
	for(int i=0; i<neighbors.size(); ++i)
	{
		if(!from.empty() && neighbors[i] == from) continue;
		
		Handler *handler;
		if(mHandlers.get(neighbors[i], handler))
		{
			Desynchronize(this);
			success|= handler->send(message);
		}
	}
	
	return success;
}

bool Overlay::sendTo(const Message &message, const BinaryString &to)
{
	Synchronize(this);

	if(to.empty())
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

BinaryString Overlay::getRoute(const BinaryString &destination, const BinaryString &from)
{
	Synchronize(this);
	
	Array<BinaryString> routes;
	getRoutes(destination, 1, routes);
	if(!routes.empty()) return routes[0];
	else return localNode();
}

int Overlay::getRoutes(const BinaryString &destination, int count, Array<BinaryString> &result)
{
	Synchronize(this);
	
	Map<BinaryString, BinaryString> sorted;
	sorted.insert(destination ^ localNode(), localNode());
	
	Array<BinaryString> neighbors;
	mHandlers.getKeys(neighbors);
	for(int i=0; i<neighbors.size(); ++i)
		sorted.insert(destination ^ neighbors[i], neighbors[i]);
	
	sorted.getValues(result);
	if(count > 0 && result.size() > count) result.resize(count);
	for(int i=0; i<result.size(); ++i)
		if(result[i] == localNode())
			result.resize(i+1);
	
	return result.size();
}

bool Overlay::registerHandler(const BinaryString &node, const Address &addr, Overlay::Handler *handler)
{
	Synchronize(this);
	
	mRemoteAddresses.insert(addr);
	
	if(!handler) 
		return false;
	
	Handler *h = NULL;
	if(mHandlers.get(node, h))
	{
		if(h == handler) 
		{
			return true;
		}
		else {	// another handler already exists
			h->addAddress(addr);
			return false;
		}
	}
	
	mHandlers.insert(node, handler);
	mThreadPool.launch(handler);
	
	// On first connection, schedule store to publish in DHT
	if(mHandlers.size() == 1)
		Scheduler::Global->schedule(Store::Instance); 
	
	return true;
}

bool Overlay::unregisterHandler(const BinaryString &node, const Set<Address> &addrs, Overlay::Handler *handler)
{
	Synchronize(this);
	
	mRemoteAddresses.erase(addrs.begin(), addrs.end());
	
	if(!handler)
		return false;
	
	Handler *h = NULL;
	if(!mHandlers.get(node, h) || h != handler)
		return false;
	
	mHandlers.erase(node);
	return true;
}

bool Overlay::track(const String &tracker, Set<Address> &result)
{
	if(tracker.empty()) return false;
	
	String url;
	if(tracker.contains("://")) url = tracker;
	else url = "http://" + tracker;
	
	LogDebug("Overlay::track", "Contacting tracker " + url);	
	
	try {
		
		url+= String(url[url.size()-1] == '/' ? "" : "/") + "teapotnet/tracker?id=" + localNode().toString();
		
		// Dirty hack to test if tracker is private or public
		bool trackerIsPrivate = false;
		List<Address> trackerAddresses;
		Address::Resolve(tracker, trackerAddresses);
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
		
		// Mix our own addresses with known public addresses
		//getKnownPublicAdresses(tmp);
		//addresses.insertAll(tmp);
		
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
			SerializableSet<Address> addrs;
			JsonSerializer serializer(&json);
			if(!serializer.input(addrs)) return false;
			
			// TODO: Sanitize addrs
			result = addrs;
			return !result.empty();
		}
		
		LogWarn("Overlay::track", "Tracker HTTP error: " + String::number(code)); 
	}
	catch(const std::exception &e)
	{
		LogWarn("Overlay::track", e.what()); 
	}
	
	return false;
}


void Overlay::serialize(Serializer &s) const
{
	Synchronize(this);

	ConstObject object;
	object["publickey"] = &mPublicKey;
	object["privatekey"] = &mPrivateKey;
	s.write(object);
}

bool Overlay::deserialize(Serializer &s)
{
	Synchronize(this);
	
	mPublicKey.clear();
	mPrivateKey.clear();
	
	Object object;
	object["publickey"] = &mPublicKey;
	object["privatekey"] = &mPrivateKey;
	
	if(!s.read(object))
		return false;
	
	// TODO: Sanitize
	return true;
}

bool Overlay::isInlineSerializable(void) const
{
        return false;
}

Overlay::Message::Message(void)
{
	clear();
}

Overlay::Message::Message(uint8_t type, const BinaryString &content, const BinaryString &destination, const BinaryString &source)
{
	clear();
	
	this->type = type;
	this->source = source;
	this->destination = destination;
	this->content = content;
}

Overlay::Message::~Message(void)
{
	
}

void Overlay::Message::clear(void)
{
	version = 0;
	flags = 0x00;
	ttl = 128;	// TODO
	type = Message::Dummy;

	source.clear();
	destination.clear();
	content.clear();
}

void Overlay::Message::serialize(Serializer &s) const
{
	// TODO
	s.write(source);
	s.write(destination);
	s.write(content);
}

bool Overlay::Message::deserialize(Serializer &s)
{
	// TODO
	if(!s.read(source)) return false;
	AssertIO(s.read(destination));
	AssertIO(s.read(content));
}

Overlay::Backend::Backend(Overlay *overlay) :
	mOverlay(overlay)
{
	Assert(mOverlay);
}

Overlay::Backend::~Backend(void)
{
	
}

bool Overlay::Backend::handshake(SecureTransport *transport, const Address &addr, const BinaryString &remote)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Rsa::PublicKey publicKey;
		
		bool verifyPublicKey(const Array<Rsa::PublicKey> &chain)
		{
			if(chain.empty()) return false;
			publicKey = chain[0];
			
			LogDebug("Overlay::Backend::handshake", String("Remote node is ") + publicKey.digest().toString());
			return true;		// Accept
		}
	};

	// Add certificate
	transport->addCredentials(mOverlay->certificate(), false);

	// Set verifier
	MyVerifier verifier;
	transport->setVerifier(&verifier);
	
	// Do handshake
	transport->handshake();
	Assert(transport->hasCertificate());
	
	BinaryString identifier = verifier.publicKey.digest();
	if(remote.empty() || remote == identifier)
	{
		// Handshake succeeded
		LogDebug("Overlay::Backend::handshake", "Handshake succeeded");
		Handler *handler = new Handler(mOverlay, transport, identifier, addr);
		return true;
	}
	else {
		LogDebug("Overlay::Backend::handshake", "Handshake failed");
		return false;
	}
}

void Overlay::Backend::run(void)
{
	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(Backend *backend, SecureTransport *transport, const Address &addr)
		{
			this->backend = backend; 
			this->transport = transport;
			this->addr = addr;
		}
		
		void run(void)
		{
			try {
				backend->handshake(transport, addr, "");
			}
			catch(const std::exception &e)
			{
				LogDebug("Overlay::Backend::HandshakeTask", e.what());
			}
	
			delete this;	// autodelete
		}
		
	private:
		Backend *backend;
		SecureTransport *transport;
		Address addr;
	};

	while(true)
	{
		SecureTransport *transport = NULL;
		HandshakeTask *task = NULL;
		
		try {
			Address addr;
			transport = listen(&addr);
			if(!transport) break;
			
			LogDebug("Overlay::Backend::run", "Incoming connection from " + addr.toString());
			
			task = new HandshakeTask(this, transport, addr);
			mThreadPool.launch(task);
		}
		catch(const std::exception &e)
		{
			LogError("Overlay::Backend::run", e.what());
			delete transport;
			delete task;
			break;
		}
	}
	
	LogWarn("Overlay::Backend::run", "Closing backend");
}

Overlay::StreamBackend::StreamBackend(Overlay *overlay, int port) :
	Backend(overlay),
	mSock(port)
{

}

Overlay::StreamBackend::~StreamBackend(void)
{
	
}

bool Overlay::StreamBackend::connect(const Set<Address> &addrs, const BinaryString &remote)
{
	Set<Address> localAddrs;
	getAddresses(localAddrs);
	
	for(Set<Address>::const_reverse_iterator it = addrs.rbegin();
		it != addrs.rend();
		++it)
	{
		if(localAddrs.contains(*it))
			continue;
		
		try {
			if(connect(*it, remote))
				return true;
		}
		catch(const std::exception &e)
		{
			LogDebug("Overlay::StreamBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Overlay::StreamBackend::connect(const Address &addr, const BinaryString &remote)
{
	Socket *sock = NULL;
	SecureTransport *transport = NULL;
	
	LogDebug("Overlay::StreamBackend::connect", "Trying address " + addr.toString() + " (TCP)");
	
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
	
	return handshake(transport, addr, remote);
}

SecureTransport *Overlay::StreamBackend::listen(Address *addr)
{
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, addr, true);	// ask for certificate
		if(transport) return transport;
	}
	
	return NULL;
}

void Overlay::StreamBackend::getAddresses(Set<Address> &set) const
{
	mSock.getLocalAddresses(set);
}

Overlay::DatagramBackend::DatagramBackend(Overlay *overlay, int port) :
	Backend(overlay),
	mSock(port)
{
	
}

Overlay::DatagramBackend::~DatagramBackend(void)
{
	
}

bool Overlay::DatagramBackend::connect(const Set<Address> &addrs, const BinaryString &remote)
{
	SerializableSet<Address> localAddrs;
	getAddresses(localAddrs);
	
	for(Set<Address>::const_reverse_iterator it = addrs.rbegin();
		it != addrs.rend();
		++it)
	{
		if(localAddrs.contains(*it))
			continue;
		
		try {
			if(connect(*it, remote))
				return true;
		}
		catch(const std::exception &e)
		{
			LogDebug("Overlay::DatagramBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Overlay::DatagramBackend::connect(const Address &addr, const BinaryString &remote)
{
	DatagramStream *stream = NULL;
	SecureTransport *transport = NULL;
	
	LogDebug("Overlay::DatagramBackend::connect", "Trying address " + addr.toString() + " (UDP)");
	
	try {
		stream = new DatagramStream(&mSock, addr);
		transport = new SecureTransportClient(stream, NULL);
		transport->setMtu(1452);	// UDP over IPv6 on ethernet
	}
	catch(...)
	{
		delete stream;
		throw;
	}
	
	return handshake(transport, addr, remote);
}

SecureTransport *Overlay::DatagramBackend::listen(Address *addr)
{
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, addr, true);	// ask for certificate
		transport->setMtu(1452);	// UDP over IPv6 on ethernet
		if(transport) return transport;
	}
	
	return NULL;
}

void Overlay::DatagramBackend::getAddresses(Set<Address> &set) const
{
	mSock.getLocalAddresses(set);
}

Overlay::Handler::Handler(Overlay *overlay, Stream *stream, const BinaryString &node, const Address &addr) :
	mOverlay(overlay),
	mStream(stream),
	mNode(node)
{
	if(node == mOverlay->localNode())
		throw Exception("Spawned a handler for local node");
	
	if(!mOverlay->registerHandler(mNode, addr, this))
		throw Exception("A handler already exists for the same flow");
	
	mAddrs.insert(addr);
}

Overlay::Handler::~Handler(void)
{
	mOverlay->unregisterHandler(mNode, mAddrs, this);	// should be done already
	
	delete mStream;
}

bool Overlay::Handler::recv(Message &message)
{
	Synchronize(this);
	
	BinarySerializer s(mStream);

	while(true)
	{
		Desynchronize(this);
		
		try {
			// 32-bit control block
			if(!s.read(message.version))
			{
				if(!mStream->nextRead()) return false;
				continue;
			}
			
			AssertIO(s.read(message.flags));
			AssertIO(s.read(message.ttl));
			AssertIO(s.read(message.type));
			
			// 32-bit size block
			uint8_t sourceSize, destinationSize;
			uint16_t contentSize;
			AssertIO(s.read(sourceSize));
			AssertIO(s.read(destinationSize));
			AssertIO(s.read(contentSize));
			
			// data
			message.source.clear();
			message.destination.clear();
			message.content.clear();
			AssertIO(mStream->readBinary(message.source, sourceSize) == sourceSize);
			AssertIO(mStream->readBinary(message.destination, destinationSize) == destinationSize);
			AssertIO(mStream->readBinary(message.content, contentSize) == contentSize);

			mStream->nextRead();	// switch to next datagram if this is a datagram stream
			
			if(message.source.empty())	continue;
			if(message.ttl == 0)		continue;
			--message.ttl;
			return true;
		}
		catch(const IOException &e)
		{
			if(!mStream->nextRead())
			{
				LogWarn("Overlay::Handler", "Connexion unexpectedly closed");
				return false;
			}
			
			LogWarn("Overlay::Handler", "Truncated message");
		}
	}
	
	return false;
}

bool Overlay::Handler::send(const Message &message)
{
	Synchronize(this);
	
	BinarySerializer s(mStream);

	BinaryString source = (!message.source.empty() ? message.source : mOverlay->localNode());
	
	try {
		Desynchronize(this);
		
		// 32-bit control block
		s.write(message.version);
		s.write(message.flags);
		s.write(message.ttl);
		s.write(message.type);
		
		// 32-bit size block
		s.write(uint8_t(source.size()));
		s.write(uint8_t(message.destination.size()));
		s.write(uint16_t(message.content.size()));
		
		// data
		mStream->writeBinary(source);
		mStream->writeBinary(message.destination);
		mStream->writeBinary(message.content);

		mStream->nextWrite();	// switch to next datagram if this is a datagram stream
	}
	catch(std::exception &e)
	{
		LogWarn("Overlay::Handler::send", String("Sending failed: ") + e.what());
		mStream->close();
		return false;
	}
	
	return true;
}

void Overlay::Handler::addAddress(const Address &addr)
{
	Synchronize(this);
	mAddrs.insert(addr);
}

void Overlay::Handler::process(void)
{
	Synchronize(this);
	
	Message message;
	while(recv(message))
	{
		Desynchronize(this);
		//LogDebug("Overlay::Handler", "Received datagram");
		mOverlay->incoming(message, mNode);
	}
}

void Overlay::Handler::run(void)
{
	LogDebug("Overlay::Handler", "Starting handler");
	
	try {
		process();
		
		LogDebug("Overlay::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogWarn("Overlay::Handler", String("Closing handler: ") + e.what());
	}
	
	mOverlay->unregisterHandler(mNode, mAddrs, this);
	
	notifyAll();
	Thread::Sleep(10.);	// TODO
	delete this;		// autodelete
}

}
