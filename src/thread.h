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

#ifndef ARC_THREAD_H
#define ARC_THREAD_H

#include "include.h"
#include "synchronizable.h"

namespace arc
{

class Thread
{
public:
	Thread(void);											// start the run() member function on start()
	template<typename T> Thread(void (*func)(void));		// start func() immediately
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

	template<typename T>
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
	bool		mRunning;
	bool		mJoined;
	bool		mAutoDelete;
};

template<typename T> Thread::Thread(void (*func)(void)) :
		mRunning(false),
		mJoined(true),
		mAutoDelete(false)
{
	Assert(func != NULL);
	VoidWrapper<T> *wrapper = new VoidWrapper<T>;
	wrapper->thread = this;
	wrapper->func = func;
	start(wrapper);
}

template<typename T> Thread::Thread(void (*func)(T*), T *arg) :
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

template<typename T> void Thread::VoidWrapper<T>::call(void)
{
	func();
}

template<typename T> void Thread::ArgWrapper<T>::call(void)
{
	func(reinterpret_cast<T*>(arg));
}

}

#endif

