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

#include "tpn/signal.h"
#include "tpn/time.h"

namespace tpn
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
	
	mutex.mLockedBy = pthread_self();
	mutex.mLockCount = oldLockCount;
	mutex.unlock();

	if(ret) throw Exception("Unable to wait for signal");
}

bool Signal::wait(Mutex &mutex, double &timeout)
{
	Time t1;
	t1+= timeout;
	struct timespec ts;
	t1.toStruct(ts);
	
	mutex.lock();
	int oldLockCount = mutex.mLockCount;
	mutex.mLockCount = 0;
	
	int ret = pthread_cond_timedwait(&mCond, &mutex.mMutex, &ts);

	mutex.mLockedBy = pthread_self();
	mutex.mLockCount = oldLockCount;
	mutex.unlock();

	if(ret == ETIMEDOUT) 
	{
		timeout = 0.;
		return false;
	}
	
	Time t2;
	timeout = std::max(t1-t2, 0.);	// time left
	
	if(ret == 0) return true;
	else throw Exception("Unable to wait for signal");
}

bool Signal::wait(Mutex &mutex, const double &timeout)
{
	double tmp = timeout;
	return wait(mutex, tmp);
}
  
}
