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

#ifndef TPOT_CORE_H
#define TPOT_CORE_H

#include "include.h"
#include "address.h"
#include "stream.h"
#include "serversocket.h"
#include "socket.h"
#include "pipe.h"
#include "thread.h"
#include "mutex.h"
#include "signal.h"
#include "identifier.h"
#include "message.h"
#include "request.h"
#include "synchronizable.h"
#include "map.h"
#include "array.h"
#include "http.h"
#include "interface.h"

namespace tpot
{

// Core of the TeapotNet node
// Implements the TPOT protocol
// This is a singleton class, all users use it.  
class Core : public Thread, protected Synchronizable, public HttpInterfaceable
{
public:
	static Core *Instance;
	
	class Listener
	{
	public:
		virtual void message(Message *message) = 0;
		virtual void request(Request *request) = 0;
	};
	
	Core(int port);
	~Core(void);
	
	void registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
		       		const ByteString &secret,
				Listener *listener = NULL);
	void unregisterPeering(const Identifier &peering);
			     
	void addPeer(Socket *sock, const Identifier &peering);
	bool hasPeer(const Identifier &peering);
	
	void sendMessage(const Message &message);
	unsigned addRequest(Request *request);
	void removeRequest(unsigned id);

	void http(const String &prefix, Http::Request &request);

private:
	void run(void);

	class Handler : public Thread, protected Synchronizable
	{
	public:
		Handler(Core *core, Socket *sock);
		~Handler(void);

		void setPeering(const Identifier &peering);
		
		void sendMessage(const Message &message);
		void addRequest(Request *request);
		void removeRequest(unsigned id);

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
		void run(void);

		Identifier mPeering, mRemotePeering;
		Core	*mCore;
		Socket  *mSock;
		Stream  *mStream;
		Map<unsigned, Request*> mRequests;
		Map<unsigned, Request::Response*> mResponses;

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
			Map<unsigned, ByteStream*> mTransferts;	// TODO
			Queue<Message>	mMessagesQueue;
			Queue<Request*> mRequestsQueue;
			Array<Request*> mRequestsToRespond;
			bool mShouldStop;
			friend class Handler;
		};

		Sender mSender;
	};

	void addHandler(const Identifier &peer, Handler *Handler);
	void removeHandler(const Identifier &peer, Handler *handler);

	ServerSocket mSock;
	Map<Identifier, Identifier> mPeerings;
	Map<Identifier, ByteString> mSecrets;
	Map<Identifier, Listener*> mListeners;
	
	Map<Identifier, Handler*> mHandlers;
	
	Map<unsigned, Request*> mRequests;
	unsigned mLastRequest;

	friend class Handler;
};

}

#endif
