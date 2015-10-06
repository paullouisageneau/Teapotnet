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
class Overlay : protected Synchronizable, public Task
{
public:
	struct Message : public Serializable
	{
		static const uint8_t Invalid	= 0x00;
		static const uint8_t Hello	= 0x01;
		static const uint8_t Ping	= 0x02;
		static const uint8_t Pong	= 0x03;
		static const uint8_t Offer	= 0x04;
		static const uint8_t Suggest	= 0x05;
		static const uint8_t Tunnel	= 0x10;
		
		Message(void);
		Message(uint8_t type, const BinaryString &content = "", const BinaryString &destination = "");
		~Message(void);
		
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
		BinaryString content;
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

	// Addresses
	void getAddresses(Set<Address> &set) const;
	
	// Connections
	bool connect(const Set<Address> &addrs, const BinaryString &remote = "", bool async = false);
	int connectionsCount(void) const;
	
	// Message interface
	bool recv(Message &message);
	bool send(const Message &message);

	// DHT
	void storeValue(const BinaryString &key, const BinaryString &value);
	bool retrieveValue(const BinaryString &key, BinaryString &value);
	
	void run(void);
	
private:
	// Routing
	bool incoming(Message &message, const BinaryString &from);
	bool route(const Message &message, const BinaryString &from = "");
	bool broadcast(const Message &message, const BinaryString &from = "");
	bool sendTo(const Message &message, const BinaryString &to);
	
	class Backend : public Thread
	{
	public:
		Backend(Overlay *overlay);
		virtual ~Backend(void);
		
		virtual bool connect(const Set<Address> &addrs, const BinaryString &remote = "") = 0;
		virtual SecureTransport *listen(void) = 0;
		
		virtual void getAddresses(Set<Address> &set) const { set.clear(); }
		
	protected:
		bool handshake(SecureTransport *transport, const BinaryString &remote = "");
		void run(void);
		
	private:
		Overlay *mOverlay;
		ThreadPool mThreadPool;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(Overlay *overlay, int port);
		~StreamBackend(void);
		
		bool connect(const Set<Address> &addrs, const BinaryString &remote);
		bool connect(const Address &addr, const BinaryString &remote);
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
		
		bool connect(const Set<Address> &addrs, const BinaryString &remote);
		bool connect(const Address &addr, const BinaryString &remote);
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
		
		Overlay *mOverlay;
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
	Map<BinaryString, Handler*> mHandlers;
	
	Queue<Message> mIncoming;
	Synchronizable mIncomingSync;

	Map<BinaryString, BinaryString> mRoutes;
};

}

#endif

