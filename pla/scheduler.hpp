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

#ifndef PLA_SCHEDULER_H
#define PLA_SCHEDULER_H

#include "pla/threadpool.hpp"

namespace pla 
{

class Scheduler : protected ThreadPool
{
public:
	using clock = std::chrono::steady_clock;
	
	Scheduler(size_t threads = 1);
	~Scheduler(void);
	
	template<class F, class... Args>
	auto schedule(clock::time_point time, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;
	
	void join(void);
	
private:
	std::multimap<clock::time_point, std::shared_ptr<std::packaged_task<void()> > > scheduleMap;
	std::condition_variable scheduleCondition;
	std::thread thread;
};

inline Scheduler::Scheduler(size_t threads) :
	ThreadPool(threads)
{
	thread = std::thread([this]()
	{
		std::unique_lock<std::mutex> lock(mutex);
		while(!this->stop || !this->scheduleMap.empty())
		{
			if(this->scheduleMap.empty())
			{
				this->scheduleCondition.wait(lock);
			}
			else {
				clock::time_point time = this->scheduleMap.begin()->first;
				if(time <= clock::now())
				{
					auto task = this->scheduleMap.begin()->second;
					this->scheduleMap.erase(this->scheduleMap.begin());
					tasks.emplace([task] { (*task)(); });
					condition.notify_one();
				}
				else {
					this->scheduleCondition.wait_until(lock, time);
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
auto Scheduler::schedule(clock::time_point time, F&& f, Args&&... args) 
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	std::future<type> result = task->get_future();	

	{
		std::unique_lock<std::mutex> lock(mutex);
		if(stop) throw std::runtime_error("schedule on stopped Scheduler");
		scheduleMap.emplace(std::make_pair(time, task));
	}
	
	scheduleCondition.notify_all();
	return result;
}

inline void Scheduler::join(void)
{
	ThreadPool::join();
}

}

#endif
