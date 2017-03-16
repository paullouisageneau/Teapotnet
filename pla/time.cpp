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

#include "pla/time.hpp"
#include "pla/exception.hpp"
#include "pla/string.hpp"
#include "pla/list.hpp"

namespace pla
{

std::mutex Time::TimeMutex;

Time Time::Now(void)
{
	return Time();
}

double Time::StructToSeconds(const struct timeval &tv)
{
	return double(tv.tv_sec) + double(tv.tv_usec)/1000000.;
}

double Time::StructToSeconds(const struct timespec &ts)
{
	return double(ts.tv_sec) + double(ts.tv_nsec)/1000000000.;
}

void Time::SecondsToStruct(double secs, struct timeval &tv)
{
	double isecs = 0.;
	double fsecs = std::modf(secs, &isecs);
	tv.tv_sec = time_t(isecs);
	tv.tv_usec = long(fsecs*1000000.);
}

void Time::SecondsToStruct(double secs, struct timespec &ts)
{
	double isecs = 0.;
	double fsecs = std::modf(secs, &isecs);
	ts.tv_sec = time_t(isecs);
	ts.tv_nsec = long(fsecs*100000000.);
}

Time::Time(void) :
	mFormat(Timestamp)
{
	timeval tv;
	Assert(gettimeofday(&tv, NULL) == 0);
	mTime = tv.tv_sec;
}

Time::Time(time_t time) :
	mTime(time),
	mFormat(Timestamp)
{

}

Time::Time(const String &str)
{
	parse(str);
}

Time::~Time(void)
{

}

int Time::hour(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	int result = std::localtime(&mTime)->tm_hour;	// not thread safe
	return result;
}

int Time::minute(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	int result = std::localtime(&mTime)->tm_min;		// not thread safe
	return result;
}

int Time::second(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	int result = std::localtime(&mTime)->tm_sec;		// not thread safe
	return result;
}

int Time::day(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	int result = std::localtime(&mTime)->tm_mday;	// not thread safe
	return result;
}

int Time::month(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	int result = std::localtime(&mTime)->tm_mon;		// not thread safe
	TimeMutex.unlock();
	return result;
}

int Time::year(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	int result = 1900 + std::localtime(&mTime)->tm_year;	// not thread safe
	return result;
}

String Time::toDisplayDate(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	char buffer[256];
	std::strftime(buffer, 256, "%x %X", std::localtime(&mTime));	// not thread safe
	return String(buffer);
}

String Time::toHttpDate(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	char buffer[256];
	std::strftime(buffer, 256, "%a, %d %b %Y %H:%M:%S", std::gmtime(&mTime));	// not thread safe
	return String(buffer) + " GMT";
}

String Time::toIsoDate(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	char buffer[256];
	std::strftime(buffer, 256, "%Y-%m-%d", std::localtime(&mTime));	// not thread safe
	return String(buffer);
}

String Time::toIsoTime(void) const
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	char buffer[256];
	std::strftime(buffer, 256, "%Y-%m-%d %H:%M:%S", std::localtime(&mTime));	// not thread safe
	return String(buffer);
}

time_t Time::toUnixTime(void) const
{
	return mTime;
}

void Time::toStruct(struct timeval &tv) const
{
	tv.tv_sec = mTime;
	tv.tv_usec = 0;
}

void Time::toStruct(struct timespec &ts) const
{
	ts.tv_sec = mTime;
	ts.tv_nsec = 0;
}

double Time::toSeconds(void) const
{
	return std::difftime(mTime, time_t(0));
}

double Time::toHours(void) const
{
	return toSeconds()/3600;
}

double Time::toDays(void) const
{
	return toSeconds()/86400;
}

void Time::addSeconds(double seconds)
{
	std::unique_lock<std::mutex> lock(TimeMutex);
	struct tm t = *std::localtime(&mTime);	// not thread safe
	t.tm_sec += int(seconds);
	mTime = std::mktime(&t);
}

void Time::addHours(double hours)
{
	addSeconds(hours*3600);
}

void Time::addDays(double days)
{
	addSeconds(days*86400);
}

Time &Time::operator+= (duration d)
{
	addSeconds(d.count());
	return *this;
}

Time Time::operator+ (duration d) const
{
	Time result(*this);
	result+= d;
	return result;
}

Time &Time::operator-= (duration d)
{
	addSeconds(-d.count());
	return *this;
}

Time Time::operator- (duration d) const
{
	Time result(*this);
	result-= d;
	return result;
}

duration Time::operator- (const Time &t) const
{
	return seconds(std::difftime(mTime, t.mTime));
}

bool Time::operator< (const Time &t)
{
	return (mTime < t.mTime);
}

bool Time::operator> (const Time &t)
{
	return (mTime > t.mTime);
}

bool Time::operator== (const Time &t)
{
	return (mTime == t.mTime);
}

bool Time::operator!= (const Time &t)
{
	return !(*this == t);
}

Time::operator time_t(void) const
{
	return toUnixTime();
}

void Time::serialize(Serializer &s) const
{
	switch(mFormat)
	{
		case IsoDate:
			s << toIsoDate();
			break;

		case IsoTime:
			s << toIsoTime();
			break;

		default:
			s << int64_t(mTime);
			break;
	}
}

bool Time::deserialize(Serializer &s)
{
	switch(mFormat)
	{
		case IsoDate:
		case IsoTime:
		{
			String str;
			if(!(s >> str)) return false;
			parse(str);
			break;
		}

		default:
		{
			int64_t tmp = 0;
			if(!(s >> tmp)) return false;
			mTime = time_t(tmp);
			break;
		}
	}

	return true;
}

bool Time::isNativeSerializable(void) const
{
	return true;
}

void Time::setSerializationFormat(SerializationFormat format)
{
	mFormat = format;
}

void Time::parse(const String &str)
{
	// Supported formats :
	// 1994-11-06 08:49:37            ; ISO date and time
	// Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	// Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	// Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format

	const String months[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};

	mTime = time_t(0);

	if(str.trimmed().empty())
		return;

	List<String> list;
	str.trimmed().explode(list, ' ');

	struct tm tms;
	std::memset(&tms, 0, sizeof(tms));
	tms.tm_isdst = -1;

	switch(list.size())
	{
		case 1:
		{
			if(!str.contains('-'))
			{
				// Unix timestamp as integer
				str.extract(mTime);
				return;
			}

			// ISO YYYY-MM-DD
			List<String> dateParts;
			str.explode(dateParts, '-');
			Assert(dateParts.size() == 3);
			tms.tm_year = dateParts.front().toInt()-1900;	dateParts.pop_front();
			tms.tm_mon  = dateParts.front().toInt()-1;	dateParts.pop_front();
			tms.tm_mday = dateParts.front().toInt();	dateParts.pop_front();
			break;
		}

		case 2: // ISO YYYY-MM-DD HH:MM:SS
		{
			String tmp;
			tmp = list.front(); list.pop_front();
			List<String> dateParts;
			tmp.explode(dateParts, '-');
			Assert(dateParts.size() == 3);
			tms.tm_year = dateParts.front().toInt()-1900;	dateParts.pop_front();
			tms.tm_mon  = dateParts.front().toInt()-1;	dateParts.pop_front();
			tms.tm_mday = dateParts.front().toInt();	dateParts.pop_front();

			tmp = list.front(); list.pop_front();
			List<String> hourParts;
			tmp.explode(hourParts, ':');
			Assert(hourParts.size() == 3);
			tms.tm_hour = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_min  = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_sec  = hourParts.front().toInt(); hourParts.pop_front();
			break;
		}

		case 4:	// RFC 850
		{
			list.pop_front();       // we don't care about day of week

			String tmp;
			tmp = list.front(); list.pop_front();
			List<String> dateParts;
			tmp.explode(dateParts, '-');
			Assert(dateParts.size() == 3);
			tms.tm_mday = dateParts.front().toInt(); dateParts.pop_front();
			tmp  = dateParts.front().toLower(); dateParts.pop_front();
			int m = 0; while(m < 12 && months[m] != tmp) ++m;
			Assert(m < 12);
			tms.tm_mon = m;
			tms.tm_year = dateParts.front().toInt(); dateParts.pop_front();

			tmp = list.front(); list.pop_front();
			List<String> hourParts;
			tmp.explode(hourParts, ':');
			Assert(hourParts.size() == 3);
			tms.tm_hour = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_min  = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_sec  = hourParts.front().toInt(); hourParts.pop_front();

			String utc = list.front().toLower(); list.pop_front();
			tmp = utc.cut('+');
			Assert(utc == "UTC" || utc == "GMT");
			if(!tmp.empty()) tms.tm_hour = tms.tm_hour - tmp.toInt();
			break;
		}

		case 5:	// asctime() format
		{
			list.pop_front();	// we don't care about day of week

			String tmp;
			tmp = list.front().toLower(); list.pop_front();
			int m = 0; while(m < 12 && months[m] != tmp) ++m;
			Assert(m < 12);
			tms.tm_mon = m;

			tms.tm_mday = list.front().toInt(); list.pop_front();

			tmp = list.front(); list.pop_front();
			List<String> hourParts;
			tmp.explode(hourParts, ':');
			Assert(hourParts.size() == 3);
			tms.tm_hour = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_min  = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_sec  = hourParts.front().toInt(); hourParts.pop_front();

			tms.tm_year = list.front().toInt() - 1900; list.pop_front();
			break;
		}

		case 6: // RFC 1123
		{
			list.pop_front();       // we don't care about day of week

			tms.tm_mday = list.front().toInt(); list.pop_front();

			String tmp;
			tmp = list.front().toLower(); list.pop_front();
			int m = 0; while(m < 12 && months[m] != tmp) ++m;
			Assert(m < 12);
			tms.tm_mon = m;

			tms.tm_year = list.front().toInt() - 1900; list.pop_front();

			tmp = list.front(); list.pop_front();
			List<String> hourParts;
			tmp.explode(hourParts, ':');
			Assert(hourParts.size() == 3);
			tms.tm_hour = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_min  = hourParts.front().toInt(); hourParts.pop_front();
			tms.tm_sec  = hourParts.front().toInt(); hourParts.pop_front();

			String utc = list.front().toUpper(); list.pop_front();
			tmp = utc.cut('+');
			Assert(utc == "UTC" || utc == "GMT");
			if(!tmp.empty()) tms.tm_hour = tms.tm_hour - tmp.toInt();
			break;
		}

		default:
			throw Exception(String("Unknown date format: ") + str);
	}

	std::unique_lock<std::mutex> lock(TimeMutex);

	char *tz = getenv("TZ");
	putenv(const_cast<char*>("TZ=UTC"));
	tzset();

	mTime = std::mktime(&tms);

	if(tz)
	{
		char *buf = reinterpret_cast<char*>(std::malloc(3 + strlen(tz) + 1));
		if(buf)
		{
			std::strcpy(buf,"TZ=");
			std::strcat(buf, tz);
			putenv(buf);
		}
	}
	else {
		putenv(const_cast<char*>("TZ="));
	}
	tzset();

	if(mTime == time_t(-1))
		throw Exception(String("Invalid date: ") + str);
}

}
