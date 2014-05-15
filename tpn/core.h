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
	struct Missive : public Serializable
	{
		Missive(void);
		~Missive(void);
		
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
		ByteArray payload;		// max 1 KB
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
		
		virtual bool anounce(const String &prefix, ByteArray &out) = 0;
		
	private:
		StringSet mPublishedPrefixes;
	};
	
	class Subscriber
	{
	public:
		Subscriber(void);
		~Subscriber(void);
		
		void subscribe(const String &prefix);
		void unsubscribe(const String &prefix);
		
		virtual bool beacon(const String &prefix, ByteArray &out) = 0;
		virtual bool incoming(const String &prefix, ByteArray &data) = 0;	// return false to delegate
		
	private:
		StringSet mSubscribedPrefixes;
	};
	
	class Caller
	{
	public:
		Caller(const BinaryString &target);
		~Caller(void);
	
		void start(void);
		void stop(void);
		
	private:
		BinaryString mTarget;
	};
	
	// TODO: deprecated
	class Listener
	{
	public:
		virtual void connected(const Identifier &peering, bool incoming) = 0;
		virtual void disconnected(const Identifier &peering) = 0;
		virtual bool notification(const Identifier &peering, Notification *notification) = 0;
		virtual bool request(const Identifier &peering, Request *request) = 0;
	};
	
	Core(int port);
	~Core(void);
	
	// Global
	String getName(void) const;
	void getAddresses(List<Address> &list) const;
	void getKnownPublicAdresses(List<Address> &list) const;
	bool isPublicConnectable(void) const;
	
	// Peerings
	// TODO: listener is deprecated
	void registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
		       		const BinaryString &secret,
				Listener *listener = NULL);
	void unregisterPeering(const Identifier &peering);
	bool hasRegisteredPeering(const Identifier &peering);
	
	// Publish/Subscribe
	void publish(const String &prefix, Publisher *publisher);
	void unpublish(const String &prefix, Publisher *publisher);
	void subscribe(const String &prefix, Subscriber *subscriber);
	void unsubscribe(const String &prefix, Subscriber *subscriber);
	
	// Caller
	void registerCaller(const BinaryString &target, Caller *splicer);
	void unregisterCaller(const BinaryString &target, Caller *splicer);
	
	// Routing
	void route(Missive &missive, const Identifier &from);
	void broadcast(Missive &missive, const Identifier &from)
	void addRoute(const Identifier &id, const Identifier &route);
	bool getRoute(const Identifier &id, Identifier &route);
	
	// TODO
	void addPeer(Stream *bs, const Address &remoteAddr, const Identifier &peering);
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
		
		bool incoming(Missive &missive);
		
	private:
		Queue<Missive> mQueue;
		Synchronizable mQueueSync;
		
		class TunnelWrapper : public Stream
		{
		public:
			TunnelWrapper(const Identifier &local, const Identifier &remote);
			~TunnelWrapper(void);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			
			bool incoming(Missive &missive);
			
		private:
			Identifier mLocal, mRemote;
			Queue<Missive> mQueue;
			Synchronizable mQueueSync;
		};
	};
	
	class Handler : public Task, public Synchronizable
	{
	public:
		Handler(Core *core, Stream *stream);
		~Handler(void);
		
	private:
		bool recv(Missive &missive);
		void send(const Missive &missive);
		void schedule(const Missive &missive, const Time &time);
		//void cancel(...);
		bool incoming(Missive &missive);
		void route(Missive &missive);

		void process(void);
		void run(void);

		Identifier mLocal, mRemote;
		
		Core	*mCore;
		Stream  *mStream;
		Scheduler mScheduler;
		
		bool mIsIncoming;
		bool mIsAnonymous;
		bool mStopping;
	};

	bool addHandler(const Identifier &peer, Handler *Handler);
	bool removeHandler(const Identifier &peer, Handler *handler);

	Map<String, Set<Publisher*> >  mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	
	String mName;
	Scheduler mThreadPool;
	Scheduler mScheduler;
	Cache mCache;
	
	List<Backend*> mBackends;
	TunnelBackend *mTunnelBackend;
	Map<Identifier, Identifier> mRoutes;
	
	Map<Identifier, Handler*> mHandlers;
	
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, BinaryString> mSecrets;
	Map<Identifier, Listener*> mListeners;
	
	Time mLastPublicIncomingTime;
	Map<Address, int> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif

