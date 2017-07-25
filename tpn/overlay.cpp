/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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
#include "tpn/network.hpp"	// for sendCalls()

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
const int Overlay::StoreNeighbors = 3;
const int Overlay::DefaultTtl = 16;

Overlay::Overlay(int port) :
		mPool(2 + 5),
		mFirstRun(true)
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

	// Generate local node id
	mLocalNode = mPublicKey.fingerprint<Sha3_256>();

	// Create certificate
	mCertificate = std::make_shared<SecureTransport::RsaCertificate>(mPublicKey, mPrivateKey, localNode().toString());

	// Define node name
	mLocalName = Config::Get("node_name");
	if(mLocalName.empty())
	{
		char hostname[HOST_NAME_MAX];
		if(gethostname(hostname,HOST_NAME_MAX) == 0)
			mLocalName = hostname;

		if(mLocalName.empty() || mLocalName == "localhost")
			mLocalName = localNode().toString();
	}

	LogDebug("Overlay", "Instance name is \"" + mLocalName + "\"");
	LogDebug("Overlay", "Local node is " + mLocalNode.toString());
	save();

	// Create backends
	mBackends.push_back(std::make_shared<DatagramBackend>(this, port));
	mBackends.push_back(std::make_shared<StreamBackend>(this, port));

	for(auto b : mBackends)
		mPool.enqueue([b]()
		{
			b->run();
		});

	start(seconds(1.));
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

void Overlay::start(duration delay)
{
	mFirstRun = true;
	mRunAlarm.schedule(Alarm::clock::now() + delay, [this]()
	{
		mPool.enqueue([this]()
		{
			update();
		});
	});
}

String Overlay::localName(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
	Assert(!mLocalName.empty());
	return mLocalName;
}

BinaryString Overlay::localNode(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
	return mLocalNode;
}

const Rsa::PublicKey &Overlay::publicKey(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
	return mPublicKey;
}

const Rsa::PrivateKey &Overlay::privateKey(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
	return mPrivateKey;
}

sptr<SecureTransport::Certificate> Overlay::certificate(void) const
{
	//std::unique_lock<std::mutex> lock(mMutex);
	return mCertificate;
}

int Overlay::getAddresses(Set<Address> &set) const
{
	std::unique_lock<std::mutex> lock(mMutex);

	set.clear();
	for(auto b : mBackends)
	{
		Set<Address> backendSet;
		b->getAddresses(backendSet);
		set.insertAll(backendSet);
	}

	return set.size();
}

int Overlay::getRemoteAddresses(const BinaryString &remote, Set<Address> &set) const
{
	std::unique_lock<std::mutex> lock(mMutex);

	set.clear();
	if(remote.empty()) return 0;

	sptr<Handler> handler;
	if(mHandlers.get(remote, handler))
		handler->getAddresses(set);

	return set.size();
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

		if(filteredAddrs.empty()) return false;

		if(!remote.empty()) LogDebug("Overlay::connect", "Trying node " + remote.toString());

		auto connectTask = [this, filteredAddrs, remote](List<sptr<Backend> > backends)
		{
			for(auto b : backends)
			{
				for(const auto &addr : filteredAddrs)
				{
					try {
						if(b->connect(addr, remote))
						{
							bool changed = false;
							{
								std::unique_lock<std::mutex> lock(mMutex);
								auto it = mKnownPeers.find(addr);
								if(!remote.empty() && (it == mKnownPeers.end() || it->second != remote))
								{
									mKnownPeers.insert(addr, remote);
									changed = true;
								}
							}

							if(changed) save();
							return true;
						}
						else {
							bool changed = false;
							{
								std::unique_lock<std::mutex> lock(mMutex);
								if(mKnownPeers.contains(addr) && Random().uniform(0, 100) == 0)
								{
									mKnownPeers.erase(addr);
									changed = true;
								}
							}

							if(changed) save();
						}
					}
					catch(const std::exception &e)
					{
						LogWarn("Overlay::connect", e.what());
					}
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
	catch(const std::exception &e)
	{
		LogError("Overlay::connect", e.what());
		return false;
	}

	return false;
}

bool Overlay::connect(const Address &addr, const BinaryString &remote, bool async)
{
	Set<Address> addrs;
	addrs.insert(addr);
	return connect(addrs, remote, async);
}

bool Overlay::isConnected(const BinaryString &remote) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mHandlers.contains(remote);
}

bool Overlay::waitConnection(void) const
{
	return waitConnection(duration(-1));
}

bool Overlay::waitConnection(duration timeout) const
{
	if(timeout < duration::zero())
		timeout = milliseconds(Config::Get("request_timeout").toDouble());

	std::unique_lock<std::mutex> lock(mMutex);
	return mCondition.wait_for(lock, timeout, [this]() {
		return !mHandlers.empty();
	});
}

int Overlay::connectionsCount(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mHandlers.size();
}

bool Overlay::recv(Message &message, duration timeout)
{
	std::unique_lock<std::mutex> lock(mIncomingMutex);

	if(timeout > duration::zero())
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
	if(getRoutes(key, StoreNeighbors, nodes))
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
	List<BinaryString> values;
	List<Time> times;
	if(Store::Instance->retrieveValue(key, values, times))
	{
		while(!values.empty())
		{
			Assert(!times.empty());
			Message message(Message::Value, BinaryString::number(uint64_t(times.front())) + values.front(), node, key);
			push(message);
			values.pop_front();
			times.pop_front();
		}
	}
}

bool Overlay::retrieve(const BinaryString &key, Set<BinaryString> &values)
{
	return retrieve(key, values, duration(-1));
}

bool Overlay::retrieve(const BinaryString &key, Set<BinaryString> &values, duration timeout)
{
	if(timeout < duration::zero())
		timeout = milliseconds(Config::Get("request_timeout").toDouble());

  waitConnection(timeout);

	std::unique_lock<std::mutex> lock(mRetrieveMutex);

	bool sent = true;
	if(!mRetrievePending.contains(key))
	{
		mRetrievePending.insert(key);
		sent = send(Message(Message::Retrieve, "", key));
	}

	if(sent)
	{
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

	if(message.source.empty())
		message.source = from;

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
			// Read addresses
			Set<Address> addrs;
			BinarySerializer(&message.content) >> addrs;

			// Add known addresses
			Set<Address> remoteAddresses;
			getRemoteAddresses(message.source, remoteAddresses);
			addrs.insertAll(remoteAddresses);

			// Send suggest message
			Message suggest(Message::Suggest);
			suggest.ttl = message.ttl;
			suggest.source = message.source;
			BinarySerializer(&suggest.content) << addrs;

			BinaryString distance = message.source ^ localNode();
			Array<BinaryString> neighbors;
			mHandlers.getKeys(neighbors);
			for(int i=0; i<neighbors.size(); ++i)
			{
				if(message.source != neighbors[i]
					&& (message.source ^ neighbors[i]) <= distance)
				{
					suggest.destination = neighbors[i];
					send(suggest);
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
			// In Retrieve messages, key is in the destination field
			const BinaryString &key = message.destination;

			//LogDebug("Overlay::Incoming", "Retrieve " + key.toString());

			route(message, from);

			List<BinaryString> values;
			List<Time> times;
			Store::Instance->retrieveValue(key, values, times);
			while(!values.empty())
			{
				Assert(!times.empty());
				send(Message(Message::Value, BinaryString::number(uint64_t(times.front())) + values.front(), message.source, key));
				values.pop_front();
				times.pop_front();
			}

			//push(message);	// useless
			break;
		}

	// Store value in DHT
	case Message::Store:
		{
			// In Store messages, key is in the destination field
			const BinaryString &key = message.destination;
			const BinaryString &value = message.content;

			//LogDebug("Overlay::Incoming", "Store " + key.toString());

			Time oldTime = Store::Instance->getValueTime(key, value);
			Time now = Time::Now();

			if(now - oldTime >= seconds(60.)) // 1 min
        		{
				Array<BinaryString> nodes;
				if(getRoutes(key, StoreNeighbors, nodes))
				{
					for(int i=0; i<nodes.size(); ++i)
					{
						if(nodes[i] == localNode()) Store::Instance->storeValue(key, value, Store::Distributed, now);
						else if(nodes[i] != from) sendTo(message, nodes[i]);
					}
				}
			}

			{
				std::unique_lock<std::mutex> lock(mRetrieveMutex);

				if(mRetrievePending.contains(key))
				{
					mRetrievePending.erase(key);
					mRetrieveCondition.notify_all();
				}
			}

			//push(message);	// useless
			break;
		}

	// Response to retrieve from DHT
	case Message::Value:
		{
			// In Value messages, key is in the source field
			const BinaryString &key = message.source;

			//LogDebug("Overlay::Incoming", "Value " + key.toString());

			uint64_t ts = 0;
			BinaryString value = message.content;
			if(!value.readBinary(ts) || value.empty()) return false;

			Store::Instance->storeValue(key, value, Store::Distributed, Time(ts));

			{
				std::unique_lock<std::mutex> lock(mRetrieveMutex);

				if(mRetrievePending.contains(key))
				{
					mRetrievePending.erase(key);
					mRetrieveCondition.notify_all();
				}
			}

			route(message, from);
			push(message);
			break;
		}

	// Ping
	case Message::Ping:
		{
			//LogDebug("Overlay::incoming", "Ping from " + message.source.toString());
			send(Message(Message::Pong, message.content, message.source));
			break;
		}

	// Pong
	case Message::Pong:
		{
			//LogDebug("Overlay::incoming", "Pong from " + message.source.toString());
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
		std::unique_lock<std::mutex> lock(mIncomingMutex);
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
	Assert(handler);

	sptr<Handler> currentHandler;
	Set<Address>  currentAddrs;
	bool isFirst = false;
	{
		std::unique_lock<std::mutex> lock(mMutex);

		isFirst = (mHandlers.empty());

		if(mHandlers.get(node, currentHandler))
		{
			mHandlers.erase(node);
			LogDebug("Overlay::registerHandler", "Replacing handler for " + node.toString());

			currentHandler->getAddresses(currentAddrs);
			currentHandler->stop();
		}

		mHandlers.insert(node, handler);
		handler->addAddresses(currentAddrs);
		handler->start();

		mRemoteAddresses.insert(addr);
	}

	// On first connection
	if(isFirst)
	{
		// Send network calls
		Network::Instance->sendCalls();
		Network::Instance->sendBeacons();

		// Schedule store to publish in DHT
		Store::Instance->start();
	}

	mCondition.notify_all();
}

void Overlay::unregisterHandler(const BinaryString &node, const Set<Address> &addrs, Overlay::Handler *handler)
{
	sptr<Handler> currentHandler; // prevent handler deletion on erase
	bool isLast = false;
	{
		std::unique_lock<std::mutex> lock(mMutex);

		if(!mHandlers.get(node, currentHandler) || currentHandler.get() != handler)
			return;

		mHandlers.erase(node);

		for(auto &a : addrs)
			mRemoteAddresses.erase(a);

		isLast = (mHandlers.empty());
	}

	// If it was the last handler
	if(isLast)
	{
		// Clear pending retrieve requests
		{
			std::unique_lock<std::mutex> lock(mRetrieveMutex);
			mRetrievePending.clear();
		}

		mRetrieveCondition.notify_all();

		// Try to reconnect now
		mRunAlarm.schedule(Alarm::clock::now());
	}

	mCondition.notify_all();
}

bool Overlay::track(const String &tracker, unsigned count, Map<BinaryString, Set<Address> > &result)
{
	result.clear();
	if(tracker.empty()) return false;

	String url;
	if(tracker.contains("://")) url = tracker;
	else url = "http://" + tracker;

	LogDebug("Overlay::track", "Contacting tracker " + url);

	try {
		url+= String(url[url.size()-1] == '/' ? "" : "/") + "teapotnet/tracker?id=" + localNode().toString() + "&count=" + String::number(count);

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
		.insert("privatekey", mPrivateKey)
		.insert("peers", mKnownPeers);
}

bool Overlay::deserialize(Serializer &s)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mPublicKey.clear();
	mPrivateKey.clear();
	mLocalNode.clear();

	Map<Address, BinaryString> peers;

	if(!(s >> Object()
		.insert("publickey", mPublicKey)
		.insert("privatekey", mPrivateKey)
		.insert("peers", peers)))
		return false;

	// TODO: Sanitize

	mLocalNode = mPublicKey.fingerprint<Sha3_256>();
	mKnownPeers.insertAll(peers);
	return true;
}

void Overlay::update(void)
{
	try {
		const int minConnectionsCount = Config::Get("min_connections").toInt();

		Set<Address> externalAddrs;
		Config::GetExternalAddresses(externalAddrs);

		if(connectionsCount() < minConnectionsCount)
		{
			Map<Address, BinaryString> peers;
			{
				std::unique_lock<std::mutex> lock(mMutex);
				for(const auto &p : mKnownPeers)
					if(!mHandlers.contains(p.second))
						peers.insert(p.first, p.second);
			}

			for(const auto &p : peers)
				connect(p.first, p.second, mFirstRun);	// async only if first run
		}

		unsigned count = minConnectionsCount*2;
		Map<BinaryString, Set<Address> > result;
		if(track(Config::Get("tracker"), count, result))
		{
			for(const auto &p : result)
			{
				if(p.first == localNode())
				{
					std::unique_lock<std::mutex> lock(mMutex);

					// Store external addresses for Offer messages
					mLocalAddresses = externalAddrs;
					mLocalAddresses.insertAll(p.second);

					// TODO: external address discovery by other nodes

					if(!mLocalAddresses.empty())
					{
						BinaryString content;
						BinarySerializer(&content) << mLocalAddresses;
						broadcast(Message(Message::Offer, content));
					}
				}
				else {
					if(connectionsCount() < minConnectionsCount)
						connect(p.second, p.first, false);	// sync
				}
			}

			if(connectionsCount() < minConnectionsCount) mRunAlarm.schedule(seconds(60.));	// 1 min
			else mRunAlarm.schedule(seconds(Random().uniform(1140., 1260.)));   // ~10 min
		}
		else {
			mRunAlarm.schedule(seconds(60.));	// 1 min
		}
	}
	catch(const std::exception &e)
	{
		LogError("Overlay::run", e.what());
		mRunAlarm.schedule(seconds(60.));   // 1 min
	}

	mFirstRun = false;
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
	ttl = DefaultTtl;
	type = Message::Dummy;

	source.clear();
	destination.clear();
	content.clear();
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
			return true;	// Accept
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

	BinaryString identifier = verifier.publicKey.fingerprint<Sha3_256>();
	if(remote.empty() || remote == identifier)
	{
		// Handshake succeeded
		LogInfo("Overlay::Backend::handshake", String("Connected node: ") + identifier.toString());

		sptr<Handler> handler = std::make_shared<Handler>(mOverlay, transport, identifier, addr);
		mOverlay->registerHandler(identifier, addr, handler);
		return true;
	}
	else {
		LogDebug("Overlay::Backend::handshake", String("Unexpected node: ") + identifier.toString());
		delete transport;
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
					LogDebug("Overlay::Backend::run", String("Handshake failed: ") + e.what());
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

bool Overlay::StreamBackend::connect(const Address &addr, const BinaryString &remote)
{
	const duration timeout = milliseconds(Config::Get("idle_timeout").toDouble());
	const duration connectTimeout = milliseconds(Config::Get("connect_timeout").toDouble());

	try {
		Set<Address> localAddrs;
		getAddresses(localAddrs);
		if(localAddrs.contains(addr))
			return false;

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

	return false;
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

bool Overlay::DatagramBackend::connect(const Address &addr, const BinaryString &remote)
{
	const duration timeout = milliseconds(Config::Get("idle_timeout").toDouble());
	const unsigned int mtu = 1452; // UDP over IPv6 on ethernet

	try {
		Set<Address> localAddrs;
		getAddresses(localAddrs);
		if(localAddrs.contains(addr))
			return false;

		if(Config::Get("force_http_tunnel").toBool())
			return false;

		LogDebug("Overlay::DatagramBackend::connect", "Trying address " + addr.toString() + " (UDP)");

		DatagramStream *stream = NULL;
		SecureTransport *transport = NULL;
		try {
			stream = new DatagramStream(&mSock, addr);
			stream->setTimeout(timeout);
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

	return false;
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
	// Close
	stop();

	// Join threads
	mSenderThread.join();
	if(mThread.get_id() == std::this_thread::get_id()) mThread.detach();
	else if(mThread.joinable()) mThread.join();

	// Delete stream
	std::unique_lock<std::mutex> lock(mMutex);
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
	return mSender.push(message);
}

void Overlay::Handler::start(void)
{
	mThread = std::thread([this]()
	{
		run();

		Set<Address> addrs;
		getAddresses(addrs);
		mOverlay->unregisterHandler(mNode, addrs, this);
	});

	mSenderThread = std::thread([this]()
	{
		mSender.run();
	});
}

void Overlay::Handler::stop(void)
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mStream->close();
		mStop = true;
	}
	
	mSender.stop();
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
		Message message;
		while(recv(message) && !mStop)
		{
			//LogDebug("Overlay::Handler", "Received message");
			mOverlay->incoming(message, mNode);
		}

		LogDebug("Overlay::Handler::run", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogWarn("Overlay::Handler::run", String("Closing handler: ") + e.what());
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

			mCondition.wait_for(lock, timeout, [this]() {
				return !mQueue.empty() || mStop;
			});

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
		mStream->close();
		mStop = true;
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
