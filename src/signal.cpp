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

#include "signal.h"

namespace tpot
{

Signal::Signal(void)
{
	if(pthread_cond_init(&mCond, NULL) != 0)
		throw Exception("Unable to create new signal: Condition variable creation failed");
}

Signal::~Signal(void)
{
	pthread_cond_destroy(&mCond);
}

void Signal::launch(void)
{
	if(pthread_cond_signal(&mCond) != 0)
		throw Exception("Unable to notify signal");
}

void Signal::launchAll(void)
{
	if(pthread_cond_broadcast(&mCond) != 0)
		throw Exception("Unable to broadcast signal");
}

void Signal::wait(Mutex &mutex)
{
	mutex.lock();
	int oldLockCount = mutex.mLockCount;
	mutex.mLockCount = 0;
	
	int ret = pthread_cond_wait(&mCond, &mutex.mMutex);
	
	mutex.mLockCount = oldLockCount;
	mutex.unlock();

	if(ret) throw Exception("Unable to wait for signal");
}

bool Signal::wait(Mutex &mutex, unsigned timeout)
{
	timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t t = uint64_t(tv.tv_sec)*1000 + uint64_t(tv.tv_usec)*1000 + timeout;
	timespec ts;
	ts.tv_sec = t/1000;
	ts.tv_nsec = (t%1000)*1000000;
	
	mutex.lock();
	int oldLockCount = mutex.mLockCount;
	mutex.mLockCount = 0;
	
	int ret = pthread_cond_timedwait(&mCond, &mutex.mMutex, &ts);

	mutex.mLockCount = oldLockCount;
	mutex.unlock();

	if(ret == 0) return true;
	else if(ret == ETIMEDOUT) return false;
	else throw Exception("Unable to wait for signal");
}

}
