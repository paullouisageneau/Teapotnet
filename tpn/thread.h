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

#ifndef TPN_THREAD_H
#define TPN_THREAD_H

#include "tpn/include.h"
#include "tpn/task.h"
#include "tpn/synchronizable.h"

namespace tpn
{

class Thread : public Task
{
public:
	static void Sleep(double secs);
	
	Thread(Task *task = NULL);				// start the run() member function on start()
	Thread(void (*func)(void));				// start func() immediately
	template<typename T> Thread(void (*func)(T*), T *arg);	// start func(arg) immediately
	virtual ~Thread(void);

	void start(bool autoDelete = false);
	void join(void);
	void terminate(void);
	bool isRunning(void);
	
protected:
	virtual void run(void);

private:
	static void *ThreadRun (void *myThread);
	static void *ThreadCall(void *myWrapper);
	friend void *ThreadRun (void *myThread);
	friend void *ThreadCall(void *myWrapper);

	struct Wrapper
	{
		Thread *thread;
		virtual void call(void) = 0;
	};

	struct VoidWrapper : public Wrapper
	{
		void (*func)(void);
		void call(void);
	};

	template<typename T>
	struct ArgWrapper : public Wrapper
	{
		void (*func)(T*);
		T *arg;
		void call(void);
	};

	void start(Wrapper *wrapper);

	pthread_t 	mThread;
	Task		*mTask;
	bool		mRunning;
	bool		mJoined;
	bool		mAutoDelete;
};

template<typename T> Thread::Thread(void (*func)(T*), T *arg) :
		mTask(this),
		mRunning(false),
		mJoined(true),
		mAutoDelete(false)
{
	Assert(func != NULL);
	ArgWrapper<T> *wrapper = new ArgWrapper<T>;
	wrapper->thread = this;
	wrapper->func = func;
	wrapper->arg = arg;
	start(wrapper);
}

template<typename T> void Thread::ArgWrapper<T>::call(void)
{
	func(reinterpret_cast<T*>(arg));
}

}

#endif

