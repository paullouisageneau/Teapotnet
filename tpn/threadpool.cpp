/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/scheduler.h"
#include "tpn/exception.h"
#include "tpn/string.h"

namespace tpn
{

ThreadPool::ThreadPool(unsigned min, unsigned max, unsigned limit) :
	mTask(NULL),
	mMin(min),
	mMax(max),
	mLimit(limit)
{
	Synchronize(this);

	while(mWorkers.size() < mMin)
	{
		Worker *worker = new Worker(this);
		mWorkers.insert(worker);
		worker->start(true);
	}
}

ThreadPool::~ThreadPool(void)
{
	clear();
}

void ThreadPool::launch(Task *task)
{
	Worker *worker;
	try
	{
		Synchronize(this);
		Assert(!mTask);
		
		String status;
		status << mAvailableWorkers.size() << "/" << mWorkers.size() << " workers available (min=" << mMin << ", max=" << mMax << ", limit=" << mLimit << ")";
		LogDebug("ThreadPool::launch", status);
		
		while(mAvailableWorkers.empty())
		{
			if(!mLimit || mWorkers.size() < mLimit)
			{
				worker = new Worker(this);
				mWorkers.insert(worker);
				worker->start(true);
				break;
			}
			
			wait();
		}
		
		mTask = task;
		notify();

		while(mTask)
                        mBackSignal.wait(*this);
	}
	catch(const Exception &e)
	{
		LogWarn("ThreadPool::launch", String("Failed: ") + e.what());
		throw;
	}
}

void ThreadPool::join(void)
{
	Synchronize(this);
	
	// Threads are always running so we can't use join() here
	while(mAvailableWorkers.size() < mWorkers.size())
		wait();
}

void ThreadPool::clear(void)
{
	Synchronize(this);
	
	while(!mWorkers.empty())
	{
		for(	Set<Worker*>::iterator it = mWorkers.begin(); 
			it != mWorkers.end();
			++it)
		{
			Worker *worker = *it;
			worker->stop();
		}

		notifyAll();
		wait();
	}
		
	mWorkers.clear();
	mAvailableWorkers.clear();
}

ThreadPool::Worker::Worker(ThreadPool *pool) :
	mThreadPool(pool),
	mShouldStop(false)
{

}

ThreadPool::Worker::~Worker(void)
{
	Synchronize(mThreadPool);
	mThreadPool->mAvailableWorkers.erase(this);
	mThreadPool->mWorkers.erase(this);
	mThreadPool->notifyAll();
}

void ThreadPool::Worker::stop(void)
{
	mShouldStop = true;
}

void ThreadPool::Worker::run(void)
{
	while(true)
	{
		Synchronize(mThreadPool);

		if(!mThreadPool->mTask)
		{
			// Worker is now available      
                        mThreadPool->mAvailableWorkers.insert(this);
                        mThreadPool->notifyAll();
		}

		// Wait for Task
                while(!mThreadPool->mTask)
                { 
			double timeout = 10.;
                        while(!mThreadPool->mTask && !mShouldStop) 
				mThreadPool->wait(timeout);
		
			// Terminate if necessary
                        if(!mThreadPool->mTask)
                        {
                                if(mShouldStop || mThreadPool->mWorkers.size() > mThreadPool->mMax)
                                {
                                        mThreadPool->mAvailableWorkers.erase(this);
                                        mThreadPool->mWorkers.erase(this);
					return;
                                }
                        }
                }

		// Run task
		mThreadPool->mAvailableWorkers.erase(this);
		Task *task = mThreadPool->mTask;
                mThreadPool->mTask = NULL;
		mThreadPool->mBackSignal.launchAll();

		try {
			Desynchronize(mThreadPool);	
			task->run();
		}
		catch(...)
		{	
			LogWarn("ThreadPool::Worker", "Unhandled exception in task");
		}
	}
}

}
