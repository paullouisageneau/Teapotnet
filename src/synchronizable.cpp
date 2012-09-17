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

#include "synchronizable.h"

namespace arc
{

Synchronizable::Synchronizable(void) :
	mMutex(new Mutex),
	mSignal(new Signal)
{

}

Synchronizable::~Synchronizable(void)
{
	delete mMutex;
	delete mSignal;
}

void Synchronizable::lock(void) const
{
	mMutex->lock();
}

void Synchronizable::unlock(void) const
{
	mMutex->unlock();
}

void Synchronizable::notify(void) const
{
	mSignal->launch();
}

void Synchronizable::notifyAll(void) const
{
	mSignal->launchAll();
}

void Synchronizable::wait(void) const
{
	mSignal->wait(*mMutex);
}

void Synchronizable::wait(time_t timeout) const
{
	mSignal->wait(*mMutex, timeout);
}

}
