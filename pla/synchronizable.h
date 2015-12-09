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

#ifndef PLA_SYNCHRONIZABLE_H
#define PLA_SYNCHRONIZABLE_H

#include "pla/mutex.h"
#include "pla/signal.h"

namespace pla
{

class Synchronizable
{
public:
	Synchronizable(void);
	virtual ~Synchronizable(void);

	void lock(int count = 1) const;
	void unlock(void) const;
	int  unlockAll(void) const;

	void notify(void) const;
	void notifyAll(void) const;
	void wait(void) const;
	bool wait(double &timeout) const;
	bool wait(const double &timeout) const;

	operator Mutex&(void) const;
	
private:
	mutable Mutex mMutex;
	mutable Signal mSignal;
};

class Synchronizer
{
public:
	inline Synchronizer(const Synchronizable *_s) : s(_s)  { if(s) s->lock(); }
	inline Synchronizer(const Synchronizable &_s) : s(&_s) { s->lock(); }
	inline ~Synchronizer(void) { if(s) s->unlock(); }

private:
	const Synchronizable *s;
};

class Desynchronizer
{
public:
	inline Desynchronizer(const Synchronizable *_s) : s(_s),  c(0) { if(s) c = s->unlockAll(); }
	inline Desynchronizer(const Synchronizable &_s) : s(&_s), c(0) { c = s->unlockAll(); }
	inline ~Desynchronizer(void) { if(s) s->lock(c); }

private:
	const Synchronizable *s;
	int c;
};

inline bool boolLock(Synchronizable *s, bool b)
{
	s->lock();
	return b;
}

inline bool boolLock(Synchronizable &s, bool b)
{
	s.lock();
	return b;
}

inline bool boolUnlock(Synchronizable *s, bool b)
{
	s->unlock();
	return b;
}

inline bool boolUnlock(Synchronizable &s, bool b)
{
	s.unlock();
	return b;
}



#define Synchronize(x)   Synchronizer	__sync(x); 
#define Desynchronize(x) Desynchronizer	__desync(x)
#define Unprioritize(x)  {int __c = (x)->unlockAll(); pla::sleep(0.01); (x)->lock(__c);}
#define SyncYield(x)  {int __c = (x)->unlockAll(); yield(); (x)->lock(__c);}
#define SynchronizeTest(x,test) (boolLock(x,true) && boolUnlock(x,(test)))
#define SynchronizeStatement(x,stmt) { Synchronizer __sync(x); stmt; }
#define DesynchronizeStatement(x,stmt) { Desynchronizer __desync(x); stmt; } 

}

#endif
