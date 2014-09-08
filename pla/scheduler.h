/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef PLA_SCHEDULER_H
#define PLA_SCHEDULER_H

#include "pla/include.h"
#include "pla/threadpool.h"
#include "pla/time.h"
#include "pla/map.h"

namespace pla
{

class Scheduler : protected ThreadPool, public Thread
{
public:
	static Scheduler *Global;

	Scheduler(unsigned maxWaitingThreads = 1);
	~Scheduler(void);
	
	void schedule(Task *task, double timeout = 0.);
	void schedule(Task *task, const Time &when);
	
	void repeat(Task *task, double period);
	
	void cancel(Task *task);
	void clear(void);
	
private:
	void onTaskFinished(Task *task);
	void run(void);
	
	Map<Time, Set<Task*> > mSchedule;
	Map<Task*, Time> mNextTimes;
	Map<Task*, double> mPeriods;
};

}

#endif
