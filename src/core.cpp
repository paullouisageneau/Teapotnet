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

const Core *Core::Instance = new Core();

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
	add(sock->getRemoteAddress(), handler);
}

unsigned Core::addRequest(Request *request)
{
	++mLastRequest;
	request->mId = mLastRequest;
	mRequests.insert(mLastRequest, request);

	for(Map<Address,Handler*>::iterator it = mHandlers.begin();
			it != mHandlers.end();
			++it)
	{
		Handler *handler = it->second;
		handler->lock();
		handler->addRequest(request);
		handler->unlock();
	}

	return mLastRequest;
}

void Core::removeRequest(unsigned id)
{
	for(Map<Address,Handler*>::iterator it = mHandlers.begin();
				it != mHandlers.end();
				++it)
	{
		Handler *handler = it->second;
		handler->lock();
		handler->removeRequest(id);
		handler->unlock();
	}
}

void Core::add(const Address &addr, Core::Handler *handler)
{
	assert(handler != NULL);
	mHandlers.insert(addr, handler);
}

void Core::remove(const Address &addr)
{
	if(mHandlers.contains(addr))
	{
		delete mHandlers.get(addr);
		mHandlers.erase(addr);
	}
}

Core::Handler::Handler(Core *core, Socket *sock) :
	mCore(core),
	mSock(sock)
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

void Core::Handler::addTransfert(unsigned channel, ByteStream *in)
{
	mSender.lock();
	Assert(!mSender.mTransferts.contains(channel));
	mSender.mTransferts.insert(channel, in);
	mSender.unlock();
	mSender.notify();
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

	mSender.mSock = mSock;
	mSender.start();

	while(mSock->readLine(line))
	{
		lock();

		String command;
		line.read(command);
		command = command.toUpper();

		if(command == "DATA")
		{
			unsigned channel, size;
			line.read(channel);
			line.read(size);

			Request *request;
			if(mRequests.get(channel,request))
			{
				if(size) mSock->readBinary(request->file(),size);
				else request->setFinished();
			}

		}
		else if(command == "GET")
		{

		}

		unlock();
	}

	mSock->close();

	//mCore->remove(this);
}

const size_t Core::Handler::Sender::ChunkSize = 4096;	// TODO

void Core::Handler::Sender::run(void)
{
	char buffer[ChunkSize];

	while(true)
	{
		lock();
		if(mTransferts.empty() && mRequestsQueue.empty()) wait();

		if(!mRequestsQueue.empty())
		{
			Request *request = mRequestsQueue.front();
			*mSock<<"GET "<<request->target()<<Stream::NewLine;
			// TODO: params
			request->removePending();
			mRequestsQueue.pop();
		}

		Map<unsigned, ByteStream*>::iterator it = mTransferts.begin();
		while(it != mTransferts.end())
		{
			size_t size = it->second->readData(buffer,ChunkSize);
			*mSock<<"DATA "<<it->first<<" "<<size<<Stream::NewLine;
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
