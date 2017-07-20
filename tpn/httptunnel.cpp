/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#include "tpn/httptunnel.hpp"
#include "tpn/config.hpp"

#include "pla/exception.hpp"
#include "pla/scheduler.hpp"
#include "pla/random.hpp"
#include "pla/proxy.hpp"

namespace tpn
{

#ifdef ANDROID
String HttpTunnel::UserAgent = "Mozilla/5.0 (Android; Mobile; rv:40.0) Gecko/48.0 Firefox/48.0";	// mobile is important
#else
String HttpTunnel::UserAgent = "Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; Trident/6.0)";	// IE should be better for very restrictive environments
#endif

size_t HttpTunnel::DefaultPostSize = 1*1024;		// 1 KB
size_t HttpTunnel::MaxPostSize = 2*1024*1024;		// 2 MB
size_t HttpTunnel::MaxDownloadSize = 20*1024*1024;	// 20 MB
duration HttpTunnel::ConnTimeout = seconds(30.);
duration HttpTunnel::SockTimeout = seconds(10.);
duration HttpTunnel::FlushTimeout = seconds(0.2);
duration HttpTunnel::ReadTimeout = seconds(60.);

std::map<uint32_t, HttpTunnel::SessionEntry> HttpTunnel::Sessions;
std::mutex HttpTunnel::SessionsMutex;
std::condition_variable HttpTunnel::SessionsCondition;

HttpTunnel::Server *HttpTunnel::Incoming(Socket *sock)
{
	Http::Request request;
	try {
		sock->setTimeout(SockTimeout);
		request.recv(sock, false); // POST content is not parsed

		try {
			std::unique_lock<std::mutex> lock(SessionsMutex);

			uint32_t session = 0;
			String cookie;
			if(request.cookies.get("session", cookie)) cookie.extract(session);

			//LogDebug("HttpTunnel::Incoming", "Received " + request.method + " " + request.fullUrl + " (session="+String::hexa(session)+")");

			if(!session)
			{
				if(request.method != "GET")
				{
					LogDebug("HttpTunnel::Incoming", "Missing session number in request");
					throw 400;
				}

				// Find non-existent session
				while(!session || Sessions.find(session) != Sessions.end())
					Random().readBinary(session);

				SessionEntry entry;
				entry.server = new Server(session);
				Sessions.insert(std::make_pair(session, entry));

				Http::Response response(request, 200);
				response.headers["Cache-Control"] = "no-cache";
				response.headers["Content-Type"] = "text/html";
				response.cookies["session"] << session;

				// First response should be forced down the potential proxy as soon as possible
				lock.unlock();
				response.send(sock);
				delete sock;
				return entry.server;
			}

			auto it = Sessions.find(session);
			if(it == Sessions.end())
			{
				// Unknown or closed session
				LogDebug("HttpTunnel::Incoming", "Unknown or closed session: " + String::hexa(session));
				throw 400;
			}

			if(request.method == "GET")
			{
				if(!it->second.server)  throw 400;	// Closed session
				if(it->second.downSock) throw 409;	// Conflict

				Http::Response response(request, 200);
				response.headers["Cache-Control"] = "no-cache";
				response.headers["Content-Type"] = "application/octet-stream";
				response.cookies["session"] << session;
				response.send(sock);

				it->second.downSock = sock;
				SessionsCondition.notify_all();
				return NULL;
			}
			else {	// POST
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

				if(!it->second.server) throw 400;	// Closed session
				if(it->second.upSock)  throw 409;	// Conflict

				it->second.upSock = sock;
				SessionsCondition.notify_all();
				return NULL;
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
			response.send(sock);
		}
		catch(...)
		{

		}
	}

	delete sock;
	return NULL;
}

Socket *HttpTunnel::WaitServerUp(uint32_t session)
{
	std::unique_lock<std::mutex> lock(SessionsMutex);

	if(!SessionsCondition.wait_for(lock, ReadTimeout, [session]() {
		auto it = Sessions.find(session);
		if(it == Sessions.end()) return true;
		return (it->second.upSock != NULL);
	}))
		throw Timeout();

	auto it = Sessions.find(session);
	if(it == Sessions.end()) return NULL;

	Socket *sock = NULL;
	std::swap(it->second.upSock, sock);
	return sock;
}

Socket *HttpTunnel::WaitServerDown(uint32_t session)
{
	std::unique_lock<std::mutex> lock(SessionsMutex);

	if(!SessionsCondition.wait_for(lock, ReadTimeout, [session]() {
		auto it = Sessions.find(session);
		if(it == Sessions.end()) return true;
		return (it->second.downSock != NULL);
	}))
		throw Timeout();

	auto it = Sessions.find(session);
	if(it == Sessions.end()) return NULL;

	Socket *sock = NULL;
	std::swap(it->second.downSock, sock);
	return sock;
}

HttpTunnel::Client::Client(const Address &addr, duration timeout) :
	mAddress(addr),
	mReverse(addr.reverse()),
	mSession(0),
	mPostSize(DefaultPostSize),
	mPostLeft(0),
	mConnTimeout(ConnTimeout),
	mClosed(false)
{
	mFlusher.set([this]()
	{
		this->flush();
	});

	mUpSock.setTimeout(SockTimeout);
	mDownSock.setTimeout(SockTimeout);

	if(timeout >= duration::zero()) mConnTimeout = timeout;
	readData(NULL, 0); 	// Connect mDownSock

	Assert(mSession);	// mSession should be set
	LogDebug("HttpTunnel::Client", "Starting HTTP tunnel client session: "+String::hexa(mSession));

	// Set timeout for the following connections
	mConnTimeout = ConnTimeout;
}

HttpTunnel::Client::~Client(void)
{
	NOEXCEPTION(close());

	mFlusher.cancel();
}

void HttpTunnel::Client::close(void)
{
	if(mClosed) return;
	LogDebug("HttpTunnel::Client", "Closing HTTP tunnel client session: "+String::hexa(mSession));

	mClosed = true;
	mFlusher.cancel();

	// Properly closes up stream
	try {
		if(mUpSock.isConnected() && mPostLeft)
		{
			writePaddingUntil(2);
			if(mPostLeft >= 1) mUpSock.writeBinary(TunnelClose);
			if(mPostLeft >= 2) mUpSock.writeBinary(TunnelDisconnect);
		}
	}
	catch(const NetException &e)
	{
		// Ignored
	}

	// Close sockets
	try {
		mUpSock.close();
		mDownSock.close();
	}
	catch(const NetException &e)
	{
		// Ignored
	}

	// Reset state
	mPostLeft = 0;
	mSession = 0;
}

size_t HttpTunnel::Client::readData(char *buffer, size_t size)
{
	using clock = std::chrono::steady_clock;
	auto end = clock::now() + ReadTimeout;

	while(clock::now() < end)
	{
		if(mClosed) return 0;

		bool reconnection = !mDownSock.isConnected();
		if(reconnection)
		{
			String url;
			Assert(!mReverse.empty());
			url<<"http://"<<mReverse<<"/"<<String::random(10);

			//LogDebug("HttpTunnel::Client::readData", "GET " + url);

			Http::Request request(url, "GET");
			request.headers["User-Agent"] = UserAgent;
			if(mSession) request.cookies["session"] << mSession;

			bool hasProxy = Proxy::GetProxyForUrl(url, mAddress);
			if(hasProxy) request.url = url;			// Full URL for proxy

			try {
				mDownSock.setConnectTimeout(mConnTimeout);
				mDownSock.connect(mAddress, true);		// Connect without proxy
				request.send(&mDownSock);
			}
			catch(const NetException &e)
			{
				if(mClosed) return 0;
				if(hasProxy) LogWarn("HttpTunnel::Client", String("HTTP proxy error: ") + e.what());
				throw;
			}

			Http::Response response;
			mDownSock.setReadTimeout(SockTimeout);
			response.recv(&mDownSock);

			if(response.code != 200)
			{
				if(mSession)
				{
					if(response.code == 400)	// Closed session
					{
						LogDebug("HttpTunnel::Client", "Session closed");
						return 0;
					}
					else if(response.code == 504)	// Proxy timeout
					{
						LogDebug("HttpTunnel::Client", "HTTP proxy timeout, retrying...");
						continue;
					}
					else if(response.code == 409)	// Conflict
					{
						std::this_thread::sleep_for(seconds(1));
						continue;
					}
				}

				throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
			}

			if(!mSession)
			{
				String cookie;
				if(response.cookies.get("session", cookie)) cookie.extract(mSession);
				if(!mSession) throw NetException("HTTP transaction failed: Invalid cookie");
			}
			else {
				//LogDebug("HttpTunnel::Client::readData", "Down stream connected");
			}
		}

		if(!size) return 0;

		try {
			mDownSock.setReadTimeout(ReadTimeout);
			size_t ret = mDownSock.readData(buffer, size);
			if(ret) return ret;
		}
		catch(const NetException &e)
		{
			if(!reconnection)
			{
				mDownSock.close();
				throw;
			}
		}

		mDownSock.close();
	}

	throw Timeout();
}

void HttpTunnel::Client::writeData(const char *data, size_t size)
{
	mFlusher.cancel();

	while(size || mClosed)
	{
		if(mClosed) throw NetException("Connection closed");
		if(!mSession) readData(NULL, 0);	// ensure session is opened

		if(!mUpSock.isConnected() || !mPostLeft)
		{
			String url;
			Assert(!mReverse.empty());
			url<<"http://"<<mReverse<<"/"<<String::random(10);

			//LogDebug("HttpTunnel::Client::writeData", "POST " + url);

			Http::Request request(url, "POST");
			request.headers["User-Agent"] = UserAgent;
			request.headers["Content-Length"] << mPostSize;
			request.cookies["session"] << mSession;

			bool hasProxy = Proxy::GetProxyForUrl(url, mAddress);
			if(hasProxy) request.url = url; 	        // Full URL for proxy

			try {
				mUpSock.setConnectTimeout(mConnTimeout);
				mUpSock.connect(mAddress, true);	// Connect without proxy
				request.send(&mUpSock);
			}
			catch(const NetException &e)
			{
				if(hasProxy) LogWarn("HttpTunnel::Client", String("HTTP proxy error: ") + e.what());
				throw;
			}

			//LogDebug("HttpTunnel::Client::writeData", "Up stream connected");

			mPostLeft = mPostSize;
			mUpSock.writeBinary(TunnelOpen);	// 1 byte
			mUpSock.writeBinary(uint16_t(0));	// 2 bytes, no auth data
			mPostLeft-= 3;
		}

		if(mPostLeft > 4)
		{
			size_t len = std::min(size, mPostLeft-4);
			len = std::min(len, size_t(0xFFFF));
			mUpSock.writeBinary(TunnelData);	// 1 byte
			mUpSock.writeBinary(uint16_t(len));	// 2 bytes
			mUpSock.writeData(data, len);		// len bytes
			mPostLeft-= len + 3;
			data+= len;
			size-= len;
		}
		else {
			while(mPostLeft > 1)
			{
				mUpSock.writeBinary(TunnelPad);
				mPostLeft-= 1;
			}
		}

		Assert(mPostLeft >= 1);
		if(mPostLeft == 1)
		{
			mUpSock.writeBinary(TunnelDisconnect);
			mPostLeft = 0;
			updatePostSize(0);

			Http::Response response;
			response.recv(&mUpSock);
			mUpSock.clear();
			mUpSock.close();

			if(response.code != 200 && response.code != 204)
				throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
		}
	}

	if(!mClosed) mFlusher.schedule(FlushTimeout);
}

void HttpTunnel::Client::flush(void)
{
	if(mClosed) return;

	try {
		if(mUpSock.isConnected() && mPostLeft)
		{
			//LogDebug("HttpTunnel::Client::flush", "Flushing (padding "+String::number(std::max(int(mPostLeft)-1, 0))+" bytes)...");

			try {
				updatePostSize(mPostLeft);
				writePaddingUntil(1);
				mUpSock.writeBinary(TunnelDisconnect);
			}
			catch(const NetException &e)
			{
				// Nothing to do
			}
			catch(const Timeout &e)
			{
				// Nothing to do
			}

			mPostLeft = 0;  // Reset mPostLeft

			mUpSock.setTimeout(ReadTimeout);
			Http::Response response;
			response.recv(&mUpSock);
			mUpSock.clear();
			mUpSock.close();

			if(response.code != 200 && response.code != 204)
				throw NetException("HTTP transaction failed: " + String::number(response.code) + " " + response.message);
		}
	}
	catch(const Exception &e)
	{
		//LogWarn("HttpTunnel::Client::flush", e.what());
	}
}

void HttpTunnel::Client::writePaddingUntil(size_t left)
{
	if(mPostLeft <= left) return;

	while(mPostLeft > left + 3)
	{
		size_t len = std::min(mPostLeft - (left + 3), size_t(0xFFFF));
		mUpSock.writeBinary(TunnelPadding);    // 1 byte
		mUpSock.writeBinary(uint16_t(len));    // 2 bytes
		mUpSock.writeZero(len);
		mPostLeft-= len + 3;
	}

	while(mPostLeft > left)
	{
		mUpSock.writeBinary(TunnelPad);
		mPostLeft-= 1;
	}

	Assert(mPostLeft == left);
}

void HttpTunnel::Client::updatePostSize(size_t left)
{
	if(left == 0) mPostSize = std::min(mPostSize*2, MaxPostSize);
	mPostSize = std::max(mPostSize - left + 1, DefaultPostSize);
}

HttpTunnel::Server::Server(uint32_t session) :
	mUpSock(NULL),
	mDownSock(NULL),
	mSession(session),
	mPostBlockLeft(0),
	mDownloadLeft(0),
	mClosed(false)
{
	mFlusher.set([this]()
	{
		this->flush();
	});

	Assert(mSession);
	LogDebug("HttpTunnel::Server", "Starting HTTP tunnel server session: "+String::hexa(mSession));
}

HttpTunnel::Server::~Server(void)
{
	NOEXCEPTION(close());

	mFlusher.cancel();

	delete mUpSock;
	delete mDownSock;
	mUpSock = NULL;
	mDownSock = NULL;
}

void HttpTunnel::Server::close(void)
{
	if(mClosed) return;
	LogDebug("HttpTunnel::Server", "Closing HTTP tunnel server session: "+String::hexa(mSession));

	mClosed = true;
	mFlusher.cancel();

	{
		std::unique_lock<std::mutex> lock(SessionsMutex);
		Sessions.erase(mSession);
		SessionsCondition.notify_all();
	}
}

size_t HttpTunnel::Server::readData(char *buffer, size_t size)
{
	while(!mPostBlockLeft)
	{
		if(mClosed) return 0;

		if(mUpSock && !mUpSock->isConnected())
		{
			delete mUpSock;
			mUpSock = NULL;
		}

		if(!mUpSock)
		{
			//LogDebug("HttpTunnel::Server::readData", "Waiting for up stream...");
			mUpSock = WaitServerUp(mSession);
			if(!mUpSock) return 0;
		}

		//LogDebug("HttpTunnel::Server::readData", "Up stream connected");
		mUpSock->setTimeout(SockTimeout);

		uint8_t command;
		uint16_t len = 0;

		try {
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
			{
				mPostBlockLeft = len;
				break;
			}

			case TunnelPadding:
			{
				if(!mUpSock->ignore(len))
					throw NetException("Connection unexpectedly closed");
				break;
			}

			case TunnelPad:
			{
				// Do nothing
				break;
			}

			case TunnelClose:
			{
				mClosed = true;
				break;
			}

			case TunnelDisconnect:
			{
				Http::Response response(204);	// no content
				response.send(mUpSock);
				mUpSock->close();
				break;
			}

			default:
			{
				LogWarn("HttpTunnel::Server", "Unknown command: " + String::hexa(command));
				if(!mUpSock->ignore(len))
					throw NetException("Connection unexpectedly closed");
				break;
			}
		}
	}

	size_t r = mUpSock->readData(buffer, std::min(size, mPostBlockLeft));
	if(size && !r) throw NetException("Connection unexpectedly closed");
	mPostBlockLeft-= r;
	return r;
}

void HttpTunnel::Server::writeData(const char *data, size_t size)
{
	mFlusher.cancel();

	while(true)
	{
		if(mClosed) throw NetException("Connection closed");

		if(mDownSock && (!mDownSock->isConnected() || !mDownloadLeft))
		{
			delete mDownSock;
			mDownSock = NULL;
		}

		if(!mDownSock)
		{
			//LogDebug("HttpTunnel::Server::readData", "Waiting for down stream...");
			mDownSock = WaitServerDown(mSession);
			if(!mDownSock) throw NetException("Session aborted");
			mDownloadLeft = MaxDownloadSize;
		}

		//LogDebug("HttpTunnel::Server::writeData", "Down stream connected");
		Assert(mDownloadLeft > 0);
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

	if(!mClosed) mFlusher.schedule(FlushTimeout);
}

void HttpTunnel::Server::flush(void)
{
	if(mClosed) return;

	try {
		mDownloadLeft = 0;

		if(mDownSock && mDownSock->isConnected())
		{
			//LogDebug("HttpTunnel::Server::flush", "Flushing...");
			mDownSock->close();
		}
	}
	catch(const Exception &e)
	{
		//LogWarn("HttpTunnel::Client::flush", e.what());
	}
}

}
