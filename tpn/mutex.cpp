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

#include "tpn/mutex.h"
#include "tpn/exception.h"

namespace tpn
{

Mutex::Mutex(void) :
	mLockCount(0),
	mRelockCount(0)
{
	 if(pthread_mutex_init(&mMutex,NULL) != 0)
		 throw Exception("Unable to create mutex");
}

Mutex::~Mutex(void)
{
	pthread_mutex_destroy(&mMutex);
}

void Mutex::lock(int count)
{
	if(!count) return;
	
	if(mLockCount == 0 || !pthread_equal(mLockedBy, pthread_self()))
	{
		if(pthread_mutex_lock(&mMutex) != 0)
			throw Exception("Unable to lock mutex");
	}

	mLockedBy = pthread_self();
	mLockCount+= count;
}

bool Mutex::tryLock(void)
{
	if(mLockCount == 0 || !pthread_equal(mLockedBy, pthread_self()))
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

int Mutex::unlockAll(void)
{
 	mRelockCount = mLockCount;
	mLockCount = 0;

	if(mRelockCount)
	{
		if(pthread_mutex_unlock(&mMutex) != 0)
			throw Exception("Unable to unlock mutex");
	}
	
	return mRelockCount;
}

void Mutex::relockAll(void)
{
	lock(mRelockCount);
	mRelockCount = 0;
}

int Mutex::lockCount(void) const
{
	return mLockCount; 
}

}
