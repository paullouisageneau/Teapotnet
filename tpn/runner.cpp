/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#include "tpn/runner.h"

namespace tpn
{

Runner::Runner(void)
{
	
}

Runner::~Runner(void)
{
	NOEXCEPTION(clear());	// wait for the thread to finish
}

void Runner::schedule(Task *task)
{
	Synchronize(this);
	mTasks.push_back(task);
	if(!isRunning()) start();
}

void Scheduler::cancel(Task *task)
{
	Synchronize(this);
	mTasks.remove(task);
}

void Scheduler::clear(void)
{
	Synchronize(this);
	mTasks.clear();
	join();
}

void Runner::run(void)
{
	while(true)
	{
		try {
			Synchronize(this);
			if(mTasks.empty()) break;
			
			Task *task = mTasks.front();
			mTasks.pop_front();
			
			DesynchronizeStatement(this, task->run());
		}
		catch(const std::exception &e)
		{
			LogWarn("Scheduler::run", e.what());
		}
	}
}

}
