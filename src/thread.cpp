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

#include "thread.h"

namespace arc
{

Thread::Thread(void) :
		mIsRunning(false)
{	

}

Thread::~Thread(void)
{
	if(mIsRunning)
	{
		waitTermination();
	}
}

void Thread::start(void)
{
	if(pthread_create(&mThread, NULL, &ThreadEntry, reinterpret_cast<void*>(this)) != 0)
			throw Exception("Thread creation failed");
}

void Thread::waitTermination(void) const
{
	 void *dummy = NULL;
	 if(mIsRunning) pthread_join(mThread,&dummy);
}

void Thread::terminate(void)
{
	pthread_cancel(mThread);
	mIsRunning = false;
}

bool Thread::isRunning(void)
{
	return mIsRunning;
}

void Thread::sleep(unsigned time)
{
	unlock();
	msleep(time);
	lock();
}

void *Thread::ThreadEntry(void *arg)
{
	Thread *thread = reinterpret_cast<Thread*>(arg);
	thread->mIsRunning = true;
	thread->run();
	thread->mIsRunning = false;
	pthread_exit(NULL);
}

}
