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

#include "pla/synchronizable.h"

namespace pla
{

Synchronizable::Synchronizable(void)
{

}

Synchronizable::~Synchronizable(void)
{

}

void Synchronizable::lock(int count) const
{
	mMutex.lock(count);
}

void Synchronizable::unlock(void) const
{
	mMutex.unlock();
}

int Synchronizable::unlockAll(void) const
{
	return mMutex.unlockAll();
}

void Synchronizable::notify(void) const
{
	mSignal.launch();
}

void Synchronizable::notifyAll(void) const
{
	mSignal.launchAll();
}

void Synchronizable::wait(void) const
{
	mSignal.wait(mMutex);
}

bool Synchronizable::wait(double &timeout) const
{
	return mSignal.wait(mMutex, timeout);
}

bool Synchronizable::wait(const double &timeout) const
{
	return mSignal.wait(mMutex, timeout);
}

Synchronizable::operator Mutex&(void) const
{
	return mMutex;
}

}
