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

Overlay *Overlay::Instance = NULL;


Overlay::Overlay(int port) :
		mThreadPool(4, 16, Config::Get("max_connections").toInt()),
		mLastPublicIncomingTime(0)
{
	// Generate RSA key
	Random rnd(Random::Key);
	Rsa rsa(4096);
	rsa.generate(mPublicKey, mPrivateKey);

	// Define node name
	mName = Config::Get("node_name");
	if(mName.empty())
	{
		char hostname[HOST_NAME_MAX];
		if(gethostname(hostname,HOST_NAME_MAX) == 0)
			mName = hostname;

		if(mName.empty() || mName == "localhost")
			mName = node.toString();
	}
	
	LogInfo("Overlay", "Instance name is \"" + name() + "\", node is " + node().toString());
	
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
		
		throw;
	}
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
}

String Overlay::name(void) const
{
	Synchronize(this);
	Assert(!mName.empty());
	return mName;
}

BinaryString Overlay::node(void) const
{
	Synchronize(this);
	return mPublicKey.digest();
}

const Rsa::PublicKey &User::publicKey(void) const
{
	Synchronize(this);
	return mPublicKey; 
}

const Rsa::PrivateKey &User::privateKey(void) const
{
	Synchronize(this);
	return mPrivateKey; 
}

SecureTransport::Certificate *User::certificate(void) const
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

bool Overlay::connect(const Set<Address> &addrs)
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

int Overlay::connectionsCount(void) const
{
	Synchronize(this);
	return mHandlers.size();
}

bool Overlay::route(const Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	// Drop if TTL is zero
	if(message.ttl == 0)
		return false;
	
	if(!message.destination.empty())
	{
		// 1st case: neighbour
		if(mHandlers.contains(message.destination))
			return send(message, message.destination);
		
		// 2nd case: routing table entry exists
		BinaryString route;
		if(mRoutes.get(message.destination, route))
			return send(message, route);
	}
	
	// 3rd case: no routing table entry
	return broadcast(message, from);
}

bool Overlay::broadcast(const Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	Array<BinaryString> neighbors;
	mHandlers.getKeys(neighbors);
	
	bool success = false;
	for(int i=0; i<neighbors.size(); ++i)
	{
		if(neighbors[i] == from) continue;
		
		Handler *handler;
		if(mHandlers.get(neighbors[i], handler))
		{
			Desynchronize(this);
			success|= handler->send(message);
		}
	}
	
	return success;
}

bool Overlay::send(const Message &message, const BinaryString &to)
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

void Overlay::addRoute(const BinaryString &node, const BinaryString &route)
{
	Synchronize(this);
	
	if(node.empty() || route.empty())
		return;
	
	if(node == route)
		return;
	
	mRoutes.insert(node, route);
}

bool Overlay::getRoute(const BinaryString &node, BinaryString &route)
{
	Synchronize(this);
	
	Map<BinaryString, BinaryString>::iterator it = mRoutes.find(node);
	if(it == mRoutes.end()) return false;
	route = it->second;
	return true;
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
		
		LogWarn("Overlay::track", "Tracker HTTP error: " + String::number(code)); 
	}
	catch(const std::exception &e)
	{
		LogWarn("Overlay::track", e.what()); 
	}
	
	return false;
}

Overlay::Message::Message(void) :
	version(0),
	flags(0),
	type(Forward),
	content(Empty),
	hops(0)
{
	
}

Overlay::Message::~Message(void)
{
	
}

void Overlay::Message::prepare(const BinaryString &source, const BinaryString &destination, uint8_t type, uint8_t content)
{
	this->source = source;
	this->destination = destination;
	this->type = type;
	this->content = content;
	payload.clear();
}

void Overlay::Message::clear(void)
{
	source.clear();
	destination.clear();
	payload.clear();
}

void Overlay::Message::serialize(Serializer &s) const
{
	// TODO
	s.write(source);
	s.write(destination);
	s.write(payload);
}

bool Overlay::Message::deserialize(Serializer &s)
{
	// TODO
	if(!s.read(source)) return false;
	AssertIO(s.read(destination));
	AssertIO(s.read(payload));
}

Overlay::Backend::Backend(Overlay *overlay) :
	mOverlay(overlay),
	mAnonymousClientCreds(),
	mPrivateSharedKeyServerCreds(String::hexa(overlay->getNumber()))
{
	Assert(mOverlay);
}

Overlay::Backend::~Backend(void)
{
	
}

bool Overlay::Backend::process(SecureTransport *transport)
{
	LogDebug("Overlay::Backend::process", "Setting certificate");
	
	// Add certificate
	transport->addCredentials(mOverlay->certificate());
	
	// Handshake
	return handshake(transport, false);
}

bool Overlay::Backend::handshake(SecureTransport *transport, bool async)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Rsa::PublicKey publicKey;
		
		bool verifyCertificate(const Rsa::PublicKey &pub)
		{
			publicKey = pub;
			return true;		// Accept
		}
	};

	class HandshakeTask : public Task
	{
	public:
		HandshakeTask(SecureTransport *transport) { this->transport = transport; }
		
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
				
				// Handshake succeeded
				LogDebug("Overlay::Backend::handshake", "Success, spawning new handler");
				Handler *handler = new Handler(this, stream, verifier.publicKey.digest());
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
		SecureTransport *transport;
	};
	
	HandshakeTask *task = NULL;
	try {
		if(async)
		{
			task = new HandshakeTask(mOverlay, transport, local, remote);
			mThreadPool.launch(task);
			return true;
		}
		else {
			HandshakeTask stask(mOverlay, transport, local, remote);
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
			handshake(transport, true);	// async
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

bool Overlay::StreamBackend::connect(const Set<Address> &addrs)
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
			LogDebug("Overlay::StreamBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Overlay::StreamBackend::connect(const Address &addr)
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
	
	return process(transport);
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

bool Overlay::DatagramBackend::connect(const Set<Address> &addrs)
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
			LogDebug("Overlay::DatagramBackend::connect", e.what());
		}
	}
	
	return false;
}

bool Overlay::DatagramBackend::connect(const Address &addr)
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
	
	return process(transport);
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
	mInstance(node)
{
	Overlay::Instance->registerHandler(mInstance, this);
}

Overlay::Handler::~Handler(void)
{
	Overlay::Instance->unregisterHandler(mInstance, this);	// should be done already
	
	delete mSender;
	delete mStream;
}

bool Overlay::Handler::recv(Message &datagram)
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

bool Overlay::Handler::send(const Message &datagram)
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

/*bool Overlay::Handler::incoming(const Message &message)
{
	Synchronize(this);
	
	if(message.content != Message::Tunnel && message.content != Message::Data)
		LogDebug("Overlay::Handler", "Incoming message (content=" + String::number(unsigned(message.content)) + ", size=" + String::number(unsigned(message.payload.size())) + ")");
	
	const BinaryString &source = message.source;
	BinaryString payload = message.payload;		// copy
	
	switch(message.content)
	{
		case Message::Tunnel:
		{
			Overlay::node->dispatchTunneler::Tunnel(message);
			break; 
		}
		
		case Message::Notify:
		{
			Notification notification;
			JsonSerializer json(&payload);
			json.read(notification);
			
			// TODO: correct sync
			// TODO: getListeners function in Overlay
			Desynchronize(this);
			Synchronize(Overlay::Instance);
			Map<BinaryString, Set<Listener*> >::iterator it = mOverlay->mListeners.find(source);
			while(it != mOverlay->mListeners.end() && it->first == source)
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
				
				if(!mOverlay->outgoing(source, Message::Forward, Message::Data, payload))
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
				mOverlay->unregisterAllCallers(target);
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
			return mOverlay->matchSubscribers(path, (source == mRemote ? source : BinaryString::Null), &publisher);
		}
		
		case Message::Subscribe:
		{
			Desynchronize(this);
			
			BinarySerializer serializer(&payload);
			
			String path;
			AssertIO(serializer.read(path));
		
			mOverlay->addRemoteSubscriber(source, path, (source != mRemote));
			return true;
		}
		
		default:
			return false;
	}
	
	return true;
}*/

void Overlay::Handler::process(void)
{
	Synchronize(this);
	
	Message datagram;
	while(recv(datagram))
	{
		Desynchronize(this);
		//LogDebug("Overlay::Handler", "Received datagram");
		Overlay::Instance->incoming(datagram);
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
	
	mOverlay->unregisterHandler(mInstance, this);
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}

}
