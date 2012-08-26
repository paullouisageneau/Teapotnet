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

namespace arc
{

Core *Core::Instance = new Core();

Core::Core(void) :
		mLastRequest(0)
{

}

Core::~Core(void)
{

}

void Core::add(Socket *sock)
{
	assert(sock != NULL);
	Handler *handler = new Handler(this,sock);
	handler->start();
}

unsigned Core::addRequest(Request *request)
{
	++mLastRequest;
	request->mId = mLastRequest;
	mRequests.insert(mLastRequest, request);

	if(request->mReceiver == Identifier::Null)
	{
		for(Map<Identifier,Handler*>::iterator it = mHandlers.begin();
				it != mHandlers.end();
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
		if(!mHandlers.get(request->mReceiver, handler)) return 0;	// TODO
		handler->lock();
		handler->addRequest(request);
		handler->unlock();
	}

	return mLastRequest;
}

void Core::removeRequest(unsigned id)
{
	for(Map<Identifier,Handler*>::iterator it = mHandlers.begin();
				it != mHandlers.end();
				++it)
	{
		Handler *handler = it->second;
		handler->lock();
		handler->removeRequest(id);
		handler->unlock();
	}
}

void Core::add(const Identifier &peer, Core::Handler *handler)
{
	Assert(handler != NULL);
	mHandlers.insert(peer, handler);
}

void Core::remove(const Identifier &peer)
{
	if(mHandlers.contains(peer))
	{
		delete mHandlers.get(peer);
		mHandlers.erase(peer);
	}
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
	String line;
	if(!mSock->readLine(line))
		return;

	String proto;
	String version;
	line.readString(proto);
	line.readString(version);

	// TODO: auth
	// Initialize mPeer here !

	// mCore->add(mPeer,this);

	mSender.start();

	while(mSock->readLine(line))
	{
		lock();

		String command;
		line.read(command);
		command = command.toUpper();

		// Read parameters
		StringMap parameters;
		while(true)
		{
			String name;
			AssertIO(mSock->readLine(name));
			if(name.empty()) break;

			String value = name.cut(':');
			name.trim();
			value.trim();
			parameters.insert(name,value);
		}

		if(command == "R")
		{
			unsigned id;
			String status;
			unsigned channel;

			line.read(id);
			line.read(status);
			line.read(channel);

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

				request->addResponse(response);

			}
		}
		else if(command == "D")
		{
			unsigned channel, size;
			line.read(channel);
			line.read(size);

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
			String &target = line;

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

	//mCore->remove(this);
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
			if(request->mIsData) *mSock<<"G";
			else *mSock<<"I";
			*mSock<<" "<<request->target()<<Stream::NewLine;

			StringMap &parameters = request->mParameters;
			for(	StringMap::iterator it = parameters.begin();
						it != parameters.end();
						++it)
			{
					*mSock<<it->first<<": "<<it->second<<Stream::NewLine;
			}
			*mSock<<Stream::NewLine;

			request->removePending();
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

					*mSock<<"R "<<request->id()<<" "<<status<<" "<<channel<<Stream::NewLine;

					StringMap &parameters = response->mParameters;
					for(	StringMap::iterator it = parameters.begin();
							it != parameters.end();
							++it)
					{
						*mSock<<it->first<<": "<<it->second<<Stream::NewLine;
					}
					*mSock<<Stream::NewLine;
				}
			}
		}

		Map<unsigned, ByteStream*>::iterator it = mTransferts.begin();
		while(it != mTransferts.end())
		{
			size_t size = it->second->readData(buffer,ChunkSize);
			*mSock<<"D "<<it->first<<" "<<size<<Stream::NewLine;
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
