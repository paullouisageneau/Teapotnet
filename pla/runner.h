/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef PLA_RUNNER_H
#define PLA_RUNNER_H

#include "pla/include.h"
#include "pla/thread.h"
#include "pla/synchronizable.h"
#include "pla/list.h"

namespace pla
{

class Runner : protected Thread, protected Synchronizable
{
public:
	Runner(void);
	~Runner(void);
	
	void schedule(Task *task);
	void cancel(Task *task);
	void clear(void);
	
private:
	void run(void);
	
	List<Task*> mTasks;
};

}

#endif
