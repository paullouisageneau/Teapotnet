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

#include "pla/datagramsocket.hpp"
#include "pla/exception.hpp"
#include "pla/string.hpp"
#include "pla/time.hpp"

namespace pla
{

const size_t DatagramSocket::MaxDatagramSize = 1500;

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

void DatagramSocket::getLocalAddresses(Set<Address> &set) const
{
	set.clear();

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
		LogDebug("DatagramSocket", "Local hostname is not resolvable");
		if(getaddrinfo("localhost", service.c_str(), &aiHints, &aiList) != 0)
		{
			set.insert(bindAddr);
			return;
		}
	}

	addrinfo *ai = aiList;
	while(ai)
	{
		if(ai->ai_family == AF_INET || ai->ai_family == AF_INET6)
		{
			Address addr(ai->ai_addr,ai->ai_addrlen);
			String host = addr.host();

			if(ai->ai_addr->sa_family != AF_INET6 || host.substr(0,4) != "fe80")
			{
				if(addr == bindAddr)
				{
					set.clear();
					set.insert(addr);
					break;
				}

				set.insert(addr);
			}
		}

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
		if(!sa) break;

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
			if(sa->sa_family != AF_INET6 || host.substr(0,4) != "fe80")
			{
				addr.set(host, mPort);
				if(addr == bindAddr)
				{
					set.clear();
					set.insert(addr);
					break;
				}
				set.insert(addr);
			}
		}

		ifa = ifa->ifa_next;
	}

	freeifaddrs(ifas);
#endif
}

void DatagramSocket::getHardwareAddresses(Set<BinaryString> &set) const
{
	set.clear();

#ifdef WINDOWS
	IP_ADAPTER_ADDRESSES adapterInfo[16];
	DWORD dwBufLen = sizeof(IP_ADAPTER_ADDRESSES)*16;

	DWORD dwStatus = GetAdaptersAddresses(getBindAddress().addrFamily(), 0, NULL, adapterInfo, &dwBufLen);
	if(dwStatus != ERROR_SUCCESS)
		throw NetException("Unable to retrive hardware addresses");

	IP_ADAPTER_ADDRESSES *pAdapterInfo = adapterInfo;
	while(pAdapterInfo)
	{
		if(pAdapterInfo->PhysicalAddressLength)
			set.insert(BinaryString((char*)pAdapterInfo->PhysicalAddress, pAdapterInfo->PhysicalAddressLength));

		pAdapterInfo = pAdapterInfo->Next;
	}
#else
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[BufferSize];

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(mSock, SIOCGIFCONF, &ifc) == -1)
		throw NetException("Unable to retrive hardware addresses");

	for(	struct ifreq* it = ifc.ifc_req;
		it != ifc.ifc_req + ifc.ifc_len/sizeof(struct ifreq);
		++it)
	{
		strcpy(ifr.ifr_name, it->ifr_name);

		if (ioctl(mSock, SIOCGIFFLAGS, &ifr) == 0)
		{
			if(!(ifr.ifr_flags & IFF_LOOPBACK))
			{
				if (ioctl(mSock, SIOCGIFHWADDR, &ifr) == 0)
				{
					// Note: hwaddr.sa_data is big endian
					set.insert(BinaryString(reinterpret_cast<char*>(ifr.ifr_hwaddr.sa_data), size_t(IFHWADDRLEN)));
				}
			}
		}
	}
#endif
}

void DatagramSocket::bind(int port, bool broadcast, int family)
{
	close();
	mPort = port;

	// Obtain local Address
	addrinfo *aiList = NULL;
	addrinfo aiHints;
	std::memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = family;
	aiHints.ai_socktype = SOCK_DGRAM;
	aiHints.ai_protocol = 0;
	aiHints.ai_flags = AI_PASSIVE;
	String service;
	service << port;
	if(getaddrinfo(NULL, service.c_str(), &aiHints, &aiList) != 0)
		throw NetException("Local binding address resolution failed for UDP port "+service);

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

		// Necessary for DTLS
#ifdef LINUX
		int val = IP_PMTUDISC_DO;
		setsockopt(mSock, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
#else
		setsockopt(mSock, IPPROTO_IP, IP_DONTFRAG, reinterpret_cast<char*>(&enabled), sizeof(enabled));
#endif

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
	std::unique_lock<std::mutex> lock(mStreamsMutex);

	for(auto it = mStreams.begin(); it != mStreams.end(); ++it)
		for(auto jt = it->second.begin(); jt != it->second.end(); ++jt)
		{
			DatagramStream *stream = *jt;

			std::unique_lock<std::mutex> lock(stream->mMutex);
			stream->mSock = NULL;
			lock.unlock();
			stream->mCondition.notify_all();
		}

	mStreams.clear();

	if(mSock != INVALID_SOCKET)
	{
		::closesocket(mSock);
		mSock = INVALID_SOCKET;
		mPort = 0;
	}
}

int DatagramSocket::read(char *buffer, size_t size, Address &sender, duration timeout)
{
	return recv(buffer, size, sender, timeout, 0);
}

int DatagramSocket::peek(char *buffer, size_t size, Address &sender, duration timeout)
{
	return recv(buffer, size, sender, timeout, MSG_PEEK);
}

void DatagramSocket::write(const char *buffer, size_t size, const Address &receiver)
{
	send(buffer, size, receiver, 0);
}

bool DatagramSocket::read(Stream &stream, Address &sender, duration timeout)
{
	stream.clear();
	char buffer[MaxDatagramSize];
	int size = MaxDatagramSize;
	size = read(buffer, size, sender, timeout);
	if(size < 0) return false;
	stream.writeData(buffer,size);
	return true;
}

bool DatagramSocket::peek(Stream &stream, Address &sender, duration timeout)
{
	stream.clear();
	char buffer[MaxDatagramSize];
	int size = MaxDatagramSize;
	size = peek(buffer, size, sender, timeout);
	if(size < 0) return false;
	stream.writeData(buffer,size);
	return true;
}

void DatagramSocket::write(Stream &stream, const Address &receiver)
{
	char buffer[MaxDatagramSize];
	size_t size = stream.readData(buffer,MaxDatagramSize);
	write(buffer, size, receiver);
	stream.clear();
}

bool DatagramSocket::wait(duration timeout)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSock, &readfds);

	struct timeval tv;
	durationToStruct(timeout, tv);
	int ret = ::select(SOCK_TO_INT(mSock)+1, &readfds, NULL, NULL, &tv);
	if (ret < 0) throw Exception("Unable to wait on socket");
	return (ret > 0);
}

int DatagramSocket::recv(char *buffer, size_t size, Address &sender, duration timeout, int flags)
{
	using clock = std::chrono::steady_clock;
	std::chrono::time_point<clock> end;
	if(timeout >= duration::zero()) end = clock::now() + std::chrono::duration_cast<clock::duration>(timeout);
	else end = std::chrono::time_point<clock>::max();

	do {
		duration left = end - std::chrono::steady_clock::now();
		if(!wait(left)) break;

		char datagramBuffer[MaxDatagramSize];
		sockaddr_storage sa;
		socklen_t sl = sizeof(sa);
		int result = ::recvfrom(mSock, datagramBuffer, MaxDatagramSize, flags | MSG_PEEK, reinterpret_cast<sockaddr*>(&sa), &sl);
		if(result < 0) throw NetException("Unable to read from socket (error " + String::number(sockerrno) + ")");
		sender.set(reinterpret_cast<sockaddr*>(&sa),sl);

		std::unique_lock<std::mutex> lock(mStreamsMutex);
		auto it = mStreams.find(sender.unmap());
		if(it == mStreams.end())
		{
			size = std::min(result, int(size));
			std::memcpy(buffer, datagramBuffer, size);

			if(!(flags & MSG_PEEK))
				::recvfrom(mSock, datagramBuffer, MaxDatagramSize, flags, reinterpret_cast<sockaddr*>(&sa), &sl);
			return size;
		}

		BinaryString tmp(datagramBuffer, size_t(result));
		for(auto jt = it->second.begin(); jt != it->second.end(); ++jt)
		{
			DatagramStream *stream = *jt;
			Assert(stream);
			std::unique_lock<std::mutex> lock(stream->mMutex);

			if(stream->mIncoming.size() < DatagramStream::MaxQueueSize)
				stream->mIncoming.push(tmp);

			lock.unlock();
			stream->mCondition.notify_all();
		}

		::recvfrom(mSock, datagramBuffer, MaxDatagramSize, flags & ~MSG_PEEK, reinterpret_cast<sockaddr*>(&sa), &sl);
	}
	while(std::chrono::steady_clock::now() <= end);

	return -1;
}

void DatagramSocket::send(const char *buffer, size_t size, const Address &receiver, int flags)
{
	int result = ::sendto(mSock, buffer, size, flags, receiver.addr(), receiver.addrLen());
	if(result < 0) throw NetException("Unable to write to socket (error " + String::number(sockerrno) + ")");
}

void DatagramSocket::accept(DatagramStream &stream)
{
	BinaryString buffer;
	Address sender;
	read(buffer, sender);

	unregisterStream(&stream);

	stream.mSock = this;
	stream.mAddr = sender;
	std::swap(stream.mBuffer, buffer);
}

void DatagramSocket::registerStream(DatagramStream *stream)
{
	Assert(stream);
	Address addr(stream->mAddr.unmap());

	std::unique_lock<std::mutex> lock(mStreamsMutex);
	mStreams[addr].insert(stream);
}

void DatagramSocket::unregisterStream(DatagramStream *stream)
{
	Assert(stream);
	Address addr(stream->mAddr.unmap());

	std::unique_lock<std::mutex> lock(mStreamsMutex);
	mStreams[addr].erase(stream);
	if(mStreams[addr].empty())
		mStreams.erase(addr);
}

duration DatagramStream::DefaultTimeout = seconds(60.); // 1 min
int DatagramStream::MaxQueueSize = 100;

DatagramStream::DatagramStream(void) :
	mSock(NULL),
	mOffset(0),
	mTimeout(DefaultTimeout)
{

}

DatagramStream::DatagramStream(DatagramSocket *sock, const Address &addr) :
	mSock(sock),
	mAddr(addr),
	mOffset(0),
	mTimeout(DefaultTimeout)
{
	Assert(mSock);
	mSock->registerStream(this);
}

DatagramStream::~DatagramStream(void)
{
	close();
}

Address DatagramStream::getLocalAddress(void) const
{
	// Warning: this is actually different from local address
	if(mSock) return mSock->getBindAddress();
	else return Address();
}

Address DatagramStream::getRemoteAddress(void) const
{
	return mAddr;
}

void DatagramStream::setTimeout(duration timeout)
{
	mTimeout = timeout;
}

size_t DatagramStream::readData(char *buffer, size_t size)
{
	if(!waitData(mTimeout)) throw Timeout();

	std::unique_lock<std::mutex> lock(mMutex);

	if(mIncoming.empty()) return 0;
	Assert(mOffset <= mIncoming.front().size());
	size = std::min(size, size_t(mIncoming.front().size() - mOffset));
	std::memcpy(buffer, mIncoming.front().data() + mOffset, size);
	mOffset+= size;
	return size;
}

void DatagramStream::writeData(const char *data, size_t size)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(!mSock) throw NetException("Datagram stream closed");
	mBuffer.writeData(data, size);
}

bool DatagramStream::waitData(duration timeout)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(mSock && mIncoming.empty())
	{
		this->mCondition.wait_for(lock, timeout, [this]() {
			return (!mSock || !this->mIncoming.empty());
		});
	}

	return (!mSock || !mIncoming.empty());
}

bool DatagramStream::nextRead(void)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(mIncoming.empty()) return false;
	mIncoming.pop();
	mOffset = 0;
	return true;
}

bool DatagramStream::nextWrite(void)
{
	mSock->write(mBuffer.data(), mBuffer.size(), mAddr);
	mBuffer.clear();
	return true;
}

void DatagramStream::close(void)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(mSock)
	{
		while(!mIncoming.empty()) mIncoming.pop();
		mSock->unregisterStream(this);
		mSock = NULL;
	}

	mCondition.notify_all();
}

bool DatagramStream::isDatagram(void) const
{
	return true;
}

}
