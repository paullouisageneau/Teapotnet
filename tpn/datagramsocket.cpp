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

#include "tpn/datagramsocket.h"
#include "tpn/exception.h"
#include "tpn/string.h"
#include "tpn/time.h"

namespace tpn
{

const int DatagramSocket::MaxDatagramSize = 1500;

DatagramSocket::DatagramSocket(int port, bool broadcast) :
		mSock(INVALID_SOCKET)
{
	bind(port, broadcast);
}

DatagramSocket::DatagramSocket(const Address &local, bool broadcast) :
		mSock(INVALID_SOCKET)
{
 	bind(local, broadcast);  
}

DatagramSocket::~DatagramSocket(void)
{
	NOEXCEPTION(close());
}

Address DatagramSocket::getBindAddress(void) const
{
	char hostname[HOST_NAME_MAX];
	if(gethostname(hostname,HOST_NAME_MAX))
		throw NetException("Cannot retrieve hostname");

	sockaddr_storage sa;
	socklen_t sl = sizeof(sa);
	int ret = ::getsockname(mSock, reinterpret_cast<sockaddr*>(&sa), &sl);
	if(ret < 0) throw NetException("Cannot obtain Address of socket");

	return Address(reinterpret_cast<sockaddr*>(&sa), sl);
}

void DatagramSocket::getLocalAddresses(List<Address> &list) const
{
	list.clear();
  
	Address bindAddr = getBindAddress();
  
#ifdef NO_IFADDRS
	// Retrieve hostname
	char hostname[HOST_NAME_MAX];
	if(gethostname(hostname,HOST_NAME_MAX))
		throw NetException("Cannot retrieve hostname");

	// Resolve hostname
	addrinfo *aiList = NULL;
	addrinfo aiHints;
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_UNSPEC;
	aiHints.ai_socktype = SOCK_DGRAM;
	aiHints.ai_protocol = 0;
	aiHints.ai_flags = 0;
	String service;
	service << mPort;
	if(getaddrinfo(hostname, service.c_str(), &aiHints, &aiList) != 0)
	{
		LogWarn("DatagramSocket", "Local hostname is not resolvable !");
		if(getaddrinfo("localhost", service.c_str(), &aiHints, &aiList) != 0)
		{
			list.push_back(bindAddr);
			return;
		}
	}
	
	addrinfo *ai = aiList;  
	while(ai)
	{
		Address addr(ai->ai_addr,ai->ai_addrlen);
		if(addr == bindAddr)
		{
			list.clear();
			list.push_back(addr);
			break;
		}
		
		if(ai->ai_family == AF_INET || ai->ai_family == AF_INET6)
			list.push_back(addr);
		
		ai = ai->ai_next;
	}
	
	freeaddrinfo(aiList);
#else
	ifaddrs *ifas;
	if(getifaddrs(&ifas) < 0)
		throw NetException("Unable to list network interfaces");

        ifaddrs *ifa = ifas;
	while(ifa)
	{
		sockaddr *sa = ifa->ifa_addr;
		socklen_t len = 0;
		switch(sa->sa_family) 
		{
			case AF_INET:  len = sizeof(sockaddr_in);  break;
			case AF_INET6: len = sizeof(sockaddr_in6); break;
		}
		
		if(len)
		{
			Address addr(sa, len);
			String host = addr.host();
			if(host.substr(0,4) != "fe80")
			{
				addr.set(host, mPort);
				if(addr == bindAddr)
				{
					list.clear();
					list.push_back(addr);
					break;
				}
				list.push_back(addr);
			}
		}
		
		ifa = ifa->ifa_next;
	}

	freeifaddrs(ifas);
#endif
}

void DatagramSocket::bind(int port, bool broadcast)
{
	close();
	
	mPort = port;
	
	// Obtain local Address
	addrinfo *aiList = NULL;
	addrinfo aiHints;
	std::memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_UNSPEC;
	aiHints.ai_socktype = SOCK_DGRAM;
	aiHints.ai_protocol = 0;
	aiHints.ai_flags = AI_PASSIVE;
	String service;
	service << port;
	if(getaddrinfo(NULL, service.c_str(), &aiHints, &aiList) != 0)
		throw NetException("Local binding address resolution failed for UDP port "+port);
	
	try {
		// Prefer IPv6
		addrinfo *ai = aiList;
		while(ai && ai->ai_family != AF_INET6)
			ai = ai->ai_next;
		if(!ai) ai = aiList;
			
		// Create socket
		mSock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if(mSock == INVALID_SOCKET)
		{
			addrinfo *first = ai;
			ai = aiList;
			while(ai)
			{
				if(ai != first) mSock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
				if(mSock != INVALID_SOCKET) break;
				ai = ai->ai_next;
			}
			if(!ai) throw NetException("Datagram socket creation failed");
		}


		// Set options
		int enabled = 1;
		int disabled = 0;
		setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&enabled), sizeof(enabled));
		if(broadcast) setsockopt(mSock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&enabled), sizeof(enabled));
		if(ai->ai_family == AF_INET6) 
			setsockopt(mSock, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&disabled), sizeof(disabled)); 
		
		// Bind it
		if(::bind(mSock, ai->ai_addr, ai->ai_addrlen) != 0)
			throw NetException(String("Binding failed on UDP port ") + String::number(port));

		/*
		ctl_t b = 1;
		if(ioctl(mSock,FIONBIO,&b) < 0)
		throw Exception("Cannot use non-blocking mode");
		 */
	}
	catch(...)
	{
		freeaddrinfo(aiList);
		close();
		throw;
	}
	
	freeaddrinfo(aiList);
}

void DatagramSocket::bind(const Address &local, bool broadcast)
{
	close();
	
	try {
		mPort = local.port();

		// Create datagram socket
		mSock = ::socket(local.addrFamily(), SOCK_DGRAM, 0);
		if(mSock == INVALID_SOCKET)
			throw NetException("Datagram socket creation failed");

		// Set options
		int enabled = 1;
		setsockopt(mSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&enabled), sizeof(enabled));
		if(broadcast) setsockopt(mSock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&enabled), sizeof(enabled));
		
		// Bind it
		if(::bind(mSock, local.addr(), local.addrLen()) != 0)
			throw NetException(String("Binding failed on ") + local.toString());

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

void DatagramSocket::close(void)
{
	if(mSock != INVALID_SOCKET)
	{
		::closesocket(mSock);
		mSock = INVALID_SOCKET;
		mPort = 0;
	}
}

int DatagramSocket::read(char *buffer, size_t size, Address &sender, double &timeout)
{
	if(timeout > 0.)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(mSock, &readfds);

		struct timeval tv;
		Time::SecondsToStruct(timeout, tv);
		int ret = ::select(SOCK_TO_INT(mSock)+1, &readfds, NULL, NULL, &tv);
		if (ret == -1) throw Exception("Unable to wait on socket");
		if (ret ==  0)
		{
			timeout = 0.;
			return -1;
		}
		
		timeout = Time::StructToSeconds(tv);
	}
  
	sockaddr_storage sa;
	socklen_t sl = sizeof(sa);
	size = ::recvfrom(mSock, buffer, size, 0, reinterpret_cast<sockaddr*>(&sa), &sl);
	sender.set(reinterpret_cast<sockaddr*>(&sa),sl);
	if(size < 0) throw NetException("Unable to read from socket");
	return size;
}

int DatagramSocket::read(char *buffer, size_t size, Address &sender, const double &timeout)
{
	double dummy = timeout;
	return  read(buffer, size, sender, dummy);
}

void DatagramSocket::write(const char *buffer, size_t size, const Address &receiver)
{
	size = ::sendto(mSock, buffer, size, 0, receiver.addr(), receiver.addrLen());
	if(size < 0) throw NetException("Unable to write to socket");
}

bool DatagramSocket::read(ByteStream &stream, Address &sender, double &timeout)
{
	stream.clear();
	char buffer[MaxDatagramSize];
	int size = MaxDatagramSize;
	size = read(buffer, size, sender, timeout);
	if(size < 0) return false;
	stream.writeData(buffer,size);
	return true;
}

bool DatagramSocket::read(ByteStream &stream, Address &sender, const double &timeout)
{
	double dummy = timeout;
	return  read(stream, sender, dummy);
}

void DatagramSocket::write(ByteStream &stream, const Address &receiver)
{
	char buffer[MaxDatagramSize];
	size_t size = stream.readData(buffer,MaxDatagramSize);
	write(buffer, size, receiver);
	stream.clear();
}

}
