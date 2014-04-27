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
class Core : public Thread, protected Synchronizable
{
public:
	static Core *Instance;
	
	// Routing-level message structure
	struct Missive : public Serializable
	{
		Missive(void);
		~Missive(void);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		// Fields
		Identifier source;		// 32 o
		Identifier destination;		// 32 o
		ByteArray descriptor;		// 32 o
		ByteArray data;			// 1 Ko
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
	enum LinkStatus {Disconnected, Established, Authenticated};
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
	void run(void);
	
	class Backend : protected Thread
	{
	public:
		Backend(Core *core);
		virtual ~Backend(void);
		
		virtual SecureTransport *connect(const Address &addr) = 0;
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
		SecureTransport *connect(const Identifier &remote);
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
		Handler(Core *core, Stream *stream, const Address &remoteAddr);
		~Handler(void);

		void setPeering(const Identifier &peering, bool relayed = false);
		void setStopping(void);
		
		void sendNotification(const Notification &notification);
		void addRequest(Request *request);
		void removeRequest(unsigned id);
		
		bool isIncoming(void) const;
		bool isAuthenticated(void) const;
		bool isEstablished(void) const;
		LinkStatus linkStatus(void) const;

	protected:
		static const int NotFound = 1;
		static const int RedirectionExists = 2;
		static const int RedirectionFailed = 3;
		
	  	static void sendCommand(Stream *stream,
				   	const String &command, 
		       			const String &args,
					const StringMap &parameters);
		
		static bool recvCommand(Stream *stream,
				   	String &command, 
		       			String &args,
					StringMap &parameters);
		
	private:
		void clientHandshake(void);
		void serverHandshake(void);
		void process(void);
		void run(void);

		Identifier mPeering, mRemotePeering;
		Core	*mCore;
		Stream  *mStream;
		Address mRemoteAddr;
		bool mIsIncoming;
		bool mIsRelay;
		bool mIsRelayEnabled;
		LinkStatus mLinkStatus;
		Map<unsigned, Request*> mRequests;
		Map<unsigned, Request::Response*> mResponses;
		Set<unsigned> mCancelled;
		ThreadPool mThreadPool;
		bool mStopping;

		BinaryString mObfuscatedHello;
		
		class Sender : public Thread, public Synchronizable
		{
		public:
			Sender(void);
			~Sender(void);

		private:
			static const size_t ChunkSize;
			void run(void);

			struct RequestInfo
			{
				unsigned id;
				String target;
				StringMap parameters;
				bool isData;
			};
			
			Stream *mStream;
			unsigned mLastChannel;
			Map<unsigned, Request::Response*> mTransferts;
			Queue<Notification>	mNotificationsQueue;
			Queue<RequestInfo> 	mRequestsQueue;
			Array<Request*> mRequestsToRespond;
			bool mShouldStop;
			friend class Handler;
		};

		Sender *mSender;
	};

	bool addHandler(const Identifier &peer, Handler *Handler);
	bool removeHandler(const Identifier &peer, Handler *handler);

	Map<Identifier, Set<Publisher*> >  mPublishers;
	Map<Identifier, Set<Subscriber*> > mSubscribers;
	
	String mName;
	ServerSocket mSock;
	ThreadPool mThreadPool;
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, BinaryString> mSecrets;
	Map<Identifier, Listener*> mListeners;
	Map<Identifier, Handler*>  mRedirections;
	Map<Identifier, Handler*> mHandlers;
	Set<String>  mSeenRequests;
	
	unsigned mLastRequest;

	Time mLastPublicIncomingTime;
	Synchronizable mMeetingPoint;
	Map<Address, int> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif
