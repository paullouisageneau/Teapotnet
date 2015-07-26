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
#include "tpn/notification.h"

#include "pla/address.h"
#include "pla/stream.h"
#include "pla/bytearray.h"
#include "pla/binarystring.h"
#include "pla/string.h"
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
  
class Network : protected Synchronizable
{
public:
	static Network *Instance;
	
	class Publisher
	{
	public:
<<<<<<< HEAD
		Publisher(const Identifier &peer = "");	// local peer
=======
		Publisher(const Identifier &peer = Identifier::Empty);	// local peer
>>>>>>> 4a8a42363b695cdfb1a5d30becd92e0791ceee20
		~Publisher(void);
		
		void publish(const String &prefix, const String &path = "/");
		void unpublish(const String &prefix);
		
		virtual bool anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets) = 0;
		
	private:
		Identifier mPeer;
		StringSet mPublishedPrefixes;
	};
	
	class Subscriber
	{
	public:
<<<<<<< HEAD
		Subscriber(const Identifier &peer = "");	// remote peer
=======
		Subscriber(const Identifier &peer = Identifier::Empty);	// remote peer
>>>>>>> 4a8a42363b695cdfb1a5d30becd92e0791ceee20
		~Subscriber(void);
		
		void subscribe(const String &prefix);
		void unsubscribe(const String &prefix);
		void unsubscribeAll(void);
		
		virtual bool incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target) = 0;
		virtual Identifier remote(void) const;
		virtual bool localOnly(void) const;
		
	protected:
		bool fetch(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target);
		
	private:
		Identifier mPeer;
		StringSet mSubscribedPrefixes;
		ThreadPool mThreadPool;
	};
	
	class Caller : protected Synchronizable
	{
	public:
		Caller(void);
		Caller(const BinaryString &target);
		~Caller(void);
		
		void startCalling(const BinaryString &target);
		void stopCalling(void);
		
	private:
		BinaryString mTarget;
	};
	
	class Listener
	{
	public:
		Listener(void);
		~Listener(void);
		
		void listen(const Identifier &peer);
		
		virtual void seen(const Identifier &peer) {}
		virtual void connected(const Identifier &peer) {}
		virtual bool recv(const Identifier &peer, const Notification &notification) = 0;
		virtual bool auth(const Identifier &peer, const Rsa::PublicKey &pubKey) { return false; }
		
	private:
		Set<Identifier> mPeers;
	};
	
	Network(int port);
	~Network(void);
	
	void start(void);
	void join(void);
	
	// Caller
	void registerCaller(const BinaryString &target, Caller *caller);
	void unregisterCaller(const BinaryString &target, Caller *caller);
	void unregisterAllCallers(const BinaryString &target);
	
	// Listener
	void registerListener(const Identifier &id, Listener *listener);
	void unregisterListener(const Identifier &id, Listener *listener);
	
	// Publish/Subscribe
	void publish(String prefix, Publisher *publisher);
	void unpublish(String prefix, Publisher *publisher);
	void subscribe(String prefix, Subscriber *subscriber);
	void unsubscribe(String prefix, Subscriber *subscriber);
	void advertise(String prefix, const String &path, const Identifier &source, Publisher *publisher);
	void addRemoteSubscriber(const Identifier &peer, const String &path, bool publicOnly);
	
	// Notification
	bool broadcast(const Identifier &local, const Notification &notification);
	bool send(const Identifier &local, const Identifier &remote, const Notification &notification);
	
	bool addHandler(Stream *stream, const Identifier &local, const Identifier &remote);
	bool hasHandler(const Identifier &local, const Identifier &remote);
	bool hasLink(const Identifier &local, const Identifier &remote) { return hasHandler(local, remote); }
	
private:
	class RemotePublisher : public Publisher
	{
	public:
		RemotePublisher(const List<BinaryString> targets);
		~RemotePublisher(void);
		
		bool anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets);
		
	private:
		List<BinaryString> mTargets;
	};
	
	class RemoteSubscriber : public Subscriber
	{
	public:
<<<<<<< HEAD
		RemoteSubscriber(const Identifier &remote = "", bool publicOnly = false);
=======
		RemoteSubscriber(const Identifier &remote = Identifier::Empty, bool publicOnly = false);
>>>>>>> 4a8a42363b695cdfb1a5d30becd92e0791ceee20
		~RemoteSubscriber(void);
		
		bool incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target);
		Identifier remote(void) const;
		bool localOnly(void) const;
		
	private:
		Identifier mRemote;
		bool mPublicOnly;
	};
	
	class Tunneler : public Thread
	{
	public:
		static const double DefaultTimeout = 60.;
	
		Tunneler(Network *network);
		~Tunneler(void);
		
		bool open(const BinaryString &remote, User *user);
		bool incoming(const Overlay::Message &message);
		
	private:
		class Tunnel : public Stream
		{
		public:
			Tunnel(Tunneler *tunneler, uint64_t id);
			~Tunnel(void);
			
			uint64_t id(void) const;
			
			void setTimeout(double timeout);
			
			// Stream
			size_t readData(char *buffer, size_t size);
			void writeData(const char *data, size_t size);
			bool waitData(double &timeout);
			bool waitData(const double &timeout);
			bool isDatagram(void) const;
			
			bool incoming(const Overlay::Message &message);
			
		private:
			Tunneler *tunneler;
			uint64_t mId;
			Queue<Overlay::Message> mQueue;
			Synchronizable mQueueSync;
			double mTimeout;
		};
		
		bool registerTunnel(Tunnel *tunnel);
		bool unregisterTunnel(Tunnel *tunnel);
		
		SecureTransport *listen(void);

		bool handshake(SecureTransport *transport, const BinaryString &local = "", const BinaryString &remote = "", bool async = false);
		void run(void);
		
		Network *mNetwork;
		Map<uint64_t, Tunnel*> mTunnels;
		ThreadPool mThreadPool;
		
		// Queue for listen
		Queue<Overlay::Message> mQueue;
		Synchronizable mQueueSync;
	};
	
	class Handler : protected Synchronizable, public Task
	{
	public:
		Handler(Stream *stream);
		~Handler(void);
		
		bool read(String &type, String &content);
		void wirte(const String &type, const String &content);
		
	private:
		void process(void);
		void run(void);
		
		Stream *mStream;
		Fountain::DataSource 	mSource;
		Fountain::Sink 		mSink;
		double mTokens;
		double mRedundancy;
	};

	bool registerHandler(const Identifier &local, const Identifier &remote, Handler *handler);
	bool unregisterHandler(const Identifier &local, const Identifier &remote, Handler *handler);
	
	bool outgoing(const String &type, const Serializable &content);
	bool outgoing(const Identifier &local, const Identifier &remote, const String &type, const Serializable &content);
	bool incoming(const Identifier &local, const Identifier &remote, const String &type, Serializer &serializer);
	
	bool matchPublishers(const String &path, const Identifier &source, Subscriber *subscriber = NULL);
	bool matchSubscribers(const String &path, const Identifier &source, Publisher *publisher);
	
	Overlay mOverlay;
	Tunneler mTunneler;
	ThreadPool mThreadPool;

	Map<IdentifierPair, Handler*> mHandlers;
	Map<String, Set<Publisher*> > mPublishers;
	Map<String, Set<Subscriber*> > mSubscribers;
	Map<BinaryString, Set<Caller*> > mCallers;
	Map<Identifier, Set<Listener*> > mListeners;
	List<RemoteSubscriber> mRemoteSubscribers;
	
	friend class Handler;
};

}

#endif

