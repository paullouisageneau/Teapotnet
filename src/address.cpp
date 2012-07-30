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

#include "address.h"
#include "exception.h"
#include "string.h"
#include "bytestring.h"

namespace arc
{

Address::Address(void)
{
	setNull();
}

Address::Address(const String &host, const String &port)
{
	set(host,port);
}

Address::Address(String Address)
{
	deserialize(Address);
}

Address::Address(const sockaddr *addr, socklen_t addrlen)
{
	set(addr,addrlen);
}

Address::~Address(void)
{

}

void Address::set(const String &host, const String &service)
{
	addrinfo aiHints;
	std::memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_UNSPEC;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;
	aiHints.ai_flags = 0;

	addrinfo *aiList = NULL;
	if(getaddrinfo(host.c_str(), service.c_str(), &aiHints, &aiList) != 0)
		throw NetException("Address resolution failed for "+host+":"+service);

	maddrLen = aiList->ai_addrlen;
	std::memcpy(&maddr,aiList->ai_addr,aiList->ai_addrlen);

	freeaddrinfo(aiList);
}

void Address::set(const sockaddr *addr, socklen_t addrlen)
{
	if(!addr || !addrlen)
	{
		maddrLen = 0;
		return;
	}

	maddrLen = addrlen;
	std::memcpy(&maddr,addr,addrlen);
}

void Address::setNull(void)
{
	maddrLen = 0;
}

bool Address::isNull(void) const
{
	return (maddrLen == 0);
}

void Address::serialize(Stream &s) const
{
	char host[HOST_NAME_MAX];
	char service[SERVICE_NAME_MAX];
	if(getnameinfo(getaddr(), getaddrLen(), host, HOST_NAME_MAX, service, SERVICE_NAME_MAX, NI_NUMERICHOST|NI_NUMERICSERV))
		throw InvalidData("Invalid stored network Address");

	s<<host<<':'<<service;
}

void Address::deserialize(Stream &s)
{
	String str;
	assertIO(s.readField(str));
	int separator = str.find_last_of(':');
	if(separator == String::NotFound) throw InvalidData("Invalid network Address: " + str);
	String host(str,0,separator);
	String service(str,separator+1);

	addrinfo aiHints;
	std::memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_UNSPEC;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;
	aiHints.ai_flags = 0;

	addrinfo *aiList = NULL;
	if(getaddrinfo(host.c_str(), service.c_str(), &aiHints, &aiList) != 0)
		throw InvalidData("Invalid network Address: " + str);

	maddrLen = aiList->ai_addrlen;
	std::memcpy(&maddr,aiList->ai_addr,aiList->ai_addrlen);

	freeaddrinfo(aiList);
}

void Address::serializeBinary(ByteStream &s) const
{
	switch(getaddrFamily())
	{
	case AF_INET:	// IP v4
	{
		s.writeBinary(uint8_t(4));
		const sockaddr_in *sa = reinterpret_cast<const sockaddr_in*>(&maddr);
		const char *b = reinterpret_cast<const char *>(&sa->sin_addr.s_addr);

		for(int i=0; i<4; ++i) s.writeBinary(uint8_t(b[i]));
		s.writeBinary(uint16_t(ntohs(sa->sin_port)));
		break;
	}

	case AF_INET6:	// IP v6
	{
		s.writeBinary(uint8_t(16));
		const sockaddr_in6 *sa6 = reinterpret_cast<const sockaddr_in6*>(&maddr);

		for(int i=0; i<16; ++i) s.writeBinary(uint8_t(sa6->sin6_addr.s6_addr[i]));
		s.writeBinary(uint16_t(ntohs(sa6->sin6_port)));
		break;
	}

	default:
		throw InvalidData("Stored network Address cannot be serialized to binary");
	}
}

void Address::deserializeBinary(ByteStream &s)
{
	uint8_t size = 0;
	assertIO(s.readBinary(size));

	if(size == 0)
	{
		maddrLen = 0;
		return;
	}

	switch(size)
	{
	case 4:		// IP v4
	{
		maddrLen = sizeof(sockaddr_in);
		sockaddr_in *sa = reinterpret_cast<sockaddr_in*>(&maddr);
		char *b = reinterpret_cast<char *>(&sa->sin_addr.s_addr);

		sa->sin_family = AF_INET;
		uint8_t u;
		for(int i=0; i<4; ++i)
		{
				assertIO(s.readBinary(u)); b[i] = u;
		}
		uint16_t port;
		assertIO(s.readBinary(port));
		sa->sin_port = htons(port);
		break;
	}

	case 16:	// IP v6
	{
		maddrLen = sizeof(sockaddr_in6);
		sockaddr_in6 *sa6 = reinterpret_cast<sockaddr_in6*>(&maddr);

		sa6->sin6_family = AF_INET6;
		uint8_t u;
		for(int i=0; i<16; ++i)
		{
			assertIO(s.readBinary(u));
			sa6->sin6_addr.s6_addr[i] = u;
		}
		uint16_t port;
		assertIO(s.readBinary(port));
		sa6->sin6_port = htons(port);
		break;
	}

	default:
		throw InvalidData("Invalid network Address");
	}
}

const sockaddr *Address::getaddr(void) const
{
	return reinterpret_cast<const sockaddr*>(&maddr);
}

int Address::getaddrFamily(void) const
{
	return reinterpret_cast<const sockaddr_in*>(&maddr)->sin_family;
}

socklen_t Address::getaddrLen(void) const
{
	return maddrLen;
}

bool operator < (const Address &a1, const Address &a2)
{
	if(a1.getaddrLen() != a2.getaddrLen()) return a1.getaddrLen() < a2.getaddrLen();
	return std::memcmp(a1.getaddr(),a2.getaddr(),a1.getaddrLen()) < 0;
}

bool operator > (const Address &a1, const Address &a2)
{
	if(a1.getaddrLen() != a2.getaddrLen()) return a1.getaddrLen() > a2.getaddrLen();
	else return std::memcmp(a1.getaddr(),a2.getaddr(),a1.getaddrLen()) > 0;
}

bool operator == (const Address &a1, const Address &a2)
{
	if(a1.getaddrLen() != a2.getaddrLen()) return false;
	else return std::memcmp(a1.getaddr(),a2.getaddr(),a1.getaddrLen()) == 0;
}

bool operator != (const Address &a1, const Address &a2)
{
	return !(a1 == a2);
}

}
