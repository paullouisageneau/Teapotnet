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
		mPool(2 + 3)	// TODO
{
	mFileName = "keys";
	load();
	
	// Generate RSA key if necessary
	if(mPublicKey.isNull())
	{
		LogDebug("Overlay", "Generating new keys...");
		Random rnd(Random::Key);
		Rsa rsa(4096);
		rsa.generate(mPublicKey, mPrivateKey);
	}
	
	// Create certificate
	mCertificate = std::make_shared<SecureTransport::RsaCertificate>(mPublicKey, mPrivateKey, localNode().toString());
	
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
	
	LogDebug("Overlay", "Instance name is \"" + localName() + "\"");
	LogDebug("Overlay", "Local node is " + localNode().toString());
	save();
	
	// Create backends
	mBackends.push_back(std::make_shared<DatagramBackend>(this, port));
	mBackends.push_back(std::make_shared<StreamBackend>(this, port));
	
	for(auto b : mBackends)
		mPool.enqueue([b]()
		{
			b->run();
		});

	// Start
	mRunAlarm.schedule(Alarm::clock::now() + seconds(1.), [this]()
	{
		run();
	});
}

Overlay::~Overlay(void)
{

}

void Overlay::load()
{
	if(!File::Exist(mFileName)) return;
	File file(mFileName, File::Read);
	JsonSerializer serializer(&file);
	serializer >> *this;
	file.close();
}

void Overlay::save() const
{
	SafeWriteFile file(mFileName);
	JsonSerializer serializer(&file);
	serializer << *this;
	file.close();
}

void Overlay::join(void)
{
	mPool.join();
	mRunAlarm.join();
}

String Overlay::localName(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	Assert(!mName.empty());
	return mName;
}

BinaryString Overlay::localNode(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mPublicKey.digest();
}

const Rsa::PublicKey &Overlay::publicKey(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mPublicKey; 
}

const Rsa::PrivateKey &Overlay::privateKey(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mPrivateKey; 
}

sptr<SecureTransport::Certificate> Overlay::certificate(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mCertificate;
}

void Overlay::getAddresses(Set<Address> &set) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	set.clear();
	for(auto b : mBackends)
	{
		Set<Address> backendSet;
		b->getAddresses(backendSet);
		set.insertAll(backendSet);
	}
}

bool Overlay::connect(const Set<Address> &addrs, const BinaryString &remote, bool async)
{
	try {
		if(isConnected(remote)) return true;
		
		Set<Address> filteredAddrs;
		for(auto &a : addrs)
		{
			Address tmp(a);
			tmp.setPort(0);	// so it matches any port
			if(!mRemoteAddresses.contains(tmp))
				filteredAddrs.insert(a);
		}
		
		if(!filteredAddrs.empty())
		{
			auto connectTask = [filteredAddrs, remote](List<sptr<Backend> > backends)
			{
				for(auto b : backends)
				{
					try {
						if(b->connect(filteredAddrs, remote))
							return true;
					}
					catch(const std::exception &e)
					{
						LogWarn("Overlay::connect", e.what());
					}
				}
				
				return false;
			};
			
			if(async)
			{
				mPool.enqueue(connectTask, mBackends);
				return true;
			}
			else {
				return connectTask(mBackends);
			}
		}
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::connect", e.what());
		return false;
	}
	
	return false;
}

bool Overlay::isConnected(const BinaryString &remote) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mHandlers.contains(remote);
}

int Overlay::connectionsCount(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mHandlers.size();
}

bool Overlay::recv(Message &message, duration timeout)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(mIncoming.empty() && timeout > duration::zero())
	{
		mIncomingCondition.wait_for(lock, timeout, [this]() {
			return !mIncoming.empty();
		});
	}
	
	if(mIncoming.empty())
		return false;
	
	message = mIncoming.front();
	mIncoming.pop();
	return true;
}

bool Overlay::send(const Message &message)
{
	return route(message);	// Alias	
}

void Overlay::store(const BinaryString &key, const BinaryString &value)
{
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
	send(Message(Message::Retrieve, "", key));
	
	// Push Value messages in local queue
	BinaryString node(localNode());
	Set<BinaryString> values;
	if(Store::Instance->retrieveValue(key, values))
	{
		for(auto &v : values)
		{
			Message message(Message::Value, v, node, key);
			push(message);
		}
	}
}

bool Overlay::retrieve(const BinaryString &key, Set<BinaryString> &values)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	bool sent = false;
	if(!mRetrievePending.contains(key))
	{
		mRetrievePending.insert(key);
		sent = send(Message(Message::Retrieve, "", key));
	}
	
	if(sent)
	{
		duration timeout = milliseconds(Config::Get("request_timeout").toDouble());
	
		mRetrieveCondition.wait_for(lock, timeout, [this, key]() {
			return !mRetrievePending.contains(key);
		});
	}
	
	mRetrievePending.erase(key);
	Store::Instance->retrieveValue(key, values);
	return !values.empty();
}

bool Overlay::incoming(Message &message, const BinaryString &from)
{
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
				
				Set<Address> addrs;
				BinarySerializer(&message.content) >> addrs;
				connect(addrs, message.source);
			}
			break;
		}

	// Retrieve value from DHT
	case Message::Retrieve:
		{
			//LogDebug("Overlay::Incoming", "Retrieve " + message.destination.toString());
			
			route(message);
			
			Set<BinaryString> values;
			Store::Instance->retrieveValue(message.destination, values);
			for(auto it = values.begin(); it != values.end(); ++it)
				send(Message(Message::Value, *it, message.source, message.destination));
			
			//push(message);	// useless
			break;
		}
		
	// Store value in DHT
	case Message::Store:
		{
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
			
			if(mRetrievePending.contains(message.content))
			{
				mRetrievePending.erase(message.content);
				mRetrieveCondition.notify_all();
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
			
			if(mRetrievePending.contains(message.content))
			{
				mRetrievePending.erase(message.content);
				mRetrieveCondition.notify_all();
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
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mIncoming.push(message);
	}

	mIncomingCondition.notify_one();
	return true;
}

bool Overlay::route(const Message &message, const BinaryString &from)
{
	// Drop if TTL is zero
	if(message.ttl == 0) return false;
	
	// Drop if self
	if(message.destination == localNode()) return false;
	
	// Drop if not connected
	if(mHandlers.empty()) return false;
		
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
	//LogDebug("Overlay::sendTo", "Broadcasting message");
	
	Array<BinaryString> neighbors;
	mHandlers.getKeys(neighbors);
	
	bool success = false;
	for(int i=0; i<neighbors.size(); ++i)
	{
		if(!from.empty() && neighbors[i] == from) continue;
		
		sptr<Handler> handler;
		if(mHandlers.get(neighbors[i], handler))
		{
			success|= handler->send(message);
		}
	}
	
	return success;
}

bool Overlay::sendTo(const Message &message, const BinaryString &to)
{
	if(to.empty())
	{
		broadcast(message);
		return true;
	}
	
	sptr<Handler> handler;
	if(mHandlers.get(to, handler))
	{
		//LogDebug("Overlay::sendTo", "Sending message via " + to.toString());
		handler->send(message);
		return true;
	}
	
	return false;
}

int Overlay::getRoutes(const BinaryString &destination, int count, Array<BinaryString> &result)
{
	result.clear();
	
	Map<BinaryString, BinaryString> sorted;
	Array<BinaryString> neighbors;
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mHandlers.getKeys(neighbors);
	}
	
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
	result.clear();
	
	Map<BinaryString, BinaryString> sorted;
	Array<BinaryString> neighbors;
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mHandlers.getKeys(neighbors);
	}

	for(int i=0; i<neighbors.size(); ++i)
		sorted.insert(destination ^ neighbors[i], neighbors[i]);
	
	sorted.getValues(result);
	return result.size();
}

void Overlay::registerHandler(const BinaryString &node, const Address &addr, sptr<Overlay::Handler> handler)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mRemoteAddresses.insert(addr);
	
	Set<Address> otherAddrs;
	sptr<Handler> h;
	if(mHandlers.get(node, h))
	{
		mHandlers.erase(node);
		LogDebug("Overlay::registerHandler", "Replacing handler for " + node.toString());
		
		h->getAddresses(otherAddrs);
		h->stop();
	}
	
	Assert(handler);
	mHandlers.insert(node, handler);
	handler->addAddresses(otherAddrs);
	handler->start();
	
	// On first connection, schedule store to publish in DHT
	if(mHandlers.size() == 1)
		Store::Instance->start(); 
}

void Overlay::unregisterHandler(const BinaryString &node, const Set<Address> &addrs, Overlay::Handler *handler)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	sptr<Handler> h;
	if(!mHandlers.get(node, h) || h.get() != handler)
		return;
		
	for(auto &a : addrs)
		mRemoteAddresses.erase(a);
	
	mHandlers.erase(node);

	// If it was the last handler, try to reconnect now
	if(mHandlers.empty())
		mRunAlarm.schedule(Alarm::clock::now());
}

bool Overlay::track(const String &tracker, Map<BinaryString, Set<Address> > &result)
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
		for(auto it = trackerAddresses.begin(); it != trackerAddresses.end(); ++it)
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
		for(auto it = addresses.begin(); it != addresses.end(); ++it)
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
			if(!(serializer >> result)) return false;
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
	std::unique_lock<std::mutex> lock(mMutex);

	s << Object()
		.insert("publickey", mPublicKey)
		.insert("privatekey", mPrivateKey);
}

bool Overlay::deserialize(Serializer &s)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	mPublicKey.clear();
	mPrivateKey.clear();
	
	if(!(s >> Object()
		.insert("publickey", mPublicKey)
		.insert("privatekey", mPrivateKey)))
		return false;
	
	// TODO: Sanitize
	return true;
}

void Overlay::run(void)
{
	try {
		const int minConnectionsCount = Config::Get("min_connections").toInt();

		Map<BinaryString, Set<Address> > result;
		if(track(Config::Get("tracker"), result))
			if(connectionsCount() < minConnectionsCount)
				for(auto it = result.begin(); it != result.end(); ++it)
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
		
		Set<Address> addrs;
		Config::GetExternalAddresses(addrs);
		addrs.insertAll(mLocalAddresses);
		
		// TODO: external address discovery by other nodes
		
		if(!addrs.empty())
		{
			BinaryString content;
			BinarySerializer(&content) << addrs;
			broadcast(Message(Message::Offer, content));
		}
		
		if(connectionsCount() < minConnectionsCount) mRunAlarm.schedule(seconds(Random().uniform(0.,120.)));	// avg 1 min
		else mRunAlarm.schedule(seconds(600.));   // 10 min
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::run", e.what());
		mRunAlarm.schedule(seconds(60.));   // 1 min	
	}
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
	s << source;
	s << destination;
	s << content;
}

bool Overlay::Message::deserialize(Serializer &s)
{
	// TODO
	if(!(s >> source)) return false;
	AssertIO(s >> destination);
	AssertIO(s >> content);
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
		
		bool verifyPublicKey(const std::vector<Rsa::PublicKey> &chain)
		{
			if(chain.empty()) return false;
			publicKey = chain[0];
			
			LogDebug("Overlay::Backend::handshake", String("Remote node is ") + publicKey.digest().toString());
			return true;		// Accept
		}
	};

	// Add certificate
	transport->addCredentials(mOverlay->certificate().get(), false);

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
		
		sptr<Handler> handler = std::make_shared<Handler>(mOverlay, transport, identifier, addr);
		mOverlay->registerHandler(identifier, addr, handler);
		return true;
	}
	else {
		LogDebug("Overlay::Backend::handshake", "Handshake failed");
		return false;
	}
}

void Overlay::Backend::run(void)
{
	while(true)
	{
		SecureTransport *transport = NULL;
		try {
			Address addr;
			transport = listen(&addr);
			if(!transport) break;
			
			LogDebug("Overlay::Backend::run", "Incoming connection from " + addr.toString());
			
			mOverlay->mPool.enqueue([this, transport, addr]()
			{
				try {
					handshake(transport, addr, "");
				}
				catch(const std::exception &e)
				{
					LogDebug("Overlay::Backend::run", e.what());
				}
			});
		}
		catch(const std::exception &e)
		{
			LogError("Overlay::Backend::run", e.what());
			delete transport;
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
	
	for(auto it = addrs.rbegin(); it != addrs.rend(); ++it)
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
	const duration timeout = milliseconds(Config::Get("idle_timeout").toDouble());
	const duration connectTimeout = milliseconds(Config::Get("connect_timeout").toDouble());
	
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
	const duration connectTimeout = milliseconds(Config::Get("connect_timeout").toDouble());
	
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
	const duration timeout = milliseconds(Config::Get("idle_timeout").toDouble());
	const duration dataTimeout = milliseconds(Config::Get("connect_timeout").toDouble());
	
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
	
	Set<Address> localAddrs;
	getAddresses(localAddrs);
	
	for(auto it = addrs.rbegin(); it != addrs.rend(); ++it)
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
	const duration timeout = milliseconds(Config::Get("idle_timeout").toDouble());
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
	mStop(false),
	mSender(overlay, stream)
{
	if(node == mOverlay->localNode())
		throw Exception("Spawned a handler for local node");
	
	addAddress(addr);
}

Overlay::Handler::~Handler(void)
{
	mOverlay->unregisterHandler(mNode, mAddrs, this);	// should be done already
	
	stop();
	mThread.join();
	mSenderThread.join();
	
	delete mStream;
}

bool Overlay::Handler::recv(Message &message)
{
	while(true)
	{
		try {
			BinarySerializer s(mStream);
			
			// 32-bit control block
			if(!(s >> message.version))
			{
				if(!mStream->nextRead()) break;
				continue;
			}
			
			AssertIO(s >> message.flags);
			AssertIO(s >> message.ttl);
			AssertIO(s >> message.type);
			
			// 32-bit size block
			uint8_t sourceSize, destinationSize;
			uint16_t contentSize;
			AssertIO(s >> sourceSize);
			AssertIO(s >> destinationSize);
			AssertIO(s >> contentSize);
			
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
	std::unique_lock<std::mutex> lock(mMutex);
	return mSender.push(message);
}

void Overlay::Handler::start(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	mThread = std::thread([this]()
	{
		run();
	});

	mSenderThread = std::thread([this]()
	{
		mSender.run();
	});
}

void Overlay::Handler::stop(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mStop = true;
	mSender.stop();
	mStream->close();
}

void Overlay::Handler::addAddress(const Address &addr)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mAddrs.insert(addr);
}

void Overlay::Handler::addAddresses(const Set<Address> &addrs)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mAddrs.insertAll(addrs);
}

void Overlay::Handler::getAddresses(Set<Address> &set) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	set = mAddrs;
}

BinaryString Overlay::Handler::node(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mNode;
}

void Overlay::Handler::run(void)
{
	LogDebug("Overlay::Handler::run", "Starting handler");
	
	try {
		process();
		
		LogDebug("Overlay::Handler::run", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogWarn("Overlay::Handler::run", String("Closing handler: ") + e.what());
	}
	
	mOverlay->unregisterHandler(mNode, mAddrs, this);
	
	mSender.stop();	
}

void Overlay::Handler::process(void)
{
	Message message;
	while(recv(message) && !mStop)
	{
		//LogDebug("Overlay::Handler", "Received message");
		mOverlay->incoming(message, mNode);
	}
}

Overlay::Handler::Sender::Sender(Overlay *overlay, Stream *stream) :
	mOverlay(overlay),
	mStream(stream),
	mStop(false)
{
	
}

Overlay::Handler::Sender::~Sender(void)
{

}

bool Overlay::Handler::Sender::push(const Message &message)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	if(mQueue.size() < Overlay::MaxQueueSize)
	{
		mQueue.push(message);
		lock.unlock();
		mCondition.notify_all();
		return true;
	}
	
	return false;
}

void Overlay::Handler::Sender::stop(void)
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mStop = true;
	}

	mCondition.notify_all();
}

void Overlay::Handler::Sender::run(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	try {
		while(!mStop)
		{
			const duration timeout = milliseconds(Config::Get("keepalive_timeout").toDouble());
			
			if(mQueue.empty())
			{
				mCondition.wait_for(lock, timeout, [this]() {
					return !mQueue.empty() || mStop;
				});
			}
			
			if(mStop) break;
			
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
	BinaryString source = message.source;
	if(message.source.empty())
		source = mOverlay->localNode();

	BinaryString header;
	BinarySerializer s(&header);
	
	// 32-bit control block
	s << message.version;
	s << message.flags;
	s << message.ttl;
	s << message.type;
	
	// 32-bit size block
	s << uint8_t(source.size());
	s << uint8_t(message.destination.size());
	s << uint16_t(message.content.size());
	
	mStream->writeBinary(header);
	
	// data
	mStream->writeBinary(source);
	mStream->writeBinary(message.destination);
	mStream->writeBinary(message.content);
	
	mStream->nextWrite();	// switch to next datagram if this is a datagram stream
}

}
