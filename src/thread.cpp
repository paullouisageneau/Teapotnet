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

#include "thread.h"

namespace tpot
{

Thread::Thread(void) :
		mRunning(false),
		mJoined(true),
		mAutoDelete(false)
{	

}

Thread::Thread(void (*func)(void)) :
		mRunning(false),
		mJoined(true),
		mAutoDelete(false)
{
	Assert(func != NULL);
	VoidWrapper *wrapper = new VoidWrapper;
	wrapper->thread = this;
	wrapper->func = func;
	start(wrapper);
}

Thread::~Thread(void)
{
	if(!mJoined) join();
}

void Thread::start(bool autoDelete)
{
	if(mRunning) return;
	if(!mJoined) join();

	mJoined = mAutoDelete = autoDelete;

	if(pthread_create(&mThread, NULL, &ThreadRun, reinterpret_cast<void*>(this)) != 0)
		throw Exception("Thread creation failed");

	if(mAutoDelete)
		pthread_detach(mThread);
}

void Thread::start(Thread::Wrapper *wrapper)
{
	if(mRunning) return;
	if(!mJoined) join();
	mJoined = false;
	mAutoDelete = false;

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
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_thread_attach_np();
#endif
	
	try {
		thread->run();
	}
	catch(const std::exception &e)
	{
		Log("Thread::ThreadRun", String("WARNING: Unhandled exception in thread: ") + e.what()); 
	}
	catch(...)
	{
		Log("Thread::ThreadRun", String("WARNING: Unhandled unknown exception in thread")); 
	}
	
	thread->mRunning = false;
	if(thread->mAutoDelete) delete thread;
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_thread_detach_np();
#endif
	pthread_exit(NULL);
}

void *Thread::ThreadCall(void *myWrapper)
{
	Wrapper *wrapper = reinterpret_cast<Wrapper*>(myWrapper);
	wrapper->thread->mRunning = true;
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_thread_attach_np();
#endif
	
	try {
		wrapper->call();
	}
	catch(const std::exception &e)
	{
		Log("Thread::ThreadCall", String("WARNING: Unhandled exception in thread: ") + e.what()); 
	}
	catch(...)
	{
		Log("Thread::ThreadCall", String("WARNING: Unhandled unknown exception in thread")); 
	}
	
	delete wrapper;
	wrapper->thread->mRunning = false;
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_thread_detach_np();
#endif
	pthread_exit(NULL);
}

void Thread::VoidWrapper::call(void)
{
	func();
}

}
