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
#include "tpn/notification.h"
#include "tpn/interface.h"
#include "tpn/cache.h"
#include "tpn/identifier.h"

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
	
	// Routing-level message structure
	struct Message : public Serializable
	{
		// type
		static const uint8_t Forward   = 0;
		static const uint8_t Broadcast = 1;
		static const uint8_t Lookup    = 2;
		
		// content
		static const uint8_t Empty     = 0;
		static const uint8_t Tunnel    = 1;
		static const uint8_t Notify    = 2;
		static const uint8_t Ack       = 3;
		static const uint8_t Call      = 4;
		static const uint8_t Data      = 5;
		static const uint8_t Cancel    = 6;
		static const uint8_t Publish   = 7;
		static const uint8_t Subscribe = 8;
		
		Message(void);
		~Message(void);
		
		void prepare(const Identifier &source, const Identifier &destination, 
				uint8_t type = Forward, uint8_t content = Empty);
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

	struct Locator
	{
		Locator(User *user, const Identifier &id);
		Locator(User *user, const Address &addr);
		Locator(User *user, const Set<Address> &addrs);
		~Locator(void);
		
		User *user;			// local user
		Identifier	identifier;	// remote identifier
		Identifier	peering;	// remote peering identifier for PSK
		BinaryString	secret;		// secret for PSK
		Set<Address>	addresses;	// adresses for direct connection
	};

	class Publisher
	{
	public:
		Publisher(void);
		~Publisher(void);
		
		void publish(const String &prefix);
		void publish(const String &prefix, const String &path, const BinaryString &target);
		void unpublish(const String &prefix);
		
		virtual bool anounce(const Identifier &peer, const String &prefix, const String &path, BinaryString &target) = 0;
		
	private:
		StringSet mPublishedPrefixes;
	};
	
	class Subscriber
	{
	public:
		Subscriber(const Identifier &peer = Identifier::Null);
		~Subscriber(void);
		
		void subscribe(const String &prefix);
		void unsubscribe(const String &prefix);
		
		virtual bool incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target) = 0;
		
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
		virtual bool auth(const Identifier &peer, BinaryString &secret)         { return false; }
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
	bool connect(const Locator &locator);
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
	void advertise(String prefix, const String &path, const BinaryString &target);
	
	// Notification
	void broadcast(const Notification &notification);
	bool send(const Identifier &peer, const Notification &notification);
	
	// Routing
	void route(const Message &message, const Identifier &from = Identifier::Null);
	void broadcast(const Message &message, const Identifier &from = Identifier::Null);
	bool send(const Message &message, const Identifier &to);
	void addRoute(const Identifier &id, const Identifier &route);
	bool getRoute(const Identifier &id, Identifier &route);
	
	bool addPeer(Stream *bs, const Identifier &local, const Identifier &remote);
	bool hasPeer(const Identifier &remote);
	
private:
	class Backend : public Thread
	{
	public:
		Backend(Core *core);
		virtual ~Backend(void);
		
		virtual bool connect(const Locator &locator) = 0;
		virtual SecureTransport *listen(void) = 0;
		
		virtual void getAddresses(Set<Address> &set) const { set.clear(); }
		
	protected:
		bool process(SecureTransport *transport, const Locator &locator);	// do the client handshake
	
		Core *mCore;
	
	private:
		bool handshake(SecureTransport *transport, const Identifier &local, const Identifier &remote, bool async = false);
		void run(void);
		
		ThreadPool mThreadPool;
		
		SecureTransportClient::Anonymous	mAnonymousClientCreds;
		SecureTransportServer::Anonymous	mAnonymousServerCreds;
		SecureTransportServer::PrivateSharedKey	mPrivateSharedKeyServerCreds;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(Core *core, int port);
		~StreamBackend(void);
		
		bool connect(const Locator &locator);
		bool connect(const Address &addr, const Locator &locator);
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
		
		bool connect(const Locator &locator);
		bool connect(const Address &addr, const Locator &locator);
		SecureTransport *listen(void);
		
		void getAddresses(Set<Address> &set) const;
		
	private:
		DatagramSocket mSock;
	};
	
	class TunnelBackend : public Backend
	{
	public:
		static const double DefaultTimeout = 10.;

		TunnelBackend(Core *core);
		~TunnelBackend(void);
		
		// Backend
		bool connect(const Locator &locator);
		SecureTransport *listen(void);
		
		bool incoming(const Message &message);
		
	private:
		// Queue for listen
		Queue<Message> mQueue;
		Synchronizable mQueueSync;
		
		class TunnelWrapper : public Stream
		{
		public:
			TunnelWrapper(Core *core, const Identifier &local, const Identifier &remote);
			~TunnelWrapper(void);
			
			void setTimeout(double timeout);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			bool waitData(double &timeout);
			bool waitData(const double &timeout);
			bool isDatagram(void) const;
			
			bool incoming(const Message &message);
	
		private:
			Core *mCore;
			Identifier mLocal, mRemote;
			Queue<Message> mQueue;
                	Synchronizable mQueueSync;
			double mTimeout;
		};

		Map<IdentifierPair, TunnelWrapper*> mWrappers;
	};
	
	class Handler : public Task, protected Synchronizable
	{
	public:
		Handler(Core *core, Stream *stream, const Identifier &local, const Identifier &remote);
		~Handler(void);
		
		Identifier local(void) const;
		Identifier remote(void) const;
		
		// Notify
		void notify(const Identifier &id, Stream &payload, bool ack = true);
		
		class Sender : public Task, protected Synchronizable
		{
		public:
			Sender(Handler *handler, const BinaryString &destination);
			~Sender(void);
			
			void addTarget(const BinaryString &target, unsigned tokens = 1);
			void removeTarget(const BinaryString &target);
			void addTokens(unsigned tokens);
			void removeTokens(unsigned tokens);
			bool empty(void) const;
			void run(void);
			
			void notify(Stream &payload, bool ack = true);
			void ack(Stream &payload);
			void acked(Stream &payload);
			
		private:
			Handler *mHandler;
			BinaryString mDestination;
			Map<BinaryString, unsigned> mTargets;
			BinaryString mNextTarget;
			unsigned mTokens;
			
			class SendTask : public Task
			{
			public:
				SendTask(Sender *sender, uint32_t sequence, const Message &message, double delay, int count);
				~SendTask(void);
				void run(void);
				
			private:
				Sender *mSender;
				Message mMessage;
				int mLeft;
				uint32_t mSequence;
			};
			
			Scheduler mScheduler;
			Map<uint32_t, SendTask> mUnacked;
			uint32_t mCurrentSequence;
		};
		
	private:
		bool recv(Message &message);
		void send(const Message &message);
		void route(const Message &message);
		bool incoming(const Message &message);
		void outgoing(const Identifier &dest, uint8_t type, uint8_t content, Stream &payload);

		void process(void);
		void run(void);

		Core	*mCore;
		Stream  *mStream;
		Mutex	mStreamReadMutex, mStreamWriteMutex;

		Identifier mLocal, mRemote;
		
		Map<BinaryString, Sender*> mSenders;
		
		Runner mRunner;
		
		bool mIsIncoming;
		bool mIsAnonymous;
		bool mStopping;
		
		friend class Core;
	};

	bool addHandler(const Identifier &peer, Handler *Handler);
	bool removeHandler(const Identifier &peer, Handler *handler);
	
	void outgoing(uint8_t type, uint8_t content, Stream &payload);
	void outgoing(const Identifier &dest, uint8_t type, uint8_t content, Stream &payload);
	
	bool matchPublishers(const String &path, const Identifier &source, Subscriber *subscriber = NULL);
	bool matchSubscribers(const String &path, const Identifier &source, const List<BinaryString> &targets);
	
	uint64_t mNumber;
	String mName;
	ThreadPool mThreadPool;
	Scheduler mScheduler;
	
	List<Backend*> mBackends;
	TunnelBackend *mTunnelBackend;
	Map<Identifier, Identifier> mRoutes;
	
	Map<Identifier, Handler*> mHandlers;
	
	Map<String, Set<Publisher*> > mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<Identifier, Set<Listener*> > mListeners;
	
	Time mLastPublicIncomingTime;
	Map<Address, Time> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif

