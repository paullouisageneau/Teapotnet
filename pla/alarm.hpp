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

#ifndef PLA_ALARM_H
#define PLA_ALARM_H

#include "pla/scheduler.hpp"

#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace pla
{

class Alarm
{
public:
	using clock = std::chrono::steady_clock;
	typedef std::chrono::duration<double> duration;
	typedef std::chrono::time_point<clock, duration> time_point;

	Alarm(void);
	template<class F, class... Args> Alarm(F&& f, Args&&... args);
	~Alarm(void);

	template<class F, class... Args>
	auto set(F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	template<class F, class... Args>
	auto schedule(time_point time, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	template<class F, class... Args>
	auto schedule(duration d, F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	void schedule(time_point time);
	void schedule(duration d);

	void cancel(void);
	void join(void);

	bool isScheduled(void) const;

private:
	static Scheduler scheduler;

	std::function<void()> function;
	Scheduler::task_id taskid;
	bool joining;
};

inline Alarm::Alarm(void) : joining(false)
{

}

inline Alarm::~Alarm(void)
{
	cancel();
	// No join to allow alarm autodeletion
}

template<class F, class... Args>
Alarm::Alarm(F&& f, Args&&... args) : Alarm()
{
	set(std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F, class... Args>
auto Alarm::set(F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	std::future<type> result = task->get_future();

	function = [task]()
	{
		(*task)();
		task->reset();
	};

	return result;
}

template<class F, class... Args>
auto Alarm::schedule(time_point time, F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	using type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	std::future<type> result = task->get_future();

	if(joining) throw std::runtime_error("schedule on closing Alarm");
	scheduler.schedule(taskid, time, function = [task]()
	{
		(*task)();
		task->reset();
	});

	return result;
}

template<class F, class... Args>
auto Alarm::schedule(duration d, F&& f, Args&&... args)
	-> std::future<typename std::result_of<F(Args...)>::type>
{
	return schedule(clock::now() + d, std::forward<F>(f), std::forward<Args>(args)...);
}

inline void Alarm::schedule(time_point time)
{
	if(joining) throw std::runtime_error("schedule on closing Alarm");
	scheduler.schedule(taskid, time, function);
}

inline void Alarm::schedule(duration d)
{
	schedule(clock::now() + d);
}

inline void Alarm::cancel(void)
{
	scheduler.cancel(taskid);
}

inline void Alarm::join(void)
{
	joining = true;
	scheduler.wait(taskid);
}

inline bool Alarm::isScheduled(void) const
{
	return scheduler.isScheduled(taskid);
}

}

#endif
