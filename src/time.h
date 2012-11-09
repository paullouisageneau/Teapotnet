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

#ifndef TPOT_TIME_H
#define TPOT_TIME_H

#include "include.h"
#include "serializable.h"
#include "thread.h"
#include "mutex.h"

namespace tpot
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

	String toHttpDate(void) const;
	time_t toUnixTime(void) const;
	
	double operator - (const Time &t);
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
