/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#include "pla/alarm.hpp"

namespace pla 
{

thread_local bool Alarm::AutoDeleted = false;

Alarm::Alarm(void) : time(time_point::min()), stop(false)
{
	thread = std::thread([this]()
	{
		while(true)
		{
			std::unique_lock<std::mutex> lock(mutex);
			
			if(!stop && time == time_point::min())
				condition.wait(lock);
			
			if(stop) break;
			
			if(time > clock::now())
			{
				condition.wait_until(lock, time);
			}
			else {
				time = time_point::min();
				if(function) 
				{
					std::function<void()> f = function;
					lock.unlock();
					f();
					if(AutoDeleted) break;
				}
			}
		}
	});
}

Alarm::~Alarm(void)
{
	join();
}

void Alarm::schedule(time_point time)
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		if(stop) throw std::runtime_error("schedule on stopped Alarm");
		this->time = time;
	}
	
	condition.notify_all();
}

void Alarm::schedule(duration d)
{
	schedule(clock::now() + d);
}

void Alarm::cancel(void)
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		time = time_point::min();
	}
	
	condition.notify_all();
}

void Alarm::join(void)
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		stop = true;
	}
	
	condition.notify_all();
	
	if(thread.get_id() == std::this_thread::get_id()) 
	{
		thread.detach();
		AutoDeleted = true;
	}
	else {
		if(thread.joinable()) thread.join();
	}
}

}
