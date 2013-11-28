/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/core.h"
#include "tpn/html.h"
#include "tpn/sha512.h"
#include "tpn/aescipher.h"
#include "tpn/httptunnel.h"
#include "tpn/config.h"

namespace tpn
{

Core *Core::Instance = NULL;


Core::Core(int port) :
		mSock(port),
		mLastRequest(0),
		mLastPublicIncomingTime(0)
{
	Interface::Instance->add("/peers", this);
	
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
	Interface::Instance->remove("/peers");
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
	return (Time::Now()-mLastPublicIncomingTime <= 2*3600.); 
}

void Core::registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
		       		const ByteString &secret,
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

bool Core::addPeer(ByteStream *bs, const Address &remoteAddr, const Identifier &peering, bool async)
{
	Assert(bs);
	Synchronize(this);
	
	bool hasPeering = (peering != Identifier::Null);
	
	if(hasPeering && !mPeerings.contains(peering))
		throw Exception("Added peer with unknown peering");
	
	{
		Desynchronize(this);
		LogDebug("Core", "Spawning new handler");
		Handler *handler = new Handler(this, bs, remoteAddr);
		if(hasPeering) handler->setPeering(peering);

		if(async)
		{
			handler->start(true);
			return true;
		}
		else {
			Synchronize(handler);
			handler->start(true);	// autodelete
			
			// Timeout is just a security here
			const double timeout = milliseconds(Config::Get("tpot_read_timeout").toInt());
			if(!handler->wait(timeout*4)) return false;
			return handler->isAuthenticated();
		}
	}
}

bool Core::addPeer(Socket *sock, const Identifier &peering, bool async)
{
	const double timeout = milliseconds(Config::Get("tpot_read_timeout").toInt());
	sock->setTimeout(timeout);
	return addPeer(static_cast<ByteStream*>(sock), sock->getRemoteAddress(), peering, async);
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
				Address addr = sock->getRemoteAddress();
                        	LogDebug("Core::run", "Incoming connection from " + addr.toString());
                        	if(addr.isPublic()) mLastPublicIncomingTime = Time::Now();

				// TODO: this is not a clean way to proceed
				const size_t peekSize = 5;	
				char peekData[peekSize];
				sock->setTimeout(milliseconds(Config::Get("tpot_timeout").toInt()));
				if(sock->peekData(peekData, peekSize) != peekSize)
					continue;
	
				sock->setTimeout(milliseconds(Config::Get("tpot_read_timeout").toInt()));

				ByteStream *bs = sock;
				if(std::memcmp(peekData, "GET ", 4) == 0
					|| std::memcmp(peekData, "POST ", 5) == 0)
				{
					// This is HTTP, forward connection to HttpTunnel
					bs = HttpTunnel::Incoming(sock);
					if(!bs) continue;
				}
				
				LogInfo("Core", "Incoming peer from " + addr.toString() + " (tunnel=" + (bs != sock ? "true" : "false") + ")");
				addPeer(bs, addr, Identifier::Null, true);	// async
			}
			catch(const Exception &e)
			{
				LogDebug("Core::run", String("Processing failed: ") + e.what());
				delete sock;
			}
		}
	}
	catch(const NetException &e)
	{

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
  
	request->mId = ++mLastRequest;

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
		Handler *handler;
		if(mHandlers.get(identifiers[i], handler))
		{
			Desynchronize(this);
			handler->addRequest(request);
		}
	}
	
	return mLastRequest;
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

void Core::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	
	if(prefix == "/peers")
	{
		if(request.url == "/")
		{
			Http::Response response(request, 200);
			response.send();

			Html page(response.sock);
			page.header("Peers");
			page.open("h1");
			page.text("Peers");
			page.close("h1");

			if(mHandlers.empty()) page.text("No peer...");
			else for(Map<Identifier,Handler*>::iterator it = mHandlers.begin();
							it != mHandlers.end();
							++it)
			{
				String str(it->first.toString());
				page.link(str,String("/peers/")+str);
				page.br();
			}

			page.footer();
		}
		else {
			//Identifier ident;
			//list.front() >> ident;

			// TODO
		}
	}
	else throw 404;
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

Core::Handler::Handler(Core *core, ByteStream *bs, const Address &remoteAddr) :
	mCore(core),
	mRawStream(bs),
	mStream(NULL),
	mRemoteAddr(remoteAddr),
	mSender(NULL),
	mIsIncoming(true),
	mIsAuthenticated(false),
	mStopping(false)
{

}

Core::Handler::~Handler(void)
{	
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
		
		LogDebug("Core::Handler", "Adding request " + String::number(request->id()));
		
		request->addPending(mPeering);
		mRequests.insert(request->id(), request);
	}
	
	if(mSender)
	{
		Synchronize(mSender);
		
		Sender::RequestInfo requestInfo;
		requestInfo.id = request->id();
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
		LogDebug("Core::Handler", "Removing request " + String::number(id));
	
		Request *request = it->second;
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

bool Core::Handler::isAuthenticated(void) const
{
	return mIsAuthenticated;
}

void Core::Handler::process(void)
{
	String command, args;
	StringMap parameters;
  
	try {
		Synchronize(this);
		LogDebug("Core::Handler", "Starting...");
	  
		// Set up obfuscation cipher
		ByteString tmp;
		Sha512::Hash(String("TeapotNet"), tmp);
		ByteString tmpkey, tmpiv;
		tmp.readBinary(tmpkey, 32);	// 256 bits
		tmp.readBinary(tmpiv, 16);	// 128 bits
		tmp.clear();		

		AesCipher *cipher = new AesCipher(mRawStream);
		cipher->setEncryptionKey(tmpkey);
		cipher->setEncryptionInit(tmpiv);
		cipher->setDecryptionKey(tmpkey);
		cipher->setDecryptionInit(tmpiv);
		mStream = cipher;
		
		cipher->dumpStream(&mObfuscatedHello);
		
		ByteString nonce_a, salt_a, iv_a;
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
		if(command != "H") throw Exception("Unexpected command: " + command);
		
		String appname, appversion, instance;
		Identifier peering, nonce_b;
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
				if(!Config::Get("relay_enabled").toBool()) return;
			  
				const double meetingStepTimeout = milliseconds(std::min(Config::Get("meeting_timeout").toInt()/3, Config::Get("request_timeout").toInt()));
			  
				double timeout = meetingStepTimeout;
				mCore->mMeetingPoint.lock();
				while(timeout)
				{
					if(SynchronizeTest(mCore, mCore->mRedirections.contains(mPeering))) break;
					mCore->mMeetingPoint.wait(timeout);
				}
				mCore->mMeetingPoint.unlock();
				
				Handler *handler = NULL;
				if(SynchronizeTest(mCore, mCore->mRedirections.get(mPeering, handler)))
				{
					if(handler)
					{
						LogDebug("Core::Handler", "Connection already forwarded");
					}
					else {
					  	//Log("Core::Handler", "Reached forwarding meeting point");
						SynchronizeStatement(mCore, mCore->mRedirections.insert(mPeering, this));
						mCore->mMeetingPoint.notifyAll();
						wait(meetingStepTimeout);
						SynchronizeStatement(mCore, mCore->mRedirections.erase(mPeering));
					}
		
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
				
				String remote;
				
				{
					Desynchronize(this);

					request.submit();
					request.wait(meetingStepTimeout);
						
					for(int i=0; i<request.responsesCount(); ++i)
					{
						if(!request.response(i)->error() && request.response(i)->parameter("remote", remote))
							break;
					}
				}
				
				if(!remote.empty())
				{
					LogDebug("Core::Handler", "Got positive response for peering");
						
					remote >> mRemotePeering;
					SynchronizeStatement(mCore, mCore->mRedirections.insert(mRemotePeering, NULL));
						
					Handler *otherHandler = NULL;
					
					double timeout = meetingStepTimeout;
					mCore->mMeetingPoint.lock();
					mCore->mMeetingPoint.notifyAll();
					while(timeout)
					{
						SynchronizeStatement(mCore, mCore->mRedirections.get(mRemotePeering, otherHandler));
						if(otherHandler) break;
						mCore->mMeetingPoint.wait(timeout);
					}
					mCore->mMeetingPoint.unlock();
						
					if(otherHandler)
					{
						Stream     *otherStream    = NULL;
						ByteStream *otherRawStream = NULL;
					  
						{
							Synchronize(otherHandler);
							
							otherStream    = otherHandler->mStream;
							otherRawStream = otherHandler->mRawStream;
							otherHandler->mStream      = NULL;
							otherHandler->mRawStream   = NULL;
							
							mRawStream->writeBinary(otherHandler->mObfuscatedHello);
							otherRawStream->writeBinary(mObfuscatedHello);
							mObfuscatedHello.clear();
						
							otherHandler->notifyAll();
						}
						
						otherHandler = NULL;
						
						LogInfo("Core::Handler", "Successfully forwarded connection");
						
						// Transfer
						ByteStream::Transfer(mRawStream, otherRawStream);
						
						delete otherStream;
						delete otherRawStream;
					}
					else {
						LogWarn("Core::Handler", "No other handler reached forwarding meeting point");
					}
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
		
		ByteString secret;
		if(SynchronizeTest(mCore, !mCore->mSecrets.get(peering, secret)))
			throw Exception(String("Warning: No secret for peering: ") + peering.toString());
	
		// Derivate session key	
		ByteString key_a;
                Sha512::DerivateKey(secret, salt_a, key_a, Sha512::CryptRounds);

		// Get authentication key
		ByteString authkey_a;
		key_a.readBinary(authkey_a, 32);	// 256 bits

		// Generate HMAC
		ByteString hmac_a;
		Sha512::AuthenticationCode(authkey_a, nonce_b, hmac_a);
	
		parameters.clear();
		parameters["method"] << "DIGEST";
		parameters["cipher"] << "AES256";
		parameters["salt"] << salt_a;
		parameters["init"] << iv_a;
		sendCommand(mStream, "A", hmac_a.toString(), parameters);
		
		DesynchronizeStatement(this, AssertIO(recvCommand(mStream, command, args, parameters)));
	
		if(command != "A") throw Exception("Unexpected command: " + command);
		
		String strMethod = "DIGEST";
		String strCipher = "AES256";
		if(parameters.get("method", strMethod)) strMethod = strMethod.toUpper();
		if(parameters.get("cipher", strCipher)) strCipher = strCipher.toUpper();

		// Only one method is supported for now
		if(strMethod != "DIGEST") throw Exception("Unknown authentication method: " + strMethod);
		if(strCipher != "AES256") throw Exception("Unknown authentication cipher: " + strCipher);
		
		ByteString test_b, salt_b, iv_b;
		args >> test_b;
		parameters["salt"] >> salt_b;
		parameters["init"] >> iv_b;		

		// Derivate remote session key
		ByteString key_b;
                Sha512::DerivateKey(secret, salt_b, key_b, Sha512::CryptRounds);
	
		// Get remote authentication key
		ByteString authkey_b;
                key_b.readBinary(authkey_b, 32);	// 256 bits
		Assert(key_a.size() == 32);    	 	// 256 bits

		// Generate remote HMAC
		ByteString hmac_b;
		Sha512::AuthenticationCode(authkey_b, nonce_a, hmac_b);
		
		if(mIsIncoming) Thread::Sleep(uniform(0.0, 0.5));

		if(!test_b.constantTimeEquals(hmac_b)) throw Exception("Authentication failed (remote="+appversion+")");
		LogInfo("Core::Handler", "Authentication successful: " + mPeering.getName() + " (remote="+appversion+")");
		mIsAuthenticated = true;		

		// Set up new cipher for the connection
		delete cipher;
		cipher = new AesCipher(mRawStream);
		cipher->setEncryptionKey(key_a);
		cipher->setEncryptionInit(iv_a);
		cipher->setDecryptionKey(key_b);
		cipher->setDecryptionInit(iv_b);
		mStream = cipher;
		
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
				args.read(dummy);
			}
			else if(command == "R")	// Response
			{
				unsigned id;
				int status;
				unsigned channel;
				args.read(id);
				args.read(status);
				args.read(channel);
				
				Request *request;
				if(mRequests.get(id,request))
				{
					Synchronize(request);
				  
				  	Request::Response *response;
					if(channel)
					{
						LogDebug("Core::Handler", "Received response for request "+String::number(id)+", status "+String::number(status)+", receiving on channel "+String::number(channel));
	
						ByteStream *sink = NULL;
						if(request->mContentSink)
						{
							if(!request->hasContent())
								sink = request->mContentSink;
						}
						else sink = new TempFile;	// TODO: or ByteString ?
						
						response = new Request::Response(status, parameters, sink);
						response->mChannel = channel;
						if(sink) mResponses.insert(channel, response);
						mCancelled.clear();
					}
					else {
						LogDebug("Core::Handler", "Received response for request "+String::number(id)+", status "+String::number(status)+", no data");
						response = new Request::Response(status, parameters);
					}

					response->mPeering = mPeering;
					response->mTransfertStarted = true;
					request->addResponse(response);
					if(response->status() != Request::Response::Pending) 
						request->removePending(mPeering);	// this triggers the notification
				}
				else LogDebug("Core::Handler", "Received response for unknown request "+String::number(id));
			}
			else if(command == "D")	// Data block
			{
				unsigned channel;
				args.read(channel);
				
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
						response->mTransfertFinished = true;
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
				args.read(channel);
				args.read(status);
				
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
				args.read(channel);
				
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
				args.read(id);
				String &target = args;
			  	LogDebug("Core::Handler", "Received request "+String::number(id));

				Request *request = new Request(target, (command == "G"));
				request->setParameters(parameters);
				request->mId = id;
				request->mRemoteAddr = mRemoteAddr;
				
				Listener *listener = NULL;
				if(!SynchronizeTest(mCore, mCore->mListeners.get(peering, listener)))
				{
					LogDebug("Core::Handler", "No listener for request " + String::number(id));
				}
				else {
					try {
						Desynchronize(this);
						if(!listener->request(peering, request)) break;
					}
					catch(const Exception &e)
					{
						LogWarn("Core::Handler", String("Listener failed to process request "+String::number(id)+": ") + e.what()); 
					}
				}
					
				if(request->responsesCount() == 0) 
					request->addResponse(new Request::Response(Request::Response::Failed));
					
				mSender->lock();
				mSender->mRequestsToRespond.push_back(request);
				request->mResponseSender = mSender;
				mSender->unlock();
				mSender->notify();
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
	process();
	notifyAll();
	
	// TODO
	Thread::Sleep(5.);	
	Synchronize(this);
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
						
						LogDebug("Core::Handler::Sender", "Sending response " + String::number(j) + " for request " + String::number(request->id()));
						
						int status = response->status();
						if(status == Request::Response::Success && j != request->responsesCount()-1)
							status = Request::Response::Pending;
						
						String args;
						args << request->id() << " " << status << " " <<channel;
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
				LogDebug("Core::Handler::Sender", "Sending request "+String::number(request.id));
				
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
					ByteStream *content = response->content();
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
				
				mRequestsToRespond.erase(i);
				request->mId = 0;	// request MUST NOT be suppressed from the core like a sent request !
				delete request; 
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
