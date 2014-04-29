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
#include "tpn/request.h"
#include "tpn/threadpool.h"
#include "tpn/synchronizable.h"
#include "tpn/map.h"
#include "tpn/array.h"
#include "tpn/http.h"
#include "tpn/interface.h"

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
		
		void prepare(const Identifier &source, const Identifier &target);
		void clear(void);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		// Fields
		Identifier source;		// 32 o
		Identifier target;		// 32 o
		ByteArray data;			// 1 Ko
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
		
		void publish(const Identifier &id);
		void unpublish(const Identifier &id);
		
		void outgoing(const Missive &missive);
		
	private:
		Set<Identifier> mPublishedIds;
	};
	
	class Subscriber
	{
	public:
		Subscriber(void);
		~Subscriber(void);
		
		void subscribe(const Identifier &id);
		void unsubscribe(const Identifier &id);
		
		virtual bool incoming(Missive &missive) = 0;	// return false to delegate
		
	private:
		Set<Identifier> mSubscribedIds;
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
	void publish(const Identifier &id, Publisher *publisher);
	void unpublish(const Identifier &id, Publisher *publisher);
	void subscribe(const Identifier &id, Subscriber *subscriber);
	void unsubscribe(const Identifier &id, Subscriber *subscriber);
	
	// TODO
	LinkStatus addPeer(Stream *bs, const Address &remoteAddr, const Identifier &peering, bool async = false);
	LinkStatus addPeer(Socket *sock, const Identifier &peering, bool async = false);
	bool hasPeer(const Identifier &peering);
	bool getInstancesNames(const Identifier &peering, Array<String> &array);
	
	// TODO: deprecated
	bool sendNotification(const Notification &notification);
	unsigned addRequest(Request *request);
	void removeRequest(unsigned id);

private:
	bool isRequestSeen(const Request *request);
	
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
	
	class TunnelBackend : public Backend, public Subscriber
	{
	public:
		TunnelBackend(void);
		~TunnelBackend(void);
		
		// Backend
		SecureTransport *connect(const Locator &locator);
		SecureTransport *listen(void);
		
		// Subscriber
		bool incoming(Missive &missive);
		
	private:
		Queue<Missive> mQueue;
		Synchronizable mQueueSync;
		
		class TunnelWrapper : public Stream, public Subscriber, public Publisher
		{
		public:
			TunnelWrapper(const Identifier &local, const Identifier &remote);
			~TunnelWrapper(void);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			
			// Subscriber
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

		void process(void);
		void run(void);

		Identifier mLocal, mRemote;
		
		Core	*mCore;
		Stream  *mStream;
		ThreadPool mThreadPool;
		
		bool mIsIncoming;
		bool mIsAnonymous;
		bool mStopping;

		class Sender : public Thread, public Synchronizable
		{
		public:
			Sender(void);
			~Sender(void);

		private:
			void run(void);
			
			Stream *mStream;
			bool mShouldStop;
			
			friend class Handler;
		};

		Sender *mSender;
	};

	bool addHandler(const Identifier &peer, Handler *Handler);
	bool removeHandler(const Identifier &peer, Handler *handler);

	// Routing
	void addRoute(const Identifier &id, const Identifier &route);
	bool getRoute(const Identifier &id, Identifier &route);

	Map<Identifier, Set<Publisher*> >  mPublishers;
	Map<Identifier, Set<Subscriber*> > mSubscribers;
	
	String mName;
	ThreadPool mThreadPool;
	
	List<Backend*> mBackends;
	Map<Identifier, Identifier> mRoutes;
	
	Map<Identifier, Handler*> mHandlers;
	
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, BinaryString> mSecrets;
	Map<Identifier, Listener*> mListeners;
	
	Set<String>  mSeenRequests;
	
	unsigned mLastRequest;

	Time mLastPublicIncomingTime;
	Synchronizable mMeetingPoint;
	Map<Address, int> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif

