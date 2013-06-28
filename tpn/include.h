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

#ifndef TPN_INCLUDE_H
#define TPN_INCLUDE_H

#define DEBUG
#define APPNAME		"TeapotNet"
#define APPVERSION	"0.4.9"
#define APPAUTHOR	"Paul-Louis Ageneau"
#define APPLINK		"http://www.teapotnet.org/"
#define SOURCELINK	"http://www.teapotnet.org/source/"
#define DOWNLOADURL	"http://www.teapotnet.org/download/"


#if defined(_WIN32) || defined(_WIN64)
	#define WINDOWS
	#define _WIN32_WINNT 0x0501
	#define __MSVCRT_VERSION__ 0x0601
	#ifdef __MINGW32__
		#define MINGW
	#endif
	#define NO_IFADDRS
#endif

#ifdef __ANDROID__
	#ifndef ANDROID
		#define ANDROID
	#endif
	#define NO_IFADDRS
	
	#include <jni.h>
	#include <android/log.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cmath>
#include <cctype>

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

#include <dirent.h>
#include <stdlib.h>

#ifdef PTW32_STATIC_LIB
#include "win32/pthread.h"
#else
#include <pthread.h>
#endif

// Windows compatibility
#ifdef WINDOWS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#undef min
#undef max

typedef SOCKET socket_t;
typedef SOCKADDR sockaddr;
typedef int socklen_t;
typedef u_long ctl_t;
#define ioctl ioctlsocket
#define sockerrno WSAGetLastError()
#define SEWOULDBLOCK	WSAEWOULDBLOCK
#define SEAGAIN		WSAEWOULDBLOCK
#define SEADDRINUSE	WSAEADDRINUSE
#define SOCK_TO_INT(x) 0

#define mkdir(d,x) _mkdir(d)

#ifdef MINGW
#include <sys/stat.h>
#include <sys/time.h>
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27 
#endif
#endif

#else

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
//#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sched.h>

#ifndef NO_IFADDRS
#include <ifaddrs.h>
#endif

typedef int socket_t;
typedef int ctl_t;
#define closesocket close
#define sockerrno errno
#define SEWOULDBLOCK	EWOULDBLOCK
#define SEAGAIN		EAGAIN
#define SEADDRINUSE	EADDRINUSE
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

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

namespace tpn
{

#ifdef WINDOWS
typedef signed char		int8_t;		// 8 bits
typedef signed short		int16_t;	// 16 bits
typedef signed int		int32_t;	// 32 bits
typedef signed long long	int64_t;	// 64 bits
typedef unsigned char		uint8_t;	// 8 bits
typedef unsigned short		uint16_t;	// 16 bits
typedef unsigned int		uint32_t;	// 32 bits
typedef unsigned long long	uint64_t;	// 64 bits

typedef struct __stat64 stat_t;
inline int stat(const char *path, stat_t *buf) { return ::_stat64(path,buf); }
inline int _stat(const char *path, stat_t *buf) { return tpn::stat(path,buf); }

#else
typedef ::int8_t		int8_t;		// 8 bits
typedef ::int16_t		int16_t;	// 16 bits
typedef ::int32_t		int32_t;	// 32 bits
typedef ::int64_t		int64_t;	// 64 bits
typedef ::uint8_t		uint8_t;	// 8 bits
typedef ::uint16_t		uint16_t;	// 16 bits
typedef ::uint32_t		uint32_t;	// 32 bits
typedef ::uint64_t		uint64_t;	// 64 bits

typedef struct stat64 stat_t;
inline int stat(const char *path, stat_t *buf) { return ::stat64(path,buf); }

#endif

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

inline void msleep(unsigned msecs)
{
#ifdef WINDOWS
	Sleep(msecs);
#else
	struct timespec ts;
	ts.tv_sec = msecs/1000;
	ts.tv_nsec = (msecs%1000)*1000000;
	nanosleep(&ts, NULL);
#endif
}

inline void yield(void)
{
#ifdef WINDOWS
	Sleep(0);
#else
	sched_yield();
#endif
}

#define VAR(x) std::cout<<""#x"="<<x<<std::endl;

#define List std::list
#define Queue std::queue
#define Stack std::stack
#define Deque std::deque

const size_t BufferSize = 4*1024;	// 4 KiB

}

#ifndef TPN_MUTEX_H
#include "tpn/mutex.h"

namespace tpn
{

extern Mutex LogMutex;
extern int LogLevel;

inline unsigned threadId(pthread_t thread)
{
	static unsigned next = 0;
	static std::map<char*, unsigned> ids;
	char *p = 0;
	std::memcpy(&p, &thread, std::min(sizeof(thread),sizeof(p)));
	if(ids.find(p) == ids.end()) ids[p] = next++;
	return ids[p];
}

#define LEVEL_TRACE	0
#define LEVEL_DEBUG	1
#define LEVEL_INFO	2
#define LEVEL_WARN	3
#define LEVEL_ERROR	4

#define LogTrace(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_TRACE, prefix, value)
#define LogDebug(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_DEBUG, prefix, value)
#define LogInfo(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_INFO, prefix, value)
#define LogWarn(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_WARN, prefix, value)
#define LogError(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_ERROR, prefix, value)

#define Log(prefix, value)		LogInfo(prefix, value)

template<typename T> void LogImpl(const char *file, int line, int level, const char *prefix, const T &value)
{
	if(level < LogLevel) return;
  
	const char *strLevel;
	switch(level)
	{
	  case LEVEL_TRACE:	strLevel = "Trace:";	break;
	  case LEVEL_DEBUG:	strLevel = "Debug:";	break;
	  case LEVEL_INFO:	strLevel = "Info:";	break;
	  case LEVEL_WARN:	strLevel = "WARNING:";	break;
	  default:		strLevel = "ERROR:";	break;
	}
	
	std::ostringstream oss;
	oss.fill(' ');
#ifdef DEBUG
	oss<<file<<':'<<std::dec<<line;
	std::string tmp = oss.str();
	oss.str("");
	oss<<tmp;
	if(tmp.size() < 25) oss<<std::string(25-tmp.size(), ' ');
	oss<<' '<<std::setw(4)<<threadId(pthread_self())<<' ';
#endif
	oss<<std::setw(25)<<prefix<<' '<<std::setw(8)<<strLevel<<' '<<value;

	LogMutex.lock();
	
	try {
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_VERBOSE, "teapotnet", oss.str().c_str());
#else
	std::cout<<oss.str()<<std::endl;
#endif
	}
	catch(...)
	{
		LogMutex.unlock();
		throw;
	}
	
	LogMutex.unlock();
}

}

#endif

#endif
