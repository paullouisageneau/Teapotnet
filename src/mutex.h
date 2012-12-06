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

#ifndef TPOT_MUTEX_H
#define TPOT_MUTEX_H

#include "include.h"

namespace tpot
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
	void relockAll(void);
	
	int lockCount(void) const;
	
private:
	pthread_mutex_t mMutex;
	pthread_t mLockedBy;
	int mLockCount;
	int mRelockCount;

	friend class Signal;
};

}

#endif
