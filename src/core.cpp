/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "core.h"
#include "html.h"
#include "sha512.h"

namespace arc
{

Core *Core::Instance = NULL;


Core::Core(int port) :
		mSock(port),
		mLastRequest(0)
{
	Interface::Instance->add("/peers", this);
}

Core::~Core(void)
{
	Interface::Instance->remove("/peers");
	mSock.close();	// useless
}

void Core::registerPeering(	const Identifier &peering,
				const Identifier &remotePeering,
		       		const ByteString &secret)
{
	mPeerings[peering] = remotePeering;
	mSecrets[peering] = secret;
}

void Core::unregisterPeering(const Identifier &peering)
{
	mPeerings.erase(peering);
	mSecrets.erase(peering);
}

void Core::addPeer(Socket *sock, const Identifier &peering)
{
	assert(sock != NULL);
	
	if(peering != Identifier::Null && !mPeerings.contains(peering))
		throw Exception("Added peer with unknown peering");
	
	Handler *handler = new Handler(this,sock);
	if(peering != Identifier::Null) handler->setPeering(peering);
	handler->start(true); // handler will destroy itself
}

bool Core::hasPeer(const Identifier &peering)
{
	return mPeers.contains(peering);
}

void Core::run(void)
{
	try {
		while(true)
		{
			Socket *sock = new Socket;
			mSock.accept(*sock);
			addPeer(sock, Identifier::Null);
		}
	}
	catch(const NetException &e)
	{
		return;
	}
}

unsigned Core::addRequest(Request *request)
{
	++mLastRequest;
	request->mId = mLastRequest;
	mRequests.insert(mLastRequest, request);

	if(request->mReceiver == Identifier::Null)
	{
		for(Map<Identifier,Handler*>::iterator it = mPeers.begin();
				it != mPeers.end();
				++it)
		{
			Handler *handler = it->second;
			handler->lock();
			handler->addRequest(request);
			handler->unlock();
		}
	}
	else {
		// TODO: This should be modified to allow routing
		Handler *handler;
		if(!mPeers.get(request->mReceiver, handler)) return 0;	// TODO
		handler->lock();
		handler->addRequest(request);
		handler->unlock();
	}

	return mLastRequest;
}

void Core::removeRequest(unsigned id)
{
	for(Map<Identifier,Handler*>::iterator it = mPeers.begin();
				it != mPeers.end();
				++it)
	{
		Handler *handler = it->second;
		handler->lock();
		handler->removeRequest(id);
		handler->unlock();
	}
}

void Core::http(const String &prefix, Http::Request &request)
{
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

			if(mPeers.empty()) page.text("No peer...");
			else for(Map<Identifier,Handler*>::iterator it = mPeers.begin();
							it != mPeers.end();
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

void Core::addHandler(const Identifier &peer, Core::Handler *handler)
{
	Assert(handler != NULL);
	if(!mPeers.contains(peer)) mPeers.insert(peer, handler);
	else {
		if(mPeers[peer] != handler)
			throw Exception("Another handler is already registered");
	}
}

void Core::removeHandler(const Identifier &peer, Core::Handler *handler)
{
	if(mPeers.contains(peer))
	{
		Handler *h = mPeers.get(peer);
		if(h != handler) return;
		delete h;
		mPeers.erase(peer);
	}
}

void Core::Handler::sendCommand(Socket *sock, const String &command, const String &args, const StringMap &parameters)
{
	*sock<<command<<" "<<args<<Stream::NewLine;

	for(	StringMap::const_iterator it = parameters.begin();
		it != parameters.end();
		++it)
	{
		*sock<<it->first<<": "<<it->second<<Stream::NewLine;
	}
	*sock<<Stream::NewLine;
}
		
bool Core::Handler::recvCommand(Socket *sock, String &command, String &args, StringMap &parameters)
{
	if(!sock->readLine(command)) return false;
	args = command.cut(' ');
	command = command.toUpper();
	
	parameters.clear();
	while(true)
	{
		String name;
		AssertIO(sock->readLine(name));
		if(name.empty()) break;

		String value = name.cut(':');
		name.trim();
		value.trim();
		parameters.insert(name,value);
	}
	
	return true;
}

Core::Handler::Handler(Core *core, Socket *sock) :
	mCore(core),
	mSock(sock),
	mSender(sock)
{

}

Core::Handler::~Handler(void)
{	
	delete mSock;
	delete mHandler;
}

void Core::Handler::setPeering(const Identifier &peering)
{
	mPeering = peering;
}

void Core::Handler::addRequest(Request *request)
{
	mSender.lock();
	request->addPending();
	mSender.mRequestsQueue.push(request);
	mSender.unlock();
	mSender.notify();

	mRequests.insert(request->id(),request);
}

void Core::Handler::removeRequest(unsigned id)
{
	mRequests.erase(id);
}

void Core::Handler::run(void)
{
	ByteString nonce_a, salt_a;
	for(int i=0; i<16; ++i)
	{
	  	char c = char(rand()%256);	// TODO
		nonce_a.push_back(c);
		c = char(rand()%256);		// TODO
		salt_a.push_back(c);  
	}
  
    	String command, args;
	StringMap parameters;
  
  	if(mRemotePeering != Identifier::Null)
	{
	 	args.clear();
	  	args << mRemotePeering;
		parameters.clear();
		parameters["Application"] << APPNAME;
		parameters["Version"] << APPVERSION;
		parameters["Nonce"] << nonce_a;
	 	sendCommand(mSock, "H", args, parameters);
	}
  
	if(!recvCommand(mSock, command, args, parameters)) return;
	if(command != "H") throw Exception("Unexpected command");
	
	String appname, appversion;
	ByteString peering, nonce_b;
	args.read(peering);
	parameters["Application"] >> appname;
	parameters["Version"] >> appversion;
	parameters["Nonce"] >> nonce_b;

	if(mPeering != Identifier::Null && mPeering != peering) 
			throw Exception("Peering in response does not match: " + peering.toString());
	
	ByteString secret;
	if(peering.size() != 64) throw Exception("Invalid peering: " + peering.toString());	// TODO: useless
	if(!mCore->mPeerings.get(peering, mRemotePeering)) throw Exception("Unknown peering: " + peering.toString());
	if(!mCore->mSecrets.get(peering, secret)) throw Exception("No secret for peering: " + peering.toString());
	
	if(mPeering == Identifier::Null)
	{
	 	mPeering = peering;
		
		args.clear();
	  	args << mRemotePeering;
		parameters.clear();
		parameters["Application"] << APPNAME;
		parameters["Version"] << APPVERSION;
		parameters["Nonce"] << nonce_a;
	 	sendCommand(mSock, "H", args, parameters);
	}
	
	String agregate_a;
	agregate_a.writeLine(secret);
	agregate_a.writeLine(salt_a);
	agregate_a.writeLine(nonce_b);
	agregate_a.writeLine(mPeering);
	
	ByteString hash_a;
	Sha512::Hash(agregate_a, hash_a, Sha512::CryptRounds);
	
	parameters.clear();
	parameters["Digest"] << hash_a;
	parameters["Salt"] << salt_a;
	sendCommand(mSock, "A", "DIGEST", parameters);
	
	AssertIO(recvCommand(mSock, command, args, parameters));
	if(command != "A") throw Exception("Unexpected command");
	if(args.toUpper() != "DIGEST") throw Exception("Unknown authentication method " + args.toString());
	
	ByteString salt_b, test_b;
	parameters["Digest"] >> test_b;
	parameters["Salt"] >> salt_b;
	
	String agregate_b;
	agregate_b.writeLine(secret);
	agregate_b.writeLine(salt_b);
	agregate_b.writeLine(nonce_a);
	agregate_b.writeLine(mRemotePeering);
	
	ByteString hash_b;
	Sha512::Hash(agregate_b, hash_b, Sha512::CryptRounds);
	
	if(test_b != hash_b) throw Exception("Authentication failed");
	
	mCore->addHandler(mPeering,this);

	mSender.start();
	
	while(recvCommand(mSock, command, args, parameters))
	{
		lock();

		if(command == "R")
		{
			unsigned id;
			String status;
			unsigned channel;
			args.read(id);
			args.read(status);
			args.read(channel);

			Request *request;
			if(mRequests.get(id,request))
			{
				Request::Response *response;

				if(channel)
				{
					ByteStream *sink = request->mContentSink; // TODO
					response = new Request::Response(status, parameters, sink);
					mResponses.insert(channel,response);
				}
				else response = new Request::Response(status, parameters);

				response->mPeering = mPeering;
				request->addResponse(response);

				// TODO: Only one response expected for now
				mRequests.erase(id);

				request->removePending();	// TODO
			}
		}
		else if(command == "D")
		{
			unsigned channel, size;
			args.read(channel);
			args.read(size);

			Request::Response *response;
			if(mResponses.get(channel,response))
			{
				if(size) mSock->readBinary(*response->content(),size);
				else {
					response->content()->close();
					mRequests.erase(channel);
				}
			}
			else mSock->ignore(size);	// TODO: error
		}
		else if(command == "I" || command == "G")
		{
			String &target = args;

			Request *request = new Request;
			request->setTarget(target, (command == "G"));
			request->setParameters(parameters);
			request->execute();

			mSender.lock();
			mSender.mRequestsToRespond.push_back(request);
			request->mResponseSender = &mSender;
			mSender.unlock();
			mSender.notify();
		}

		unlock();
	}

	mSock->close();

	mCore->removeHandler(mPeering, this);
}

const size_t Core::Handler::Sender::ChunkSize = 4096;	// TODO

Core::Handler::Sender::Sender(Socket *sock) :
		mSock(sock),
		mLastChannel(0)
{

}

Core::Handler::Sender::~Sender(void)
{
	for(int i=0; i<mRequestsToRespond.size(); ++i)
	{
		Request *request = mRequestsToRespond[i];
		if(request->mResponseSender == this)
			request->mResponseSender = NULL;
	}
}

void Core::Handler::Sender::run(void)
{
	char buffer[ChunkSize];

	while(true)
	{
		lock();
		if(mTransferts.empty()
				&& mRequestsQueue.empty()
				&& mRequestsToRespond.empty())
			wait();

		if(!mRequestsQueue.empty())
		{
			Request *request = mRequestsQueue.front();
			
			String command;
			if(request->mIsData) command = "G";
			else command = "I";
			 
			Handler::sendCommand(mSock, command, request->target(), request->mParameters);
			mRequestsQueue.pop();
		}

		for(int i=0; i<mRequestsToRespond.size(); ++i)
		{
			Request *request = mRequestsToRespond[i];
			for(int j=0; j<request->responsesCount(); ++j)
			{
				Request::Response *response = request->response(i);
				if(!response->mIsSent)
				{
					String status = "OK";
					unsigned channel = 0;

					if(response->content())
					{
						++mLastChannel;
						channel = mLastChannel;
						mTransferts.insert(channel,response->content());
					}
					
					String args;
					args << request->id() <<" "<<status<<" "<<channel;
					Handler::sendCommand(mSock, "R", args, response->mParameters);
				}
			}
		}

		Map<unsigned, ByteStream*>::iterator it = mTransferts.begin();
		while(it != mTransferts.end())
		{
			size_t size = it->second->readData(buffer,ChunkSize);
			
			String args;
			args << it->first << " " << size;
			Handler::sendCommand(mSock, "D", args, StringMap());
			mSock->writeData(buffer, size);

			if(size == 0)
			{
				delete it->second;
				mTransferts.erase(it++);
			}
			else ++it;
		}
		unlock();
	}
}

}
