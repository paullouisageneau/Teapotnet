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

#include "signal.h"

namespace arc
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
	if(pthread_cond_signal(&mCond))
		throw Exception("Unable to notify signal");
}

void Signal::launchAll(void)
{
	if(pthread_cond_broadcast(&mCond))
		throw Exception("Unable to broadcast signal");
}

void Signal::wait(Mutex &mutex)
{
	if(pthread_cond_wait(&mCond, &mutex.mMutex) != 0)
		throw Exception("Unable to wait for signal");
}

bool Signal::wait(Mutex &mutex, time_t timeout)
{
	timespec ts;
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000000;

	int ret = pthread_cond_timedwait(&mCond, &mutex.mMutex, &ts);
	if(ret == 0) return true;
	if(ret == ETIMEDOUT) return false;
	else throw Exception("Unable to wait for signal");
}

}
