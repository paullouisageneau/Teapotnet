/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Platform.                                      *
 *                                                                       *
 *   Platform is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Platform is distributed in the hope that it will be useful, but     *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Platform.                                        *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef PLA_INCLUDE_H
#define PLA_INCLUDE_H

#if defined(_WIN32) || defined(_WIN64)
	#define WINDOWS
	#ifdef __MINGW32__
		#define MINGW
	#endif
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0x0501
	#endif
	#ifndef __MSVCRT_VERSION__
		#define __MSVCRT_VERSION__ 0x0601
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

#ifdef __APPLE__
#define MACOSX
#endif

#ifdef __linux__
#define LINUX
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <ctime>

#include <algorithm>
#include <limits>
#include <iostream>
#include <iterator>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <future>

#include <dirent.h>
#include <stdlib.h>

// Windows compatibility
#ifdef WINDOWS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <wincrypt.h>
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
#define IP_DONTFRAG	IP_DONTFRAGMENT
#define SOCK_TO_INT(x) 0

#define mkdirmod(d,m) mkdir(d)

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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <net/if.h>
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
#define mkdirmod(d,m) mkdir(d,m)

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

#ifndef LINUX
#define MSG_NOSIGNAL 0
#endif

namespace pla
{

#ifdef WINDOWS
typedef signed char					int8_t;		// 8 bits
typedef signed short				int16_t;	// 16 bits
typedef signed int					int32_t;	// 32 bits
typedef signed long long		int64_t;	// 64 bits
typedef unsigned char				uint8_t;	// 8 bits
typedef unsigned short			uint16_t;	// 16 bits
typedef unsigned int				uint32_t;	// 32 bits
typedef unsigned long long	uint64_t;	// 64 bits

typedef struct __stat64 stat_t;
inline int stat(const char *path, stat_t *buf) { return ::_stat64(path,buf); }
inline int _stat(const char *path, stat_t *buf) { return pla::stat(path,buf); }

#else
typedef ::int8_t		int8_t;		// 8 bits
typedef ::int16_t		int16_t;	// 16 bits
typedef ::int32_t		int32_t;	// 32 bits
typedef ::int64_t		int64_t;	// 64 bits
typedef ::uint8_t		uint8_t;	// 8 bits
typedef ::uint16_t	uint16_t;	// 16 bits
typedef ::uint32_t	uint32_t;	// 32 bits
typedef ::uint64_t	uint64_t;	// 64 bits

#ifdef MACOSX
typedef struct stat stat_t;
inline int stat(const char *path, stat_t *buf) { return ::stat(path,buf); }
#else
typedef struct stat64 stat_t;
inline int stat(const char *path, stat_t *buf) { return ::stat64(path,buf); }
#endif

#endif

typedef unsigned char	byte;
typedef float		float32_t;	// 32 bits float
typedef double	float64_t;	// 64 bits float

#define CONST64(n) n ## ULL	// 64 bits unsigned constant

// Aliases
template<typename T> using sptr = std::shared_ptr<T>;
template<typename T> using wptr = std::weak_ptr<T>;

// Typedefs
typedef std::chrono::duration<double> duration;
typedef std::chrono::duration<double> seconds;
typedef std::chrono::duration<double, std::milli> milliseconds;
typedef std::chrono::duration<double, std::micro> microseconds;
typedef std::chrono::duration<double, std::nano>  nanoseconds;

// Utility functions
template<typename T> T bounds(T val, T val_min, T val_max)
{
	if(val_min > val_max) std::swap(val_min,val_max);
	return std::min(val_max,std::max(val_min,val));
}

template<typename T> T sqr(T x)
{
	return x*x;
}

template<typename T> unsigned int bitcount(T n)
{
	static T m1  = (~T(0)) / 3;
	static T m2  = (~T(0)) / 5;
	static T m4  = (~T(0)) / 17;
	static T h01 = (~T(0)) / 255;

	n-= (n >> 1) & m1;			// Put count of each 2 bits into those 2 bits
	n = (n & m2) + ((n >> 2) & m2);		// Put count of each 4 bits into those 4 bits
	n = (n + (n >> 4)) & m4;		// Put count of each 8 bits into those 8 bits

	return (n * h01) >> (sizeof(T)*8 - 8);  // Returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
}

inline void memxor(char *a, const char *b, size_t size)
{
	unsigned long *la = reinterpret_cast<unsigned long*>(a);
	const unsigned long *lb = reinterpret_cast<const unsigned long*>(b);
	const size_t n = size / sizeof(unsigned long);
	for(size_t i = 0; i < n; ++i)
		la[i]^= lb[i];
	for(size_t i = n*sizeof(unsigned long); i < size; ++i)
		a[i]^= b[i];
}

inline duration structToDuration(const struct timeval &tv)
{
	return seconds(double(tv.tv_sec) + double(tv.tv_usec)/1000000.);
}

inline duration structToDuration(const struct timespec &ts)
{
	return seconds(double(ts.tv_sec) + double(ts.tv_nsec)/1000000000.);
}

inline void durationToStruct(duration d, struct timeval &tv)
{
	double secs = seconds(d).count();
	double isecs = 0.;
	double fsecs = std::modf(secs, &isecs);
	tv.tv_sec = time_t(isecs);
	tv.tv_usec = long(fsecs*1000000.);
}

inline void durationToStruct(duration d, struct timespec &ts)
{
	double secs = seconds(d).count();
	double isecs = 0.;
	double fsecs = std::modf(secs, &isecs);
	ts.tv_sec = time_t(isecs);
	ts.tv_nsec = long(fsecs*100000000.);
}

#define Queue std::queue
#define Stack std::stack
#define Deque std::deque

const size_t BufferSize = 4*1024;	// 4 KiB

extern std::mutex LogMutex;
extern bool ForceLogToFile;
extern int LogLevel;
extern std::map<std::thread::id, unsigned> ThreadsMap;

std::string GetFormattedLogTime(void);

#define LEVEL_TRACE	0
#define LEVEL_DEBUG	1
#define LEVEL_INFO	2
#define LEVEL_WARN	3
#define LEVEL_ERROR	4

template<typename T> void LogImpl(const char *file, int line, int level, const char *prefix, const T &value)
{
	if(level < pla::LogLevel) return;

	std::lock_guard<std::mutex> lock(LogMutex);

	unsigned mythreadid = 0;
	auto it = ThreadsMap.find(std::this_thread::get_id());
	if(it != ThreadsMap.end()) mythreadid = it->second;
	else {
		mythreadid = unsigned(ThreadsMap.size()) + 1;
		ThreadsMap[std::this_thread::get_id()] = mythreadid;
	}

	const char *strLevel;
	switch(level)
	{
		case LEVEL_TRACE:	strLevel = "Trace:";	break;
		case LEVEL_DEBUG:	strLevel = "Debug:";	break;
		case LEVEL_INFO:	strLevel = "Info:";		break;
		case LEVEL_WARN:	strLevel = "WARNING:";	break;
		default:					strLevel = "ERROR:";	break;
	}

	std::ostringstream oss;
	oss.fill(' ');
	oss<<pla::GetFormattedLogTime()<<' ';
#ifdef DEBUG
	std::ostringstream tmp;
	tmp<<file<<':'<<std::dec<<line;
	oss<<tmp.str();
	if(tmp.str().size() < 28) oss<<std::string(28-tmp.str().size(), ' ');
	tmp.str("");
	tmp<<mythreadid<<'@'<<prefix;
	oss<<' '<<std::setw(36)<<tmp.str()<<' ';
#endif
	oss<<std::setw(8)<<strLevel<<' '<<value;

#ifdef ANDROID
	__android_log_print(ANDROID_LOG_VERBOSE, "teapotnet", "%s", oss.str().c_str());
#else
	if(!pla::ForceLogToFile) std::cout<<oss.str()<<std::endl;
	else {
		std::ofstream log("log.txt", std::ios_base::app | std::ios_base::out);
		if(log.is_open())
		{
			log<<oss.str()<<std::endl;
			log.close();
		}
	}
#endif
}

#define LogTrace(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_TRACE, prefix, value)
#define LogDebug(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_DEBUG, prefix, value)
#define LogInfo(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_INFO, prefix, value)
#define LogWarn(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_WARN, prefix, value)
#define LogError(prefix, value)		LogImpl(__FILE__, __LINE__, LEVEL_ERROR, prefix, value)
#define Log(prefix, value)		LogInfo(prefix, value)
#define NOEXCEPTION(stmt)		try { stmt; } catch(const std::exception &e) { LogWarn("Exception", e.what()); } catch(...) {}

// Debug tools
#define VAR(x) 				{ std::ostringstream s; s<<""#x"="<<x; Log("VAR", s.str()); }

}

#endif
