/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
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

#include "tpn/httptunnel.h"
#include "tpn/exception.h"
#include "tpn/scheduler.h"
#include "tpn/config.h"

namespace tpn
{

String HttpTunnel::UserAgent = "Mozilla/5.0 (Android; Mobile; rv:23.0) Gecko/23.0 Firefox/23.0";	// mobile is important
size_t HttpTunnel::DefaultPostSize = 4*1024;	// 4 KB
size_t HttpTunnel::MaxPostSize = 10*1024*1024;	// 10 MB
double HttpTunnel::ConnTimeout = 10.;
double HttpTunnel::ReadTimeout = 5.;
double HttpTunnel::FlushTimeout = 0.5;

Map<uint32_t,HttpTunnel::Server*> 	HttpTunnel::Sessions;
Mutex					HttpTunnel::SessionsMutex;
uint32_t				HttpTunnel::LastSession = 0;

HttpTunnel::Server *HttpTunnel::Incoming(Socket *sock)
{
	Http::Request request;
	try {
		try {
			sock->setTimeout(ReadTimeout);
			request.recv(*sock);
		
			uint32_t session = 0;

			String cookie;
			if(request.cookies.get("session", cookie))
				cookie.extract(session);	

			Server *server = NULL;
			bool isNew = false;

			if(!session)
			{
				if(request.method != "GET") 
				{
					LogDebug("HttpTunnel::Incoming", "Missing session number in POST request");
					throw 400;
				}
				
				SessionsMutex.lock();
				session = ++LastSession;
				SessionsMutex.unlock();

				server = new Server(session);   // Server registers the session
				isNew = true;
			}
			else {
				SessionsMutex.lock();
				Sessions.get(session, server);
				SessionsMutex.unlock();
			}

			if(server)
			{
				if(request.method == "GET") 
				{
					Synchronize(server);
					if(!server->mDownSock)
					{
						Http::Response response(request, 200);
						response.cookies["session"] << session;
						response.send(*sock);
						
						server->mDownSock = sock;
						server->notifyAll();
						if(isNew) return server;
						else return NULL;
					}
				}
				else {
					Assert(!isNew);
					
					uint8_t command;
					AssertIO(sock->readBinary(command));
					if(!command == TunnelOpen)
					{
						// Remote implementation is probably bogus
						LogWarn("HttpTunnel::Incoming", "Invalid tunnel opening sequence");
						throw 400;
					}

					uint16_t len;
					AssertIO(sock->readBinary(len));
					AssertIO(sock->ignore(len));    // auth data ignored
					
					Synchronize(server);
					if(!server->mUpSock)
					{
						Assert(server->mPostBlockLeft == 0);	
						server->mUpSock = sock;
						server->mUpRequest = request;
						server->notifyAll();
						return NULL;
					}
				}
			}
			else {
				LogDebug("HttpTunnel::Incoming", "Unknown session number: " + String::number(session));
				throw 400;
			}
		}
		catch(const Timeout &e)
		{
			throw 408;
		}
		catch(const NetException &e)
		{
			LogDebug("HttpTunnel::Incoming", e.what());
			// no throw
		}
		catch(const Exception &e)
		{
			LogWarn("HttpTunnel::Incoming", e.what());
			throw 500;
		}
	}
	catch(int code)
	{
		try {
			Http::Response response(request, code);
			response.send(*sock);
		}
		catch(...)
		{
			
		}
	}
	
	delete sock;
	return NULL;		
}

HttpTunnel::Client::Client(const Address &addr) :
	mAddress(addr),
	mUpSock(NULL), 
	mDownSock(NULL),
	mFlushTask(this),
	mSession(0),
	mPostSize(DefaultPostSize),
	mPostLeft(0)
{
	readData(NULL, 0); 	// Connect mDownSock
	Assert(mSession);	// mSession should be set
}
	
HttpTunnel::Client::~Client(void)
{
	Scheduler::Global->remove(&mFlushTask);

	delete mDownSock;
        delete mUpSock;
}

size_t HttpTunnel::Client::readData(char *buffer, size_t size)
{
	Synchronize(this);

	if(!mDownSock) mDownSock = new Socket;

	while(true)
	{
		if(!mDownSock->isConnected())
		{
			mDownSock->setTimeout(ConnTimeout);
			mDownSock->connect(mAddress, true);
			mDownSock->setTimeout(ReadTimeout);		
 
			String url;
			url<<"/"<<String::random(8);
			Http::Request request(url, "GET");
			request.headers["Host"] = mAddress.toString();
			request.headers["User-Agent"] = UserAgent;
			if(mSession) request.cookies["session"] << mSession;
			request.send(*mDownSock);
		
			Http::Response response;
			response.recv(*mDownSock);

			if(response.code != 200)
			{
				if(mSession) break;	// Connection closed
				throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
			}
			
			String cookie;
                        if(request.cookies.get("session", cookie))
                                cookie.extract(mSession);

			if(!mSession)
			{
				throw NetException("HTTP transaction failed: invalid cookie");
			}
		}
	
		size_t ret = mDownSock->readData(buffer, size);
		if(ret) return ret;
	}

	return 0;
}

void HttpTunnel::Client::writeData(const char *data, size_t size)
{
	Synchronize(this);

	Assert(mSession);
	if(!mUpSock) mUpSock = new Socket;	

	Scheduler::Global->remove(&mFlushTask);

	while(size)
	{
		if(!mUpSock->isConnected())
		{
			mUpSock->setTimeout(ConnTimeout);
			mUpSock->connect(mAddress, true);
			mUpSock->setTimeout(ReadTimeout);			

			String url;
			url<<"/"<<String::random(8);
			Http::Request request(url, "POST");
			request.headers["Host"] = mAddress.toString();
			request.headers["User-Agent"] = UserAgent;
			request.headers["Content-Length"] << mPostSize;
			if(mSession) request.cookies["session"] << mSession;
			request.send(*mUpSock);
			
			mUpSock->writeBinary(TunnelOpen);
			mUpSock->writeBinary(uint16_t(0));	// no auth data

			mPostLeft = mPostSize;
		}
	
		if(mPostLeft >= 4)
		{
			size_t len = std::min(size, mPostLeft-3);
			len = std::min(len, size_t(0xFFFF));
			mUpSock->writeBinary(TunnelData);	// 1 byte
			mUpSock->writeBinary(uint16_t(len));	// 2 bytes
			mUpSock->writeData(data, len);		// len bytes
			mPostLeft-= len + 3;
			data+= len;
			size-= len;
		}
		else {
			while(mPostLeft > 1) 
			{
				mUpSock->writeBinary(TunnelPad);
				mPostLeft-= 1;
			}
		}

		if(mPostLeft <= 1)
		{
			if(mPostLeft == 1) mUpSock->writeBinary(TunnelDisconnect);
			mPostLeft = 0;
			updatePostSize(0);
			
			Http::Response response;
			response.recv(*mUpSock);
			mUpSock->close();
		}
	}

	Scheduler::Global->schedule(&mFlushTask, FlushTimeout);
}

void HttpTunnel::Client::flush(void)
{
	Synchronize(this);

	if(mUpSock && mUpSock->isConnected())
	{	
		updatePostSize(mPostLeft);
		
		while(mPostLeft >= 4)
                {
                        size_t len = std::min(mPostLeft-3, size_t(0xFFFF));
                        mUpSock->writeBinary(TunnelPadding);    // 1 byte
                        mUpSock->writeBinary(uint16_t(len));    // 2 bytes
                        mUpSock->writeZero(len);
                        mPostLeft-= len + 3;
                }
        
		while(mPostLeft > 1) 
		{
			mUpSock->writeBinary(TunnelPad);
			mPostLeft-= 1;
                }

		if(mPostLeft == 1) mUpSock->writeBinary(TunnelDisconnect);
		mPostLeft = 0;

		Http::Response response;
		response.recv(*mUpSock);
		mUpSock->close();
	}
}

void HttpTunnel::Client::updatePostSize(size_t left)
{
	if(left == 0)
	{
		mPostSize = std::min(mPostSize*2, MaxPostSize);
	}
	else {
		const double t = 0.5;
		const double targetSize = mPostSize - left;
		mPostSize = size_t((1.-t)*mPostSize + t*targetSize + 0.5);
	}
}

HttpTunnel::Server::Server(uint32_t session) :
	mUpSock(NULL), 
	mDownSock(NULL),
	mFlushTask(this),
	mSession(session),
	mPostBlockLeft(0),
	mClosed(false)		
{
	Assert(mSession);

	SessionsMutex.lock();
	Sessions.insert(mSession, this);
	SessionsMutex.unlock();
}

HttpTunnel::Server::~Server(void)
{
	Scheduler::Global->remove(&mFlushTask);

	SessionsMutex.lock();
        Sessions.erase(mSession);
        SessionsMutex.unlock();

	delete mDownSock;
        delete mUpSock;
}

size_t HttpTunnel::Server::readData(char *buffer, size_t size)
{
	Synchronize(this);

	while(!mPostBlockLeft)
	{
		if(mUpSock && !mUpSock->isConnected())
		{
			delete mUpSock;
			mUpSock = NULL;
		}

		double timeleft = ConnTimeout;
		while(!mUpSock)
		{
			if(mClosed) return 0;
			if(!wait(timeleft)) throw Timeout();
		}

		uint8_t command;
		if(!mUpSock->readBinary(command))
		{
			if(mClosed) return 0;
			throw NetException("Connection lost");
		}

		uint16_t len = 0;
		if(!(command & 0x40))
			mUpSock->readBinary(len); 

		switch(command)
		{
		case TunnelData:
			mPostBlockLeft = len;
			break;

		case TunnelPadding:
			mUpSock->ignore(len);
			break;

		case TunnelPad:
			// Do nothing
			break;

		case TunnelClose:
			mClosed = true;
			break;

		case TunnelDisconnect:
			{
				Http::Response response(mUpRequest, 200);
				response.send(*mUpSock);
				mUpRequest.clear();
                		delete mUpSock;
                		mUpSock = NULL;
				break;
			}

		default:
			LogWarn("HttpTunnel::Server", "Unknown command: " + String::hexa(command));	
			mUpSock->ignore(len);
                        break;
		}
	}

	Assert(mUpSock);
	
	size = std::min(size, mPostBlockLeft);
	if(mUpSock->readData(buffer, size) != size)
		throw NetException("Connection lost");
	
	mPostBlockLeft-= size;
}

void HttpTunnel::Server::writeData(const char *data, size_t size)
{
	Synchronize(this);

	if(mDownSock && !mDownSock->isConnected())
	{
		delete mDownSock;
		mDownSock = NULL;
	}

	Scheduler::Global->remove(&mFlushTask);

	double timeleft = ConnTimeout;
	while(!mDownSock)
	{
		if(!wait(timeleft)) throw Timeout();
	}

	mDownSock->writeData(data, size);
	Scheduler::Global->schedule(&mFlushTask, FlushTimeout);
}

void HttpTunnel::Server::flush(void)
{
	Synchronize(this);

        if(mDownSock && mDownSock->isConnected())
        {
		delete mDownSock;
		mDownSock = NULL;
        }
}

}
