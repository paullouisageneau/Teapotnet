/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_MUTEX_H
#define TPN_MUTEX_H

#include "tpn/include.h"

namespace tpn
{

// Recusive mutex implementation
class Mutex
{
public:
	Mutex(void);
	virtual ~Mutex(void);

	// Lock and Unlock are cumulative
	// They can be called multiple times
	void lock(int count = 1);
	bool tryLock(void);
	void unlock(void);	
	int  unlockAll(void);
	
	int lockCount(void) const;
	
private:
	static void GlobalLock(void);
	static void GlobalUnlock(void);
	static pthread_mutex_t GlobalMutex;

	pthread_mutex_t mMutex;
	pthread_t mLockedBy;
	int mLockCount;
	int mRelockCount;

	friend class Signal;
};

class MutexLocker
{
public:
        inline MutexLocker(Mutex *_m) : m(_m) { m->lock(); }
        inline ~MutexLocker(void) { m->unlock(); }

private:
        Mutex *m;
};

}

#endif
