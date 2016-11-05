/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#include "tpn/include.hpp"

#include "pla/address.hpp"
#include "pla/stream.hpp"
#include "pla/bytearray.hpp"
#include "pla/binarystring.hpp"
#include "pla/string.hpp"
#include "pla/socket.hpp"
#include "pla/serversocket.hpp"
#include "pla/datagramsocket.hpp"
#include "pla/securetransport.hpp"
#include "pla/threadpool.hpp"
#include "pla/alarm.hpp"
#include "pla/serializable.hpp"
#include "pla/object.hpp"
#include "pla/map.hpp"
#include "pla/set.hpp"
#include "pla/array.hpp"
#include "pla/http.hpp"

namespace tpn
{

// Overlay network implementation
class Overlay : public Serializable
{
public:
	static const int MaxQueueSize;
	
	struct Message : public Serializable
	{
		// Non-routable messages
		static const uint8_t Dummy	= 0x00;
		static const uint8_t Offer	= 0x01;
		static const uint8_t Suggest	= 0x02;
		static const uint8_t Retrieve   = 0x03;
		static const uint8_t Store	= 0x04;
		static const uint8_t Value	= 0x05;
		
		// Routable messages
		static const uint8_t Call	= 0x80|0x01;
		static const uint8_t Data	= 0x80|0x02;
		static const uint8_t Tunnel	= 0x80|0x03;
		static const uint8_t Ping	= 0x80|0x04;
		static const uint8_t Pong	= 0x80|0x05;
		
		Message(void);
		Message(uint8_t type, 
			const BinaryString &content = "", 
			const BinaryString &destination = "",
			const BinaryString &source = "");
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
	
	void load(void);
	void save(void) const;
	void join(void);
	
	// Global
	String localName(void) const;
	BinaryString localNode(void) const;
	const Rsa::PublicKey &publicKey(void) const;
	const Rsa::PrivateKey &privateKey(void) const;
	sptr<SecureTransport::Certificate> certificate(void) const;

	// Addresses
	void getAddresses(Set<Address> &set) const;
	
	// Connections
	bool connect(const Set<Address> &addrs, const BinaryString &remote = "", bool async = false);
	bool isConnected(const BinaryString &remote) const;
	int connectionsCount(void) const;
	
	// Message interface
	bool recv(Message &message, duration timeout);
	bool send(const Message &message);

	// DHT
	void store(const BinaryString &key, const BinaryString &value);
	void retrieve(const BinaryString &key);					// async
	bool retrieve(const BinaryString &key, Set<BinaryString> &values);	// sync
	
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	
private:
	// Routing
	bool incoming(Message &message, const BinaryString &from);
	bool push(Message &message);
	bool route(const Message &message, const BinaryString &from = "");
	bool broadcast(const Message &message, const BinaryString &from = "");
	bool sendTo(const Message &message, const BinaryString &to);
	int getRoutes(const BinaryString &destination, int count, Array<BinaryString> &result);
	int getNeighbors(const BinaryString &destination, Array<BinaryString> &result);
	
	void run(void);
	
	class Backend
	{
	public:
		Backend(Overlay *overlay);
		virtual ~Backend(void);
		
		virtual bool connect(const Set<Address> &addrs, const BinaryString &remote = "") = 0;
		virtual SecureTransport *listen(Address *addr = NULL) = 0;
		virtual void getAddresses(Set<Address> &set) const { set.clear(); }
		
		void run(void);
		
	protected:
		bool handshake(SecureTransport *transport, const Address &addr, const BinaryString &remote = "");

		Overlay *mOverlay;
	};
	
	class StreamBackend : public Backend
	{
	public:
		StreamBackend(Overlay *overlay, int port);
		~StreamBackend(void);
		
		bool connect(const Set<Address> &addrs, const BinaryString &remote);
		bool connect(const Address &addr, const BinaryString &remote);
		bool connectHttp(const Address &addr, const BinaryString &remote);
		SecureTransport *listen(Address *addr = NULL);
		
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
		SecureTransport *listen(Address *addr = NULL);
		
		void getAddresses(Set<Address> &set) const;
		
	private:
		DatagramSocket mSock;
	};
	
	class Handler
	{
	public:
		Handler(Overlay *overlay, Stream *stream, const BinaryString &node, const Address &addr);	// stream will be deleted
		~Handler(void);
		
		bool recv(Message &message);
		bool send(const Message &message);
		
		void start(void);
		void stop(void);
		
		void addAddress(const Address &addr);
		void addAddresses(const Set<Address> &addrs);
		void getAddresses(Set<Address> &set) const;
		BinaryString node(void) const;

	private:
		void run(void);
		void process(void);
		
		Overlay *mOverlay;
		Stream  *mStream;
		BinaryString mNode;
		Set<Address> mAddrs;
		bool mStop;
		
		mutable std::mutex mMutex;
		
		std::thread mThread;
		std::thread mSenderThread;
		
		class Sender
		{
		public:
			Sender(Overlay *overlay, Stream *stream);
			~Sender(void);
			
			bool push(const Message &message);
			void stop(void);
			
			void run(void);

		private:
			void send(const Message &message);
			
			Overlay *mOverlay;
			Stream *mStream;
			Queue<Message> mQueue;
			bool mStop;

			mutable std::mutex mMutex;
			mutable std::condition_variable mCondition;
		};
		
		Sender mSender;
	};

	void registerHandler(const BinaryString &node, const Address &addr, sptr<Handler> handler);
	void unregisterHandler(const BinaryString &node, const Set<Address> &addrs, Handler *handler);
	
	bool track(const String &tracker, Map<BinaryString, Set<Address> > &result);
	
	ThreadPool mPool;
	
	String mLocalName;
	String mFileName;
	Rsa::PublicKey	mPublicKey;
	Rsa::PrivateKey	mPrivateKey;
	BinaryString mLocalNode;
	sptr<SecureTransport::Certificate> mCertificate;

	List<sptr<Backend> > mBackends;
	Map<BinaryString, sptr<Handler> > mHandlers;
	Set<Address> mRemoteAddresses, mLocalAddresses;
	
	Queue<Message> mIncoming;
	Set<BinaryString> mRetrievePending;
	
	Alarm mRunAlarm;

	mutable std::mutex mMutex;
	
	mutable std::mutex mIncomingMutex;
	mutable std::condition_variable mIncomingCondition;
	
	mutable std::mutex mRetrieveMutex;
	mutable std::condition_variable mRetrieveCondition;
};

}

#endif

