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

#include "tpn/httptunnel.h"
#include "tpn/exception.h"
#include "tpn/scheduler.h"
#include "tpn/config.h"

namespace tpn
{

#ifdef ANDROID
String HttpTunnel::UserAgent = "Mozilla/5.0 (Android; Mobile; rv:24.0) Gecko/24.0 Firefox/24.0";	// mobile is important
#else
String HttpTunnel::UserAgent = "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; Trident/6.0)";	// IE should be better for very restrictive environments
#endif

size_t HttpTunnel::DefaultPostSize = 1*1024;		// 1 KB
size_t HttpTunnel::MaxPostSize = 2*1024*1024;		// 2 MB
size_t HttpTunnel::MaxDownloadSize = 20*1024*1024;	// 20 MB
double HttpTunnel::ConnTimeout = 20.;
double HttpTunnel::SockTimeout = 10.;
double HttpTunnel::FlushTimeout = 0.2;
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

			LogDebug("HttpTunnel::Incoming", "Received " + request.method + " " + request.fullUrl + " (session="+String::number(session)+")");

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
				while(!session || Sessions.contains(session))
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
						response.headers["Cache-Control"] = "no-cache";
						response.cookies["session"] << session;

						// First response should be forced down the potential proxy as soon as possible	
						if(isNew)
						{
							response.headers["Content-Type"] = "text/html";
							response.send(*sock);
							delete sock;
							return server;
						}
	
						response.headers["Content-Type"] = "application/octet-stream";
						response.send(*sock);
						server->mDownSock = sock;
						server->mDownloadLeft = MaxDownloadSize;
						server->notifyAll();
						Scheduler::Global->schedule(&server->mFlushTask, ReadTimeout*0.8);
						return NULL;
					}
				}
				else {
					Assert(!isNew);
					
					uint8_t command;
					if(!sock->readBinary(command))
						throw NetException("Connection unexpectedly closed");
					
					if(command != TunnelOpen)
					{
						// Remote implementation is probably bogus
						LogWarn("HttpTunnel::Incoming", "Invalid tunnel opening sequence");
						throw 400;
					}

					uint16_t len;
					if(!sock->readBinary(len) || !sock->ignore(len))	// auth data ignored
						throw NetException("Connection unexpectedly closed");
					
					Synchronize(server);
					if(server->mUpSock) throw 409;	// Conflict
					if(server->mClosed) throw 400;	// Closed session
					Assert(server->mPostBlockLeft == 0);
					server->mUpSock = sock;
					server->mUpRequest = request;
					server->notifyAll();
					return NULL;
				}
			}
			else {
				// Unknown or closed session
				LogDebug("HttpTunnel::Incoming", "Unknown or closed session: " + String::number(session));
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
	NOEXCEPTION(close());
}

void HttpTunnel::Client::close(void)
{
	Synchronize(this);

	Scheduler::Global->remove(&mFlushTask);
	
	try {
		if(mUpSock && mUpSock->isConnected() && mPostLeft)
		{	
			writePaddingUntil(2);
			if(mPostLeft >= 1) mUpSock->writeBinary(TunnelClose);
			if(mPostLeft >= 2) mUpSock->writeBinary(TunnelDisconnect);
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

	Time endTime = Time::Now() + mConnTimeout;
	while(true)
	{
		if(Time::Now() >= endTime) throw Timeout();

		bool freshConnection = false;
		if(!mDownSock->isConnected())
		{
			freshConnection = true;

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
				double timeout = std::max(endTime - Time::Now(), 0.);
				mDownSock->setConnectTimeout(timeout);
                		
				DesynchronizeStatement(this, mDownSock->connect(addr, true));		// Connect without proxy
                		request.send(*mDownSock);
        		}
			catch(const NetException &e)
        		{
                		if(hasProxy) LogWarn("HttpTunnel::Client", String("HTTP proxy error: ") + e.what());
                		throw;
        		}

			double timeout = std::max(endTime - Time::Now(), 0.);
			mDownSock->setReadTimeout(timeout);

			Http::Response response;
			response.recv(*mDownSock);

			if(response.code != 200)
			{
				if(mSession)
				{
					if(response.code == 400)	// Closed session
					{
						LogDebug("HttpTunnel::Client", "Session closed");
                                                break;
					}
					else if(response.code == 504)	// Proxy timeout
					{
						LogDebug("HttpTunnel::Client", "HTTP proxy timeout, retrying...");
						continue;
					}
					else if(response.code == 409)	// Conflict
					{
						wait(1.);
						continue;
					}
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
		catch(const NetException &e)
		{
			if(!freshConnection)
			{
				mDownSock->close();
				throw;
			}
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
		if(!mUpSock->isConnected() || !mPostLeft)
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

                        	Socket *sock = mUpSock;	mUpSock = NULL; // Necessary because of the flush task
				DesynchronizeStatement(this, sock->connect(addr, true));	// Connect without proxy
				mUpSock = sock;

				request.send(*mUpSock);
			}
                        catch(const NetException &e)
                        {
                                if(hasProxy) LogWarn("HttpTunnel::Client", String("HTTP proxy error: ") + e.what());
                                throw;
                        }

			mPostLeft = mPostSize;
			
			mUpSock->writeBinary(TunnelOpen);	// 1 byte
			mUpSock->writeBinary(uint16_t(0));	// 2 bytes, no auth data
			mPostLeft-= 3;
		}
	
		if(mPostLeft > 4)
		{
			size_t len = std::min(size, mPostLeft-4);
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
		
		Assert(mPostLeft >= 1);
		if(mPostLeft == 1)
		{
			mUpSock->writeBinary(TunnelDisconnect);
			mPostLeft = 0;
			updatePostSize(0);
			
			Http::Response response;
			response.recv(*mUpSock);
			mUpSock->clear();
			mUpSock->close();

			if(response.code != 200 && response.code != 204)
                                throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
		}
	}

	Scheduler::Global->schedule(&mFlushTask, FlushTimeout);
}

void HttpTunnel::Client::flush(void)
{
	Synchronize(this);

	try {
		if(mUpSock && mUpSock->isConnected() && mPostLeft)
		{
			LogDebug("HttpTunnel::Client::flush", "Flushing (padding "+String::number(std::max(int(mPostLeft)-1, 0))+" bytes)...");
		
			try {
				updatePostSize(mPostLeft);
				writePaddingUntil(1);
				mUpSock->writeBinary(TunnelDisconnect);
			}
			catch(const NetException &e)
			{
				// Nothing to do
			}

			mPostLeft = 0;  // Reset mPostLeft

			Http::Response response;
			response.recv(*mUpSock);
			mUpSock->clear();
			mUpSock->close();

			if(response.code != 200 && response.code != 204)
				throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
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

	if(mPostLeft <= left) return;
	
	while(mPostLeft > left + 3)
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

	Assert(mPostLeft == left);
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
	mDownloadLeft(0),
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
			if(mClosed) return 0;
			//LogDebug("HttpTunnel::Server::readData", "Waiting for connection...");
			if(!wait(timeleft)) throw Timeout();
		}

		//LogDebug("HttpTunnel::Server::readData", "Connection OK");

		uint8_t command;
		uint16_t len = 0;
		
		try {
			Desynchronize(this);

			if(!mUpSock->readBinary(command))
				throw NetException("Connection unexpectedly closed");
			
			if(!(command & 0x40))
				if(!mUpSock->readBinary(len))
					throw NetException("Connection unexpectedly closed");
		}
		catch(const Exception &e)
		{
			throw NetException(String("Unable to read HTTP tunnel command: ") + e.what());
		}

		//LogDebug("HttpTunnel::Server::readData", "Incoming command: " + String::hexa(command, 2) + " (length " + String::number(len) + ")");

		switch(command)
		{
		case TunnelData:
			mPostBlockLeft = len;
			break;

		case TunnelPadding:
		{
			Desynchronize(this);
			if(!mUpSock->ignore(len))
				throw NetException("Connection unexpectedly closed");
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
			mUpSock->close();
			break;
		}

		default:
			LogWarn("HttpTunnel::Server", "Unknown command: " + String::hexa(command));	
			if(!mUpSock->ignore(len))
				throw NetException("Connection unexpectedly closed");
                        break;
		}
	}

	Assert(mUpSock);
	Assert(mPostBlockLeft > 0);

	size_t r;
	DesynchronizeStatement(this, r = mUpSock->readData(buffer, std::min(size, mPostBlockLeft)));
	if(size && !r) throw NetException("Connection unexpectedly closed");
	mPostBlockLeft-= r;
	return r;
}

void HttpTunnel::Server::writeData(const char *data, size_t size)
{
	Synchronize(this);

	while(true)
	{
		if(mDownSock && (!mDownSock->isConnected() || !mDownloadLeft))
		{
			delete mDownSock;
			mDownSock = NULL;
		}

		Scheduler::Global->remove(&mFlushTask);

		double timeleft = ConnTimeout;
		while(!mDownSock)
		{
			if(mClosed) throw NetException("Connection closed");
			//LogDebug("HttpTunnel::Server::writeData", "Waiting for connection...");
			if(!wait(timeleft)) throw Timeout();
		}

		//LogDebug("HttpTunnel::Server::writeData", "Connection OK");
		Assert(mDownloadLeft >= 1);
		if(!size) break;

		if(mDownloadLeft == MaxDownloadSize)    // if no data has already been sent
		{
			try {
				// Try only 
				mDownSock->writeData(data, 1);
                		data++;
                        	size--;
                        	mDownloadLeft--;
			}
			catch(const NetException &e)
			{
				mDownSock->close();
				continue;
			}
		}
	
		size_t s = std::min(size, mDownloadLeft);
		mDownSock->writeData(data, s);
		data+= s;
		size-= s;
		mDownloadLeft-= s;
		if(!size) break;
	
		mDownSock->close();
	}
	
	Scheduler::Global->schedule(&mFlushTask, FlushTimeout);
}

void HttpTunnel::Server::flush(void)
{
	Synchronize(this);

	if(mDownSock && mDownSock->isConnected())
	{
		LogDebug("HttpTunnel::Server::flush", "Flushing...");
		mDownSock->close();
	}

	delete mDownSock;
        mDownSock = NULL;
}

}

