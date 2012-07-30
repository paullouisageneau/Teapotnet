/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef ARC_THREAD_H
#define ARC_THREAD_H

#include "include.h"
#include "synchronizable.h"

namespace arc
{

class Thread: virtual public Synchronizable
{
public:
	Thread(void);
	virtual ~Thread(void);

	void start(void);
	void waitTermination(void) const;
	void terminate(void);
	bool isRunning(void);

	virtual void run(void) = 0;

protected:
	void sleep(unsigned time);

private:
	static void *ThreadEntry(void *arg);
	friend void *ThreadEntry(void *arg);

	pthread_t 	mThread;
	bool		mIsRunning;
};

}

#endif

