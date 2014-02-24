/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/core.h"
#include "tpn/html.h"
#include "tpn/sha512.h"
#include "tpn/cipher.h"
#include "tpn/httptunnel.h"
#include "tpn/config.h"
#include "tpn/scheduler.h"
#include "tpn/bytearray.h"

#include <cryptopp/pwdbased.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include <cryptopp/aes.h>

namespace tpn
{

Core *Core::Instance = NULL;


Core::Core(int port) :
		mSock(port),
		mThreadPool(4, 16, Config::Get("max_connections").toInt()),
		mLastRequest(0),
		mLastPublicIncomingTime(0)
{
	mName = Config::Get("instance_name");
	
	if(mName.empty())
	{
		char hostname[HOST_NAME_MAX];
		if(!gethostname(hostname,HOST_NAME_MAX)) 
			mName = hostname;
		
		if(mName.empty() || mName == "localhost")
		{
		#ifdef ANDROID
			mName = String("android.") + String::number(unsigned(pseudorand()%1000), 4);
		#else
			mName = String(".") + String::random(6);
		#endif
			Config::Put("instance_name", mName);
			
			const String configFileName = "config.txt";
			Config::Save(configFileName);
		}
	}
}

Core::~Core(void)
{

}

String Core::getName(void) const
{
	Synchronize(this);
	return mName;
}

void Core::getAddresses(List<Address> &list) const
{
	Synchronize(this);
	mSock.getLocalAddresses(list);
}

void Core::getKnownPublicAdresses(List<Address> &list) const
{
	Synchronize(this);
	list.clear();
	for(	Map<Address, int>::const_iterator it = mKnownPublicAddresses.begin();
		it != mKnownPublicAddresses.end();
		++it)
	{
		list.push_back(it->first);
	}
}

bool Core::isPublicConnectable(void) const
{
	return (Time::Now()-mLastPublicIncomingTime <= 3600.); 
}

void Core::registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
		       		const BinaryString &secret,
				Core::Listener *listener)
{
	Synchronize(this);
	
	mPeerings[peering] = remotePeering;
	mSecrets[peering] = secret;
	if(listener) mListeners[peering] = listener;
	else mListeners.erase(peering);
}

void Core::unregisterPeering(const Identifier &peering)
{
	Synchronize(this);
	
	mPeerings.erase(peering);
	mSecrets.erase(peering);
}

bool Core::hasRegisteredPeering(const Identifier &peering)
{
	Synchronize(this);
	return mPeerings.contains(peering);
}

Core::LinkStatus Core::addPeer(Stream *bs, const Address &remoteAddr, const Identifier &peering, bool async)
{
	Assert(bs);
	Synchronize(this);
	
	bool hasPeering = (peering != Identifier::Null);
	
	if(hasPeering && !mPeerings.contains(peering))
		throw Exception("Added peer with unknown peering");
	
	{
		Desynchronize(this);
		//LogDebug("Core", "Spawning new handler");
		Handler *handler = new Handler(this, bs, remoteAddr);
		if(hasPeering) handler->setPeering(peering);

		if(async)
		{
			mThreadPool.launch(handler);
			return Core::Disconnected;
		}
		else {
			Synchronize(handler);
			mThreadPool.launch(handler);
			
			// Timeout is just a security here
			const double timeout = milliseconds(Config::Get("tpot_read_timeout").toInt());
			if(!handler->wait(timeout*4)) return Core::Disconnected;
			return handler->linkStatus();
		}
	}
}

Core::LinkStatus Core::addPeer(Socket *sock, const Identifier &peering, bool async)
{
	const double timeout = milliseconds(Config::Get("tpot_read_timeout").toInt());
	sock->setTimeout(timeout);
	return addPeer(static_cast<Stream*>(sock), sock->getRemoteAddress(), peering, async);
}

bool Core::hasPeer(const Identifier &peering)
{
	Synchronize(this);
	return mHandlers.contains(peering);
}

bool Core::getInstancesNames(const Identifier &peering, Array<String> &array)
{
	array.clear();
	
	Map<Identifier,Handler*>::iterator it = mHandlers.lower_bound(peering);
	if(it == mHandlers.end() || it->first != peering) return false;
		
	while(it != mHandlers.end() && it->first == peering)
	{
		String name = it->first.getName();
		if(name.empty()) name = "default";
		array.push_back(name);
		++it;
	}
	
	return true;
}

bool Core::isRequestSeen(const Request *request)
{
	Synchronize(this);
	
	String uid;
	request->mParameters.get("uid", uid);
	if(!uid.empty())
	{
		if(mSeenRequests.contains(uid)) return true;
		mSeenRequests.insert(uid);
		
		class RemoveSeenTask : public Task
		{
		public:
			RemoveSeenTask(const String &uid) { this->uid = uid; }
			
			void run(void)
			{
				Synchronize(Core::Instance);
				Core::Instance->mSeenRequests.erase(uid);
				delete this;
			}
			
		private:
			String uid;
		};
		
		Scheduler::Global->schedule(new RemoveSeenTask(uid), 60000);
		return false;
	}
	
	return false;
}

void Core::run(void)
{
	LogDebug("Core", "Starting...");
	
	try {
		while(true)
		{
			Thread::Sleep(0.01);

			Socket *sock = new Socket;
			mSock.accept(*sock);
			
			try {
				Address addr;
				const size_t peekSize = 5;	
				char peekData[peekSize];
				
				try {
					addr = sock->getRemoteAddress();
					LogDebug("Core::run", "Incoming connection from " + addr.toString());
					
					if(addr.isPublic() && addr.isIpv4()) // TODO: isPublicConnectable() currently reports state for ipv4 only
						mLastPublicIncomingTime = Time::Now();

					sock->setTimeout(milliseconds(Config::Get("tpot_timeout").toInt()));
					if(sock->peekData(peekData, peekSize) != peekSize)
						continue;
		
					sock->setTimeout(milliseconds(Config::Get("tpot_read_timeout").toInt()));
				}
				catch(const std::exception &e)
				{
					delete sock;
					throw;
				}
			
				Stream *bs = NULL;
				
				if(std::memcmp(peekData, "GET ", 4) == 0
					|| std::memcmp(peekData, "POST ", 5) == 0)
				{
					// This is HTTP, forward connection to HttpTunnel
					bs = HttpTunnel::Incoming(sock);
					if(!bs) continue;
				}
				else {
					bs = sock;
				}
				
				LogInfo("Core", "Incoming peer from " + addr.toString() + " (tunnel=" + (bs != sock ? "true" : "false") + ")");
				addPeer(bs, addr, Identifier::Null, true);	// async
			}
			catch(const std::exception &e)
			{
				LogDebug("Core::run", String("Processing failed: ") + e.what());
			}
		}
	}
	catch(const NetException &e)
	{
		LogDebug("Core::run", e.what());
	}
	
	LogDebug("Core", "Finished");
}

bool Core::sendNotification(const Notification &notification)
{
	Synchronize(this);
	
	Array<Identifier> identifiers;
	
	if(notification.mPeering == Identifier::Null)
	{
		mHandlers.getKeys(identifiers);
	}
	else {
		Map<Identifier,Handler*>::iterator it = mHandlers.lower_bound(notification.mPeering);
		if(it == mHandlers.end() || it->first != notification.mPeering) return false;
			
		Array<Handler*> handlers;
		while(it != mHandlers.end() && it->first == notification.mPeering)
		{
			identifiers.push_back(it->first);
			++it;
		}
	}
	
	for(int i=0; i<identifiers.size(); ++i)
	{
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			handler->sendNotification(notification);
		}
	}
	
	return (!identifiers.empty());
}

unsigned Core::addRequest(Request *request)
{
	Synchronize(this);

	{
		Synchronize(request);
		if(!request->mId)
			request->mId = ++mLastRequest;
		
		String uid;
		request->mParameters.get("uid", uid);
		if(uid.empty())
		{
			uid = String::random(16);
			request->mParameters["uid"] = uid;
		}
		
		mSeenRequests.insert(uid);
	}

	Array<Identifier> identifiers;
	if(request->mReceiver == Identifier::Null)
	{
		mHandlers.getKeys(identifiers);
	}
	else {
		Map<Identifier,Handler*>::iterator it = mHandlers.lower_bound(request->mReceiver);
		if(it == mHandlers.end() || it->first != request->mReceiver)
			throw Exception("Request receiver is not connected");
		
		while(it != mHandlers.end() && it->first == request->mReceiver)
		{
			identifiers.push_back(it->first);
			++it;
		}
	}

	if(identifiers.empty()) request->notifyAll();
	else for(int i=0; i<identifiers.size(); ++i)
	{
		if(identifiers[i] == request->mNonReceiver)
			continue;
		
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			handler->addRequest(request);
		}
	}
	
	return request->mId;
}

void Core::removeRequest(unsigned id)
{
	Synchronize(this);
  
	Array<Identifier> identifiers;
	mHandlers.getKeys(identifiers);
	
	for(int i=0; i<identifiers.size(); ++i)
	{
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			handler->removeRequest(id);
		}
	}
}

bool Core::addHandler(const Identifier &peer, Core::Handler *handler)
{
	Assert(handler != NULL);
	Synchronize(this);
	
	Handler *h = NULL;
	if(mHandlers.get(peer, h))
		return (h == handler);

	mHandlers.insert(peer, handler);
	return true;
}

bool Core::removeHandler(const Identifier &peer, Core::Handler *handler)
{
	Assert(handler != NULL);
	Synchronize(this);
  
	Handler *h = NULL;
	if(!mHandlers.get(peer, h) || h != handler)
		return false;
	
	mHandlers.erase(peer);
	return true;
}

void Core::Handler::sendCommand(Stream *stream, const String &command, const String &args, const StringMap &parameters)
{
	String line;
	line << command << " " << args.lineEncode();
	LogTrace("Core::Handler", "<< " + line);
	
	line << Stream::NewLine;
  	*stream << line;

	for(	StringMap::const_iterator it = parameters.begin();
		it != parameters.end();
		++it)
	{
	  	line.clear();
		line << it->first << ": " << it->second.lineEncode();
		LogTrace("Core::Handler", "<< " + line);
		line << Stream::NewLine;
		*stream << line;
	}
	*stream << Stream::NewLine;
}
		
bool Core::Handler::recvCommand(Stream *stream, String &command, String &args, StringMap &parameters)
{
	command.clear();
	if(!stream->readLine(command)) return false;
	LogTrace("Core::Handler", ">> " + command);
	
	args = command.cut(' ').lineDecode();
	command = command.toUpper();
	
	parameters.clear();
	while(true)
	{
		String name;
		AssertIO(stream->readLine(name));
		if(name.empty()) break;
		LogTrace("Core::Handler", ">> " + name);
		
		String value = name.cut(':');
		name.trim();
		value.trim();
		name = name.toLower();	// TODO: backward compatibility, should be removed
		parameters.insert(name,value.lineDecode());
	}
	
	return true;
}

Core::Handler::Handler(Core *core, Stream *bs, const Address &remoteAddr) :
	mCore(core),
	mRawStream(bs),
	mStream(NULL),
	mRemoteAddr(remoteAddr),
	mSender(NULL),
	mIsIncoming(true),
	mLinkStatus(Disconnected),
	mThreadPool(0, 1, 8),
	mStopping(false)
{

}

Core::Handler::~Handler(void)
{	
	if(mStream)
	{
		
	}
	
	delete mSender;
	delete mStream;
	delete mRawStream;
}

void Core::Handler::setPeering(const Identifier &peering)
{
	Synchronize(this);
	mPeering = peering;
	if(peering == Identifier::Null) mIsIncoming = true;
	else {
		mIsIncoming = false;
		if(mPeering.getName().empty())
		{
			LogWarn("Core::Handler", "setPeering() called with undefined instance");
			mPeering.setName("default");
		}
	}
}

void Core::Handler::setStopping(void)
{
	Synchronize(this);
	mStopping = true;
}

void Core::Handler::sendNotification(const Notification &notification)
{
	{
		Synchronize(this);
		if(mStopping) return;
		
		//LogDebug("Core::Handler", "Sending notification");
	}
	
	if(mSender)
	{
		Synchronize(mSender);
		mSender->mNotificationsQueue.push(notification);
		mSender->notify();
	}
}

void Core::Handler::addRequest(Request *request)
{
	{
		Synchronize(this);
		if(mStopping) return;
		
		//LogDebug("Core::Handler", "Adding request " + String::number(request->mId));
		
		request->addPending(mPeering);
		mRequests.insert(request->mId, request);
	}
	
	if(mSender)
	{
		Synchronize(mSender);
		
		Sender::RequestInfo requestInfo;
		requestInfo.id = request->mId;
		requestInfo.target = request->target();
		requestInfo.parameters = request->mParameters;
		requestInfo.isData = request->mIsData;
		mSender->mRequestsQueue.push(requestInfo);
	
		mSender->notify();
	}
}

void Core::Handler::removeRequest(unsigned id)
{
	Synchronize(this);
	if(mStopping) return;
	
	Map<unsigned, Request*>::iterator it = mRequests.find(id);
	if(it != mRequests.end())
	{
		//LogDebug("Core::Handler", "Removing request " + String::number(id));
	
		Request *request = it->second;
		Synchronize(request);
	
		for(int i=0; i<request->responsesCount(); ++i)
		{
			Request::Response *response = request->response(i);
			if(response->mChannel) mResponses.erase(response->mChannel);
		}
		
		request->removePending(mPeering);
		mRequests.erase(it);
	}
	
	
}

bool Core::Handler::isIncoming(void) const
{
	return mIsIncoming;
}

bool Core::Handler::isEstablished(void) const
{
	return (mLinkStatus == Established || mLinkStatus == Authenticated);
}

bool Core::Handler::isAuthenticated(void) const
{
	return (mLinkStatus == Authenticated);
}

Core::LinkStatus Core::Handler::linkStatus(void) const
{
	return mLinkStatus;
}

void Core::Handler::process(void)
{
	String command, args;
	StringMap parameters;
  
	try {
		Synchronize(this);
		LogDebug("Core::Handler", "Starting...");
	  
		// Set up obfuscation cipher
		String magicString = "TeapotNet";	// Attention: upper case 'T' and 'N'
		byte digest[CryptoPP::SHA512::DIGESTSIZE];
		CryptoPP::SHA512().CalculateDigest(digest, magicString.bytes(), magicString.size());
		Cipher *cipher = new Cipher(mRawStream);
		cipher->setReadCipher( new CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption(digest, 32, digest+32));
		cipher->setWriteCipher(new CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption(digest, 32, digest+32));
		cipher->dumpStream(&mObfuscatedHello);
		mStream = cipher;
		
		BinaryString nonce_a, salt_a, iv_a;
		nonce_a.writeBinary(uint32_t(Time::Now()));	// 32 bits
		nonce_a.writeRandom(28);			// total 256 bits
		salt_a.writeRandom(32);				// 256 bits
		iv_a.writeRandom(16);				// 128 bits		

		if(!mIsIncoming)	
		{
			LogDebug("Handler", "Initiating handshake...");

			if(SynchronizeTest(mCore, !mCore->mPeerings.get(mPeering, mRemotePeering)))
				throw Exception("Peering is not registered: " + mPeering.toString());
			
			mRemotePeering.setName(mCore->getName());
			
			args.clear();
			args << mRemotePeering;
			parameters.clear();
			parameters["application"] << APPNAME;
			parameters["version"] << APPVERSION;
			parameters["nonce"] << nonce_a;
			parameters["instance"] << mPeering.getName();
			parameters["relay"] << false;
			sendCommand(mStream, "H", args, parameters);
		}

		DesynchronizeStatement(this, AssertIO(recvCommand(mStream, command, args, parameters)));
		if(command == "Q") return;
		if(command != "H") throw Exception("Unexpected command: " + command);
		
		mLinkStatus = Established;	// Established means we got a response

		String appname, appversion, instance;
		Identifier peering;
		BinaryString nonce_b;
		args >> peering;
		parameters["application"] >> appname;
		parameters["version"] >> appversion;
		parameters["nonce"] >> nonce_b;
		parameters.get("instance", instance);
		
		bool relayEnabled;
		if(mIsIncoming) relayEnabled = Config::Get("relay_enabled").toBool();
		else relayEnabled = (!parameters.contains("relay") || parameters["relay"].toBool());
		
		if(!mIsIncoming && mPeering != peering) 
			throw Exception("Peering in response does not match");
		
		if(mIsIncoming)
		{
			mPeering = peering;
			
			if(mPeering.getName().empty())
			{
				LogWarn("Core::Handler", "Got peering with undefined instance");
				mPeering.setName("default");
			}
	
			if((!instance.empty() && instance != mCore->getName())
				|| SynchronizeTest(mCore, !mCore->mPeerings.get(mPeering, mRemotePeering)))
			{
				Desynchronize(this);
				
				Synchronizable *meeting = &mCore->mMeetingPoint;
				Synchronize(meeting);
			 
				if(!Config::Get("relay_enabled").toBool()) 
				{
					Desynchronize(meeting);
					sendCommand(mStream, "Q", String::number(NotFound), StringMap());
					return;
				}
			 
				const double meetingStepTimeout = milliseconds(std::min(Config::Get("meeting_timeout").toInt()/3, Config::Get("request_timeout").toInt()));
				double timeout = meetingStepTimeout;
				while(timeout > 0.)
				{
					if(SynchronizeTest(mCore, mCore->mRedirections.contains(mPeering))) break;
					if(!meeting->wait(timeout)) break;
				}
				
				Handler *handler = NULL;
				if(SynchronizeTest(mCore, mCore->mRedirections.get(mPeering, handler)))
				{
					if(handler)
					{
						Desynchronize(meeting);
						LogDebug("Core::Handler", "Connection already forwarded");
						sendCommand(mStream, "Q", String::number(RedirectionExists), StringMap());
						return;
					}
					
					//Log("Core::Handler", "Reached forwarding meeting point");
					SynchronizeStatement(mCore, mCore->mRedirections.insert(mPeering, this));
					meeting->notifyAll();
					
					timeout = meetingStepTimeout;
					while(timeout > 0.)
					{
						if(!mStream) break;
						if(!meeting->wait(timeout)) break;
					}
					
					SynchronizeStatement(mCore, mCore->mRedirections.erase(mPeering));
					if(mStream) sendCommand(mStream, "Q", String::number(RedirectionFailed), StringMap());
					return;
				}
				
				LogDebug("Core::Handler", "Got non local peering, asking peers");
				
				String adresses;
				List<Address> list;
				Config::GetExternalAddresses(list);
				for(	List<Address>::iterator it = list.begin();
					it != list.end();
					++it)
				{
					if(!adresses.empty()) adresses+= ',';
					adresses+= it->toString();
				}
					
				Request request(String("peer:") + mPeering.toString(), false);
				request.setParameter("adresses", adresses);
				if(!instance.empty()) request.setParameter("instance", instance);
				request.submit();
				request.wait(meetingStepTimeout);
				request.cancel();

				String remote;
				for(int i=0; i<request.responsesCount(); ++i)
				{
					if(!request.response(i)->error() && request.response(i)->parameter("remote", remote))
						break;
				}
				
				if(remote.empty())
				{
					Desynchronize(meeting);
					sendCommand(mStream, "Q", String::number(NotFound), StringMap());
					return;
				}
				
				LogDebug("Core::Handler", "Got positive response for peering");
					
				remote >> mRemotePeering;
				SynchronizeStatement(mCore, mCore->mRedirections.insert(mRemotePeering, NULL));
				
				Handler *otherHandler = NULL;
				
				meeting->notifyAll();
				timeout = meetingStepTimeout;
				while(timeout > 0.)
				{
					SynchronizeStatement(mCore, mCore->mRedirections.get(mRemotePeering, otherHandler));
					if(otherHandler) break;
					if(!meeting->wait(timeout)) break;
				}
			
				if(otherHandler && otherHandler->mStream)
				{
					Assert(otherHandler->mRawStream);
					
					Stream     *otherStream    = otherHandler->mStream;
					Stream *otherRawStream = otherHandler->mRawStream;
					otherHandler->mStream      = NULL;
					otherHandler->mRawStream   = NULL;
					
					mRawStream->writeBinary(otherHandler->mObfuscatedHello);
					otherRawStream->writeBinary(mObfuscatedHello);
					mObfuscatedHello.clear();
					otherHandler = NULL;
					meeting->notifyAll();
					
					if(mRawStream)
					{
						Desynchronize(meeting);
						LogInfo("Core::Handler", "Successfully forwarded connection");
					
						// Transfer
						Stream::Transfer(mRawStream, otherRawStream);
					}
					
					delete otherStream;
					delete otherRawStream;
				}
				else {
					Desynchronize(meeting);
					LogWarn("Core::Handler", "No other handler reached forwarding meeting point");
					sendCommand(mStream, "Q", String::number(RedirectionFailed), StringMap());
				}
					
				SynchronizeStatement(mCore, mCore->mRedirections.erase(mPeering));	
				return;
			}
			
			if(mPeering == mRemotePeering && mPeering.getName() == mCore->getName())
				throw Exception("Tried to connect same user on same instance");
			
			args.clear();
			args << mRemotePeering;
			parameters.clear();
			parameters["application"] << APPNAME;
			parameters["version"] << APPVERSION;
			parameters["nonce"] << nonce_a;
			parameters["instance"] << mPeering.getName();
			parameters["relay"] << relayEnabled;
			sendCommand(mStream, "H", args, parameters);
		}
		
		cipher->dumpStream(NULL);
		mObfuscatedHello.clear();
		
		BinaryString secret;
		if(SynchronizeTest(mCore, !mCore->mSecrets.get(peering, secret)))
			throw Exception(String("Warning: No secret for peering: ") + peering.toString());

		// Derivate session key
		byte key_a[64];
		CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA512>().DeriveKey(
			key_a, 64,
			byte(0),		// "purpose" ?
			secret.bytes(), secret.size(),
			salt_a.bytes(), salt_a.size(),
			10000);
		
		const byte *authkey_a  = key_a;
                const byte *cryptkey_a = key_a + 32;
		
		// Compute HMAC
		byte mac_a[64];
		CryptoPP::HMAC<CryptoPP::SHA512> hmac_a(authkey_a, 32);
		CryptoPP::ArraySource(nonce_b.bytes(), nonce_b.size(), true, new CryptoPP::HashFilter(hmac_a, new CryptoPP::ArraySink(mac_a, 64)));
		
		parameters.clear();
		parameters["method"] << "DIGEST";
		parameters["cipher"] << "AES256";
		parameters["salt"] << salt_a;
		parameters["init"] << iv_a;
		sendCommand(mStream, "A", ByteArray(mac_a, 64).toString(), parameters);
		
		DesynchronizeStatement(this, AssertIO(recvCommand(mStream, command, args, parameters)));
		if(command == "Q") return;
		if(command != "A") throw Exception("Unexpected command: " + command);
		
		String strMethod = "DIGEST";
		String strCipher = "AES256";
		if(parameters.get("method", strMethod)) strMethod = strMethod.toUpper();
		if(parameters.get("cipher", strCipher)) strCipher = strCipher.toUpper();

		// Only one method is supported for now
		if(strMethod != "DIGEST") throw Exception("Unknown authentication method: " + strMethod);
		if(strCipher != "AES256") throw Exception("Unknown authentication cipher: " + strCipher);
		
		BinaryString test_b, salt_b, iv_b;
		args >> test_b;
		parameters["salt"] >> salt_b;
		parameters["init"] >> iv_b;		

		// Derivate remote session key
		byte key_b[64];
		CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA512>().DeriveKey(
			key_b, 64,
			byte(0),		// "purpose" ?
			secret.bytes(), secret.size(),
			salt_b.bytes(), salt_b.size(),
			10000);
		
		const byte *authkey_b  = key_b;
                const byte *cryptkey_b = key_b + 32;

		// Verify remote HMAC
		byte mac_b[64];
		CryptoPP::HMAC<CryptoPP::SHA512> hmac_b(authkey_b, 32);

		//const int verificationFlags = CryptoPP::HashVerificationFilter::THROW_EXCEPTION | CryptoPP::HashVerificationFilter::HASH_AT_END;
		//CryptoPP::StringSource(nonce_a + test_b, true, new CryptoPP::HashVerificationFilter(hmac_b, NULL, verificationFlags));

		bool result = false;
		CryptoPP::StringSource(test_b + nonce_a, true, new CryptoPP::HashVerificationFilter(hmac_b, new CryptoPP::ArraySink((byte*)&result, sizeof(result))));
		if(!result) throw Exception("Authentication failed (remote="+appversion+")");
		
		LogInfo("Core::Handler", "Authentication successful: " + mPeering.getName() + " (remote="+appversion+")");
		mLinkStatus = Authenticated;

		// Set up the new cipher for the connection
		Assert(iv_a.size() >= 16);
		Assert(iv_b.size() >= 16);
		cipher->setReadCipher( new CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption(cryptkey_b, 32, reinterpret_cast<const byte*>(iv_b.data())));
		cipher->setWriteCipher(new CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption(cryptkey_a, 32, reinterpret_cast<const byte*>(iv_a.data())));
		
		if(!mIsIncoming && relayEnabled && mRemoteAddr.isPublic())
		{
			Synchronize(mCore);
			LogDebug("Core::Handler", "Found potential relay " + mRemoteAddr.toString());
			if(mCore->mKnownPublicAddresses.contains(mRemoteAddr)) mCore->mKnownPublicAddresses[mRemoteAddr] += 1;
			else mCore->mKnownPublicAddresses[mRemoteAddr] = 1;
		}
	}
	catch(const IOException &e)
	{
		LogDebug("Core::Handler", "Handshake aborted");
		return;
	}
	catch(const std::exception &e)
	{
		LogWarn("Core::Handler", String("Handshake failed: ") + e.what()); 
		return;
	}
	
	Identifier peering;
	SynchronizeStatement(this, peering = mPeering);
	
	try {
		// Register the handler
                if(!mCore->addHandler(mPeering, this))
                {
                        LogInfo("Core::Handler", "Duplicate handler for the peering, exiting.");
                        return;
                }
		
		// WARNING: Do not simply return after this point, sender is starting
		notifyAll();
		
		// Start the sender
		mSender = new Sender;
		mSender->mStream = mStream;
		mSender->start();
		Thread::Sleep(0.1);
		
		Listener *listener = NULL;
		if(SynchronizeTest(mCore, mCore->mListeners.get(peering, listener)))
		{
			try {
				listener->connected(peering, mIsIncoming);
			}
			catch(const Exception &e)
			{
				LogWarn("Core::Handler", String("Listener connected callback failed: ")+e.what());
			}
		}

		// Main loop
		LogDebug("Core::Handler", "Entering main loop");
		while(recvCommand(mStream, command, args, parameters))
		{
			Synchronize(this);

			if(command == "K")	// Keep Alive
			{
				String dummy;
				Assert(args.read(dummy));
			}
			else if(command == "R")	// Response
			{
				unsigned id;
				int status;
				unsigned channel;
				Assert(args.read(id));
				Assert(args.read(status));
				Assert(args.read(channel));
				
				Request *request;
				if(mRequests.get(id,request))
				{
					Synchronize(request);
				  
				  	Request::Response *response;
					if(channel)
					{
						//LogDebug("Core::Handler", "Received response for request "+String::number(id)+", status "+String::number(status)+", receiving on channel "+String::number(channel));
	
						Stream *sink = NULL;
						if(request->mContentSink)
						{
							if(!request->hasContent())
								sink = request->mContentSink;
						}
						else sink = new TempFile;
						
						response = new Request::Response(status, parameters, sink);
						response->mChannel = channel;
						if(sink) 
						{
							mResponses.insert(channel, response);
							mCancelled.clear();
						}
					}
					else {
						//LogDebug("Core::Handler", "Received response for request "+String::number(id)+", status "+String::number(status)+", no data");
						response = new Request::Response(status, parameters);
					}

					response->mPeering = peering;
					request->addResponse(response);
					if(response->status() != Request::Response::Pending) 
						request->removePending(peering);	// this triggers the notification
				}
				else LogDebug("Core::Handler", "Received response for unknown request "+String::number(id));
			}
			else if(command == "D")	// Data block
			{
				unsigned channel;
				Assert(args.read(channel));
				
				unsigned size = 0;
				if(parameters.contains("length")) parameters["length"].extract(size);

				Request::Response *response;
				if(mResponses.get(channel,response))
				{
				 	Assert(response->content());
					if(size) {
					  	size_t len = mStream->readData(*response->content(), size);
						if(len != size) throw IOException("Incomplete data chunk");
					}
					else {
						LogDebug("Core::Handler", "Finished receiving on channel "+String::number(channel));
						response->content()->close();
						response->mStatus = Request::Response::Finished;
						mResponses.erase(channel);
					}
				}
				else {
					AssertIO(mStream->ignore(size));
					
					if(mCancelled.find(channel) == mCancelled.end())
					{
						mCancelled.insert(channel);
					  
						args.clear();
						args.write(channel);
						parameters.clear();
						
						Desynchronize(this);
						LogDebug("Core::Handler", "Sending cancel on channel "+String::number(channel));
						SynchronizeStatement(mSender, Handler::sendCommand(mStream, "C", args, parameters));
					}
				}
			}
			else if(command == "E")	// Error
			{
				unsigned channel;
				int status;
				Assert(args.read(channel));
				Assert(args.read(status));
				
				Request::Response *response;
				if(mResponses.get(channel,response))
				{
				 	Assert(response->content() != NULL);
					
					LogDebug("Core::Handler", "Error on channel "+String::number(channel)+", status "+String::number(status));
					
					Assert(status > 0);
				
					response->mStatus = status;
					response->content()->close();
					mResponses.erase(channel);
				}
				//else LogDebug("Core::Handler", "Received error for unknown channel "+String::number(channel));
			}
			else if(command == "C")	// Cancel
			{
				unsigned channel;
				Assert(args.read(channel));
				
				Synchronize(mSender);
				Request::Response *response;
				if(mSender->mTransferts.get(channel, response))
				{
					LogDebug("Core::Handler", "Received cancel for channel "+String::number(channel));
					response->mTransfertFinished = true;
					mSender->mTransferts.erase(channel);
				}
				//else LogDebug("Core::Handler", "Received cancel for unknown channel "+String::number(channel));
			}
			else if(command == "I" || command == "G") // Request
			{
			  	unsigned id;
				Assert(args.read(id));
				String &target = args;
			  	LogDebug("Core::Handler", "Received request "+String::number(id));

				Request *request = new Request(target, (command == "G"));
				request->setParameters(parameters);
				request->mRemoteId = id;
				request->mRemoteAddr = mRemoteAddr;
				
				if(mCore->isRequestSeen(request))
				{
					request->addResponse(new Request::Response(Request::Response::AlreadyResponded));
		
					Synchronize(mSender);	
					mSender->mRequestsToRespond.push_back(request);
					request->mResponseSender = mSender;
					mSender->notify();
					continue;
				}
				
				Listener *listener = NULL;
				if(!SynchronizeTest(mCore, mCore->mListeners.get(peering, listener)))
				{
					LogDebug("Core::Handler", "No listener for request " + String::number(id));
				}
				
				class RequestTask : public Task
				{
				public:
					RequestTask(	const Identifier &peering,
							Listener *listener,
							Request *request, 
							Sender *sender)
					{
						this->peering = peering;
						this->listener = listener;
						this->request = request;
						this->sender = sender;
					}
					
					void run(void)
					{
						try {
							if(listener) listener->request(peering, request);
						}
						catch(const Exception &e)
						{
							LogWarn("RequestTask::run", String("Listener failed to process request "+String::number(request->mRemoteId)+": ") + e.what()); 
						}
						
						try {
							{
								Synchronize(request);
								
								if(request->responsesCount() == 0) 
									request->addResponse(new Request::Response(Request::Response::Failed));
					
								for(int i=0; i<request->responsesCount(); ++i)
								{
									Request::Response *response = request->response(i);
									response->mTransfertStarted = false;
									response->mTransfertFinished = false;
								}
							}
							
							{
								Synchronize(sender);
								
								sender->mRequestsToRespond.push_back(request);
								request->mResponseSender = sender;
								sender->notify();
							}
						}
						catch(const Exception &e)
						{
							LogWarn("RequestTask::run", e.what()); 
						}
						
						delete this;	// autodelete
					}
					
				private:
					Identifier peering;
					Listener *listener;
					Request *request;
					Sender  *sender;
				};
				
				mThreadPool.launch(new RequestTask(peering, listener, request, mSender));
			}
			else if(command == "M")
			{
				unsigned length = 0;
				if(parameters.contains("length")) 
				{
					parameters["length"].extract(length);
					parameters.erase("length");
				}
			  
				//LogDebug("Core::Handler", "Received notification");
				
				Notification notification;
				notification.setParameters(parameters);
				notification.mPeering = mPeering;
				notification.mContent.reserve(length);
				mStream->read(notification.mContent, length);
				
				Listener *listener = NULL;
				if(!SynchronizeTest(mCore, mCore->mListeners.get(peering, listener)))
				{
					LogDebug("Core::Handler", "No listener, dropping notification");
				}
				else {
					try {
						Desynchronize(this);
						if(!listener->notification(peering, &notification)) break;
					}
					catch(const Exception &e)
					{
						LogWarn("Core::Handler", String("Listener failed to process the notification: ") + e.what());
					}
				}
			}
			else if(command == "Q")	// Optionnal, closing the connection is sufficient
			{
				break;
			}
			else {
				LogWarn("Core::Handler", "Unknown command: " + command);

				unsigned length = 0;
				if(parameters.contains("length")) parameters["length"].extract(length);
				if(length) AssertIO(mStream->ignore(length));
			}
			
			if(mStopping) break;
		}

		LogDebug("Core::Handler", "Finished");
	}
	catch(const std::exception &e)
	{
		LogWarn("Core::Handler", e.what()); 
	}

	// Wait for tasks to finish
	mThreadPool.clear();
	
	try {
		Synchronize(this);

		mStopping = true;

		for(Map<unsigned, Request::Response*>::iterator it = mResponses.begin();
			it != mResponses.end();
			++it)
		{
			it->second->mStatus = Request::Response::Interrupted;
			it->second->content()->close();
		}

		for(Map<unsigned, Request*>::iterator it = mRequests.begin();
			it != mRequests.end();
			++it)
		{
			it->second->removePending(mPeering);
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Handler", e.what());
	}
	
	try {
		Synchronize(mCore);
		
		mCore->removeHandler(mPeering, this);
		 
		if(mCore->mKnownPublicAddresses.contains(mRemoteAddr))
		{
			mCore->mKnownPublicAddresses[mRemoteAddr]-= 1;
			if(mCore->mKnownPublicAddresses[mRemoteAddr] == 0)
				mCore->mKnownPublicAddresses.erase(mRemoteAddr);
		}
	}
	catch(const std::exception &e)
	{
		LogError("Core::Handler", e.what()); 
	}
	
	Listener *listener = NULL;
	if(SynchronizeTest(mCore, mCore->mListeners.get(peering, listener)))
	{
		try {
			listener->disconnected(peering);
		}
		catch(const Exception &e)
		{
			LogWarn("Core::Handler", String("Listener disconnected callback failed: ") + e.what());
		}
	}
	
	// Stop the sender
	if(mSender && mSender->isRunning())
	{
		SynchronizeStatement(mSender, mSender->mShouldStop = true);
		mSender->notify();
		mSender->join();	
	}
}

void Core::Handler::run(void)
{
	try {
		process();
	}
	catch(const std::exception &e)
	{
		LogWarn("Core::Handler::run", String("Unhandled exception: ") + e.what()); 
	}
	catch(...)
	{
		LogWarn("Core::Handler::run", String("Unhandled unknown exception")); 
	}
		
	notifyAll();
	Thread::Sleep(5.);	// TODO
	delete this;		// autodelete
}

const size_t Core::Handler::Sender::ChunkSize = BufferSize;

Core::Handler::Sender::Sender(void) :
		mLastChannel(0),
		mShouldStop(false)
{

}

Core::Handler::Sender::~Sender(void)
{
	try {
		Map<unsigned, Request::Response*>::iterator it = mTransferts.begin();
		while(it != mTransferts.end())
		{
			int status = Request::Response::Interrupted;	
			String args;
			args << it->first << status;
			Handler::sendCommand(mStream, "E", args, StringMap());
			++it;
		}
	}
	catch(const NetException &e)
	{
		// Nothing to do, the other side will close the transferts anyway
	}
	
	for(int i=0; i<mRequestsToRespond.size(); ++i)
		delete mRequestsToRespond[i];
}

void Core::Handler::Sender::run(void)
{
	try {
		LogDebug("Core::Handler::Sender", "Starting");
		Assert(mStream);
		
		const double readTimeout = milliseconds(Config::Get("tpot_read_timeout").toInt());
		
		while(true)
		{
			Synchronize(this);
			if(mShouldStop) break;

			if(mNotificationsQueue.empty()
				&& mRequestsQueue.empty()
			  	&& mTransferts.empty())
			{
				// Keep Alive
				String args;
				args << unsigned(cryptrand());
				StringMap parameters;
				DesynchronizeStatement(this, Handler::sendCommand(mStream, "K", args, parameters));

				//LogDebug("Core::Handler::Sender", "No pending tasks, waiting");
				wait(readTimeout/2);
				if(mShouldStop) break;
			}
			
			for(int i=0; i<mRequestsToRespond.size(); ++i)
			{
				Request *request = mRequestsToRespond[i];
				Synchronize(request);
				
				for(int j=0; j<request->responsesCount(); ++j)
				{
					Request::Response *response = request->response(j);
					if(!response->mTransfertStarted)
					{
						unsigned channel = 0;

						response->mTransfertStarted = true;
						if(!response->content()) response->mTransfertFinished = true;
						else {
							++mLastChannel;
							channel = mLastChannel;
							
							LogDebug("Core::Handler::Sender", "Start sending on channel "+String::number(channel));
							mTransferts.insert(channel,response);
						}
						
						LogDebug("Core::Handler::Sender", "Sending response " + String::number(j) + " for request " + String::number(request->mRemoteId));
						
						int status = response->status();
						if(status == Request::Response::Success && j != request->responsesCount()-1)
							status = Request::Response::Pending;
						
						String args;
						args << request->mRemoteId << " " << status << " " <<channel;
						DesynchronizeStatement(this, Handler::sendCommand(mStream, "R", args, response->mParameters));
					}
				}
			}
			
			if(!mNotificationsQueue.empty())
			{
				const Notification &notification = mNotificationsQueue.front();
				unsigned length = notification.content().size();
				
				//LogDebug("Core::Handler::Sender", "Sending notification");

				String args = "";
				StringMap parameters = notification.parameters();
				parameters["length"] << length;
				
				DesynchronizeStatement(this, Handler::sendCommand(mStream, "M", args, parameters));
				DesynchronizeStatement(this, mStream->write(notification.mContent));
				mNotificationsQueue.pop();
			}
			  
			if(!mRequestsQueue.empty())
			{
				const RequestInfo &request = mRequestsQueue.front();
				//LogDebug("Core::Handler::Sender", "Sending request "+String::number(request.id));
				
				String command;
				if(request.isData) command = "G";
				else command = "I";
				
				String args;
				args << request.id << " " << request.target;
				DesynchronizeStatement(this, Handler::sendCommand(mStream, command, args, request.parameters));
				
				mRequestsQueue.pop();
			}

			Array<unsigned> channels;
			mTransferts.getKeys(channels);
			
			for(int i=0; i<channels.size(); ++i)
			{
				SyncYield(this);
			  
				// Check for tasks with higher priority
				if(!mNotificationsQueue.empty()
				|| !mRequestsQueue.empty())
					break;
			  	
				for(int j=0; j<mRequestsToRespond.size(); ++j)
				{
					Synchronize(mRequestsToRespond[j]);
					for(int k=0; k<mRequestsToRespond[j]->responsesCount(); ++k)
						if(!mRequestsToRespond[j]->response(k)->mTransfertStarted) 
							break;
				}
				
				unsigned channel = channels[i];
				Request::Response *response;
				if(!mTransferts.get(channel, response)) continue;
				
				char buffer[ChunkSize];
				size_t size = 0;
				
				try {
					Stream *content = response->content();
					size = content->readData(buffer, ChunkSize);
				}
				catch(const Exception &e)
				{
					LogWarn("Core::Handler::Sender", "Error on channel " + String::number(channel) + ": " + e.what());
					
					response->mTransfertFinished = true;
					mTransferts.erase(channel);
					
					String args;
					args << channel << " " << Request::Response::ReadFailed;
					StringMap parameters;
					parameters["notification"] = e.what();
					DesynchronizeStatement(this, Handler::sendCommand(mStream, "E", args, parameters));
					continue;
				}

				String args;
				args << channel;
				StringMap parameters;
				parameters["length"] << size;
				DesynchronizeStatement(this, Handler::sendCommand(mStream, "D", args, parameters));

				if(size == 0)
				{
					LogDebug("Core::Handler::Sender", "Finished sending on channel "+String::number(channel));
					response->mTransfertFinished = true;
					mTransferts.erase(channel);
				}
				else {
				 	DesynchronizeStatement(this, mStream->writeData(buffer, size));
				}
			}
			
			for(int i=0; i<mRequestsToRespond.size(); ++i)
			{
				Request *request = mRequestsToRespond[i];
				
				{
					Synchronize(request);
					
					if(request->isPending()) continue;

					bool finished = true;
					for(int j=0; j<request->responsesCount(); ++j)
					{
						Request::Response *response = request->response(j);
						finished&= response->mTransfertFinished;
					}

					if(!finished) continue;
				}
				
				if(!request->mId) delete request;
				mRequestsToRespond.erase(i);
			}
		}
		
		LogDebug("Core::Handler::Sender", "Finished");
	}
	catch(const std::exception &e)
	{
		LogError("Core::Handler::Sender", e.what()); 
	}
}

}
