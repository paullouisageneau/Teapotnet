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
		
		ByteArray descriptor;		// max 256 B
		ByteArray payload;		// max 1 KB TODO
	};

	struct Locator
	{
		Locator(const Identifier &id);
		Locator(const Address &addr);
		~Locator(void);
	
		Identifier	identifier;
		List<Address>	addresses;
	};

	class Publisher
	{
	public:
		Publisher(void);
		~Publisher(void);
		
		void publish(const String &prefix);
		void unpublish(const String &prefix);
		
		virtual bool anounce(const String &prefix, BinaryString &target) = 0;
		
	private:
		StringSet mPublishedPrefixes;
	};
	
	class Subscriber
	{
	public:
		Subscriber(const Identifier &peer);
		~Subscriber(void);
		
		void subscribe(const String &prefix);
		void unsubscribe(const String &prefix);
		
		virtual bool incoming(const String &prefix, const BinaryString &target) = 0;	// return false to delegate
		
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
		virtual void seen(const Identifier &identifier) = 0;
		virtual bool notification(const Identifier &identifier, Stream &payload) = 0;
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
	void registerListener(Listener *listener);
	void unregisterListener(Listener *listener);
	
	// Publish/Subscribe
	void publish(const String &prefix, Publisher *publisher);
	void unpublish(const String &prefix, Publisher *publisher);
	void subscribe(const Identifier &peer, const String &prefix, Subscriber *subscriber);
	void unsubscribe(const Identifier &peer, const String &prefix, Subscriber *subscriber);
	
	// Routing
	void route(Message &message, const Identifier &from);
	void broadcast(Message &message, const Identifier &from);
	void addRoute(const Identifier &id, const Identifier &route);
	bool getRoute(const Identifier &id, Identifier &route);
	
	// TODO
	bool addPeer(Socket *sock, const Identifier &id);
	bool addPeer(Stream *bs, const Address &remoteAddr, const Identifier &id);
	bool hasPeer(const Identifier &peering);
	bool getInstancesNames(const Identifier &peering, Array<String> &array);
	
private:
	class Backend : protected Thread
	{
	public:
		Backend(Core *core);
		virtual ~Backend(void);
		
		virtual SecureTransport *connect(const Locator &locator) = 0;
		virtual SecureTransport *listen(void) = 0;
		
	protected:
		void addIncoming(Stream *stream);	// Push the new stream to the core
		void run(void);
		
		Core *mCore;
		ThreadPool mThreadPool;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(int port);
		~StreamBackend(void);
		
		SecureTransport *connect(const Locator &locator);
		SecureTransport *connect(const Address &addr);
		SecureTransport *listen(void);
		
	private:
		ServerSocket mSock;
	};
	
	class DatagramBackend : public Backend
	{
	public:
		DatagramBackend(int port);
		~DatagramBackend(void);
		
		SecureTransport *connect(const Locator &locator);
		SecureTransport *connect(const Address &addr);
		SecureTransport *listen(void);
		
	private:
		DatagramSocket mSock;
	};
	
	class TunnelBackend : public Backend
	{
	public:
		TunnelBackend(void);
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
		
		// Publish/Subscribe
		void publish(const String &prefix, Publisher *publisher);
		void unpublish(const String &prefix, Publisher *publisher);
		void subscribe(const String &prefix, Subscriber *subscriber);
		void unsubscribe(const String &prefix, Subscriber *subscriber);
		
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

		Map<String, Set<Publisher*> >  mPublishers;
		Map<String, Set<Subscriber*> > mSubscribers;
		Map<BinaryString, Sender*> mSenders;
		
		Runner mRunner;
		
		bool mIsIncoming;
		bool mIsAnonymous;
		bool mStopping;
	};

	bool addHandler(const Identifier &peer, Handler *Handler);
	bool removeHandler(const Identifier &peer, Handler *handler);

	String mName;
	Scheduler mThreadPool;
	Scheduler mScheduler;
	
	List<Backend*> mBackends;
	TunnelBackend *mTunnelBackend;
	Map<Identifier, Identifier> mRoutes;
	
	Map<Identifier, Handler*> mHandlers;
	
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, BinaryString> mSecrets;
	Set<Listener*> mListeners;
	
	Time mLastPublicIncomingTime;
	Map<Address, int> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif

