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

#include "pla/binaryserializer.h"
#include "pla/jsonserializer.h"
#include "pla/securetransport.h"
#include "pla/crypto.h"
#include "pla/random.h"
#include "pla/http.h"

namespace tpn
{

Overlay::Overlay(int port) :
		mThreadPool(4, 16, Config::Get("max_connections").toInt()),
		mLastPublicIncomingTime(0)
{
	// Generate RSA key
	Random rnd(Random::Key);
	Rsa rsa(4096);
	rsa.generate(mPublicKey, mPrivateKey);

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
		
		// Delete certificate
		delete mCertificate;
		
		throw;
	}
	
	LogInfo("Overlay", "Instance name is \"" + localName() + "\", node is " + localNode().toString());
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

void Overlay::getKnownPublicAdresses(Set<Address> &set) const
{
	Synchronize(this);
	mKnownPublicAddresses.getKeys(set);
}

bool Overlay::isPublicConnectable(void) const
{
	return (Time::Now()-mLastPublicIncomingTime <= 3600.); 
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
				Backend *backend = *it;
				if(backend->connect(addrs, remote))
					return true;
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
		
		if(async)
		{
			task = new ConnectTask(mBackends, addrs, remote);
			mThreadPool.launch(task);
			return true;
		}
		else {
			ConnectTask stask(mBackends, addrs, remote);
			return stask.connect();
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

bool Overlay::recv(Message &message)
{
	Synchronize(&mIncomingSync);

	while(mIncoming.empty())
		mIncomingSync.wait();

	message = mIncoming.front();
	mIncoming.pop();
	return true;
}

bool Overlay::send(const Message &message)
{
	return route(message);
}

void Overlay::registerEndpoint(const BinaryString &id)
{
	Synchronize(this);
	mEndpoints.insert(id);
}

void Overlay::unregisterEndpoint(const BinaryString &id)
{
	Synchronize(this);
	mEndpoints.remove(id);
}

void Overlay::storeValue(const BinaryString &key, const BinaryString &value)
{
	// TODO
}

bool Overlay::retrieveValue(const BinaryString &key, BinaryString &value)
{
	// TODO
}

void Overlay::run(void)
{
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

	if(message.destination != localNode())
		return route(message, from);

	// Message is for us
	switch(message.type)
	{
	case Message::Invalid:
		// Ignored
		break;
		
	case Message::Ping:
		{
			LogDebug("Overlay::incoming", "Ping to " + message.destination.toString());
			route(Message(Message::Pong, message.source, message.content));
			break;
		}
		
	case Message::Pong:
		{
			LogDebug("Overlay::incoming", "Pong from " + message.source.toString());
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
					route(message);
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
		
	default:
		{
			Synchronize(&mIncomingSync);
			mIncoming.push(message);
			mIncomingSync.notifyAll();
			return false;
		}
	}

	return true;
}

bool Overlay::route(const Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	// Drop if TTL is zero
	if(message.ttl == 0) return false;
	
	// Drop if not connected
	if(mHandlers.empty()) return false;

	// Special case: only one route
	if(mHandlers.size() == 1)
		return sendTo(message, mHandlers.begin()->first);
	
	// Special case: neighbour
	if(mHandlers.contains(message.destination))
		return sendTo(message, message.destination);
		
	// Get route
	BinaryString route;
	if(mRoutes.get(message.destination, route))
	{
		// If message encountered a dead end
		if(route == from)
		{
			Array<BinaryString> neighbors;
			mHandlers.getKeys(neighbors);
			
			BinaryString min;
			for(int i=0; i<neighbors.size(); ++i)
			{
					BinaryString distance = message.destination ^ neighbors[i];
					if(min.empty() || distance < min)
					{
						route = neighbors[i];
						min = distance;    
					}
			}
			
			if(min.empty())
			{
				mRoutes.erase(message.destination);
				return false;
			}
			
			mRoutes.insert(message.destination, route);
		}
		
		// Best guess for source
		if(!mRoutes.contains(message.source))
			mRoutes.insert(message.source, from);
	}
	else {
		mRoutes.insert(message.destination, route);
		mRoutes.insert(message.source, from);
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

bool Overlay::registerHandler(const BinaryString &node, Overlay::Handler *handler)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	Handler *h = NULL;
	if(mHandlers.get(node, h))
		return (h == handler);
	
	mHandlers.insert(node, handler);
	
	return true;
}

bool Overlay::unregisterHandler(const BinaryString &node, Overlay::Handler *handler)
{
	Synchronize(this);
	
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
	LogDebug("Overlay::track", "Contacting tracker " + tracker);
	
	try {
		String url("http://" + tracker + "/teapotnet/");
		
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

Overlay::Message::Message(void)
{
	clear();
}

Overlay::Message::Message(uint8_t type, const BinaryString &destination, const BinaryString &content)
{
	clear();
	
	this->type = type;
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
	ttl = 10;	// TODO
	type = Message::Invalid;

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

bool Overlay::Backend::handshake(SecureTransport *transport, const BinaryString &remote, bool async)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Rsa::PublicKey publicKey;
		
		bool verifyPublicKey(const Array<Rsa::PublicKey> &chain)
		{
			if(chain.empty()) return false;
			publicKey = chain[0];
			return true;		// Accept
		}
	};

	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(Overlay *overlay, SecureTransport *transport, const BinaryString &remote = "")
		{
			this->overlay = overlay; 
			this->transport = transport;
			this->remote = remote;
		}
		
		bool handshake(void)
		{
			LogDebug("Overlay::Backend::handshake", "HandshakeTask starting...");
			
			try {
				// Set verifier
				MyVerifier verifier;
				transport->setVerifier(&verifier);
				
				// Do handshake
				transport->handshake();
				Assert(transport->hasCertificate());
				
				BinaryString identifier = verifier.publicKey.digest();
				if(!remote.empty() && remote != identifier)
					throw Exception("Unexpected identifier");
				
				// Handshake succeeded
				LogDebug("Overlay::Backend::handshake", "Success, spawning new handler");
				Handler *handler = new Handler(overlay, transport, identifier);
				return true;
			}
			catch(const std::exception &e)
			{
				LogInfo("Overlay::Backend::handshake", String("Handshake failed: ") + e.what());
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
		Overlay *overlay;
		SecureTransport *transport;
		BinaryString remote;
	};
	
	HandshakeTask *task = NULL;
	try {
		LogDebug("Overlay::Backend::handshake", "Setting certificate");
	
		// Add certificate
		transport->addCredentials(mOverlay->certificate(), false);
		
		if(async)
		{
			task = new HandshakeTask(mOverlay, transport, remote);
			mThreadPool.launch(task);
			return true;
		}
		else {
			HandshakeTask stask(mOverlay, transport, remote);
			return stask.handshake();
		}
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::Backend::handshake", e.what());
		delete task;
		delete transport;
		return false;
	}
}

void Overlay::Backend::run(void)
{
	try {
		while(true)
		{
			SecureTransport *transport = listen();
			if(!transport) break;
			
			LogDebug("Overlay::Backend::run", "Incoming connection");
			
			// Add server credentials
			transport->addCredentials(mOverlay->certificate(), false);
			
			// No remote node specified, accept any node
			handshake(transport, "", true);	// async
		}
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::Backend::run", e.what());
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
		catch(const NetException &e)
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
	
	return handshake(transport, remote);
}

SecureTransport *Overlay::StreamBackend::listen(void)
{
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, true);	// ask for certificate
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
		catch(const NetException &e)
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
	}
	catch(...)
	{
		delete stream;
		throw;
	}
	
	return handshake(transport, remote);
}

SecureTransport *Overlay::DatagramBackend::listen(void)
{
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, true);	// ask for certificate
		if(transport) return transport;
	}
	
	return NULL;
}

void Overlay::DatagramBackend::getAddresses(Set<Address> &set) const
{
	mSock.getLocalAddresses(set);
}

Overlay::Handler::Handler(Overlay *overlay, Stream *stream, const BinaryString &node) :
	mOverlay(overlay),
	mStream(stream),
	mNode(node)
{
	mOverlay->registerHandler(mNode, this);
}

Overlay::Handler::~Handler(void)
{
	mOverlay->unregisterHandler(mNode, this);	// should be done already
	
	delete mStream;
}

bool Overlay::Handler::recv(Message &message)
{
	Synchronize(this);
	
	BinarySerializer s(mStream);

	while(true)
	{
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
			AssertIO(mStream->readBinary(message.source, sourceSize));
			AssertIO(mStream->readBinary(message.destination, destinationSize));
			AssertIO(mStream->readBinary(message.content, contentSize));

			mStream->nextRead();	// switch to next datagram if this is a datagram stream

			if(message.source.empty())	continue;
			if(message.ttl == 0)		continue;
			--message.ttl;
			return true;
		}
		catch(...)
		{
			if(!mStream->nextRead())
			{
				LogWarn("Overlay::Handler", "Unexpected end of stream while reading");
				return false;
			}
			
			LogWarn("Overlay::Handler", "Truncated message");
		}
	}
}

bool Overlay::Handler::send(const Message &message)
{
	Synchronize(this);
	
	BinarySerializer s(mStream);

	// 32-bit control block
	s.write(message.version);
	s.write(message.flags);
	s.write(message.ttl);
	s.write(message.type);

	// 32-bit size block
	s.write(uint8_t(message.source.size()));
	s.write(uint8_t(message.destination.size()));
	s.write(uint16_t(message.content.size()));
	
	// data
	mStream->writeBinary((message.source.empty() ? message.source : mOverlay->localNode()));
	mStream->writeBinary(message.destination);
	mStream->writeBinary(message.content);

	mStream->nextWrite();	// switch to next datagram if this is a datagram stream
	return true;
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
	try {
		LogDebug("Overlay::Handler", "Starting handler");
	
		process();
		
		LogDebug("Overlay::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogDebug("Overlay::Handler", String("Closing handler: ") + e.what());
	}
	
	mOverlay->unregisterHandler(mNode, this);
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}

}
