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

#ifndef PLA_TIME_H
#define PLA_TIME_H

#include "pla/include.hpp"
#include "pla/serializable.hpp"

namespace pla
{

// Thread-safe wrapper around time_t
class Time : public Serializable
{
public:
	static Time Now(void);
	static double StructToSeconds(const struct timeval &tv);
	static double StructToSeconds(const struct timespec &ts);
	static void SecondsToStruct(double secs, struct timeval &tv);
	static void SecondsToStruct(double secs, struct timespec &ts);

	Time(void);
	Time(time_t time);
	Time(const String &str);
	~Time(void);

	int hour(void) const;
	int minute(void) const;
	int second(void) const;
	int day(void) const;
	int month(void) const;
	int year(void) const;

	String toDisplayDate(void) const;
	String toHttpDate(void) const;
	String toIsoDate(void) const;
	String toIsoTime(void) const;
	time_t toUnixTime(void) const;
	void   toStruct(struct timeval &ts) const;
	void   toStruct(struct timespec &ts) const;

	double toSeconds(void) const;
	double toHours(void) const;
	double toDays(void) const;

	void addSeconds(double seconds);
	void addHours(double hours);
	void addDays(double days);

	Time &operator += (duration d);
	Time operator + (duration d) const;
	Time &operator -= (duration d);
	Time operator - (duration d) const;
	duration operator - (const Time &t) const;
	bool operator < (const Time &t);
	bool operator > (const Time &t);
	bool operator == (const Time &t);
	bool operator != (const Time &t);
	operator time_t(void) const;

	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isNativeSerializable(void) const;

	enum SerializationFormat { Timestamp, IsoDate, IsoTime };
	void setSerializationFormat(SerializationFormat format);

private:
	static std::mutex TimeMutex;

	void parse(const String &str);

	time_t mTime;
	SerializationFormat mFormat;
};

}

#endif
