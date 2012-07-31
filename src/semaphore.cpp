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

#include "semaphore.h"

namespace arc
{

Semaphore::Semaphore(int value) :
		mValue(value)
{

}

Semaphore::~Semaphore(void)
{

}

void Semaphore::acquire(void)
{
	mMutex.lock();

	while(true)
	{
		if(mValue > 0)
		{
			mValue--;
			mMutex.unlock();
			return;
		}

		mSignal.wait(mMutex);
	}
}

bool Semaphore::tryAcquire(void)
{
	mMutex.lock();
	if(mValue > 0)
	{
		mValue--;
		mMutex.unlock();
		return true;
	}

	mMutex.unlock();
	return false;
}

void Semaphore::release(void)
{
	mMutex.lock();
	mValue++;
	mMutex.unlock();
	mSignal.launch();
}

}
