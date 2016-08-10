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

#ifndef PLA_THREADPOOL_H
#define PLA_THREADPOOL_H

#include <multimap>

#include "pla/threadpool.h"

namespace pla 
{

class Scheduler : protected ThreadPool
{
	using clock = std::chrono::steady_clock;
	
public:
	Scheduler(size_t threads = 1);
	~Scheduler(void);
	
	template<class F, class... Args>
	auto schedule(clock::time_point time, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;
	
	void join(void);
	
private:
	std::multimap<clock::time_point, std::function<void()> > schedule;
	std::thread thread;
	std::conditional_variable scheduleCondition;
};

inline Scheduler::Scheduler(size_t threads) :
	ThreadPool(threads)
{
	thread = std::thread([this]()
	{
		std::unique_lock<std::mutex> lock(mutex);
		while(!this->stop || !this->schedule.empty())
		{
			if(this->schedule.empty())
			{
				this->scheduleCondition.wait(lock);
			}
			else {
				clock::time_point time = this->schedule.begin()->first;
				if(time > clock::now())
				{
					this->scheduleCondition.wait_until(lock, time);
				}
				
				if(elapse.count() <= 0.)
				{
					tasks.emplace(std::move(this->schedule.begin()->second));
					condition.notify_one();
				}
			}
		}
	});
}

inline Scheduler::~Scheduler(void)
{
	thread.join();
	join();
}

template<class F, class... Args>
auto ThreadPool::schedule(clock::time_point time, F&& f, Args&&... args) 
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared< std::packaged_task<type()> >(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
	
	std::future<type> result = task->get_future();
	
	{
		std::unique_lock<std::mutex> lock(mutex);
		if(stop) throw std::runtime_error("schedule on stopped Scheduler");
		schedule.insert(std::make_pair(time, task));
	}
	
	scheduleCondition.notify_all();
	return result;
}

inline void ThreadPool::join(void)
{
	mThreadPool.join();
}

}

#endif
