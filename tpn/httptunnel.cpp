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

#ifdef ANDROID
String HttpTunnel::UserAgent = "Mozilla/5.0 (Android; Mobile; rv:23.0) Gecko/23.0 Firefox/23.0";	// mobile is important
#else
String HttpTunnel::UserAgent = "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; Trident/6.0)";	// IE should be better for very restrictive environments
#endif

size_t HttpTunnel::DefaultPostSize = 1*1024;	// 1 KB
size_t HttpTunnel::MaxPostSize = 10*1024*1024;	// 10 MB
double HttpTunnel::ConnTimeout = 20.;
double HttpTunnel::SockTimeout = 10.;
double HttpTunnel::FlushTimeout = 0.5;
double HttpTunnel::ReadTimeout = 300.;

Map<uint32_t,HttpTunnel::Server*> 	HttpTunnel::Sessions;
Mutex					HttpTunnel::SessionsMutex;

HttpTunnel::Server *HttpTunnel::Incoming(Socket *sock)
{
	Http::Request request;
	try {
		try {
			sock->setTimeout(SockTimeout);
			request.recv(*sock, false); // POST content is not parsed
		
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
				while(!session || Sessions.contains(session));
					session = pseudorand();
				Sessions.insert(session, NULL);
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
					if(!server->mDownSock && !server->mClosed)
					{
						Http::Response response(request, 200);
						response.headers["Content-Type"] = "application/octet-stream";
						response.headers["Cache-Control"] = "no-cache";
						response.cookies["session"] << session;
						response.send(*sock);
				
						// First response should be forced down the potential proxy as soon as possible	
						if(isNew)
						{
							delete sock;
							return server;
						}

						server->mDownSock = sock;
						server->notifyAll();
						return NULL;
					}
				}
				else {
					Assert(!isNew);
					
					uint8_t command;
					AssertIO(sock->readBinary(command));
					if(command != TunnelOpen)
					{
						// Remote implementation is probably bogus
						LogWarn("HttpTunnel::Incoming", "Invalid tunnel opening sequence");
						throw 400;
					}

					uint16_t len;
					AssertIO(sock->readBinary(len));
					AssertIO(sock->ignore(len));    // auth data ignored
					
					Synchronize(server);
					if(!server->mUpSock && !server->mClosed)
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

HttpTunnel::Client::Client(const Address &addr, double timeout) :
	mAddress(addr),
	mReverse(addr.reverse()),
	mUpSock(NULL), 
	mDownSock(NULL),
	mFlushTask(this),
	mSession(0),
	mPostSize(DefaultPostSize),
	mPostLeft(0),
	mConnTimeout(ConnTimeout)
{
	if(timeout > 0.) mConnTimeout = timeout;
	readData(NULL, 0); 	// Connect mDownSock
	Assert(mSession);	// mSession should be set
	mConnTimeout = ConnTimeout;
}
	
HttpTunnel::Client::~Client(void)
{
	close();
}

void HttpTunnel::Client::close(void)
{
	Synchronize(this);

	Scheduler::Global->remove(&mFlushTask);
	
	try {
		if(mUpSock && mUpSock->isConnected())
		{	
			writePaddingUntil(2);
			mUpSock->writeBinary(TunnelClose);
			mUpSock->writeBinary(TunnelDisconnect);
			mPostLeft = 0;
		}
	}
	catch(const NetException &e)
	{
		// Ignored
	}
	
	delete mDownSock; mDownSock = NULL;
	delete mUpSock; mUpSock = NULL;
	
	mSession = 0;
}

size_t HttpTunnel::Client::readData(char *buffer, size_t size)
{
	Synchronize(this);

	if(!mDownSock)
	{
		mDownSock = new Socket;
		mDownSock->setTimeout(SockTimeout);
	}

	Time endTime = Time::Now() + (mSession ? ReadTimeout : mConnTimeout);
	while(true)
	{
		if(Time::Now() >= endTime) throw Timeout();

		if(!mDownSock->isConnected())
		{
			String url;
			Assert(!mReverse.empty());
                        url<<"http://"<<mReverse<<"/"<<String::random(10);

			LogDebug("HttpTunnel::Client::readData", "GET " + url);

			Http::Request request(url, "GET");
                        request.headers["User-Agent"] = UserAgent;
                        if(mSession) request.cookies["session"] << mSession;

        		Address addr = mAddress;
        		bool hasProxy = Config::GetProxyForUrl(url, addr);
        		if(hasProxy) request.url = url;			// Full URL for proxy

			try {
				double timeout = std::max(endTime - Time::Now(), milliseconds(100));
				mDownSock->setConnectTimeout(std::min(mConnTimeout, timeout));
                		mDownSock->connect(addr, true);		// Connect without proxy
                		request.send(*mDownSock);
        		}
			catch(const NetException &e)
        		{
                		if(hasProxy) LogWarn("HttpTunnel::Client", String("HTTP proxy error: ") + e.what());
                		throw;
        		}

			double timeout = std::max(endTime - Time::Now(), milliseconds(100));
			mDownSock->setReadTimeout(timeout);

			Http::Response response;
			response.recv(*mDownSock);

			if(response.code != 200)
			{
				if(mSession)
				{
					if(response.code == 504) continue;	// Proxy timeout
					else break;				// Connection closed
				}

				throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
			}
			
			String cookie;
                        if(response.cookies.get("session", cookie))
                                cookie.extract(mSession);

			if(!mSession)
			{
				throw NetException("HTTP transaction failed: Invalid cookie");
			}

			mDownSock->setReadTimeout(ReadTimeout);
		}
	
		if(!size) return 0;
		
		try {
			Desynchronize(this);
			size_t ret = mDownSock->readData(buffer, size);
			if(ret) return ret;
		}
		catch(const Timeout &e)
		{
			// Nothing to do
		}
		catch(const NetException &e)
		{
			// Nothing to do
		}
		
		mDownSock->close();
	}

	return 0;
}

void HttpTunnel::Client::writeData(const char *data, size_t size)
{
	Synchronize(this);

	if(!mSession) readData(NULL, 0);	// ensure session is opened
	if(!mUpSock) 
	{
		mUpSock = new Socket;	
		mUpSock->setTimeout(SockTimeout);
	}

	Scheduler::Global->remove(&mFlushTask);

	while(size)
	{
		if(!mUpSock->isConnected())
		{
			String url;
			Assert(!mReverse.empty());
                        url<<"http://"<<mReverse<<"/"<<String::random(10);

			LogDebug("HttpTunnel::Client::writeData", "POST " + url);

                        Http::Request request(url, "POST");
                        request.headers["User-Agent"] = UserAgent;
                        request.headers["Content-Length"] << mPostSize;
			request.cookies["session"] << mSession;

                        Address addr = mAddress;
                        bool hasProxy = Config::GetProxyForUrl(url, addr);
                        if(hasProxy) request.url = url; 	        // Full URL for proxy

                        try {
				mUpSock->setConnectTimeout(mConnTimeout);
                                mUpSock->connect(addr, true);		// Connect without proxy
                        	request.send(*mUpSock);
			}
                        catch(const NetException &e)
                        {
                                if(hasProxy) LogWarn("HttpTunnel::Client", String("HTTP proxy error: ") + e.what());
                                throw;
                        }

			mPostLeft = mPostSize;
			
			mUpSock->writeBinary(TunnelOpen);
			mUpSock->writeBinary(uint16_t(0));	// no auth data
			mPostLeft-= 3;
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
			mUpSock->writeBinary(TunnelDisconnect);
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

	try {
		if(mUpSock && mUpSock->isConnected())
		{
			LogDebug("HttpTunnel::Client::flush", "Flushing (padding "+String::number(std::max(int(mPostLeft)-1, 0))+" bytes)...");
			
			updatePostSize(mPostLeft);
			writePaddingUntil(1);
			mUpSock->writeBinary(TunnelDisconnect);
			mPostLeft = 0;
			
			Http::Response response;
			response.recv(*mUpSock);
			mUpSock->close();
		}
	}
	catch(const Exception &e)
	{
		LogWarn("HttpTunnel::Client::flush", e.what());
	}
}

void HttpTunnel::Client::writePaddingUntil(size_t left)
{
	Synchronize(this);
	Assert(mUpSock);
	
	while(mPostLeft >= left + 4)
	{
		size_t len = std::min(mPostLeft-left-3, size_t(0xFFFF));
		mUpSock->writeBinary(TunnelPadding);    // 1 byte
		mUpSock->writeBinary(uint16_t(len));    // 2 bytes
		mUpSock->writeZero(len);
		mPostLeft-= len + 3;
	}
        
	while(mPostLeft > left) 
	{
		mUpSock->writeBinary(TunnelPad);
		mPostLeft-= 1;
	}
}

void HttpTunnel::Client::updatePostSize(size_t left)
{
	if(left == 0)
	{
		mPostSize = std::min(mPostSize*2, MaxPostSize);
	}
	else {
		mPostSize = std::max(mPostSize - left + 1, DefaultPostSize);
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
	close();
}

void HttpTunnel::Server::close(void)
{
	Synchronize(this);

	Scheduler::Global->remove(&mFlushTask);
	
	SessionsMutex.lock();
        Sessions.erase(mSession);
        SessionsMutex.unlock();
	
	delete mDownSock; mDownSock = NULL;
	delete mUpSock; mUpSock = NULL;
	
	mClosed = true;
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

		double timeleft = ReadTimeout;
		while(!mUpSock)
		{
			LogDebug("HttpTunnel::Server::readData", "Waiting for connection...");
			if(mClosed) return 0;
			if(!wait(timeleft)) throw Timeout();
		}

		//LogDebug("HttpTunnel::Server::readData", "Connection OK");

		uint8_t command;
		uint16_t len = 0;
		
		{
			Desynchronize(this);

			if(!mUpSock->readBinary(command))
			{
				if(mClosed) return 0;
				continue;
			}
			
			if(!(command & 0x40))
				if(!mUpSock->readBinary(len))
					continue;

			//LogDebug("HttpTunnel::Server::readData", "Incoming command: " + String::hexa(command, 2) + " (length " + String::number(len) + ")");
		}

		switch(command)
		{
		case TunnelData:
			mPostBlockLeft = len;
			break;

		case TunnelPadding:
		{
			Desynchronize(this);
			mUpSock->ignore(len);
			break;
		}

		case TunnelPad:
			// Do nothing
			break;

		case TunnelClose:
			mClosed = true;
			break;

		case TunnelDisconnect:
		{
			Http::Response response(mUpRequest, 204);	// no content
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
	DesynchronizeStatement(this, size = mUpSock->readData(buffer, size));
	mPostBlockLeft-= size;
	return size;
}

void HttpTunnel::Server::writeData(const char *data, size_t size)
{
	Synchronize(this);

	while(true)
	{
		if(mDownSock && !mDownSock->isConnected())
		{
			delete mDownSock;
			mDownSock = NULL;
		}

		Scheduler::Global->remove(&mFlushTask);

		double timeleft = ConnTimeout;
		while(!mDownSock)
		{
			LogDebug("HttpTunnel::Server::writeData", "Waiting for connection...");
			if(mClosed) throw NetException("Connection closed");
			if(!wait(timeleft)) throw Timeout();
		}

		//LogDebug("HttpTunnel::Server::writeData", "Connection OK");
		
		try {
			mDownSock->writeData(data, size);
			break;
		}
		catch(const NetException &e)
		{
			delete mDownSock;
			mDownSock = NULL;
		}
	}
	
	Scheduler::Global->schedule(&mFlushTask, FlushTimeout);
}

void HttpTunnel::Server::flush(void)
{
	Synchronize(this);

	try {
		if(mDownSock && mDownSock->isConnected())
		{
			LogDebug("HttpTunnel::Server::flush", "Flushing...");
			
			delete mDownSock;
			mDownSock = NULL;
		}
        }
	catch(const Exception &e)
	{
		LogWarn("HttpTunnel::Server::flush", e.what());
	}
}

}
