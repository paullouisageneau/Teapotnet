/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPOT_SIGNAL_H
#define TPOT_SIGNAL_H

#include "include.h"
#include "exception.h"
#include "mutex.h"

namespace tpot
{

class Signal
{
public:
	Signal(void);
	virtual ~Signal(void);

	void launch(void);
	void launchAll(void);
	void wait(Mutex &mutex);
	bool wait(Mutex &mutex, unsigned &timeout);
	bool wait(Mutex &mutex, const unsigned &timeout);
	
private:
	pthread_cond_t mCond;
};

}

#endif
