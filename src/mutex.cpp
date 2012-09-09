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

#include "mutex.h"
#include "exception.h"

namespace arc
{

Mutex::Mutex(void) :
		mLockedBy(0),
		mLockCount(0)
{
	 if(pthread_mutex_init(&mMutex,NULL) != 0)
		 throw Exception("Unable to create mutex");
}

Mutex::~Mutex(void)
{
	pthread_mutex_destroy(&mMutex);
}

void Mutex::lock(void)
{
	if(mLockCount == 0 || mLockedBy != pthread_self())
	{
		if(pthread_mutex_lock(&mMutex) != 0)
			throw Exception("Unable to lock mutex");
	}

	mLockedBy = pthread_self();
	mLockCount++;
}

bool Mutex::tryLock(void)
{
	if(mLockCount == 0 || mLockedBy != pthread_self())
	{
		int ret = pthread_mutex_trylock(&mMutex);
		if(ret == EBUSY) return false;
		else if(ret != 0) throw Exception("Unable to lock mutex");
	}

	mLockedBy = pthread_self();
	mLockCount++;
	return true;
}

void Mutex::unlock(void)
{
	if(mLockCount == 0) throw Exception("Mutex is not locked");

	mLockCount--;
	if(mLockCount == 0)
	{
		if(pthread_mutex_unlock(&mMutex) != 0)
			throw Exception("Unable to unlock mutex");
	}
}

}
