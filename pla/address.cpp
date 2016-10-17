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

#include "pla/address.hpp"
#include "pla/exception.hpp"
#include "pla/string.hpp"
#include "pla/binarystring.hpp"

namespace pla
{

bool Address::Resolve(const String &host, const String &service, List<Address> &result)
{
	result.clear();
	
	addrinfo aiHints;
	std::memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_UNSPEC;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = 0;
	if(host.contains(':') || !host.containsLetters()) aiHints.ai_flags = 0;
	else aiHints.ai_flags = AI_ADDRCONFIG;

	addrinfo *aiList = NULL;
	if(getaddrinfo(host.c_str(), service.c_str(), &aiHints, &aiList) != 0)
		return false;

	addrinfo *ai = aiList;
	while(ai)
	{
		result.push_back(Address(ai->ai_addr, ai->ai_addrlen));
		ai = ai->ai_next;
	}
	
	freeaddrinfo(aiList);
	return !result.empty();
}

bool Address::Resolve(const String &host, uint16_t port, List<Address> &result)
{
	String service;
	service << port;
	return Resolve(host, service, result);
}

bool Address::Resolve(const String &str, List<Address> &result, const String &protocol)
{
	result.clear();
	
	String host, service;
	
	int separator = str.find_last_of(':');
	if(separator != String::NotFound && !String(str,separator+1).contains(']'))
	{
		host = str.substr(0,separator);
		service = str.substr(separator+1);
	}
	else {
		host = str;
		if(!protocol.empty()) service = protocol.toLower();
		else service = "http";
	}

	if(!host.empty() && host[0] == '[')
		host = host.substr(1, host.find(']')-1);
	
	if(host.empty() || service.empty()) 
		return false;
		
	return Resolve(host, service, result);
}

bool Address::Reverse(const Address &a, String &result)
{
	if(a.isNull()) return "";

        char host[HOST_NAME_MAX];
	char service[SERVICE_NAME_MAX];
        if(getnameinfo(a.addr(), a.addrLen(), host, HOST_NAME_MAX, service, SERVICE_NAME_MAX, NI_NUMERICSERV))
	{
		result = a.toString();
		return false;
	}
	
	result = host;
	if(a.addrFamily() == AF_INET6 && result.contains(':')) 
	{
		result.clear();
		result << '[' << host << ']';
	}

	result << ':' << service;
	return true;
}
	
Address::Address(void)
{
	clear();
}

Address::Address(const String &host, const String &service)
{
	set(host,service);
}

Address::Address(const String &host, uint16_t port)
{
	set(host,port);
}

Address::Address(const String &str)
{
	set(str);
}

Address::Address(const sockaddr *addr, socklen_t addrlen)
{
	set(addr,addrlen);
}

Address::~Address(void)
{

}

void Address::set(const String &host, const String &service, int family, int socktype)
{
	addrinfo aiHints;
	std::memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = family;
	aiHints.ai_socktype = socktype;
	aiHints.ai_protocol = 0;
	if(host.contains(':') || !host.containsLetters()) aiHints.ai_flags = 0;
	else aiHints.ai_flags = AI_ADDRCONFIG;

	addrinfo *aiList = NULL;
	if(getaddrinfo(host.c_str(), service.c_str(), &aiHints, &aiList) != 0)
	{
		String str;
		if(host.contains(':')) str = "[" + host + "]:" + service;
		else str = host + ":" +service;
		throw NetException("Unable to resolve address: " + str);
	}

	set(aiList->ai_addr, aiList->ai_addrlen);
	freeaddrinfo(aiList);
}

void Address::set(const String &host, uint16_t port, int family, int socktype)
{
	String service;
	service << port;
	set(host, service, family);
}

void Address::set(const String &str)
{
	fromString(str);
}

void Address::set(const sockaddr *addr, socklen_t addrlen)
{
	//std::memset(&mAddr, 0, sizeof(mAddr));
	
	if(!addr || !addrlen)
	{
		mAddrLen = 0;
		return;
	}

	mAddrLen = addrlen;
	std::memcpy(&mAddr, addr, addrlen);
}

void Address::setPort(uint16_t port)
{
	if(mAddrLen == 0)
		return;
	
	switch(addrFamily())
	{
	case AF_INET:	// IP v4
	{
		sockaddr_in *sa = reinterpret_cast<sockaddr_in*>(&mAddr);
		sa->sin_port = port;
		break;
	}

	case AF_INET6:	// IP v6
	{
		sockaddr_in6 *sa = reinterpret_cast<sockaddr_in6*>(&mAddr);
		sa->sin6_port = port;
		break;
	}

	default:
		throw Unsupported("Unable to set the port for this address family");
	}
}

void Address::clear(void)
{
	const sockaddr *null = NULL;
	set(null, 0);
}

bool Address::isNull(void) const
{
	return (mAddrLen == 0);
}

bool Address::isLocal(void) const
{
	switch(addrFamily())
	{
	case AF_INET:	// IP v4
	{
		const sockaddr_in *sa = reinterpret_cast<const sockaddr_in*>(&mAddr);
		const uint8_t *b = reinterpret_cast<const uint8_t *>(&sa->sin_addr.s_addr);
		if(b[0] == 127) return true;
		break;
	}

	case AF_INET6:	// IP v6
	{
		const sockaddr_in6 *sa6 = reinterpret_cast<const sockaddr_in6*>(&mAddr);
		const uint8_t *b = reinterpret_cast<const uint8_t *>(sa6->sin6_addr.s6_addr);
		for(int i=0; i<9; ++i) if(b[i] != 0) break;
		if(b[10] == 0xFF && b[11] == 0xFF)		// mapped
		{
			if(b[12] == 127) return true;
		}
		for(int i=10; i<15; ++i) if(b[i] != 0) break;
		if(b[15] == 1) return true;
		break;
	}
	
	}
	return false;
}

bool Address::isPrivate(void) const
{
	switch(addrFamily())
	{
	case AF_INET:	// IP v4
	{
		const sockaddr_in *sa = reinterpret_cast<const sockaddr_in*>(&mAddr);
		const uint8_t *b = reinterpret_cast<const uint8_t *>(&sa->sin_addr.s_addr);
		if(b[0] == 10) return true;
		if(b[0] == 172 && b[1] >= 16 && b[1] < 32) return true;
		if(b[0] == 192 && b[1] == 168) return true;
		if(b[0] == 169 && b[1] == 254) return true;	// link-local
		break;
	}

	case AF_INET6:	// IP v6
	{
		const sockaddr_in6 *sa6 = reinterpret_cast<const sockaddr_in6*>(&mAddr);
		const uint8_t *b = reinterpret_cast<const uint8_t *>(sa6->sin6_addr.s6_addr);
		if(b[0] == 0xFC || b[0] == 0xFD) return true; 
		for(int i=0; i<9; ++i) if(b[i] != 0) break;
		if(b[10] == 0xFF && b[11] == 0xFF)		// mapped
		{
			if(b[12] == 10) return true;
			if(b[12] == 172 && b[13] >= 16 && b[13] < 32) return true;
			if(b[12] == 192 && b[13] == 168) return true;
		}
		break;
	}
	
	}
	return false;
}

bool Address::isPublic(void) const
{
	return !isLocal() && !isPrivate();  
}

bool Address::isIpv4(void) const
{
	return (addrFamily() == AF_INET);
}

bool Address::isIpv6(void) const
{
	return (addrFamily() == AF_INET6);  
}

String Address::host(bool numeric) const
{
	if(isNull()) throw InvalidData("Requested host for null address");

	char host[HOST_NAME_MAX];
	if(getnameinfo(addr(), addrLen(), host, HOST_NAME_MAX, NULL, 0, (numeric ?  NI_NUMERICHOST : 0)))
		throw InvalidData("Invalid stored network address");
	return String(host).toLower();
}

String Address::service(bool numeric) const
{
	if(isNull()) throw InvalidData("Requested service for null address");

	char service[SERVICE_NAME_MAX];
	if(getnameinfo(addr(), addrLen(), NULL, 0, service, SERVICE_NAME_MAX, (numeric ? NI_NUMERICSERV : 0)))
		throw InvalidData("Invalid stored network address");
	return String(service).toLower();
}

uint16_t Address::port(void) const
{
	if(isNull()) throw InvalidData("Requested port for null address");

	String str(service(true));
	uint16_t port;
	str >> port;
	return port;
}

String Address::reverse(void) const
{
	if(isNull()) return "";

        char host[HOST_NAME_MAX];
	char service[SERVICE_NAME_MAX];
        if(getnameinfo(addr(), addrLen(), host, HOST_NAME_MAX, service, SERVICE_NAME_MAX, NI_NUMERICSERV))
		return toString();
        
	String str(host);
	if(addrFamily() == AF_INET6 && str.contains(':')) 
	{
		str.clear();
		str << '[' << host << ']';
	}

	str << ':' << service;
	return str;
}

Address Address::unmap(void) const
{
	if(addrFamily() == AF_INET6)
	{
		const sockaddr_in6 *sa6 = reinterpret_cast<const sockaddr_in6*>(&mAddr);
		const uint8_t *b = reinterpret_cast<const uint8_t *>(sa6->sin6_addr.s6_addr);
		
		for(int i=0; i<9; ++i) 
			if(b[i] != 0) 
				return *this;
		
		if(b[10] != 0xFF || b[11] != 0xFF)
			return *this;
		
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = sa6->sin6_port;
		std::memcpy(reinterpret_cast<uint8_t*>(&sin.sin_addr.s_addr), b + 12, 4);
		
		return Address(reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
	}
	
	return *this;
}

void Address::serialize(Serializer &s) const
{
	switch(addrFamily())
	{
	case AF_INET:	// IP v4
	{
		s << uint8_t(4);
		const sockaddr_in *sa = reinterpret_cast<const sockaddr_in*>(&mAddr);
		const uint8_t *b = reinterpret_cast<const uint8_t *>(&sa->sin_addr.s_addr);

		for(int i=0; i<4; ++i) s << b[i];
		s << uint16_t(ntohs(sa->sin_port));
		break;
	}

	case AF_INET6:	// IP v6
	{
		s << uint8_t(16);
		const sockaddr_in6 *sa6 = reinterpret_cast<const sockaddr_in6*>(&mAddr);
		
		for(int i=0; i<16; ++i) s << uint8_t(sa6->sin6_addr.s6_addr[i]);
		s << uint16_t(ntohs(sa6->sin6_port));
		break;
	}

	default:
		throw InvalidData("Stored network address cannot be serialized");
	}
}

bool Address::deserialize(Serializer &s)
{
	uint8_t size = 0;
	if(!(s >> size)) return false;

	if(size == 0)
	{
		mAddrLen = 0;
		return true;
	}

	switch(size)
	{
	case 4:		// IP v4
	{
		mAddrLen = sizeof(sockaddr_in);
		sockaddr_in *sa = reinterpret_cast<sockaddr_in*>(&mAddr);
		uint8_t *b = reinterpret_cast<uint8_t *>(&sa->sin_addr.s_addr);

		sa->sin_family = AF_INET;
		uint8_t u;
		for(int i=0; i<4; ++i)
		{
			AssertIO(s >> u); 
			b[i] = u;
		}
		uint16_t port;
		AssertIO(s >> port);
		sa->sin_port = htons(port);
		break;
	}

	case 16:	// IP v6
	{
		mAddrLen = sizeof(sockaddr_in6);
		sockaddr_in6 *sa6 = reinterpret_cast<sockaddr_in6*>(&mAddr);

		sa6->sin6_family = AF_INET6;
		uint8_t u;
		for(int i=0; i<16; ++i)
		{
			AssertIO(s >> u);
			sa6->sin6_addr.s6_addr[i] = u;
		}
		uint16_t port;
		AssertIO(s >> port);
		sa6->sin6_port = htons(port);
		break;
	}

	default:
		throw InvalidData("Invalid network address");
	}
	
	return true;
}

void Address::serialize(Stream &s) const
{
	Address a(unmap());
	
	char host[HOST_NAME_MAX];
	char service[SERVICE_NAME_MAX];
	if(getnameinfo(a.addr(), a.addrLen(), host, HOST_NAME_MAX, service, SERVICE_NAME_MAX, NI_NUMERICHOST|NI_NUMERICSERV))
		throw InvalidData("Invalid stored network address");
	
	if(a.addrFamily() == AF_INET6) s << '[' << host << ']' << ':' << service;
	else s << host << ':' << service;
}

bool Address::deserialize(Stream &s)
{
	String str;
	if(!s.read(str)) return false;
  	str.trim();
	if(str.empty()) throw InvalidData("Invalid network address");
	
	String host, service;

	int separator = str.find_last_of(':');
	if(separator != String::NotFound && !String(str,separator+1).contains(']'))
	{
		host = str.substr(0,separator);
		service = str.substr(separator+1);
	}
	else {
		host = str;
		service = "80";
	}

	if(!host.empty() && host[0] == '[')
		host = host.substr(1, host.find(']')-1);
	
	if(host.empty() || service.empty())
		throw InvalidData("Invalid network address: " + str);

	set(host, service, AF_UNSPEC);
	return true;
}

bool Address::isNativeSerializable(void) const
{
	return false;
}

bool Address::isInlineSerializable(void) const
{
	return true;
}

const sockaddr *Address::addr(void) const
{
	return reinterpret_cast<const sockaddr*>(&mAddr);
}

int Address::addrFamily(void) const
{
	return reinterpret_cast<const sockaddr_in*>(&mAddr)->sin_family;
}

socklen_t Address::addrLen(void) const
{
	return mAddrLen;
}

bool operator < (const Address &a1, const Address &a2)
{
	if(a1.addrLen() != a2.addrLen()) return a1.addrLen() < a2.addrLen();
	if(a1.addrFamily() != a2.addrFamily()) return a1.addrFamily() < a2.addrFamily();
	
	if(!a2.isLocal())
	{
		if(a1.isLocal()) return true;
		if(a1.isPrivate() && !a2.isPrivate()) return true;
	}
	if(!a1.isLocal())
	{
		if(a2.isLocal()) return false;
		if(a2.isPrivate() && !a1.isPrivate()) return false;
	}
	
	switch(a1.addrFamily())
	{
	case AF_INET:	// IP v4
	{
		const sockaddr_in *sa1 = reinterpret_cast<const sockaddr_in*>(a1.addr());
		const sockaddr_in *sa2 = reinterpret_cast<const sockaddr_in*>(a2.addr());
		return (sa1->sin_addr.s_addr < sa2->sin_addr.s_addr || (sa1->sin_addr.s_addr == sa2->sin_addr.s_addr && sa1->sin_port < sa2->sin_port && sa1->sin_port && sa2->sin_port));
	}

	case AF_INET6:	// IP v6
	{
		const sockaddr_in6 *sa1 = reinterpret_cast<const sockaddr_in6*>(a1.addr());
		const sockaddr_in6 *sa2 = reinterpret_cast<const sockaddr_in6*>(a2.addr());
		int cmp = std::memcmp(sa1->sin6_addr.s6_addr, sa2->sin6_addr.s6_addr, 16);
		return (cmp < 0 || (cmp == 0 && sa1->sin6_port < sa2->sin6_port && sa1->sin6_port && sa2->sin6_port));
	}

	default:
		return false;
	}
}

bool operator > (const Address &a1, const Address &a2)
{
	if(a1.addrLen() != a2.addrLen()) return a1.addrLen() > a2.addrLen();
	if(a1.addrFamily() != a2.addrFamily()) return a1.addrFamily() > a2.addrFamily();
	
	if(!a1.isLocal())
	{
		if(a2.isLocal()) return true;
		if(a2.isPrivate() && !a1.isPrivate()) return true;
	}
	if(!a2.isLocal())
	{
		if(a1.isLocal()) return false;
		if(a1.isPrivate() && !a2.isPrivate()) return false;
	}
	
	switch(a1.addrFamily())
	{
	case AF_INET:	// IP v4
	{
		const sockaddr_in *sa1 = reinterpret_cast<const sockaddr_in*>(a1.addr());
		const sockaddr_in *sa2 = reinterpret_cast<const sockaddr_in*>(a2.addr());
		return (sa1->sin_addr.s_addr > sa2->sin_addr.s_addr || (sa1->sin_addr.s_addr == sa2->sin_addr.s_addr && sa1->sin_port > sa2->sin_port && sa1->sin_port && sa2->sin_port));
	}

	case AF_INET6:	// IP v6
	{
		const sockaddr_in6 *sa1 = reinterpret_cast<const sockaddr_in6*>(a1.addr());
		const sockaddr_in6 *sa2 = reinterpret_cast<const sockaddr_in6*>(a2.addr());
		int cmp = std::memcmp(sa1->sin6_addr.s6_addr, sa2->sin6_addr.s6_addr, 16);
		return (cmp > 0 || (cmp == 0 && sa1->sin6_port > sa2->sin6_port && sa1->sin6_port && sa2->sin6_port));
	}

	default:
		return false;
	}
}

bool operator == (const Address &a1, const Address &a2)
{
	if(a1.addrLen() != a2.addrLen()) return false;
	if(a1.addrFamily() != a2.addrFamily()) return false;
	  
	switch(a1.addrFamily())
	{
	case AF_INET:	// IP v4
	{
		const sockaddr_in *sa1 = reinterpret_cast<const sockaddr_in*>(a1.addr());
		const sockaddr_in *sa2 = reinterpret_cast<const sockaddr_in*>(a2.addr());
		return (sa1->sin_addr.s_addr == sa2->sin_addr.s_addr && (sa1->sin_port == sa2->sin_port || !sa1->sin_port || !sa2->sin_port));
	}

	case AF_INET6:	// IP v6
	{
		const sockaddr_in6 *sa1 = reinterpret_cast<const sockaddr_in6*>(a1.addr());
		const sockaddr_in6 *sa2 = reinterpret_cast<const sockaddr_in6*>(a2.addr());
		return (std::memcmp(sa1->sin6_addr.s6_addr, sa2->sin6_addr.s6_addr, 16) == 0 && (sa1->sin6_port == sa2->sin6_port || !sa1->sin6_port || !sa2->sin6_port));
	}

	default:
		return false;
	}
}

bool operator != (const Address &a1, const Address &a2)
{
	return !(a1 == a2);
}

}
