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

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	//pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

	 if(pthread_mutex_init(&mMutex, &attr) != 0)
		 throw Exception("Unable to create mutex");
}

Mutex::~Mutex(void)
{
	pthread_mutex_destroy(&mMutex);
}

void Mutex::lock(int count)
{
	if(count <= 0) return;
	
	int ret = pthread_mutex_lock(&mMutex);
	if(ret != 0 && ret != EDEADLK)
		throw Exception("Unable to lock mutex");

	if(ret == 0) Assert(mLockCount == 0);
	mLockedBy = pthread_self();
	mLockCount+= count;
}

bool Mutex::tryLock(void)
{
	int ret = pthread_mutex_trylock(&mMutex);
        if(ret == EBUSY) return false;
       
        if(ret != 0 && ret != EDEADLK)
                throw Exception("Unable to lock mutex");

	if(ret == 0) Assert(mLockCount == 0);
	mLockedBy = pthread_self();
        mLockCount++;
	return true;
}

void Mutex::unlock(void)
{
	if(!mLockCount)
		throw Exception("Mutex is not locked");
	
	if(mLockedBy != pthread_self())
		throw Exception("Mutex is locked by another thread");
	
	mLockCount--;
	if(mLockCount == 0)
	{
		int ret = pthread_mutex_unlock(&mMutex);
		if(ret != 0) throw Exception("Unable to unlock mutex");
	}
}

int Mutex::unlockAll(void)
{
	if(!mLockCount)
		throw Exception("Mutex is not locked");
	
	if(mLockedBy != pthread_self())
		throw Exception("Mutex is locked by another thread");
	
 	mRelockCount = mLockCount;
	mLockCount = 0;

	int ret = pthread_mutex_unlock(&mMutex);
	if(ret != 0) throw Exception("Unable to unlock mutex");
	
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
