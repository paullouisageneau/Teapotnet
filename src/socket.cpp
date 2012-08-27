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

#include "socket.h"
#include "exception.h"

namespace arc
{

Socket::Socket(void) :
		mSock(INVALID_SOCKET)
{

}

Socket::Socket(const Address &Address) :
		mSock(INVALID_SOCKET)
{
	connect(Address);
}

Socket::Socket(socket_t sock)
{
	mSock = sock;
}

Socket::~Socket(void)
{
	close();
}

bool Socket::isConnected(void) const
{
	return (mSock != INVALID_SOCKET);
}

Address Socket::getRemoteAddress(void) const
{
	// TODO: NOT IMPLEMENTED
	return Address();
}

void Socket::connect(const Address &Address)
{
	close();

	try {
		// Create chunk socket
		mSock = ::socket(Address.getaddrFamily(),SOCK_STREAM,0);
		if(mSock == INVALID_SOCKET)
			throw NetException("Socket creation failed");

		// Connect it
		if(::connect(mSock,Address.getaddr(),Address.getaddrLen()) != 0)
			throw NetException(String("Connection to ")+Address.toString()+" failed");

		/*
		ctl_t b = 1;
		if(ioctl(mSock,FIONBIO,&b) < 0)
			throw Exception("Cannot use non-blocking mode");
		 */
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
		::close(mSock);
		mSock = INVALID_SOCKET;
	}
}

size_t Socket::readData(char *buffer, size_t size)
{
	int r = recv(mSock,buffer,size,0);
	if(r < 0)
	{
		close();
		throw NetException("Connection lost");
	}
	return r;
}

void Socket::writeData(const char *data, size_t size)
{
	// TODO: verify all the data has been sent
	int r = send(mSock,data,size,0);
	if(r < 0)
	{
		close();
		throw NetException("Connection lost");
	}
}

}
