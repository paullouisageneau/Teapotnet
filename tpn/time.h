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

#ifndef TPN_TIME_H
#define TPN_TIME_H

#include "tpn/include.h"
#include "tpn/serializable.h"
#include "tpn/thread.h"
#include "tpn/mutex.h"

namespace tpn
{

class Time : public Serializable
{
public:
  	static Time Now(void);
	static uint64_t Milliseconds(void);
	static void Schedule(const Time &when, Thread *thread);
	
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
	
	double toSeconds(void) const;
	int toHours(void) const;
	int toDays(void) const;
	
	void addSeconds(double seconds);
	void addHours(int hours);
	void addDays(int days);
	
	Time &operator += (double seconds);
	Time operator + (double seconds) const;
	double operator - (const Time &t) const;
	operator time_t(void) const;
	
	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);

private:
  	static Mutex TimeMutex;

	time_t mTime;
};

bool operator < (const Time &t1, const Time &t2);
bool operator > (const Time &t1, const Time &t2);
bool operator == (const Time &a1, const Time &t2);
bool operator != (const Time &a1, const Time &t2);

}

#endif
