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
#include "tpn/address.h"
#include "tpn/stream.h"
#include "tpn/bytearray.h"
#include "tpn/binarystring.h"
#include "tpn/string.h"
#include "tpn/socket.h"
#include "tpn/serversocket.h"
#include "tpn/datagramsocket.h"
#include "tpn/securetransport.h"
#include "tpn/pipe.h"
#include "tpn/thread.h"
#include "tpn/mutex.h"
#include "tpn/signal.h"
#include "tpn/identifier.h"
#include "tpn/notification.h"
#include "tpn/threadpool.h"
#include "tpn/scheduler.h"
#include "tpn/runner.h"
#include "tpn/synchronizable.h"
#include "tpn/map.h"
#include "tpn/array.h"
#include "tpn/http.h"
#include "tpn/interface.h"
#include "tpn/cache.h"

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
		static const uint8_t Empty = 0;
		static const uint8_t Tunnel = 1;
		static const uint8_t Notify = 2;
	
		Message(void);
		~Message(void);
		
		void prepare(const Identifier &source, const Identifier &destination);
		void clear(void);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		// Fields
		uint8_t version;
		uint8_t type;
		uint8_t content;
		uint16_t hops;
		
		Identifier source;		// 32 B
		Identifier destination;		// 32 B
		ByteArray payload;		// 32 B + 1 KB
	};

	struct Locator
	{
		Locator(User *user, const Identifier &id);
		Locator(User *user, const Address &addr);
		~Locator(void);
		
		User *user;
		Identifier	identifier;
		Identifier	peering;
		BinaryString	secret;
		List<Address>	addresses;
	};

	class Publisher
	{
	public:
		Publisher(void);
		~Publisher(void);
		
		void publish(const String &prefix);
		void unpublish(const String &prefix);
		
		virtual bool anounce(const Identifier &peer, const String &path, BinaryString &target) = 0;
		
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
		
		virtual bool incoming(const String &path, const BinaryString &target) = 0;
		
	private:
		Identifier mPeer;
		StringSet mSubscribedPrefixes;
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
		
		virtual void seen(const Identifier &peer) = 0;
		virtual bool recv(const Identifier &peer, const Notification &notification) = 0;
		
	private:
		Set<Identifier> mPeers;
	};
	
	Core(int port);
	~Core(void);
	
	// Global
	String getName(void) const;
	void getAddresses(List<Address> &list) const;
	void getKnownPublicAdresses(List<Address> &list) const;
	bool isPublicConnectable(void) const;
	
	// Peerings
	void registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
				const BinaryString &secret);
	void unregisterPeering(const Identifier &peering);
	bool hasRegisteredPeering(const Identifier &peering);
	
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
	bool subscribe(const Identifier &peer, const String &prefix, Subscriber *subscriber);
	bool unsubscribe(const Identifier &peer, const String &prefix, Subscriber *subscriber);
	
	// Notification
	void broadcast(const Notification &notification);
	bool send(const Identifier &peer, const Notification &notification);
	
	// Routing
	void route(Message &message, const Identifier &from = Identifier::Null);
	void broadcast(Message &message, const Identifier &from = Identifier::Null);
	bool send(Message &message, const Identifier &to);
	void addRoute(const Identifier &id, const Identifier &route);
	bool getRoute(const Identifier &id, Identifier &route);
	
	bool addPeer(Stream *bs, const Identifier &id);
	bool hasPeer(const Identifier &id);
	bool getInstancesNames(const Identifier &peering, Array<String> &array);
	
private:
	class Backend : public Thread
	{
	public:
		Backend(Core *core);
		virtual ~Backend(void);
		
		virtual SecureTransport *connect(const Locator &locator) = 0;
		virtual SecureTransport *listen(void) = 0;
		
		virtual void getAddresses(Set<Address> &set) const {}
		
	protected:
		void process(SecureTransport *transport, const Locator &locator);	// do the client handshake
		
	private:
		void doHandshake(SecureTransport *transport, const Identifier &remote);
		void run(void);
		
		Core *mCore;
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
		
		SecureTransport *connect(const Locator &locator);
		SecureTransport *connect(const Address &addr, const Locator &locator);
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
		
		SecureTransport *connect(const Locator &locator);
		SecureTransport *connect(const Address &addr, const Locator &locator);
		SecureTransport *listen(void);
		
		void getAddresses(Set<Address> &set) const;
		
	private:
		DatagramSocket mSock;
	};
	
	class TunnelBackend : public Backend
	{
	public:
		TunnelBackend(Core *core);
		~TunnelBackend(void);
		
		// Backend
		SecureTransport *connect(const Locator &locator);
		SecureTransport *listen(void);
		
		bool incoming(Message &message);
		
	private:
		Queue<Message> mQueue;
		Synchronizable mQueueSync;
		
		class TunnelWrapper : public Stream
		{
		public:
			TunnelWrapper(const Identifier &local, const Identifier &remote);
			~TunnelWrapper(void);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			
			bool incoming(Message &message);
			
		private:
			Identifier mLocal, mRemote;
			Queue<Message> mQueue;
			Synchronizable mQueueSync;
		};
	};
	
	class Handler : public Task, protected Synchronizable
	{
	public:
		Handler(Core *core, Stream *stream);
		~Handler(void);
		
		Identifier local(void) const;
		Identifier remote(void) const;
		
		// Subscribe
		void subscribe(String prefix, Subscriber *subscriber);
		void unsubscribe(String prefix, Subscriber *subscriber);
		
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
				SendTask(Sender *sender, uint32_t sequence, Message message, double delay, int count);
				~SendTask(void);
				void run(void);
				
			private:
				Sender *mSender;
				Message mMissve;
				int mLeft;
				uint32_t sequence;
			};
			
			Scheduler mScheduler;
			Map<uint32_t, SendTask> mUnacked;
			uint32_t mCurrentSequence;
		};
		
	private:
		bool recv(Message &message);
		void send(const Message &message);
		bool incoming(const Identifier &source, uint8_t content, Stream &payload);
		void outgoing(const Identifier &dest, uint8_t content, Stream &payload);
		void route(Message &message);

		void process(void);
		void run(void);

		Identifier mLocal, mRemote;
		
		Core	*mCore;
		Stream  *mStream;
		Mutex	mStreamReadMutex, mStreamWriteMutex;

		Map<String, Set<Subscriber*> > mSubscribers;
		Map<BinaryString, Sender*> mSenders;
		
		Runner mRunner;
		
		bool mIsIncoming;
		bool mIsAnonymous;
		bool mStopping;
		
		friend class Core;
	};

	bool addHandler(const Identifier &peer, Handler *Handler);
	bool removeHandler(const Identifier &peer, Handler *handler);

	String mName;
	ThreadPool mThreadPool;
	Scheduler mScheduler;
	
	List<Backend*> mBackends;
	TunnelBackend *mTunnelBackend;
	Map<Identifier, Identifier> mRoutes;
	
	Map<Identifier, Handler*> mHandlers;
	
	Map<String, Set<Publisher*> > mPublishers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<Identifier, Set<Listener*> > mListeners;
	
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, BinaryString> mSecrets;
	
	Time mLastPublicIncomingTime;
	Map<Address, int> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif

