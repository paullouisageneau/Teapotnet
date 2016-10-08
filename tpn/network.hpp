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

#ifndef TPN_NETWORK_H
#define TPN_NETWORK_H

#include "tpn/include.hpp"
#include "tpn/overlay.hpp"
#include "tpn/fountain.hpp"
#include "tpn/mail.hpp"

#include "pla/address.hpp"
#include "pla/stream.hpp"
#include "pla/bytearray.hpp"
#include "pla/binarystring.hpp"
#include "pla/string.hpp"
#include "pla/threadpool.hpp"
#include "pla/scheduler.hpp"
#include "pla/alarm.hpp"
#include "pla/map.hpp"
#include "pla/array.hpp"

namespace tpn
{

class User;
  
class Network
{
public:
	static const unsigned DefaultTokens;
	static const unsigned DefaultThreshold;

	static Network *Instance;
	
	struct Link
	{
		static const Link Null;
		
		Link(void);
		Link(const Identifier &local, const Identifier &remote, const Identifier &node = Identifier::Empty);
		~Link(void);
		
		Identifier local;
		Identifier remote;
		Identifier node;
		
		void setNull(void);
		bool isNull(void) const;
		
		bool operator < (const Network::Link &l) const;
		bool operator > (const Network::Link &l) const;
		bool operator == (const Network::Link &l) const;
		bool operator != (const Network::Link &l) const;
	};
	
	class Publisher
	{
	public:
		Publisher(const Link &link = Link::Null);
		~Publisher(void);
		
		const Link &link(void) const;
		
		void publish(const String &prefix, const String &path = "/");
		void unpublish(const String &prefix);
		void issue(const String &prefix, const Mail &mail, const String &path = "/");
		
		virtual bool anounce(const Link &link, const String &prefix, const String &path, List<BinaryString> &targets) = 0;
		
	private:
		Link mLink;
		StringSet mPublishedPrefixes;
	};
	
	class Subscriber
	{
	public:
		Subscriber(const Link &link = Link::Null);
		~Subscriber(void);
		
		const Link &link(void) const;
		
		void subscribe(const String &prefix);
		void unsubscribe(const String &prefix);
		void unsubscribeAll(void);
		
		virtual bool incoming(const Link &link, const String &prefix, const String &path, const BinaryString &target)	{ return false; }
		virtual bool incoming(const Link &link, const String &prefix, const String &path, const Mail &mail)		{ return false; }
		virtual bool localOnly(void) const;
		
	protected:
		bool fetch(const Link &link, const String &prefix, const String &path, const BinaryString &target, bool fetchContent = false);
		
	private:
		Link mLink;
		StringSet mSubscribedPrefixes;
		ThreadPool mPool;
	};
	
	class Caller
	{
	public:
		Caller(void);
		Caller(const BinaryString &target, const BinaryString &hint = "");
		~Caller(void);
		
		void startCalling(const BinaryString &target, const BinaryString &hint = "");
		void stopCalling(void);
		
		BinaryString target(void) const;
		BinaryString hint(void) const;

	private:
		BinaryString mTarget, mHint;
	};
	
	class Listener
	{
	public:
		Listener(void);
		~Listener(void);
		
		void listen(const Identifier &local, const Identifier &remote);
		void ignore(const Identifier &local, const Identifier &remote);
		void ignore(void);	
	
		virtual void seen(const Link &link) {}
		virtual void connected(const Link &link, bool status) {}
		virtual bool recv(const Link &link, const String &type, Serializer &serializer) = 0;
		virtual bool auth(const Link &link, const Rsa::PublicKey &pubKey) { return false; }
	
	private:
		Set<IdentifierPair> mPairs;
	};
	
	Network(int port);
	~Network(void);
	
	void join(void);
	
	Overlay *overlay(void);
	
	void connect(const Identifier &node, const Identifier &remote, User *user);
	
	// Caller
	void registerCaller(const BinaryString &target, Caller *caller);
	void unregisterCaller(const BinaryString &target, Caller *caller);
	void unregisterAllCallers(const BinaryString &target);
	
	// Listener
	void registerListener(const Identifier &local, const Identifier &remote, Listener *listener);
	void unregisterListener(const Identifier &local, const Identifier &remote, Listener *listener);
	
	// Publish/Subscribe
	void publish(String prefix, Publisher *publisher);
	void unpublish(String prefix, Publisher *publisher);
	void subscribe(String prefix, Subscriber *subscriber);
	void unsubscribe(String prefix, Subscriber *subscriber);
	void advertise(String prefix, const String &path, Publisher *publisher);
	void issue(String prefix, const String &path, Publisher *publisher, const Mail &mail);
	void addRemoteSubscriber(const Link &link, const String &path);
	
	// Send/Recv
	bool broadcast(const Identifier &local, const String &type, const Serializable &object);
	bool send(const Identifier &local, const Identifier &remote, const String &type, const Serializable &object);
	bool send(const Link &link, const String &type, const Serializable &object);
	
	// DHT
	void storeValue(const BinaryString &key, const BinaryString &value);
	bool retrieveValue(const BinaryString &key, Set<BinaryString> &values);
	
	// Links
	bool hasLink(const Identifier &local, const Identifier &remote);
	bool hasLink(const Link &link);
	
	void run(void);
	
private:
	class RemotePublisher : public Publisher
	{
	public:
		RemotePublisher(const List<BinaryString> targets, const Link &link = Link::Null);
		~RemotePublisher(void);
		
		bool anounce(const Link &link, const String &prefix, const String &path, List<BinaryString> &targets);
		
	private:
		List<BinaryString> mTargets;
	};
	
	class RemoteSubscriber : public Subscriber
	{
	public:
		RemoteSubscriber(const Link &link = Link::Null);
		~RemoteSubscriber(void);
		
		bool incoming(const Link &link, const String &prefix, const String &path, const BinaryString &target);
		bool incoming(const Link &link, const String &prefix, const String &path, const Mail &mail);
		bool localOnly(void) const;
	};
	
	class Tunneler
	{
	public:
		Tunneler(void);
		~Tunneler(void);
		
		bool open(const BinaryString &node, const Identifier &remote, User *user, bool async = false);
		bool incoming(const Overlay::Message &message);
		
		void run(void);
		
	private:
		class Tunnel : public Stream
		{
		public:
			Tunnel(Tunneler *tunneler, uint64_t id, const BinaryString &node);
			~Tunnel(void);
			
			uint64_t id(void) const;
			BinaryString node(void) const;
			
			void setTimeout(duration timeout);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			bool waitData(duration timeout);
			bool nextRead(void);
			bool nextWrite(void);
			bool isDatagram(void) const;
			
			bool incoming(const Overlay::Message &message);
			
		private:
			Tunneler *mTunneler;
			uint64_t mId;			// tunnel id
			BinaryString mNode;
			Queue<Overlay::Message> mQueue;	// recv queue
			size_t mOffset;			// read offset
			BinaryString mBuffer;		// write buffer
			duration mTimeout;
			bool mClosed;
			
			mutable std::mutex mMutex;
			std::condition_variable mCondition;
		};
		
		void registerTunnel(uint64_t id, sptr<Tunnel> tunnel);
		void unregisterTunnel(uint64_t id);
		
		SecureTransport *listen(BinaryString *source);

		bool handshake(SecureTransport *transport, const Link &link, bool async = false);
		
		Map<uint64_t, sptr<Tunnel> > mTunnels;
		Queue<Overlay::Message> mQueue;		// Queue for listening
		Set<BinaryString> mPending;		// Pending nodes
		ThreadPool mPool;
		bool mStop;
		
		std::thread mThread;
		mutable std::mutex mMutex;
		mutable std::condition_variable mCondition;
	};
	
	class Handler : private Stream
	{
	public:
		Handler(Stream *stream, const Link &link);
		~Handler(void);
		
		void write(const String &type, const String &record);
		void push(const BinaryString &target, unsigned tokens);
		void timeout(void);
		
	private:
		bool readRecord(String &type, String &record);
		void writeRecord(const String &type, const String &record);
		
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void flush(void);
		
		int send(bool force = false);
		
		bool recvCombination(BinaryString &target, Fountain::Combination &combination);
		void sendCombination(const BinaryString &target, const Fountain::Combination &combination);
		
		void process(void);
		void run(void);
		
		Stream *mStream;
		Link mLink;
		Alarm mTimeoutAlarm;
		Fountain::DataSource 	mSource;
		Fountain::Sink 		mSink;
		Map<BinaryString, unsigned> mTargets;

		double mTokens, mAvailableTokens, mThreshold, mAccumulator;
		double mRedundancy;
		duration mTimeout;
		bool mClosed;
		
		std::thread mThread;
		mutable std::mutex mMutex;
		mutable std::mutex mWriteMutex;
	};
	
	void registerHandler(const Link &link, sptr<Handler> handler);
	void unregisterHandler(const Link &link);
	
	bool outgoing(const String &type, const Serializable &content);
	bool outgoing(const Link &link, const String &type, const Serializable &content);
	bool incoming(const Link &link, const String &type, Serializer &serializer);
	
	bool matchPublishers(const String &path, const Link &link, Subscriber *subscriber = NULL);
	bool matchSubscribers(const String &path, const Link &link, Publisher *publisher);
	bool matchSubscribers(const String &path, const Link &link, const Mail &mail);
	bool matchCallers(const Identifier &target, const Identifier &node);
	bool matchListeners(const Identifier &identifier, const Identifier &node);
	
	void onConnected(const Link &link, bool status = true);
	bool onRecv(const Link &link, const String &type, Serializer &serializer);
	bool onAuth(const Link &link, const Rsa::PublicKey &pubKey);
	
	Overlay mOverlay;
	Tunneler mTunneler;

	Map<Link, sptr<Handler> > mHandlers;
	Map<String, Set<Publisher*> > mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<IdentifierPair, Set<Listener*> > mListeners;
	Map<Link, Map<String, sptr<RemoteSubscriber> > > mRemoteSubscribers;
	
	mutable std::mutex mHandlersMutex;
	mutable std::mutex mPublishersMutex;
	mutable std::mutex mSubscribersMutex;
	mutable std::mutex mCallersMutex;
	mutable std::mutex mListenersMutex;
	
	std::thread mThread;
	
	class Pusher
	{
	public:
		Pusher(void);
		~Pusher(void);
		
		void push(const BinaryString &target, const Identifier &destination, unsigned tokens);
		void run(void);
		
	private:
		Map<BinaryString, Map<Identifier, unsigned> > mTargets;
		unsigned mRedundant;
		
		mutable std::mutex mMutex;
		mutable std::condition_variable mCondition;
		
		std::thread mThread;
	};
	
	Pusher mPusher;

	friend class Handler;
};

}

#endif

