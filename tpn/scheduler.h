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

#ifndef TPN_SCHEDULER_H
#define TPN_SCHEDULER_H

#include "tpn/include.h"
#include "tpn/time.h"
#include "tpn/thread.h"
#include "tpn/map.h"
#include "tpn/set.h"

namespace tpn
{

class Task
{
public:
	virtual void run(void);
};
	
class Scheduler : public Thread, public Synchronizable
{
public:
	Scheduler(void);
	~Scheduler(void);
	
	void schedule(Task *task, unsigned msecs = 0);
	void schedule(Task *task, const Time &when);
	
	void repeat(Task *task, unsigned period);
	
	void remove(Task *task);
	void clear(void);
	
private:
	void run(void);
	void launch(Task *task);
	
	Map<Time, Set<Task*> > mSchedule;
	Map<Task*, Time> mNextTimes;
	Map<Task*, unsigned> mPeriods;
	
	class Worker : public Thread
	{
	public:
		Worker(Scheduler *scheduler);
		~Worker(void);
	
		void setTask(Task *task);
		
	private:
		void run(void);
		
		Scheduler *mScheduler;
		Task *mTask;
	};
	
	Set<Worker*> mWorkers;
	Set<Worker*> mAvailableWorkers;
};

}

#endif
