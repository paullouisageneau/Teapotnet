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

#ifndef ARC_MUTEX_H
#define ARC_MUTEX_H

#include "include.h"

namespace arc
{

// Recusive mutex implementation
class Mutex
{
public:
	Mutex(void);
	virtual ~Mutex(void);

	// Lock and Unlock are cumulative
	// They can be called multiple times
	void lock(void);
	bool tryLock(void);
	void unlock(void);

private:
	pthread_mutex_t mMutex;
	pthread_t mLockedBy;
	int mLockCount;

	friend class Signal;
};

}

#endif
