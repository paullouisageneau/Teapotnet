/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "pla/socket.hpp"
#include "pla/exception.hpp"
#include "pla/http.hpp"
#include "pla/proxy.hpp"

namespace pla
{

void Socket::Transfer(Socket *sock1, Socket *sock2)
{
	Assert(sock1);
	Assert(sock2);
  
	char buffer[BufferSize];
	
	while(true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sock1->mSock, &readfds);
		FD_SET(sock2->mSock, &readfds);
		
		int n = std::max(SOCK_TO_INT(sock1->mSock),SOCK_TO_INT(sock2->mSock))+1;
		int ret = ::select(n, &readfds, NULL, NULL, NULL);

		if (ret < 0) throw Exception("Unable to wait on socket");
		if (ret ==  0) break;
		
		if(FD_ISSET(sock1->mSock, &readfds))
		{
			 int count = ::recv(sock1->mSock, buffer, BufferSize, 0);
			 if(count <= 0) break;
			 sock2->writeData(buffer, count);
		}
		
		if(FD_ISSET(sock2->mSock, &readfds))
		{
			 int count = ::recv(sock2->mSock, buffer, BufferSize, 0);
			 if(count <= 0) break;
			 sock1->writeData(buffer, count);
		}
	}
	
	sock1->close();
	sock2->close();
}
  
Socket::Socket(void) :
		mSock(INVALID_SOCKET),
		mConnectTimeout(seconds(-1.)),
		mReadTimeout(seconds(-1.)),
		mWriteTimeout(seconds(-1.))
{

}

Socket::Socket(const Address &a, duration timeout) :
		mSock(INVALID_SOCKET),
		mConnectTimeout(seconds(-1.)),
		mReadTimeout(seconds(-1.)),
		mWriteTimeout(seconds(-1.))
{
	setTimeout(timeout);
	connect(a);
}

Socket::Socket(socket_t sock) :
		mConnectTimeout(seconds(-1.)),
		mReadTimeout(seconds(-1.)),
		mWriteTimeout(seconds(-1.))
{
	mSock = sock;
}

Socket::~Socket(void)
{
	NOEXCEPTION(close());
}

bool Socket::isConnected(void) const
{
	return (mSock != INVALID_SOCKET);
}

bool Socket::isReadable(void) const
{
	if(!isConnected()) return false;
	
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSock, &readfds);

	struct timeval tv;
	tv.tv_sec = 0;
        tv.tv_usec = 0;
	::select(SOCK_TO_INT(mSock)+1, &readfds, NULL, NULL, &tv);
	return FD_ISSET(mSock, &readfds);
}

bool Socket::isWriteable(void) const
{
	if(!isConnected()) return false;
	
	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(mSock, &writefds);

	struct timeval tv;
	tv.tv_sec = 0;
        tv.tv_usec = 0;
	::select(SOCK_TO_INT(mSock)+1, NULL, &writefds, NULL, &tv);
	return FD_ISSET(mSock, &writefds);
}

Address Socket::getLocalAddress(void) const
{
	sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	if(getsockname(mSock, reinterpret_cast<sockaddr*>(&addr), &len))
		throw NetException("Unable to retrieve local address");

	return Address(reinterpret_cast<sockaddr*>(&addr), len);
}

Address Socket::getRemoteAddress(void) const
{
	if(!mProxifiedAddr.isNull()) return mProxifiedAddr;

	sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	if(getpeername(mSock, reinterpret_cast<sockaddr*>(&addr), &len))
		throw NetException("Unable to retrieve remote address");

	return Address(reinterpret_cast<sockaddr*>(&addr), len);
}

void Socket::setConnectTimeout(duration timeout)
{
	mConnectTimeout = timeout;
}

void Socket::setReadTimeout(duration timeout)
{
	mReadTimeout = timeout;
}

void Socket::setWriteTimeout(duration timeout)
{
	mWriteTimeout = timeout;
}

void Socket::setTimeout(duration timeout)
{
	setConnectTimeout(timeout);
	setReadTimeout(timeout);
	setWriteTimeout(timeout);
}

void Socket::connect(const Address &addr, bool noproxy)
{
	String target = addr.toString();
	uint16_t port = addr.port();
	Address proxyAddr;
	
	if(!noproxy && addr.isPublic()
		&& port == 443
		&& Proxy::GetProxyForUrl("https://"+target+"/", proxyAddr))
	{
		connect(proxyAddr, true);
		
		try {
			Http::Request request(target, "CONNECT");
			request.version = "1.1";
			request.headers["Host"] = target;
			request.send(this);
		
			Http::Response response;
			response.recv(this);
			if(response.code != 200)
			{
				String msg = String::number(response.code) + " " + response.message;
				LogWarn("Socket::connect", String("HTTP proxy error: ") + msg);
				throw NetException(msg);
			}
		}
		catch(const Exception &e)
		{
			close();
			throw NetException(String("Connection to ") + addr.toString() + " with proxy failed: " + e.what());
		}

		mProxifiedAddr = addr;
	}
	else try {

		close();
	  
		// Create socket
		mSock = ::socket(addr.addrFamily(),SOCK_STREAM,0);
		if(mSock == INVALID_SOCKET)
			throw NetException("Socket creation failed");

		if(mConnectTimeout >= duration::zero())
		{
			ctl_t b = 1;
			if(ioctl(mSock, FIONBIO, &b) < 0)
				throw Exception("Cannot set non-blocking mode");
		
			// Initiate connection
			::connect(mSock, addr.addr(), addr.addrLen());
			
			fd_set writefds;
			FD_ZERO(&writefds);
			FD_SET(mSock, &writefds);

			struct timeval tv;
			durationToStruct(mConnectTimeout, tv);
			int ret = ::select(SOCK_TO_INT(mSock)+1, NULL, &writefds, NULL, &tv);

			if (ret < 0) 
				throw Exception("Unable to wait on socket");
			
			if (ret ==  0 || ::send(mSock, NULL, 0, 0) != 0)
				throw NetException(String("Connection to ")+addr.toString()+" failed"); 
		
			b = 0;
                	if(ioctl(mSock, FIONBIO, &b) < 0)
                        	throw Exception("Cannot set blocking mode");
		}
		else {
			// Connect it
			if(::connect(mSock,addr.addr(), addr.addrLen()) != 0)
				throw NetException(String("Connection to ")+addr.toString()+" failed");
		}
	}
	catch(...)
	{
		close();
		throw;
	}
}

void Socket::close(void)
{
	if(mSock != INVALID_SOCKET)
	{
		::closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	mProxifiedAddr.clear();
}

size_t Socket::readData(char *buffer, size_t size)
{
	return recvData(buffer, size, 0);
}

void Socket::writeData(const char *data, size_t size)
{
	sendData(data, size, 0);
}

bool Socket::waitData(duration timeout)
{
	if(mSock == INVALID_SOCKET)
		throw NetException("Socket is closed");

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSock, &readfds);

	struct timeval tv;
	durationToStruct(timeout, tv);
	int ret = ::select(SOCK_TO_INT(mSock)+1, &readfds, NULL, NULL, &tv);
	if (ret < 0) throw Exception("Unable to wait on socket");
	return (ret != 0);
}

size_t Socket::peekData(char *buffer, size_t size)
{
        return recvData(buffer, size, MSG_PEEK);
}

size_t Socket::recvData(char *buffer, size_t size, int flags)
{
	if(mSock == INVALID_SOCKET)
		throw NetException("Socket is closed");

	if(mReadTimeout >= duration::zero())
		if(!waitData(mReadTimeout)) 
			throw Timeout();

	int count = ::recv(mSock, buffer, size, flags);	
	if(count < 0)
		throw NetException("Connection lost (error " + String::number(sockerrno) + ")");

	return count;
}

void Socket::sendData(const char *data, size_t size, int flags)
{
	struct timeval tv;
	durationToStruct(std::max(mWriteTimeout, duration::zero()), tv);
	
	do {
		if(mSock == INVALID_SOCKET)
			throw NetException("Socket is closed");

		if(mWriteTimeout >= duration::zero())
		{
			fd_set writefds;
			FD_ZERO(&writefds);
			FD_SET(mSock, &writefds);

			int ret = ::select(SOCK_TO_INT(mSock)+1, NULL, &writefds, NULL, &tv);
			if (ret == -1) 
				throw Exception("Unable to wait on socket");
			if (ret == 0) 
				throw Timeout();
		}
		
		int count = ::send(mSock, data, size, flags);
		if(count < 0) 
			throw NetException("Connection lost (error " + String::number(sockerrno) + ")");
		
		data+= count;
		size-= count;
	}
	while(size);
}

}
