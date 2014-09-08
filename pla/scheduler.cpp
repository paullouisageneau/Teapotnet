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

#include "pla/scheduler.h"
#include "pla/exception.h"
#include "pla/string.h"

namespace pla
{

Scheduler *Scheduler::Global = new Scheduler(1);

Scheduler::Scheduler(unsigned maxWaitingThreads) :
	ThreadPool(0, maxWaitingThreads, 0)
{
	
}

Scheduler::~Scheduler(void)
{
	NOEXCEPTION(clear());	// wait for threads to finish
}

void Scheduler::schedule(Task *task, double timeout)
{
	Synchronize(this);
	
	schedule(task, Time::Now() + timeout);
}

void Scheduler::schedule(Task *task, const Time &when)
{
	Synchronize(this);
	
	Time nextTime;
        if(mNextTimes.get(task, nextTime))
        {
                mSchedule[nextTime].erase(task);
                mNextTimes.erase(task);
        }

	mSchedule[when].insert(task);
	mNextTimes[task] = when;
	
	Map<Time, Set<Task*> >::iterator it = mSchedule.begin();
	double d =  it->first - Time::Now();
	
	//LogDebug("Scheduler::schedule", "Scheduled task (total " + String::number(mNextTimes.size()) + ")");
	if(!isRunning()) start();
	notifyAll();
}

void Scheduler::repeat(Task *task, double period)
{
	Synchronize(this);
	
	if(period < 0.)
	{
		cancel(task);
		return;
	}
	
	if(!mNextTimes.contains(task))
		schedule(task, period);
	
	mPeriods[task] = period;
}

void Scheduler::cancel(Task *task)
{
	Synchronize(this);

	Time nextTime;
	if(mNextTimes.get(task, nextTime))
	{
		mSchedule[nextTime].erase(task);
		mNextTimes.erase(task);
	}
	
	mPeriods.erase(task);
}

void Scheduler::clear(void)
{
	Synchronize(this);
	
	mSchedule.clear();
	mNextTimes.clear();
	mPeriods.clear();
	
	ThreadPool::clear();	// wait for threads to finish
}

void Scheduler::onTaskFinished(Task *task)
{
	Synchronize(this);
	Assert(task);
	
	double period = 0.;
	if(mPeriods.get(task, period))
	{
		//LogDebug("Scheduler::run", "Re-scheduled task (period=" + String::number(period) + ")");
		schedule(task, period);
	}
}

void Scheduler::run(void)
{
	while(true)
	{
		try {
			Synchronize(this);
			if(mSchedule.empty()) break;

			Map<Time, Set<Task*> >::iterator it = mSchedule.begin();
			double d =  it->first - Time::Now();
			if(d > 0.)
			{
				//LogDebug("Scheduler::run", "Next task in " + String::number(d) + " s");
				wait(std::min(d, 60.));	// bound is necessary here in case of wall clock change
				continue;
			}
	
			Set<Task*> set = it->second;
			mSchedule.erase(it);
			
			for(Set<Task*>::iterator jt = set.begin(); jt != set.end(); ++jt)
			{
				Task *task = *jt;
				mNextTimes.erase(task);
				
				//LogDebug("Scheduler::run", "Launching task...");
				launch(task);
			}
		}
		catch(const std::exception &e)
		{
			LogWarn("Scheduler::run", e.what());
		}
	}
}

}
