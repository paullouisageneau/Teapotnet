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

#ifndef PLA_TASK_H
#define PLA_TASK_H

#include "pla/include.h"

namespace pla
{

class Task
{
public:
	Task(void) {}
	virtual ~Task(void) {}
	
	virtual void run(void) = 0;
	
	// TODO: autodeletion flag should be defined here so scheduler can properly delete tasks on destruction
};

template<class T> class AutoDeleteTask : public Task
{
public:
	AutoDeleteTask(T *target = NULL, bool autoDelete = false) : mTarget(target), mAutoDelete(autoDelete) {}
	virtual ~AutoDeleteTask(void) {}
	
	void run(void)
	{
		delete mTarget;
		if(mAutoDelete)
			delete this;
	}

private:
	T *mTarget;
	bool mAutoDelete;
};

}

#endif

