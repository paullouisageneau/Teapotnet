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

#ifndef TPN_OVERLAY_H
#define TPN_OVERLAY_H

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
  
// Overlay network implementation
class Overlay : protected Synchronizable
{
public:
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
		uint8_t ttl;
		uint8_t type;

		BinaryString source;
		BinaryString destination;
		BinaryString payload;
	};
	
	Overlay(int port);
	~Overlay(void);
	
	void start(void);
	void join(void);
	
	// Global
	String localName(void) const;
	BinaryString localNode(void) const;
	const Rsa::PublicKey &publicKey(void) const;
	const Rsa::PrivateKey &privateKey(void) const;
	SecureTransport::Certificate *certificate(void) const;

	void getAddresses(Set<Address> &set) const;
	void getKnownPublicAdresses(Set<Address> &set) const;
	bool isPublicConnectable(void) const;
	
	// Connections
	bool connect(const Set<Address> &addrs);
	int connectionsCount(void) const;
	
	// Routing
	bool route(const Message &message, const Identifier &from = Identifier::Null);
	bool broadcast(const Message &message, const Identifier &from = Identifier::Null);
	bool send(const Message &message, const Identifier &to);
	void addRoute(const Identifier &id, const Address &route);
	bool getRoute(const Identifier &id, const Address &route);
	
private:
	class Backend : public Thread
	{
	public:
		Backend(Overlay *overlay);
		virtual ~Backend(void);
		
		virtual bool connect(const Set<Address> &addrs) = 0;
		virtual SecureTransport *listen(void) = 0;
		
		virtual void getAddresses(Set<Address> &set) const { set.clear(); }
		
	protected:
		bool process(SecureTransport *transport);	// do the client handshake
	
	private:
		bool handshake(SecureTransport *transport, bool async = false);
		void run(void);
		
		ThreadPool mThreadPool;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(Overlay *overlay, int port);
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
		DatagramBackend(Overlay *overlay, int port);
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
		Handler(Overlay *overlay, Stream *stream, const BinaryString &node);	// stream will be deleted
		~Handler(void);
		
		bool recv(Message &message);
		bool send(const Message &message);
		
	private:
		void process(void);
		void run(void);
		
		Stream  *mStream;
		BinaryString mNode;
	};
	
	bool registerHandler(const BinaryString &node, Handler *handler);
	bool unregisterHandler(const BinaryString &node, Handler *handler);
	
	bool track(const String &tracker, Set<Address> &result);
	
	ThreadPool mThreadPool;
	
	String mName;
	Rsa::PublicKey	mPublicKey;
	Rsa::PrivateKey	mPrivateKey;
	SecureTransport::Certificate *mCertificate;

	List<Backend*> mBackends;
	Map<Identifier, BinaryString> mRoutes;
	Map<BinaryString, Handler*> mHandlers;
	
	Time mLastPublicIncomingTime;
	Map<Address, Time> mKnownPublicAddresses;
};

}

#endif

