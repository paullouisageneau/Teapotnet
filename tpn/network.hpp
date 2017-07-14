/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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
	static const unsigned TunnelMtu;
	static const unsigned DefaultRedundantCount;
	static const double   DefaultRedundancy;
	static const double   DefaultPacketRate;
	static const duration CallPeriod;
	static const duration CallFallbackTimeout;

	static Network *Instance;

	struct Link
	{
		static const Link Null;

		Link(void);
		Link(Identifier _local, Identifier _remote, Identifier _node = Identifier::Empty);

		void setNull(void);
		bool isNull(void) const;

		bool operator < (const Network::Link &l) const;
		bool operator > (const Network::Link &l) const;
		bool operator == (const Network::Link &l) const;
		bool operator != (const Network::Link &l) const;

		Identifier local;
		Identifier remote;
		Identifier node;
	};

	struct Locator
	{
		Locator(void);
		Locator(String _prefix, String _path = "", Link _link = Link::Null);

		String fullPath(void) const;

		Link link;
		String prefix;
		String path;
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

		virtual bool anounce(const Locator &locator, List<BinaryString> &targets) = 0;

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

		virtual bool incoming(const Locator &locator, const BinaryString &target)	{ return false; }
		virtual bool incoming(const Locator &locator, const Mail &mail)		{ return false; }
		virtual bool localOnly(void) const;

	protected:
		bool fetch(const Locator &locator, const BinaryString &target, bool fetchContent = false);

	private:
		Link mLink;
		StringSet mSubscribedPrefixes;
	};

	class Caller
	{
	public:
		Caller(void);
		Caller(const BinaryString &target);
		~Caller(void);

		void startCalling(const BinaryString &target);
		void stopCalling(void);

		BinaryString target(void) const;
		duration elapsed(void) const;

	private:
		BinaryString mTarget, mHint;
		std::chrono::steady_clock::time_point mStartTime;
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

	// Send
	bool broadcast(const Identifier &local, const String &type, const Serializable &content);
	bool send(const Identifier &local, const Identifier &remote, const String &type, const Serializable &content);
	bool send(const Link &link, const String &type, const Serializable &content);
	bool push(const BinaryString &target, unsigned tokens);
	bool push(const Link &link, const BinaryString &target, unsigned tokens);
	bool pushRaw(const BinaryString &node, const BinaryString &target, unsigned tokens);

	// DHT
	void storeValue(const BinaryString &key, const BinaryString &value);
	bool retrieveValue(const BinaryString &key, Set<BinaryString> &values);
	bool retrieveValue(const BinaryString &key, Set<BinaryString> &values, duration timeout);

	// Links
	bool hasLink(const Identifier &local, const Identifier &remote) const;
	bool hasLink(const Link &link) const;
	bool getLinkFromNode(const Identifier &node, Link &link) const;

	void sendCalls(void);
	void sendBeacons(void);

	void run(void);

private:
	class RemotePublisher : public Publisher
	{
	public:
		RemotePublisher(const List<BinaryString> targets, const Link &link = Link::Null);
		~RemotePublisher(void);

		bool anounce(const Locator &locator, List<BinaryString> &targets);

	private:
		List<BinaryString> mTargets;
	};

	class RemoteSubscriber : public Subscriber
	{
	public:
		RemoteSubscriber(const Link &link = Link::Null);
		~RemoteSubscriber(void);

		bool incoming(const Locator &locator, const BinaryString &target);
		bool incoming(const Locator &locator, const Mail &mail);
		bool localOnly(void) const;
	};

	class Tunneler
	{
	public:
		Tunneler(void);
		~Tunneler(void);

		bool open(const BinaryString &node, const Identifier &remote, User *user);
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

		void registerTunnel(uint64_t id, Tunnel *tunnel);
		void unregisterTunnel(uint64_t id);
		void removePending(const BinaryString &node);

		SecureTransport *listen(BinaryString *source);

		bool handshake(SecureTransport *transport, const Link &link);

		Map<uint64_t, Tunnel*> mTunnels;		// Tunnels
		Map<BinaryString, uint64_t> mPending;	// Pending nodes

		mutable std::mutex mTunnelsMutex;

		Queue<Overlay::Message> mQueue;		// Queue for listening
		ThreadPool mPool;
		bool mStop;

		mutable std::mutex mMutex;
		mutable std::condition_variable mCondition;

		std::thread mThread;
	};

	class Handler : private Stream
	{
	public:
		Handler(Stream *stream, const Link &link);
		~Handler(void);

		void write(const String &type, const Serializable &content);
		void write(const String &type, const String &record);
		void push(const BinaryString &target, unsigned tokens);
		void timeout(void);

	private:
		bool readRecord(String &type, String &record);
		void writeRecord(const String &type, const Serializable &content, bool dontsend = false);
		void writeRecord(const String &type, const String &record, bool dontsend = false);

		bool readString(String &str);
		void writeString(const String &str);

		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void flush(bool dontsend = false);

		bool recvCombination(BinaryString &target, Fountain::Combination &combination);
		void sendCombination(const BinaryString &target, const Fountain::Combination &combination);
		int send(bool force = false);

		void process(void);
		void run(void);

		Stream *mStream;
		Link mLink;
		Alarm mTimeoutAlarm;
		Fountain::DataSource 	mSource;
		Fountain::Sink 		mSink;
		BinaryString		mSourceBuffer;

		struct Target
		{
			BinaryString digest;
			unsigned tokens;
		};

		List<Target> mTargets;

		double mTokens, mAvailableTokens, mThreshold, mAccumulator, mLocalSideSequence, mRedundancy;
		unsigned mLocalSideSeen, mLocalSideCount, mSideSeen, mSideCount;
		bool mCongestion;
		duration mTimeout, mKeepaliveTimeout;
		bool mClosed;

		std::thread mThread;
		mutable std::mutex mMutex;
		mutable std::mutex mWriteMutex;
	};

	void registerHandler(const Link &link, sptr<Handler> handler);
	void unregisterHandler(const Link &link, Handler *handler);

	bool outgoing(const String &type, const Serializable &content);
	bool outgoing(const Link &link, const String &type, const Serializable &content);
	bool incoming(const Link &link, const String &type, Serializer &serializer);

	bool directCall(const BinaryString &target, unsigned tokens);
	bool fallbackCall(const BinaryString &target, unsigned tokens);

	bool matchPublishers(const String &path, const Link &link, Subscriber *subscriber = NULL);
	bool matchSubscribers(const String &path, const Link &link, Publisher *publisher);
	bool matchSubscribers(const String &path, const Link &link, const Mail &mail);
	bool matchCallers(const Identifier &target, const Identifier &node);
	bool matchListeners(const Identifier &identifier, const Identifier &node);

	void onConnected(const Link &link, bool status = true) const;
	bool onRecv(const Link &link, const String &type, Serializer &serializer) const;
	bool onAuth(const Link &link, const Rsa::PublicKey &pubKey) const;

	Overlay mOverlay;
	Tunneler mTunneler;
	ThreadPool mPool;

	Map<Link, sptr<Handler> > mHandlers;
	Map<String, Set<Publisher*> > mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<BinaryString, Set<BinaryString> > mCallCandidates;
	Map<IdentifierPair, Set<Listener*> > mListeners;
	Map<Link, Map<String, sptr<RemoteSubscriber> > > mRemoteSubscribers;
	Map<Identifier, List<Link> > mLinksFromNodes;

	mutable std::recursive_mutex mHandlersMutex;	// recursive so listeners can call network on event
	mutable std::recursive_mutex mListenersMutex;	// idem
	mutable std::recursive_mutex mSubscribersMutex; // recursive so publish can be called from subscribers
	mutable std::recursive_mutex mPublishersMutex;	// idem
	mutable std::mutex mRemoteSubscribersMutex;
	mutable std::mutex mCallersMutex;
	mutable std::mutex mLinksFromNodesMutex;

	std::thread mThread;

	class Pusher
	{
	public:
		Pusher(void);
		~Pusher(void);

		void push(const BinaryString &target, const Identifier &destination, unsigned tokens);
		void run(void);

	private:
		struct Target
		{
			BinaryString digest;
			unsigned tokens;
		};

		Map<BinaryString, List<Target> > mTargets;
		unsigned mRedundant;
		duration mPeriod;

		mutable std::mutex mMutex;
		mutable std::condition_variable mCondition;

		std::thread mThread;
	};

	Pusher mPusher;

	friend class Handler;
	friend class Subscriber;	// for access to threadpool
};

}

#endif
