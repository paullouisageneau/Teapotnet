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

#ifndef TPN_RUNNER_H
#define TPN_RUNNER_H

#include "tpn/include.h"
#include "tpn/thread.h"
#include "tpn/synchronizable.h"
#include "tpn/list.h"

namespace tpn
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
