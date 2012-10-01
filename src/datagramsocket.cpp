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

#include "datagramsocket.h"
#include "exception.h"
#include "string.h"

namespace tpot
{

const int DatagramSocket::MaxDatagramSize = 1500;

DatagramSocket::DatagramSocket(int port)
{
	addrinfo *aiList = NULL;
	mSock = INVALID_SOCKET;

	try {
		// Obtain local Address
		addrinfo aiHints;
		memset(&aiHints, 0, sizeof(aiHints));
		aiHints.ai_family = AF_UNSPEC;
		aiHints.ai_socktype = SOCK_DGRAM;
		aiHints.ai_protocol = 0;
		aiHints.ai_flags = AI_PASSIVE;
		String service;
		service << port;
		if(getaddrinfo(NULL, service.c_str(), &aiHints, &aiList) != 0)
			throw NetException("Local binding Address resolution failed for port "+port);

		// Create datagram socket
		mSock = socket(aiList->ai_family,aiList->ai_socktype,aiList->ai_protocol);
		if(mSock == INVALID_SOCKET)
			throw NetException("Socket creation failed");

		// Bind it
		if(bind(mSock,aiList->ai_addr,aiList->ai_addrlen) != 0)
			throw NetException("Binding failed on port "+String::number(port));

		/*
		ctl_t b = 1;
		if(ioctl(mSock,FIONBIO,&b) < 0)
		throw Exception("Cannot use non-blocking mode");
		 */

		mPort = port;

		// Clean up
		freeaddrinfo(aiList);
		aiList = NULL;
	}
	catch(...)
	{
		if(aiList) freeaddrinfo(aiList);
		if(mSock != INVALID_SOCKET) close(mSock);
		throw;
	}
}

DatagramSocket::~DatagramSocket(void)
{
	if(mSock != INVALID_SOCKET) ::closesocket(mSock);
}

Address DatagramSocket::getLocalAddress(void)
{
	const int MaxHostnameSize = HOST_NAME_MAX;

	char hostname[MaxHostnameSize];
	if(gethostname(hostname,MaxHostnameSize))
		throw NetException("Cannot retrieve hostname");

	sockaddr_storage sa;
	socklen_t sl = sizeof(sa);
	int ret = getsockname(mSock,reinterpret_cast<sockaddr*>(&sa), &sl);
	if(ret < 0) throw NetException("Cannot obtain Address of socket");

	// Résolution du nom de l'hôte
	addrinfo *aiList = NULL;
	addrinfo aiHints;
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = sa.ss_family;
	aiHints.ai_socktype = SOCK_DGRAM;
	aiHints.ai_protocol = 0;
	aiHints.ai_flags = 0;
	String service;
	service << mPort;
	if(getaddrinfo(hostname, service.c_str(), &aiHints, &aiList) != 0)
		throw NetException(String("Address resolution failed for ")+String(hostname));

	Address addr(aiList->ai_addr,aiList->ai_addrlen);

	freeaddrinfo(aiList);
	return addr;
}

int DatagramSocket::read(char *buffer, size_t size, Address &sender)
{
	sockaddr_storage sa;
	socklen_t sl = sizeof(sa);
	size = recvfrom(mSock, buffer, size, 0, reinterpret_cast<sockaddr*>(&sa), &sl);
	sender.set(reinterpret_cast<sockaddr*>(&sa),sl);
	if(size < 0) throw NetException("Unable to read");
	return size;
}

void DatagramSocket::write(const char *buffer, size_t size, const Address &receiver)
{
	size = sendto(mSock, buffer, size, 0, receiver.addr(), receiver.addrLen());
	if(size < 0) throw NetException("Unable to write");
}

void DatagramSocket::read(ByteStream &stream, Address &sender)
{
	char buffer[MaxDatagramSize];
	size_t size = MaxDatagramSize;
	size = read(buffer, size, sender);
	stream.clear();
	stream.writeData(buffer,size);
}

void DatagramSocket::write(ByteStream &stream, const Address &receiver)
{
	char buffer[MaxDatagramSize];
	size_t size = stream.readData(buffer,MaxDatagramSize);
	stream.clear();
	write(buffer, size, receiver);
}

}
