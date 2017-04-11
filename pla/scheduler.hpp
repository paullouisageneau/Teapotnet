/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#include <chrono>
#include <map>
#include <set>

namespace pla
{

class Scheduler : protected ThreadPool
{
public:
	using clock = std::chrono::steady_clock;
	typedef std::chrono::duration<double> duration;
	typedef std::chrono::time_point<clock, duration> time_point;
	struct task_id : public std::pair<time_point, unsigned>
	{
		task_id(void) : std::pair<time_point, unsigned>(clock::time_point::min(), 0) {}
	};

	Scheduler(size_t threads = 1);
	~Scheduler(void);

	template<class F, class... Args>
	auto schedule(task_id &id, time_point time, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	template<class F, class... Args>
	auto schedule(task_id &id, duration d, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	template<class F, class... Args>
	auto schedule(time_point time, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	template<class F, class... Args>
	auto schedule(duration d, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	void wait(task_id id);
	void cancel(task_id id);

	void clear(void);
	void join(void);

private:
	std::map<task_id, std::function<void()> > scheduling;
	std::set<task_id> pending;
	std::condition_variable schedulingCondition, pendingCondition;
	std::thread thread;
};

inline Scheduler::Scheduler(size_t threads) :
	ThreadPool(threads)
{
	thread = std::thread([this]()
	{
		std::unique_lock<std::mutex> lock(mutex);
		while(true)
		{
			if(scheduling.empty())
			{
				schedulingCondition.wait(lock);
			}
			else {
				task_id id = scheduling.begin()->first;
				time_point time = id.first;
				if(time > clock::now())
				{
					schedulingCondition.wait_until(lock, time);
				}
				else {
					auto task = scheduling.begin()->second;
					scheduling.erase(scheduling.begin());
					tasks.emplace(std::move(task));
					condition.notify_one();
				}
			}
		}
	});
}

inline Scheduler::~Scheduler(void)
{
	joining = true;
	clear();
	join();
}

template<class F, class... Args>
auto Scheduler::schedule(task_id &id, time_point time, F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	std::future<type> result = task->get_future();

	{
		std::unique_lock<std::mutex> lock(mutex);
		if(joining) throw std::runtime_error("schedule on closing Scheduler");

		// Remove previous task
		if(id.second)
		{
			auto it = scheduling.find(id);
			if(it != scheduling.end())
			{
				scheduling.erase(it);
				pending.erase(id);
				pendingCondition.notify_all();
			}
		}

		// Find new task id
		id.first = time;
		id.second = 1;
		while(scheduling.find(id) != scheduling.end())
			id.second++;

		// Add new task
		pending.insert(id);
		scheduling.emplace(std::make_pair(id, [task, id, this]()
		{
			(*task)();
			std::unique_lock<std::mutex> lock(mutex);
			pending.erase(id);
			pendingCondition.notify_all();
		}));
	}

	schedulingCondition.notify_all();
	return result;
}

template<class F, class... Args>
auto Scheduler::schedule(task_id &id, duration d, F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	return schedule(id, clock::now() + d, std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F, class... Args>
auto Scheduler::schedule(time_point time, F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	task_id id;
	return schedule(id, time, std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F, class... Args>
auto Scheduler::schedule(duration d, F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	task_id id;
	return schedule(id, clock::now() + d, std::forward<F>(f), std::forward<Args>(args)...);
}

inline void Scheduler::wait(Scheduler::task_id id)
{
	if(id.second)
	{
		std::unique_lock<std::mutex> lock(mutex);
		pendingCondition.wait(lock, [this, id]() {
			return pending.find(id) == pending.end();
		});
	}
}

inline void Scheduler::cancel(Scheduler::task_id id)
{
	if(id.second)
	{
		std::unique_lock<std::mutex> lock(mutex);
		auto it = scheduling.find(id);
		if(it != scheduling.end())
		{
			scheduling.erase(it);
			pending.erase(id);
			schedulingCondition.notify_all();
			pendingCondition.notify_all();
		}
	}
}

inline void Scheduler::clear(void)
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		for(auto p : scheduling)
			pending.erase(p.first);
		scheduling.clear();
	}

	schedulingCondition.notify_all();
	pendingCondition.notify_all();
	ThreadPool::clear();
}

inline void Scheduler::join(void)
{
	joining = true;

	if(thread.joinable()) thread.join();
	ThreadPool::join();
}

}

#endif
