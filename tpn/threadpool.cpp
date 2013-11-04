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
		
		//String status;
		//status << mAvailableWorkers.size() << "/" << mWorkers.size() << " workers available (min=" << mMin << ", max=" << mMax << ", limit=" << mLimit << ")";
		//LogDebug("ThreadPool::launch", status);
		
		while(mAvailableWorkers.empty())
		{
			if(!mLimit || mWorkers.size() < mLimit)
			{
				worker = new Worker(this);
				mWorkers.insert(worker);
				worker->start(true);
				break;
			}
			
			mSignal.wait(*this);
		}
		
		mTask = task;
		notify();

		while(mTask)
                        mSignal.wait(*this);
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
		mSignal.wait(*this);
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
		mSignal.wait(*this);
	}
		
	mWorkers.clear();
	mAvailableWorkers.clear();
}

void ThreadPool::onTaskFinished(Task *task)
{
	// Dummy
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
	mThreadPool->mSignal.launchAll();
}

void ThreadPool::Worker::stop(void)
{
	mShouldStop = true;
}

void ThreadPool::Worker::run(void)
{
	while(true)
	{
		Task *task = NULL;
		
		try {
			Synchronize(mThreadPool);

			if(!mThreadPool->mTask)
			{
				// Worker is now available      
				mThreadPool->mAvailableWorkers.insert(this);
				mThreadPool->mSignal.launchAll();
			}

			// Wait for Task
			while(!mThreadPool->mTask)
			{ 
				double timeout = 10.;
				while(!mThreadPool->mTask && !mShouldStop)
					if(!mThreadPool->wait(timeout))
						break;
			
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
			task = mThreadPool->mTask;
			mThreadPool->mTask = NULL;
			mThreadPool->mSignal.launchAll();
		}
		catch(const std::exception &e)
		{
			LogWarn("ThreadPool::Worker", e.what());
			break;
		}
		
		try {
			task->run();
		}
		catch(const std::exception &e)
		{	
			LogWarn("ThreadPool::Worker", String("Unhandled exception in task: ") + e.what());
		}
		catch(...)
		{	
			LogWarn("ThreadPool::Worker", String("Unknown handled exception in task"));
		}
		
		try {
			Synchronize(mThreadPool);
			mThreadPool->onTaskFinished(task);
		}
		catch(const std::exception &e)
		{
			LogWarn("ThreadPool::Worker", String("Error in finished callback: ") + e.what());
		}
	}
}

}
