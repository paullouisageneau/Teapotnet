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

#ifndef PLA_SEMAPHORE_H
#define PLA_SEMAPHORE_H

#include "pla/include.h"
#include "pla/exception.h"
#include "pla/mutex.h"
#include "pla/signal.h"

namespace pla
{

class Semaphore
{
public:
	Semaphore(int value = 1);
	virtual ~Semaphore(void);

	void acquire(void);
	bool tryAcquire(void);
	void release(void);

private:
	Mutex mMutex;
	Signal mSignal;
	int mValue;
};

}

#endif
