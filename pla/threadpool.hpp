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

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <chrono>

namespace pla 
{

class ThreadPool 
{
public:
	ThreadPool(size_t threads);	// TODO: max tasks in queue
	~ThreadPool(void);
	
	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;
	
	virtual void join(void);
	
protected:
	std::vector<std::thread > workers;
	std::queue<std::function<void()> > tasks;
	
	std::mutex mutex;
	std::condition_variable condition;
	bool stop;
};

inline ThreadPool::ThreadPool(size_t threads) : stop(false)
{
	for(size_t i = 0; i<threads; ++i)
	{
		workers.emplace_back([this] 
		{
			while(true)
			{
				std::function<void()> task;
				
				{
					std::unique_lock<std::mutex> lock(this->mutex);
					this->condition.wait(lock, [this]() { 
						return this->stop || !this->tasks.empty();
					});
					if(this->stop && this->tasks.empty()) return;
					task = std::move(this->tasks.front());
					this->tasks.pop();
				}
				
				task();
			}
		});
	}
}

inline ThreadPool::~ThreadPool(void)
{
	join();
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared< std::packaged_task<type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	std::future<type> result = task->get_future();
	
	{
		std::unique_lock<std::mutex> lock(mutex);
		if(this->stop) throw std::runtime_error("enqueue on stopped ThreadPool");
		tasks.emplace([task]()
		{
			(*task)();
		});
	}
	
	condition.notify_one();
	return result;
}

inline void ThreadPool::join(void)
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		stop = true;
	}
	
	condition.notify_all();
	
	for(std::thread &w: workers)
		if(w.joinable())
			w.join();
	
	{
		std::unique_lock<std::mutex> lock(mutex);
		workers.clear();
	}
}

}

#endif
