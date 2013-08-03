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
	mMin(min),
	mMax(max),
	mLimit(limit)
{
	while(mWorkers.size() < mMin)
	{
		Worker *worker = new Worker(this);
		mWorkers.insert(worker);
		worker->start();
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
		
		while(true)
		{
			String status;
			status << mAvailableWorkers.size() << "/" << mWorkers.size() << " workers available (min=" << mMin << ", max=" << mMax << ", limit=" << mLimit << ")";
			LogDebug("ThreadPool::launch", status);
		
			
			if(!mAvailableWorkers.empty())
			{
				worker = *mAvailableWorkers.begin();
				mAvailableWorkers.erase(worker);
				break;
			}
			else {
				
				if(!mLimit || mWorkers.size() < mLimit)
				{
					worker = new Worker(this);
					mWorkers.insert(worker);
					worker->start();
					break;
				}
			}
			
			wait();
		}
	}
	catch(const Exception &e)
	{
		LogWarn("ThreadPool::launch", String("Failed: ") + e.what());
		throw;
	}
	
	worker->runTask(task);
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
	
	for(	Set<Worker*>::iterator it = mWorkers.begin(); 
		it != mWorkers.end();
		++it)
	{
		Worker *worker = *it;
		worker->runTask(NULL);
		delete worker;		// join the thread
	}
	
	mWorkers.clear();
	mAvailableWorkers.clear();
}

ThreadPool::Worker::Worker(ThreadPool *scheduler) :
	mThreadPool(scheduler),
	mTask(NULL)
{

}

ThreadPool::Worker::~Worker(void)
{
	Synchronize(mThreadPool);
	mThreadPool->mWorkers.erase(this);
	mThreadPool->mAvailableWorkers.erase(this);
	mThreadPool->notifyAll();
}

void ThreadPool::Worker::runTask(Task *task)
{
	Synchronize(this);
	mTask = task;
	notifyAll();
}

void ThreadPool::Worker::run(void)
{
	while(true)
	{
		{
			Synchronize(this);

			if(!mTask) wait();
			if(!mTask) break;
			
			try {
				mTask->run();
			}
			catch(...)
			{	
				LogWarn("ThreadPool::Worker", "Unhandled exception in task");
			}
			
			// Caution: Task might be autodeleted here
			mTask = NULL;
		}
		
		{
			Synchronize(mThreadPool);
			
			if(mThreadPool->mWorkers.contains(this))
			{
				mThreadPool->mAvailableWorkers.insert(this);
				mThreadPool->notifyAll();
			}
			
			wait(1.);
			if(!mThreadPool->mMax || mThreadPool->mWorkers.size() > mThreadPool->mMax)
			{
				mThreadPool->mAvailableWorkers.erase(this);
				break;
			}
		}
	}
}

}
