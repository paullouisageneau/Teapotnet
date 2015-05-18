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

Network *Network::Instance = NULL;


Network::Network(int port) :
		Overlay(port),
		Tunneler(this),
		mThreadPool(4, 16, Config::Get("max_tunnels").toInt())
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
}

void Network::join(void)
{
	mTunneler.join();
	mOverlay.join();
}

void Network::registerCaller(const BinaryString &target, Caller *caller)
{
	Synchronize(this);
	mCallers[target].insert(caller);
	
	LogDebug("Network::registerCaller", "Calling " + target.toString());
	
	StringMap content;
	content["target"] = target.toString();
	outgoing("call", content);
	
	// TODO: beacons
}

void Network::unregisterCaller(const BinaryString &target, Caller *caller)
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

void Network::unregisterAllCallers(const BinaryString &target)
{
	Synchronize(this);
	mCallers.erase(target);
}

void Network::registerListener(const Identifier &id, Listener *listener)
{
	Synchronize(this);
	mListeners[id].insert(listener);
	
	//LogDebug("Network::registerListener", "Registered listener: " + id.toString());
	
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

void Network::unregisterListener(const Identifier &id, Listener *listener)
{
	Synchronize(this);
	
	Map<Identifier, Set<Listener*> >::iterator it = mListeners.find(id);
	while(it != mListeners.end() && it->first == id)
	{
		it->second.erase(listener);
		//if(it->second.erase(listener))
		//	LogDebug("Network::unregisterListener", "Unregistered listener: " + id.toString());
		
		if(it->second.empty())   
			mListeners.erase(it);
			
		++it;
	}
}

void Network::publish(String prefix, Publisher *publisher)
{
	Synchronize(this);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Network::publish", "Publishing " + prefix);
	
	mPublishers[prefix].insert(publisher);
}

void Network::unpublish(String prefix, Publisher *publisher)
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

void Network::subscribe(String prefix, Subscriber *subscriber)
{
	Synchronize(this);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);

	LogDebug("Network::subscribe", "Subscribing " + prefix);
	
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

void Network::unsubscribe(String prefix, Subscriber *subscriber)
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

void Network::advertise(String prefix, const String &path, const Identifier &source, Publisher *publisher)
{
	Synchronize(this);
	
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	LogDebug("Network::publish", "Advertising " + prefix + path);
	
	matchSubscribers(prefix, source, publisher); 
}

void Network::addRemoteSubscriber(const Identifier &peer, const String &path, bool publicOnly)
{
	Synchronize(this);
	
	mRemoteSubscribers.push_front(RemoteSubscriber(peer, publicOnly));
	mRemoteSubscribers.begin()->subscribe(path);
}

bool Network::broadcast(const Identifier &local, const Notification &notification)
{
	Synchronize(this);	
	return outgoing(local, Identifier::Null, "notif", notification);
}

bool Network::send(const Identifier &local, const Identifier &remote, const Notification &notification)
{
	Synchronize(this);
	return outgoing(local, remote, "notif", notification);
}

bool Network::addHandler(Stream *stream, const Identifier &local, const Identifier &remote)
{
	// Not synchronized
	Assert(stream);
	
	LogDebug("Network", "New handler");
	Handler *handler = new Handler(stream, local, remote);
	mThreadPool.launch(handler);
	return true;
}

bool Network::hasHandler(const Identifier &local, const Identifier &remote)
{
	Synchronize(this);
	return mHandlers.contains(IdentifierPair(local, remote));
}

bool Network::registerHandler(const Identifier &local, const Identifier &remote, Handler *handler)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	IdentifierPair pair(local, remote);
	
	Handlerer::Handler *l = NULL;
	if(mHandlerer::Handlers.get(pair, l))
		return (l == handler);
	
	mHandlerer::Handlers.insert(pair, handler);
	return true;
}

bool Network::unregisterHandler(const Identifier &local, const Identifier &remote, Handler *handler)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	IdentifierPair pair(local, remote);
	
	Handlerer::Handler *l = NULL;
	if(!mHandlerer::Handlers.get(pair, l) || l != handler)
		return false;
		
	mHandlerer::Handlers.erase();
		return true;	
}

bool Network::outgoing(const String &type, const Serializable &content)
{
	bool success = false;
	for(Map<IdentifierPair, Handler*>::iterator it = mHandlers.begin();
		it != mHandlers.end();
		++it)
	{
		success|= it->second->outgoing(type, content);
	}
	
	return success;
}

bool Network::outgoing(const Identifier &local, const Identifier &remote, const String &type, const Serializable &content)
{
	Synchronize(this);
	
	local.setNumber(getNumber());
	
	if(remote != Identifier::Null)
	{
		Map<IdentifierPair, Handler*>::iterator it = mHandlers.find(IdentifierPair(local, remote));
		if(it != mHandlers.end())
		{
			return it->second->outgoing(type, content);
		}
		
		return false;
	}
	else {
		bool success = false;
		for(Map<IdentifierPair, Handler*>::iterator it = mHandlers.lower_bound(IdentifierPair(local, Identifier::Null));
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
	// TODO	
}

bool incoming(const Message &message)
{
	Synchronize(this);
	
	if(message.destination.getNumber() != getNumber())
		return false;
	
	Map<IdentifierPair, Tunneler::Tunnel*>::iterator it = mTunneler::Tunnels.find(IdentifierPair(message.destination, message.source));	
	if(it != mTunneler::Tunnels.end()) return it->second->incoming(message);
	else return Network::Instance->mTunneler->incoming(message);
}

bool Network::matchPublishers(const String &path, const Identifier &source, Subscriber *subscriber)
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
				LogDebug("Network::Handler::incoming", "Anouncing " + path);
				
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

bool Network::matchSubscribers(const String &path, const Identifier &source, Publisher *publisher)
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

Network::Publisher::Publisher(const Identifier &peer) :
	mPeer(peer)
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

void Network::Publisher::publish(const String &prefix, const String &path)
{
	if(!mPublishedPrefixes.contains(prefix))
	{
		Network::Instance->publish(prefix, this);
		mPublishedPrefixes.insert(prefix);
	}
	
	Network::Instance->advertise(prefix, path, mPeer, this);
}

void Network::Publisher::unpublish(const String &prefix)
{
	if(mPublishedPrefixes.contains(prefix))
	{
		Network::Instance->unpublish(prefix, this);
		mPublishedPrefixes.erase(prefix);
	}
}

Network::Subscriber::Subscriber(const Identifier &peer) :
	mPeer(peer),
	mThreadPool(0, 1, 8)
{
	
}

Network::Subscriber::~Subscriber(void)
{
	unsubscribeAll();
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

Identifier Network::Subscriber::remote(void) const
{
	return Identifier::Null;
}

bool Network::Subscriber::localOnly(void) const
{
	return false;
}

bool Network::Subscriber::fetch(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target)
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
		PrefetchTask(Network::Subscriber *subscriber, const Identifier &peer, const String &prefix, const String &path, const BinaryString &target)
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
				LogWarn("Network::Subscriber::fetch", "Fetching failed for " + target.toString() + ": " + e.what());
			}
			
			delete this;	// autodelete
		}
	
	private:
		Network::Subscriber *subscriber;
		Identifier peer;
		BinaryString target;
		String prefix;
		String path;
	};
	
	PrefetchTask *task = new PrefetchTask(this, peer, prefix, path, target);
	mThreadPool.launch(task);
	return false;
}

Network::RemotePublisher::RemotePublisher(const List<BinaryString> targets):
	mTargets(targets)
{
  
}

Network::RemotePublisher::~RemotePublisher(void)
{
  
}

bool Network::RemotePublisher::anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets)
{
	targets = mTargets;
	return !targets.empty();
}

Network::RemoteSubscriber::RemoteSubscriber(const Identifier &remote, bool publicOnly) :
	mRemote(remote),
	mPublicOnly(publicOnly)
{

}

Network::RemoteSubscriber::~RemoteSubscriber(void)
{

}

bool Network::RemoteSubscriber::incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target)
{
	if(mRemote != Identifier::Null)
	{
		SerializableArray<BinaryString> array;
		array.append(target);
		
		BinaryString payload;
		BinarySerializer serializer(&payload);
		serializer.write(prefix);
		serializer.write(array);
		Network::Instance->outgoing(mRemote, Message::Forward, Message::Publish, payload); 
	}
}

Identifier Network::RemoteSubscriber::remote(void) const
{
	if(!mPublicOnly) return mRemote;
	else return Identifier::Null;
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
	for(Set<Identifier>::iterator it = mPeers.begin();
		it != mPeers.end();
		++it)
	{
		Network::Instance->unregisterListener(*it, this);
	}
}

void Network::Listener::listen(const Identifier &peer)
{
	mPeers.insert(peer);
	Network::Instance->registerListener(peer, this);
}

Network::Tunneler(Network *network) :
	Backend(network)
{

}

Network::Tunneler::~Tunneler(void)
{
	
}

bool Network::Tunneler::open(const BinaryString &remote, User *user)
{
	Assert(user);
	
	if(local.empty() || remote.empty())
		return false;
	
	if(mOverlay->connectionsCount() == 0)
		return false;
	
	LogDebug("Network::Tunneler::open", "Trying tunnel for " + remote.toString());
	
	uint64_t tunnelId;
	Random().read(tunnelId);	// Generate random tunnel ID
	BinaryString local = user->identifier();

	Tunneler::Tunnel *tunnel = NULL;
	SecureTransport *transport = NULL;
	try {
		tunnel = new Tunneler::Tunnel(this, tunnelId);
		transport = new SecureTransportClient(tunnel, NULL);
	}
	catch(...)
	{
		delete tunnel;
		throw;
	}
	
	LogDebug("Network::Tunneler::open", "Setting certificate credentials: " + user->name());
		
	// Set remote name
	transport->setHostname(remote.toString());
	
	// Add user certificate
	SecureTransportClient::Certificate *cert = user->certificate();
	if(cert) transport->addCredentials(cert, false);
	
	mThreadPool.run(tunnel);
	
	return handshake(transport, local, remote, false);	// sync
}

SecureTransport *Network::Tunneler::listen(void)
{
	Synchronize(&mQueueSync);
	
	while(mQueue.empty()) mQueueSync.wait();
	
	Message &datagram = mQueue.front();
	
	LogDebug("Network::Tunneler::listen", "Incoming tunnel from " + datagram.source.toString());

	// Read tunnel ID
	uint8_t len = 0;
	Identifier tunnelId;
	datagram.readBinary(len);
	datagram.readBinary(tunnelId, len);

	Tunneler::Tunnel *tunnel = NULL;
	SecureTransport *transport = NULL;
	try {
		tunnel = new Tunneler::Tunnel(mNetwork, tunnelId);
		transport = new SecureTransportServer(tunnel, NULL, true);	// ask for certificate
	}
	catch(...)
	{
		delete tunnel;
		mQueue.pop();
		throw;
	}
	
	mTunneler::Tunnels.insert(tunnelId, tunnel);
	mThreadPool.run(tunnel);
	tunnel->incoming(datagram);
	
	mQueue.pop();
	return transport;	// handshake done in run()
}

bool Network::Tunneler::incoming(const Message &datagram)
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
	if(mTunneler::Tunnels.get(tunnel->id(), t))
		return (t == tunnel);
	
	mTunneler::Tunnels.insert(tunnel->id(), tunnel);
	return true;
}

bool Network::Tunneler::unregisterTunnel(Tunnel *tunnel)
{
	Synchronize(this);
	
	if(!handler)
		return false;
	
	Tunneler::Tunnel *t = NULL;
	if(!mTunneler::Tunnels.get(tunnel->id(), t) || t != tunnel)
		return false;
	
	mTunneler::Tunnels.erase(tunnel->id());
		return true;	
}

bool Network::Tunneler::handshake(SecureTransport *transport, const Identifier &local, const Identifier &remote, bool async)
{
	class MyVerifier : public SecureTransport::Verifier
	{
	public:
		Identifier local, remote;
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
				SecureTransport::Credentials *creds = user->certificate();
				if(creds) transport->addCredentials(creds);
			}
			else {
				 LogDebug("Network::Tunneler::handshake", String("User does not exist: ") + name);
			}
			
			return true;	// continue handshake anyway
		}
		
		bool verifyCertificate(const Rsa::PublicKey &pub)
		{
			publicKey = pub;
			remote = Identifier(publicKey.digest());
			
			LogDebug("Network::Tunneler::handshake", String("Verifying remote certificate: ") + remote.toString());
			
			Synchronize(network);
			
			Map<Identifier, Set<Listener*> >::iterator it = network->mListeners.find(remote);
			while(it != network->mListeners.end() && it->first == remote)
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
			
			LogDebug("Network::Tunneler::handshake", "Certificate verification failed");
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
			LogDebug("Network::Tunneler::handshake", "HandshakeTask starting...");
			
			try {
				// Set verifier
				MyVerifier verifier;
				transport->setVerifier(&verifier);
				
				// Do handshake
				transport->handshake();
				Assert(transport->hasCertificate());
				
				if(!local.empty() && local != verifier.local)
					return false;

				if(!remote.empty() && remote != verifier.remote)
					return false;

				// Handshake succeeded
				LogDebug("Network::Tunneler::handshake", "Success");
				
				// TODO: addHandler in Network
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
		Identifier local, remote;
	};
	
	HandshakeTask *task = NULL;
	try {
		if(async)
		{
			task = new HandshakeTask(mNetwork, transport, local, remote);
			mThreadPool.launch(task);
			return true;
		}
		else {
			HandshakeTask stask(mNetwork, transport, local, remote);
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
			SecureTransport *transport = listen();
			if(!transport) break;
			
			LogDebug("Network::Backend::run", "Incoming tunnel");
			
			handshake(transport, Identifier::Null, Identifier::Null, true); // async
		}
	}
	catch(const std::exception &e)
	{
		LogError("Network::Tunneler::run", e.what());
	}
	
	LogWarn("Network::Backend::run", "Closing tunneler");
}

Network::Tunneler::Tunnel::Tunneler::Tunnel(Tunneler *tunneler, uint64_t id) :
	mTunneler(tunneler),
	mId(id),
	mTimeout(DefaultTimeout)
{
	mTunneler->registerTunnel(this);
}

Network::Tunneler::Tunnel::~Tunneler::Tunnel(void)
{
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
	
	const Message &message = mQueue.front();
	size = std::min(size, size_t(message.payload.size()));
	std::copy(message.payload.data(), message.payload.data() + size, buffer);
        mQueue.pop();
        return size;
}

void Network::Tunneler::Tunnel::writeData(const char *data, size_t size)
{
	Message message;
	message.prepare(mLocal, mRemote, Message::Forward, Message::Tunnel);
	message.payload.writeBinary(data, size);
	mNetwork->route(message);
}

bool Network::Tunneler::Tunnel::waitData(double &timeout)
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

bool Network::Tunneler::Tunnel::waitData(const double &timeout)
{
	double dummy = timeout;
	return waitData(dummy);
}

bool Network::Tunneler::Tunnel::isDatagram(void) const
{
	return true; 
}

bool Network::Tunneler::Tunnel::incoming(const Message &datagram)
{
	Synchronize(&mQueueSync);
	mQueue.push(datagram);
	mQueueSync.notifyAll();
	return true;
}

Network::Handler::Handler(Stream *stream) :
	mStream(stream),
	mTokens(0.),
	mRedundancy(1.1)	// TODO
{
	Network::Instance->registerHandler(this);
}

Network::Handler::~Handler(void)
{
	Network::Instance->unregisterHandler(this);	// should be done already
	
	delete mStream;
}

bool Network::Handler::read(String &type, String &content)
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

void Network::Handler::wirte(const String &type, const String &content)
{
	Synchronize(this);
	mSource.write(data, size);
	
	// TODO: tokens
}

void Network::Handler::process(void)
{
	Synchronize(this);
/*
	// TODO: correct sync
	Map<Identifier, Set<Listener*> >::iterator it = mNetwork->mListeners.find(mRemote);
	while(it != mNetwork->mListeners.end() && it->first == mRemote)
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

void Network::Handler::run(void)
{
	try {
		LogDebug("Network::Handler", "Starting handler");
	
		process();
		
		LogDebug("Network::Handler", "Closing handler");
	}
	catch(const std::exception &e)
	{
		LogDebug("Network::Handler", String("Closing handler: ") + e.what());
	}
	
	mNetwork->unregisterHandler(mAddress, this);
	
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}


}
