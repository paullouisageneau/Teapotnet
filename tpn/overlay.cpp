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

#include "tpn/overlay.hpp"
#include "tpn/httptunnel.hpp"
#include "tpn/portmapping.hpp"
#include "tpn/config.hpp"
#include "tpn/cache.hpp"
#include "tpn/store.hpp"
#include "tpn/httptunnel.hpp"

#include "pla/binaryserializer.hpp"
#include "pla/jsonserializer.hpp"
#include "pla/object.hpp"
#include "pla/socket.hpp"
#include "pla/serversocket.hpp"
#include "pla/datagramsocket.hpp"
#include "pla/securetransport.hpp"
#include "pla/crypto.hpp"
#include "pla/random.hpp"
#include "pla/http.hpp"
#include "pla/proxy.hpp"

namespace tpn
{

const int Overlay::MaxQueueSize = 128;

Overlay::Overlay(int port) :
		mThreadPool(1, Config::Get("min_connections").toInt()*2+1, Config::Get("max_connections").toInt()*2)
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
		
		if(isConnected(remote)) return true;
		
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
				launch(task);
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

bool Overlay::isConnected(const BinaryString &remote) const
{
	Synchronize(this);
	return mHandlers.contains(remote);
}

int Overlay::connectionsCount(void) const
{
	Synchronize(this);
	return mHandlers.size();
}

bool Overlay::recv(Message &message, double &timeout)
{
	Synchronize(&mIncomingSync);

	if(timeout > 0.)
	{
		while(mIncoming.empty())
			if(!mIncomingSync.wait(timeout))
				return false;
	}
	
	if(mIncoming.empty())
		return false;
	
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
	return route(message);	// Alias	
}

void Overlay::store(const BinaryString &key, const BinaryString &value)
{
	Desynchronize(this);
	Store::Instance->storeValue(key, value, Store::Distributed);

	Message message(Message::Store, value, key);
	Array<BinaryString> nodes;
	if(getRoutes(key, 4, nodes))	// TODO
	{
		for(int i=0; i<nodes.size(); ++i)
		{
			if(nodes[i] != localNode())
				sendTo(message, nodes[i]);
		}
	}
}

void Overlay::retrieve(const BinaryString &key)
{
	Desynchronize(this);
	send(Message(Message::Retrieve, "", key));
	
	// Push Value messages in local queue
	BinaryString node(localNode());
	Set<BinaryString> values;
	if(Store::Instance->retrieveValue(key, values))
		for(Set<BinaryString>::iterator it = values.begin(); it != values.end(); ++it)
		{
			Message message(Message::Value, *it, node, key);
			push(message);
		}
}

bool Overlay::retrieve(const BinaryString &key, Set<BinaryString> &values)
{
	Desynchronize(this);
	Synchronize(&mRetrieveSync);
	
	bool sent = false;
	if(!mRetrievePending.contains(key))
	{
		mRetrievePending.insert(key);
		sent = send(Message(Message::Retrieve, "", key));
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
	try {
		const int minConnectionsCount = Config::Get("min_connections").toInt();

		SerializableMap<BinaryString, SerializableSet<Address> > result;
		if(track(Config::Get("tracker"), result))
			if(connectionsCount() < minConnectionsCount)
				for(SerializableMap<BinaryString, SerializableSet<Address> >::iterator it = result.begin();
					it != result.end();
					++it)
				{
					if(it->first == localNode())
					{
						// Store external addresses for Offer messages
						mLocalAddresses = it->second;
					}
					else {
						connect(it->second, it->first, false);	// sync
					}
				}
		
		SerializableSet<Address> addrs;
		Config::GetExternalAddresses(addrs);
		addrs.insertAll(mLocalAddresses);
		
		// TODO: external address discovery by other nodes
		
		if(!addrs.empty())
		{
			BinaryString content;
			BinarySerializer(&content).write(addrs);
			broadcast(Message(Message::Offer, content));
		}
		
		if(connectionsCount() < minConnectionsCount) Scheduler::Global->schedule(this, Random().uniform(0.,120.));	// avg 1 min
		else Scheduler::Global->schedule(this, 600.);   // 10 min
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::run", e.what());
		Scheduler::Global->schedule(this, 60.);   // 1 min	
	}
}

void Overlay::launch(Task *task)
{
	mThreadPool.launch(task);
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

	//LogDebug("Overlay::incoming", "Incoming message (type=" + String::hexa(unsigned(message.type)) + ") from " + message.source.toString());
	
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
			if(!isConnected(message.source))
			{
				LogDebug("Overlay::Incoming", "Suggest " + message.source.toString());
				
				SerializableSet<Address> addrs;
				BinarySerializer serializer(&message.content);
				serializer.read(addrs);
				connect(addrs, message.source);
			}
			break;
		}

	// Retrieve value from DHT
	case Message::Retrieve:
		{
			Desynchronize(this);
			//LogDebug("Overlay::Incoming", "Retrieve " + message.destination.toString());
			
			route(message);
			
			Set<BinaryString> values;
			Store::Instance->retrieveValue(message.destination, values);
			for(Set<BinaryString>::iterator it = values.begin();
				it != values.end();
				++it)
			{
				send(Message(Message::Value, *it, message.source, message.destination));
			}
			
			//push(message);	// useless
			break;
		}
		
	// Store value in DHT
	case Message::Store:
		{
			Desynchronize(this);
			//LogDebug("Overlay::Incoming", "Store " + message.destination.toString());
			Time oldTime = Store::Instance->getValueTime(message.destination, message.content);			

			if(Time::Now() - oldTime >= 60.) // 1 min
        		{
				Array<BinaryString> nodes;
				if(getRoutes(message.destination, 4, nodes))	// TODO
                		{
                        		for(int i=0; i<nodes.size(); ++i)
                        		{
						if(nodes[i] == localNode()) Store::Instance->storeValue(message.destination, message.content, Store::Distributed);
						else if(nodes[i] != from) sendTo(message, nodes[i]);
					}
				}
			}

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
			Desynchronize(this);
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
	
	// Neighbor
	if(mHandlers.contains(message.destination))
		return sendTo(message, message.destination);
	
	Array<BinaryString> neigh;
	getNeighbors(message.destination, neigh);
	if(neigh.size() >= 2) neigh.remove(from);
	
	BinaryString route;
	for(int i=0; i<neigh.size(); ++i)
	{
		route = neigh[i];
		if(Random().uniformInt()%2 == 0) break;
	}
	
	return sendTo(message, route);
}

bool Overlay::broadcast(const Message &message, const BinaryString &from)
{
	Synchronize(this);
	
	//LogDebug("Overlay::sendTo", "Broadcasting message");
	
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
		//LogDebug("Overlay::sendTo", "Sending message via " + to.toString());
		handler->send(message);
		return true;
	}
	
	return false;
}

int Overlay::getRoutes(const BinaryString &destination, int count, Array<BinaryString> &result)
{
	Synchronize(this);
	result.clear();
	
	Map<BinaryString, BinaryString> sorted;
        Array<BinaryString> neighbors;
        mHandlers.getKeys(neighbors);
        for(int i=0; i<neighbors.size(); ++i)
                sorted.insert(destination ^ neighbors[i], neighbors[i]);

	// local node
        sorted.insert(destination ^ localNode(), localNode());

        sorted.getValues(result);	
	if(count > 0 && result.size() > count) result.resize(count);
	return result.size();
}

int Overlay::getNeighbors(const BinaryString &destination, Array<BinaryString> &result)
{
	Synchronize(this);
	result.clear();
	
	Map<BinaryString, BinaryString> sorted;
	Array<BinaryString> neighbors;
	mHandlers.getKeys(neighbors);
	for(int i=0; i<neighbors.size(); ++i)
		sorted.insert(destination ^ neighbors[i], neighbors[i]);
	
	sorted.getValues(result);
	return result.size();
}

void Overlay::registerHandler(const BinaryString &node, const Address &addr, Overlay::Handler *handler)
{
	Synchronize(this);
	Assert(handler);
	
	mRemoteAddresses.insert(addr);
	
	Set<Address> otherAddrs;
	Handler *h = NULL;
	if(mHandlers.get(node, h))
	{
		mHandlers.erase(node);
		LogDebug("Overlay::registerHandler", "Replacing handler for " + node.toString());
		
		Desynchronize(this);
		h->getAddresses(otherAddrs);
		h->stop();
	}
	
	handler->addAddresses(otherAddrs);
	mHandlers.insert(node, handler);
	launch(handler);
	launch(handler->senderTask());
	
	// On first connection, schedule store to publish in DHT
	if(mHandlers.size() == 1)
		Scheduler::Global->schedule(Store::Instance); 
}

void Overlay::unregisterHandler(const BinaryString &node, const Set<Address> &addrs, Overlay::Handler *handler)
{
	Synchronize(this);
	Assert(handler);
	
	Handler *h = NULL;
	if(mHandlers.get(node, h) && h == handler)
	{
		for(Set<Address>::iterator it = addrs.begin(); it != addrs.end(); ++it)
			mRemoteAddresses.erase(*it);
		
		mHandlers.erase(node);
	}

	// If it was the last handler, try to reconnect now
	if(mHandlers.empty())
		Scheduler::Global->schedule(this);
}

bool Overlay::track(const String &tracker, SerializableMap<BinaryString, SerializableSet<Address> > &result)
{
	result.clear();
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
			JsonSerializer serializer(&json);
			if(!serializer.input(result)) return false;
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
	ttl = 16;	// TODO
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
	
	// Set timeout
	transport->setHandshakeTimeout(milliseconds(Config::Get("connect_timeout").toInt()));
	
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
			mOverlay->launch(task);
		}
		catch(const std::exception &e)
		{
			LogError("Overlay::Backend::run", e.what());
			delete transport;
			delete task;
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
		catch(const Timeout &e)
		{
			//LogDebug("Overlay::StreamBackend::connect", e.what());
		}
		catch(const NetException &e)
		{
			//LogDebug("Overlay::StreamBackend::connect", e.what());
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
	const double timeout = milliseconds(Config::Get("idle_timeout").toInt());
	const double connectTimeout = milliseconds(Config::Get("connect_timeout").toInt());
	
	if(Config::Get("force_http_tunnel").toBool())
		return connectHttp(addr, remote);
	
	LogDebug("Overlay::StreamBackend::connect", "Trying address " + addr.toString() + " (TCP)");
	
	Socket *sock = NULL;
	try {
		sock = new Socket;
		sock->setTimeout(timeout);
		sock->setConnectTimeout(connectTimeout);
		sock->connect(addr);
	}
	catch(...)
	{
		delete sock;
		
		// Try HTTP tunnel if a proxy is available
		String url = "http://" + addr.toString() + "/";
		if(Proxy::HasProxyForUrl(url))
			return connectHttp(addr, remote);
		
		// else throw
		throw;
	}
	
	SecureTransport *transport = NULL;
	try {
		transport = new SecureTransportClient(sock, NULL, "");
	}
	catch(...)
	{
		delete sock;
		throw;
	}
	
	try {
		return handshake(transport, addr, remote);
	}
	catch(...)
	{
		delete transport;
		return connectHttp(addr, remote);	// Try HTTP tunnel
	}
}

bool Overlay::StreamBackend::connectHttp(const Address &addr, const BinaryString &remote)
{
	const double connectTimeout = milliseconds(Config::Get("connect_timeout").toInt());
	
	LogDebug("Overlay::StreamBackend::connectHttp", "Trying address " + addr.toString() + " (HTTP)");
	
	Stream *stream = NULL;
	SecureTransport *transport = NULL;
	try {
		stream = new HttpTunnel::Client(addr, connectTimeout);
		transport = new SecureTransportClient(stream, NULL, "");
	}
	catch(...)
	{
		delete stream;
		throw;
	}
	
	try {
		return handshake(transport, addr, remote);
	}
	catch(...)
	{
		delete transport;
		throw;
	}
}

SecureTransport *Overlay::StreamBackend::listen(Address *addr)
{
	const double timeout = 	milliseconds(Config::Get("idle_timeout").toInt());
	const double dataTimeout = milliseconds(Config::Get("connect_timeout").toInt());
	
	while(true)
	{
		Socket *sock = NULL;
		try {
			sock = new Socket;
			mSock.accept(*sock);
		}
		catch(const std::exception &e)
		{
			delete sock;
			throw;
		}
		
		Stream *stream = sock;
		try {
			const size_t peekSize = 5;
			char peekBuffer[peekSize];
			
			sock->setTimeout(dataTimeout);
			if(sock->peekData(peekBuffer, peekSize) != peekSize)
				throw NetException("Connection prematurely closed");
			
			sock->setTimeout(timeout);
			if(addr) *addr = sock->getRemoteAddress();
			
			if(std::memcmp(peekBuffer, "GET ", 4) == 0
				|| std::memcmp(peekBuffer, "POST ", 5) == 0)
			{
				// This is HTTP, forward connection to HttpTunnel
				stream = HttpTunnel::Incoming(sock);
				if(!stream) continue;	// eaten
			}
			
			return new SecureTransportServer(stream, NULL, true);	// ask for certificate
		}
		catch(const Timeout &e)
		{
			//LogDebug("Overlay::StreamBackend::listen", e.what());
		}
		catch(const std::exception &e)
		{
			LogWarn("Overlay::StreamBackend::listen", e.what());
		}
		
		delete stream;
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
	if(Config::Get("force_http_tunnel").toBool())
		return false;
	
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
		catch(const Timeout &e)
		{
			//LogDebug("Overlay::DatagramBackend::connect", e.what());
		}
		catch(const NetException &e)
		{
			//LogDebug("Overlay::DatagramBackend::connect", e.what());
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
	const unsigned int mtu = 1452; // UDP over IPv6 on ethernet
	
	LogDebug("Overlay::DatagramBackend::connect", "Trying address " + addr.toString() + " (UDP)");
	
	DatagramStream *stream = NULL;
	SecureTransport *transport = NULL;
	try {
		stream = new DatagramStream(&mSock, addr);
		transport = new SecureTransportClient(stream, NULL);
	}
	catch(...)
	{
		delete stream;
		throw;
	}
	
	try {
		transport->setDatagramMtu(mtu);
		return handshake(transport, addr, remote);
	}
	catch(...)
	{
		delete transport;
		throw;
	}
}

SecureTransport *Overlay::DatagramBackend::listen(Address *addr)
{
	const double timeout = milliseconds(Config::Get("idle_timeout").toInt());
	const unsigned int mtu = 1452; // UDP over IPv6 on ethernet
	
	while(true)
	{
		SecureTransport *transport = SecureTransportServer::Listen(mSock, addr, true, timeout);	// ask for certificate
		if(!transport) break;
		
		transport->setDatagramMtu(mtu);
		return transport;
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
	mNode(node),
	mSender(overlay, stream),
	mShouldStop(false)
{
	if(node == mOverlay->localNode())
		throw Exception("Spawned a handler for local node");
	
	addAddress(addr);
	mOverlay->registerHandler(mNode, addr, this);

	const double timeout = milliseconds(Config::Get("keepalive_timeout").toInt());
}

Overlay::Handler::~Handler(void)
{
	mOverlay->unregisterHandler(mNode, mAddrs, this);	// should be done already

	delete mStream;
}

bool Overlay::Handler::recv(Message &message)
{
	Synchronize(this);
	
	while(true)
	{
		try {
			Desynchronize(this);
			BinarySerializer s(mStream);
			
			// 32-bit control block
			if(!s.read(message.version))
			{
				if(!mStream->nextRead()) break;
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
			
			if(message.destination == node())
			{
				LogWarn("Overlay::Handler::recv", "Message destination is source node ?!");
				continue;
			}
			
			return true;
		}
		catch(const IOException &e)
		{
			if(!mStream->nextRead())
			{
				LogWarn("Overlay::Handler::recv", "Connexion unexpectedly closed");
				break;
			}
			
			LogWarn("Overlay::Handler::recv", "Truncated message");
		}
	}
	
	return false;
}

bool Overlay::Handler::send(const Message &message)
{
	Synchronize(this);
	return mSender.push(message);
}

void Overlay::Handler::stop(void)
{
	Synchronize(this);
	mShouldStop = true;
	mSender.stop();
	mStream->close();
}

void Overlay::Handler::addAddress(const Address &addr)
{
	Synchronize(this);
	mAddrs.insert(addr);
}

void Overlay::Handler::addAddresses(const Set<Address> &addrs)
{
	Synchronize(this);
	mAddrs.insertAll(addrs);
}

void Overlay::Handler::getAddresses(Set<Address> &set) const
{
	Synchronize(this);
	set = mAddrs;
}

BinaryString Overlay::Handler::node(void) const
{
	Synchronize(this);
	return mNode;
}

Task *Overlay::Handler::senderTask(void)
{
	return &mSender;
}

void Overlay::Handler::process(void)
{
	Synchronize(this);
	
	Message message;
	while(recv(message) && !mShouldStop)
	{
		Desynchronize(this);
		//LogDebug("Overlay::Handler", "Received message");
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
	
	mSender.stop();
	
	notifyAll();
	Thread::Sleep(10.);	// TODO
	delete this;		// autodelete
}

Overlay::Handler::Sender::Sender(Overlay *overlay, Stream *stream) :
	mOverlay(overlay),
	mStream(stream),
	mShouldStop(false)
{
	
}

Overlay::Handler::Sender::~Sender(void)
{
	
}

bool Overlay::Handler::Sender::push(const Message &message)
{
	Synchronize(this);
	
	if(mQueue.size() < Overlay::MaxQueueSize)
	{
		mQueue.push(message);
		notifyAll();
		return true;
	}
	
	return false;
}

void Overlay::Handler::Sender::stop(void)
{
	Synchronize(this);
	
	mShouldStop = true;
	notifyAll();
}

void Overlay::Handler::Sender::run(void)
{
	Synchronize(this);
	
	try {
		while(!mShouldStop)
		{
			const double timeout = milliseconds(Config::Get("keepalive_timeout").toInt());
			
			while(mQueue.empty())
			{
				if(!wait(timeout))
					break;
				
				if(mShouldStop)
					return;
			}
			
			if(!mQueue.empty())
			{
				send(mQueue.front());
				mQueue.pop();
			}
			else {
				send(Message(Message::Dummy));
			}
		}
	}
	catch(std::exception &e)
	{
		LogWarn("Overlay::Handler::Sender", String("Sending failed: ") + e.what());
		//mStream->close();
	}
}

void Overlay::Handler::Sender::send(const Message &message)
{
	Desynchronize(this);
	
	BinaryString source = message.source;
	if(message.source.empty())
		source = mOverlay->localNode();

	BinaryString header;
	BinarySerializer s(&header);
	
	// 32-bit control block
	s.write(message.version);
	s.write(message.flags);
	s.write(message.ttl);
	s.write(message.type);
	
	// 32-bit size block
	s.write(uint8_t(source.size()));
	s.write(uint8_t(message.destination.size()));
	s.write(uint16_t(message.content.size()));
	
	mStream->writeBinary(header);
	
	// data
	mStream->writeBinary(source);
	mStream->writeBinary(message.destination);
	mStream->writeBinary(message.content);
	
	mStream->nextWrite();	// switch to next datagram if this is a datagram stream
}

}
