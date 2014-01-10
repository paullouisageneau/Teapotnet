/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_THREADPOOL_H
#define TPN_THREADPOOL_H

#include "tpn/include.h"
#include "tpn/synchronizable.h"
#include "tpn/signal.h"
#include "tpn/thread.h"
#include "tpn/task.h"
#include "tpn/set.h"

namespace tpn
{

class ThreadPool : protected Synchronizable
{
public:
	ThreadPool(unsigned min = 1,
		   unsigned max = 0,
		   unsigned limit = 0);	// 0 means unlimited
	virtual ~ThreadPool(void);
	
	void launch(Task *task);
	void join(void);
	void clear(void);
	
protected:
	virtual void onTaskFinished(Task *task);
	
private:
	class Worker : public Thread
	{
	public:
		Worker(ThreadPool *pool);
		~Worker(void);
	
		void stop(void);		

	private:
		void run(void);
		
		ThreadPool *mThreadPool;
		bool mShouldStop;
	};
	
	Set<Worker*> mWorkers;
	Set<Worker*> mAvailableWorkers;
	Task *mTask;
	Signal mSignal;
	unsigned mMin, mMax, mLimit;
};

}

#endif
