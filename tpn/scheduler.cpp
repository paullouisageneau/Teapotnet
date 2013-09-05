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

Scheduler *Scheduler::Global = new Scheduler(1);

Scheduler::Scheduler(unsigned maxWaitingThreads) :
	ThreadPool(0, maxWaitingThreads, 0)
{
	
}

Scheduler::~Scheduler(void)
{
	clear();	// wait for threads to finish
}

void Scheduler::schedule(Task *task, double timeout)
{
	Synchronize(this);
	
	schedule(task, Time::Now() + timeout);
}

void Scheduler::schedule(Task *task, const Time &when)
{
	Synchronize(this);
	
	remove(task);
	mSchedule[when].insert(task);
	mNextTimes[task] = when;
	
	LogDebug("Scheduler::schedule", "Scheduled task (total " + String::number(mNextTimes.size()) + ")");
	
	if(!isRunning()) start();
	notifyAll();
}

void Scheduler::repeat(Task *task, double period)
{
	Synchronize(this);
	
	if(!period)
	{
		remove(task);
		return;
	}
	
	if(!mNextTimes.contains(task))
		schedule(task, period);
	
	mPeriods[task] = period;
}

void Scheduler::remove(Task *task)
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

void Scheduler::run(void)
{
	while(true)
	{
		Synchronize(this);
		if(mSchedule.empty()) break;

		double d =  mSchedule.begin()->first - Time::Now();
		if(d > 0.)
		{
			//LogDebug("Scheduler::run", "Next task in " + String::number(d) + " s");
			wait(std::min(d, 60.));	// bound is necessary here in case of wall clock change
			continue;
		}
		
		const Set<Task*> &set = mSchedule.begin()->second;
		for(Set<Task*>::iterator it = set.begin(); it != set.end(); ++it)
		{
			Task *task = *it;
			
			LogDebug("Scheduler::run", "Launching task...");
			launch(task);
			mNextTimes.erase(task);
			
			double period = 0.;
			if(mPeriods.get(task, period))
				schedule(task, Time::Now() + period);
		}
		
		mSchedule.erase(mSchedule.begin());
	}
	
	LogDebug("Scheduler::run", "No more tasks");
}

}
