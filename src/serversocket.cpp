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

#include "serversocket.h"
#include "exception.h"
#include "string.h"

namespace arc
{

ServerSocket::ServerSocket(void) :
	mSock(INVALID_SOCKET),
	mPort(0)
{

}

ServerSocket::ServerSocket(int port) :
	mSock(INVALID_SOCKET),
	mPort(0)
{
	listen(port);
}

ServerSocket::~ServerSocket(void)
{
	if(mSock != INVALID_SOCKET) ::close(mSock);
}

bool ServerSocket::isListening(void) const
{
	return (mSock != INVALID_SOCKET);
}

int ServerSocket::getPort(void) const
{
	if(mSock != INVALID_SOCKET) return mPort;
	else return 0;
}

Address ServerSocket::getLocalAddress(void) const
{
	// Retrieve hostname
	char hostname[HOST_NAME_MAX];
	if(gethostname(hostname,HOST_NAME_MAX))
		throw NetException("Cannot retrieve hostname");

	// remove domain name
	//std::replace(hostname, hostname+strlen(hostname), '.', '\0');

	// Resolve hostname
	addrinfo *aiList = NULL;
	addrinfo aiHints;
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_UNSPEC;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = 0;
	aiHints.ai_flags = 0;
	String service;
	service << mPort;
	if(getaddrinfo(hostname, service.c_str(), &aiHints, &aiList) != 0)
		throw NetException(String("Address resolution failed for ")+String(hostname));

	return Address(aiList->ai_addr,aiList->ai_addrlen);
}

void ServerSocket::listen(int port)
{
	close();

	mPort = port;
	addrinfo *aiList = NULL;
	mSock = INVALID_SOCKET;

	try {
		// Obtain local Address
		addrinfo aiHints;
		memset(&aiHints, 0, sizeof(aiHints));
		aiHints.ai_family = AF_UNSPEC;
		aiHints.ai_socktype = SOCK_STREAM;
		aiHints.ai_protocol = 0;
		aiHints.ai_flags = AI_PASSIVE;
		String service;
		service << port;
		if(getaddrinfo(NULL, service.c_str(), &aiHints, &aiList) != 0)
			throw NetException(String("Local binding Address resolution failed for port ")+String::number(port));

		// Create chunk socket
		mSock = socket(aiList->ai_family,aiList->ai_socktype,aiList->ai_protocol);
		if(mSock == INVALID_SOCKET)
			throw NetException("Socket creation failed");

		// Bind it
		if(bind(mSock,aiList->ai_addr,aiList->ai_addrlen) != 0)
			throw NetException(String("Binding failed on port ")+String::number(port));

		// Listen
		if(::listen(mSock, 16) != 0)
			throw NetException(String("Listening failed on port ")+String::number(port));

		/*
		ctl_t b = 1;
		if(ioctl(mSock,FIONBIO,&b) < 0)
			throw Exception("Cannot use non-blocking mode");
		 */

		// Clean up
		freeaddrinfo(aiList);
		aiList = NULL;
	}
	catch(...)
	{
		if(aiList) freeaddrinfo(aiList);
		close();
		throw;
	}
}

void ServerSocket::close(void)
{
	if(mSock != INVALID_SOCKET)
	{
		::close(mSock);
		mSock = INVALID_SOCKET;
		mPort = 0;
	}
}

Socket ServerSocket::accept(void)
{
	if(mSock == INVALID_SOCKET) throw NetException("Socket not listening");

	socket_t clientSock = ::accept(mSock, NULL, NULL);
	if(clientSock == INVALID_SOCKET) throw NetException(String("Listening socket closed on port ")+String::number(mPort));
	return Socket(clientSock);
}

}
