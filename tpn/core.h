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
#include "tpn/serversocket.h"
#include "tpn/socket.h"
#include "tpn/datagramsocket.h"
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
	
	class Tunnel : public Stream
	{
	public:
		~Tunnel(void);
		
		// Stream
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		
	protected:
		Tunnel(void);
		
	private:
		BinaryString mLocal;
		BinaryString mRemote;
	};
	
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
	
	String getName(void) const;
	void getAddresses(List<Address> &list) const;
	void getKnownPublicAdresses(List<Address> &list) const;
	bool isPublicConnectable(void) const;
	
	void registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
		       		const BinaryString &secret,
				Listener *listener = NULL);
	void unregisterPeering(const Identifier &peering);
	bool hasRegisteredPeering(const Identifier &peering);
	
	enum LinkStatus {Disconnected, Established, Authenticated};
	LinkStatus addPeer(Stream *bs, const Address &remoteAddr, const Identifier &peering, bool async = false);
	LinkStatus addPeer(Socket *sock, const Identifier &peering, bool async = false);
	bool hasPeer(const Identifier &peering);
	bool getInstancesNames(const Identifier &peering, Array<String> &array);
	
	bool sendNotification(const Notification &notification);
	unsigned addRequest(Request *request);
	void removeRequest(unsigned id);

private:
	bool isRequestSeen(const Request *request);
	void run(void);
	
	// Magic header for incoming connections
	struct Magic
	{
		Magic(void);
		Magic(uint8_t mode);
		~Magic(void);
		
		bool read(Stream &s);
		void write(Stream &s) const;
		
		// Fields
		uint8_t major;
		uint8_t minor;
		uint8_t revision;
		uint8_t mode;
	};
	
	class Backend : protected Thread
	{
	public:
		Backend(void);
		virtual ~Backend(void);
		
		virtual void connect(const Address &addr) = 0;
		virtual void listen(void) = 0;
		
		void launch(Core *core);
		void addIncoming(Stream *stream);	// Push the new stream to the core
		
	protected:
		void run(void);
		
	private:
		Core *mCore;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(int port);
		~StreamBackend(void);
		
		void connect(const Address &addr);
		void listen(void);
		
	private:
		ServerSocket mSock;
	};
	
	class DatagramBackend : public Backend
	{
	public:
		DatagramBackend(int port);
		~DatagramBackend(void);
		
		void connect(const Address &addr);
		void listen(void);
		
	private:
		DatagramSocket mSock;
	};
	
	// Routing-level datagram structure
	struct Datagram : public Serializable
	{
		Datagram(void);
		~Datagram(void);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		// Fields
		Identifier source;		// 32 o
		Identifier destination;		// 32 o
		ByteArray descriptor;		// 32 o
		ByteArray data;			// 1 Ko
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
