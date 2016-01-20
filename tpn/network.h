/*************************************************************************
 *   Copyright (C) 2011-2015 by Paul-Louis Ageneau                       *
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

#include "tpn/include.h"
#include "tpn/overlay.h"
#include "tpn/fountain.h"

#include "pla/address.h"
#include "pla/stream.h"
#include "pla/bytearray.h"
#include "pla/binarystring.h"
#include "pla/string.h"
#include "pla/task.h"
#include "pla/thread.h"
#include "pla/mutex.h"
#include "pla/signal.h"
#include "pla/threadpool.h"
#include "pla/scheduler.h"
#include "pla/runner.h"
#include "pla/synchronizable.h"
#include "pla/map.h"
#include "pla/array.h"

namespace tpn
{

class User;
  
class Network : protected Synchronizable, public Thread
{
public:
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
		
		virtual bool incoming(const Link &link, const String &prefix, const String &path, const BinaryString &target) = 0;
		virtual bool localOnly(void) const;
		
	protected:
		bool fetch(const Link &link, const String &prefix, const String &path, const BinaryString &target, bool fetchContent = false);
		
	private:
		Link mLink;
		StringSet mSubscribedPrefixes;
		ThreadPool mThreadPool;
	};
	
	class Caller : protected Synchronizable
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
		
		virtual void seen(const Link &link) {}
		virtual void connected(const Link &link, bool status) {}
		virtual bool recv(const Link &link, const String &type, Serializer &serializer) = 0;
		virtual bool auth(const Link &link, const Rsa::PublicKey &pubKey) { return false; }
	
	private:
		Set<IdentifierPair> mPairs;
	};
	
	Network(int port);
	~Network(void);
	
	void start(void);
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
	void addRemoteSubscriber(const Link &link, const String &path);
	
	// Serializable
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
	static const unsigned RedundantChunks;
	
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
		bool localOnly(void) const;
	};
	
	class Tunneler : protected Synchronizable, public Thread
	{
	public:
		Tunneler(void);
		~Tunneler(void);
		
		bool open(const BinaryString &node, const Identifier &remote, User *user, bool async = false);
		bool incoming(const Overlay::Message &message);
		
	private:
		class Tunnel : protected Synchronizable, public Stream
		{
		public:
			Tunnel(Tunneler *tunneler, uint64_t id, const BinaryString &node);
			~Tunnel(void);
			
			uint64_t id(void) const;
			BinaryString node(void) const;
			
			void setTimeout(double timeout);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			bool waitData(double &timeout);
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
			double mTimeout;
		};
		
		bool registerTunnel(Tunnel *tunnel);
		bool unregisterTunnel(Tunnel *tunnel);
		
		SecureTransport *listen(BinaryString *source);

		bool handshake(SecureTransport *transport, const Link &link, bool async = false);
		void run(void);
		
		Map<uint64_t, Tunnel*> mTunnels;
		ThreadPool mThreadPool;
		Queue<Overlay::Message> mQueue;		// Queue for listening
		Set<BinaryString> mPending;		// Pending nodes
	};
	
	class Handler : protected Synchronizable, public Task
	{
	public:
		Handler(Stream *stream, const Link &link);
		~Handler(void);
		
		void write(const String &type, const String &content);
		void timeout(void);
		
	private:
		int send(bool force = false);
		bool read(String &type, String &content);
		bool readString(String &str);
		
		void process(void);
		void run(void);
		
		Stream *mStream;
		Link mLink;
		Fountain::DataSource 	mSource;
		Fountain::Sink 		mSink;
		double mTokens, mRank;
		double mRedundancy;
		double mTimeout;
		
		class TimeoutTask : public Task
		{
		public:
			TimeoutTask(Handler *handler) { this->handler = handler; }
			void run(void) { handler->timeout(); }
		private:
			Handler *handler;
		};
		
		TimeoutTask mTimeoutTask;
	};
	
	void registerHandler(const Link &link, Handler *handler);
	void unregisterHandler(const Link &link, Handler *handler);
	
	bool outgoing(const String &type, const Serializable &content);
	bool outgoing(const Link &link, const String &type, const Serializable &content);
	bool incoming(const Link &link, const String &type, Serializer &serializer);
	
	bool matchPublishers(const String &path, const Link &link, Subscriber *subscriber = NULL);
	bool matchSubscribers(const String &path, const Link &link, Publisher *publisher);
	
	void onConnected(const Link &link, bool status = true);
	bool onRecv(const Link &link, const String &type, Serializer &serializer);
	bool onAuth(const Link &link, const Rsa::PublicKey &pubKey);
	
	Overlay mOverlay;
	Tunneler mTunneler;
	ThreadPool mThreadPool;

	Map<Link, Handler*> mHandlers;
	Set<Handler*> mOtherHandlers;
	Map<String, Set<Publisher*> > mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<IdentifierPair, Set<Listener*> > mListeners;
	Map<Link, Map<String, RemoteSubscriber> > mRemoteSubscribers;
	
	class Pusher : protected Synchronizable, public Thread
	{
	public:
		Pusher(void);
		~Pusher(void);
		
		void push(const BinaryString &target, const Identifier &destination, unsigned tokens);
		void run(void);
		
	private:
		Map<BinaryString, Map<Identifier, unsigned> > mTargets;
	};
	
	Pusher mPusher;
	
	friend class Handler;
};

}

#endif

