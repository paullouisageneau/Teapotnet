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
		mRunning(false),
		mJoined(true)
{	

}

Thread::~Thread(void)
{
	if(!mJoined) join();
}

void Thread::start(void)
{
	if(mRunning) return;
	if(!mJoined) join();
	mJoined = false;

	if(pthread_create(&mThread, NULL, &ThreadRun, reinterpret_cast<void*>(this)) != 0)
			throw Exception("Thread creation failed");
}

void Thread::start(Thread::Wrapper *wrapper)
{
	if(mRunning) return;
	if(!mJoined) join();
	mJoined = false;

	if(pthread_create(&mThread, NULL, &ThreadCall, reinterpret_cast<void*>(wrapper)) != 0)
				throw Exception("Thread creation failed");
}

void Thread::join(void)
{
	 if(!mJoined)
	 {
		 void *dummy = NULL;
		 pthread_join(mThread, &dummy);
		 mJoined = true;
	 }

}

void Thread::terminate(void)
{
	if(mRunning)
	{
		pthread_cancel(mThread);
		mRunning = false;
		mJoined = true;
	}
}

bool Thread::isRunning(void)
{
	return mRunning;
}

void Thread::run(void)
{
	// DUMMY
}

void *Thread::ThreadRun(void *myThread)
{
	Thread *thread = reinterpret_cast<Thread*>(myThread);
	thread->mRunning = true;
	thread->run();
	thread->mRunning = false;
	pthread_exit(NULL);
}

void *Thread::ThreadCall(void *myWrapper)
{
	Wrapper *wrapper = reinterpret_cast<Wrapper*>(myWrapper);
	wrapper->thread->mRunning = true;
	wrapper->call();
	delete wrapper;
	wrapper->thread->mRunning = false;
	pthread_exit(NULL);
}

}
