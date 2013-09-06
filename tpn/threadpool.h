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

#ifndef TPN_THREADPOOL_H
#define TPN_THREADPOOL_H

#include "tpn/include.h"
#include "tpn/thread.h"
#include "tpn/task.h"
#include "tpn/set.h"

namespace tpn
{

class ThreadPool : public Synchronizable
{
public:
	ThreadPool(unsigned min = 1,
		   unsigned max = 0,
		   unsigned limit = 0);	// 0 means unlimited
	virtual ~ThreadPool(void);
	
	void launch(Task *task);
	void join(void);
	void clear(void);
	
private:
	class Worker : public Thread, protected Synchronizable
	{
	public:
		Worker(ThreadPool *scheduler);
		~Worker(void);
	
		void runTask(Task *task);
		
	private:
		void run(void);
		
		ThreadPool *mThreadPool;
		Task *mTask;
	};
	
	Set<Worker*> mWorkers;
	Set<Worker*> mAvailableWorkers;
	unsigned mMin, mMax, mLimit;
};

}

#endif
