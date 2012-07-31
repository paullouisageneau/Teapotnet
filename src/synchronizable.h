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

#ifndef ARC_SYNCHRONIZABLE_H
#define ARC_SYNCHRONIZABLE_H

#include "mutex.h"
#include "signal.h"

namespace arc
{

class Synchronizable
{
public:
	Synchronizable(void);
	virtual ~Synchronizable(void);

	void lock(void) const;
	void unlock(void) const;

	void notify(void) const;
	void notifyAll(void) const;
	void wait(void) const;
	void wait(double timeout) const;

private:
	Mutex *mMutex;		// Pointers are used here to keep functions const
	Signal *mSignal;
};

class Synchronizer
{
public:
	inline Synchronizer(const Synchronizable *_s) : s(_s) { s->lock(); }
	inline ~Synchronizer(void) { s->unlock(); }

private:
	const Synchronizable *s;
};

#define synchronize(x) 	Synchronizer __sync(x)

}

#endif
