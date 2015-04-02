/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef TPN_CORE_H
#define TPN_CORE_H

#include "tpn/include.h"
#include "tpn/identifier.h"
#include "tpn/fountain.h"
#include "tpn/notification.h"

#include "pla/address.h"
#include "pla/stream.h"
#include "pla/bytearray.h"
#include "pla/binarystring.h"
#include "pla/string.h"
#include "pla/socket.h"
#include "pla/serversocket.h"
#include "pla/datagramsocket.h"
#include "pla/securetransport.h"
#include "pla/pipe.h"
#include "pla/thread.h"
#include "pla/mutex.h"
#include "pla/signal.h"
#include "pla/threadpool.h"
#include "pla/scheduler.h"
#include "pla/runner.h"
#include "pla/synchronizable.h"
#include "pla/map.h"
#include "pla/array.h"
#include "pla/http.h"

namespace tpn
{

class User;
  
// Core of the Teapotnet node
// Implements the TPN protocol
// This is a singleton class, all users use it.  
class Core : protected Synchronizable
{
public:
	static Core *Instance;
	
	struct Message : public Serializable
	{
		Message(void);
		~Message(void);
		
		void prepare(const Identifier &source, const Identifier &destination);
		void clear(void);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		// Fields
		uint8_t version;
		uint8_t flags;
		uint8_t type;
		uint8_t content;
		uint16_t hops;
		
		Identifier source;		// 40 B
		Identifier destination;		// 40 B
		BinaryString payload;
	};
	
	class Publisher
	{
	public:
		Publisher(const Identifier &peer = Identifier::Null);	// local peer
		~Publisher(void);
		
		void publish(const String &prefix, const String &path = "/");
		void unpublish(const String &prefix);
		
		virtual bool anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets) = 0;
		
	private:
		Identifier mPeer;
		StringSet mPublishedPrefixes;
	};
	
	class Subscriber
	{
	public:
		Subscriber(const Identifier &peer = Identifier::Null);	// remote peer
		~Subscriber(void);
		
		void subscribe(const String &prefix);
		void unsubscribe(const String &prefix);
		void unsubscribeAll(void);
		
		virtual bool incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target) = 0;
		virtual Identifier remote(void) const;
		virtual bool localOnly(void) const;
		
	protected:
		bool fetch(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target);
		
	private:
		Identifier mPeer;
		StringSet mSubscribedPrefixes;
		ThreadPool mThreadPool;
	};
	
	class Caller : protected Synchronizable
	{
	public:
		Caller(void);
		Caller(const BinaryString &target);
		~Caller(void);
		
		void startCalling(const BinaryString &target);
		void stopCalling(void);
		
	private:
		BinaryString mTarget;
	};
	
	class Listener
	{
	public:
		Listener(void);
		~Listener(void);
		
		void listen(const Identifier &peer);
		
		virtual void seen(const Identifier &peer) {}
		virtual void connected(const Identifier &peer) {}
		virtual bool recv(const Identifier &peer, const Notification &notification) = 0;
		virtual bool auth(const Identifier &peer, const Rsa::PublicKey &pubKey) { return false; }
		
	private:
		Set<Identifier> mPeers;
	};
	
	Core(int port);
	~Core(void);
	
	void start(void);
	void join(void);
	
	// Global
	uint64_t getNumber(void) const;
	String getName(void) const;
	void getAddresses(Set<Address> &set) const;
	void getKnownPublicAdresses(Set<Address> &set) const;
	bool isPublicConnectable(void) const;
	
	// Connections
	bool connect(const Set<Address> &addrs);
	int connectionsCount(void) const;
	
	// Caller
	void registerCaller(const BinaryString &target, Caller *caller);
	void unregisterCaller(const BinaryString &target, Caller *caller);
	void unregisterAllCallers(const BinaryString &target);
	
	// Listener
	void registerListener(const Identifier &id, Listener *listener);
	void unregisterListener(const Identifier &id, Listener *listener);
	
	// Publish/Subscribe
	void publish(String prefix, Publisher *publisher);
	void unpublish(String prefix, Publisher *publisher);
	void subscribe(String prefix, Subscriber *subscriber);
	void unsubscribe(String prefix, Subscriber *subscriber);
	void advertise(String prefix, const String &path, const Identifier &source, Publisher *publisher);
	void addRemoteSubscriber(const Identifier &peer, const String &path, bool publicOnly);
	
	// Notification
	bool broadcast(const Identifier &local, const Notification &notification);
	bool send(const Identifier &local, const Identifier &remote, const Notification &notification);
	
	// Routing
	bool route(const Message &message, const Identifier &from = Identifier::Null);
	bool broadcast(const Message &message, const Identifier &from = Identifier::Null);
	bool send(const Message &message, const Identifier &to);
	void addRoute(const Identifier &id, const Address &route);
	bool getRoute(const Identifier &id, const Address &route);
	
	bool addLink(Stream *stream, const Identifier &local, const Identifier &remote);
	bool hasLink(const Identifier &local, const Identifier &remote);
	
private:
	class RemotePublisher : public Publisher
	{
	public:
		RemotePublisher(const List<BinaryString> targets);
		~RemotePublisher(void);
		
		bool anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets);
		
	private:
		List<BinaryString> mTargets;
	};
	
	class RemoteSubscriber : public Subscriber
	{
	public:
		RemoteSubscriber(const Identifier &remote = Identifier::Null, bool publicOnly = false);
		~RemoteSubscriber(void);
		
		bool incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target);
		Identifier remote(void) const;
		bool localOnly(void) const;
		
	private:
		Identifier mRemote;
		bool mPublicOnly;
	};
	
	class Backend : public Thread
	{
	public:
		Backend(Core *core);
		virtual ~Backend(void);
		
		virtual bool connect(const Set<Address> &addrs) = 0;
		virtual SecureTransport *listen(void) = 0;
		
		virtual void getAddresses(Set<Address> &set) const { set.clear(); }
		
	protected:
		bool process(SecureTransport *transport, const Set<Address> &addrs);	// do the client handshake
	
	private:
		bool handshake(SecureTransport *transport, bool async = false);
		void run(void);
		
		ThreadPool mThreadPool;
		
		SecureTransportClient::Anonymous	mAnonymousClientCreds;
		SecureTransportServer::Anonymous	mAnonymousServerCreds;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(Core *core, int port);
		~StreamBackend(void);
		
		bool connect(const Set<Address> &addrs);
		bool connect(const Address &addr);
		SecureTransport *listen(void);
		
		void getAddresses(Set<Address> &set) const;
		
	private:
		ServerSocket mSock;
	};
	
	class DatagramBackend : public Backend
	{
	public:
		DatagramBackend(Core *core, int port);
		~DatagramBackend(void);
		
		bool connect(const Set<Address> &addrs);
		bool connect(const Address &addr);
		SecureTransport *listen(void);
		
		void getAddresses(Set<Address> &set) const;
		
	private:
		DatagramSocket mSock;
	};
	
	class Handler : public Task, protected Synchronizable
	{
	public:
		Handler(Core *core, Stream *stream, ThreadPool *pool, const Address &addr);
		~Handler(void);
		
		bool recv(Message &message);
		bool send(const Message &message);
		
	private:
		void process(void);
		void run(void);
		
		Stream  *mStream;
		Address mAddress;
	};
	
	class Tunneler : public Thread
	{
	public:
		static const double DefaultTimeout = 60.;
	
		Tunneler(void);
		~Tunneler(void);
		
		bool open(const Identifier &identifier, User *user);
		bool incoming(const Message &message);
		
	private:
		class Tunnel : public Stream
		{
		public:
			Tunnel(Tunneler *tunneler, const Identifier &local, const Identifier &remote);
			~Tunnel(void);
			
			Identifier local(void) const;
			Identifier remote(void) const;
			
			void setTimeout(double timeout);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			bool waitData(double &timeout);
			bool waitData(const double &timeout);
			bool isDatagram(void) const;
			
			bool incoming(const Message &message);
			
		private:
			Tunneler *tunneler;
			Identifier mLocal, mRemote;
			Queue<Message> mQueue;
			Synchronizable mQueueSync;
			double mTimeout;
		};
		
		bool registerTunnel(Tunnel *tunnel);
		bool unregisterTunnel(Tunnel *tunnel);
		
		SecureTransport *listen(void);
		bool handshake(SecureTransport *transport, const Identifier &local, const Identifier &remote, bool async = false);
		void run(void);
		
		Map<IdentifierPair, Tunnel*> mTunnels;
		ThreadPool mThreadPool;
		
		// Queue for listen
		Queue<Message> mQueue;
		Synchronizable mQueueSync;
	};
	
	class Link : protected Synchronizable, public Task
	{
	public:
		Link(Stream *stream);
		~Link(void);
		
		bool read(String &type, String &content);
		void wirte(const String &type, const String &content);
		
	private:
		void process(void);
		void run(void);
		
		Stream *mStream;
		Fountain::DataSource 	mSource;
		Fountain::Sink 		mSink;
		double mTokens;
		double mRedundancy;
	};

	bool registerHandler(const Address &addr, Handler *Handler);
	bool unregisterHandler(const Address &addr, Handler *handler);
	bool registerLink(const Identifier &local, const Identifier &remote, Link *link);
	bool unregisterLink(const Identifier &local, const Identifier &remote, Link *link);
	
	bool outgoing(const Identifier &local, const Identifier &remote, const String &type, const Serializable &content);
	bool incoming(const Identifier &local, const Identifier &remote, const String &type, Serializer &serializer);
	
	bool matchPublishers(const String &path, const Identifier &source, Subscriber *subscriber = NULL);
	bool matchSubscribers(const String &path, const Identifier &source, Publisher *publisher);
	
	bool track(const String &tracker, Set<Address> &result);
	
	uint64_t mNumber;
	String mName;
	ThreadPool mThreadPool;
	
	Tunneler *mTunneler;
	List<Backend*> mBackends;
	Map<Identifier, Address> mRoutes;
	
	Map<Address, Handler*> mHandlers;
	Map<IdentifierPair, Link*> mLinks;
	
	Map<String, Set<Publisher*> > mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<Identifier, Set<Listener*> > mListeners;
	List<RemoteSubscriber> mRemoteSubscribers;
	
	Time mLastPublicIncomingTime;
	Map<Address, Time> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif

