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

#ifndef ARC_INCLUDE_H
#define ARC_INCLUDE_H

#define APPNAME		"Arcanet"
#define APPVERSION	"0.1"

#if defined(_WIN32) || defined(_WIN64)
	#define WINDOWS
#endif

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>

#include <algorithm>
#include <limits>
#include <iostream>
#include <iterator>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <queue>

#include <pthread.h>

// Winsock compatibility
#ifdef WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>
#undef min
#undef max

typedef SOCKET socket_t;
typedef SOCKaddR sockaddr;
typedef int socklen_t;
typedef u_long ctl_t;
#define ioctl ioctlsocket
#define close closesocket
#define sockerrno WSAGetLasterror()
#define EWOULDBLOCK	WSAEWOULDBLOCK
#define EaddRINUSE	WSAEaddRINUSE
#define SOCK_TO_INT(x) 0

#else

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef int socket_t;
typedef int ctl_t;
#define sockerrno errno
#define INVALID_SOCKET -1
#define SOCK_TO_INT(x) (x)

#endif

#ifndef NULL
#define NULL (void*)(0L)
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 1024
#endif

#ifndef SERVICE_NAME_MAX
#define SERVICE_NAME_MAX 32
#endif

namespace arc
{

typedef signed char		sint8_t;	// 8 bits
typedef signed short		sint16_t;	// 16 bits
typedef signed int		sint32_t;	// 32 bits
typedef signed long long	sint64_t;	// 64 bits

typedef unsigned char		uint8_t;	// 8 bits
typedef unsigned short		uint16_t;	// 16 bits
typedef unsigned int		uint32_t;	// 32 bits
typedef unsigned long long	uint64_t;	// 64 bits

typedef float			float32_t;	// 32 bits float
typedef double			float64_t;	// 64 bits float

#define CONST64(n) n ## ULL	// 64 bits unsigned constant

template<typename T> T bounds(T val, T val_min, T val_max)
{
    if(val_min > val_max) std::swap(val_min,val_max);
    return std::min(val_max,std::max(val_min,val));
}

template<typename T> T sqr(T x)
{
	return x*x;
}

template<typename T> void msleep(T msecs)
{
#ifdef WINDOWS
	Sleep(unsigned(msecs));
#else
	usleep(msecs*1000);
#endif
}

template<typename T> void Log(const char *prefix, const T &value)
{
#ifdef DEBUG
	std::cout<<__FILE__<<" "<<std::dec<<__LINE__<<" "<<prefix<<": "<<str<<std::endl;
#else
	std::cout<<prefix<<": "<<value<<std::endl;
#endif
}

#define VAR(x) std::cout<<""#x"="<<x<<std::endl;

#define Set std::set
#define List std::list
#define Queue std::queue
#define Stack std::stack
#define Dequeue std::dequeue

const size_t BufferSize = 4096;

}

#endif
