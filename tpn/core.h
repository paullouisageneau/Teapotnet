/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_CORE_H
#define TPN_CORE_H

#include "tpn/include.h"
#include "tpn/address.h"
#include "tpn/stream.h"
#include "tpn/serversocket.h"
#include "tpn/socket.h"
#include "tpn/pipe.h"
#include "tpn/thread.h"
#include "tpn/mutex.h"
#include "tpn/signal.h"
#include "tpn/identifier.h"
#include "tpn/notification.h"
#include "tpn/request.h"
#include "tpn/synchronizable.h"
#include "tpn/map.h"
#include "tpn/array.h"
#include "tpn/http.h"
#include "tpn/interface.h"

namespace tpn
{

// Core of the TeapotNet node
// Implements the TPN protocol
// This is a singleton class, all users use it.  
class Core : public Thread, protected Synchronizable, public HttpInterfaceable
{
public:
	static Core *Instance;
	
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
		       		const ByteString &secret,
				Listener *listener = NULL);
	void unregisterPeering(const Identifier &peering);
	bool hasRegisteredPeering(const Identifier &peering);
	
	bool addPeer(ByteStream *bs, const Address &remoteAddr, const Identifier &peering, bool async = false);
	bool addPeer(Socket *sock, const Identifier &peering, bool async = false);
	bool hasPeer(const Identifier &peering);
	bool getInstancesNames(const Identifier &peering, Array<String> &array);
	
	bool sendNotification(const Notification &notification);
	unsigned addRequest(Request *request);
	void removeRequest(unsigned id);
	
	void http(const String &prefix, Http::Request &request);

private:
	void run(void);

	class Handler : public Thread, public Synchronizable
	{
	public:
		Handler(Core *core, ByteStream *bs, const Address &remoteAddr);
		~Handler(void);

		void setPeering(const Identifier &peering);
		
		void sendNotification(const Notification &notification);
		void addRequest(Request *request);
		void removeRequest(unsigned id);
		
		bool isIncoming(void) const;
		bool isAuthenticated(void) const;

	protected:
	  	static void sendCommand(Stream *stream,
				   	const String &command, 
		       			const String &args,
					const StringMap &parameters);
		
		static bool recvCommand(Stream *stream,
				   	String &command, 
		       			String &args,
					StringMap &parameters);
		
	private:
		void process(void);
		void run(void);

		Identifier mPeering, mRemotePeering;
		Core	*mCore;
		ByteStream  *mRawStream;
		Stream  *mStream;
		Address mRemoteAddr;
		bool mIsIncoming;
		bool mIsAuthenticated;
		Map<unsigned, Request*> mRequests;
		Map<unsigned, Request::Response*> mResponses;
		Set<unsigned> mCancelled;
		bool mStopping;

		ByteString mObfuscatedHello;
		
		class Sender : public Thread, public Synchronizable
		{
		public:
			Sender(void);
			~Sender(void);

		private:
			static const size_t ChunkSize;

			void run(void);

			Stream *mStream;
			unsigned mLastChannel;
			Map<unsigned, Request::Response*> mTransferts;
			Queue<Notification>	mNotificationsQueue;
			Queue<Request*> mRequestsQueue;
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
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, ByteString> mSecrets;
	Map<Identifier, Listener*> mListeners;
	Map<Identifier, Handler*>  mRedirections;
	Map<Identifier, Handler*> mHandlers;
	
	Map<unsigned, Request*> mRequests;
	unsigned mLastRequest;

	Time mLastPublicIncomingTime;
	Synchronizable mMeetingPoint;
	Map<Address, int> mKnownPublicAddresses;
	
	friend class Handler;
};

}

#endif
