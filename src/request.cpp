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

#include "request.h"
#include "core.h"

namespace arc
{

Request::Request(void) :
		mId(0),		// 0 = invalid id
		mPendingCount(0)
{

}

Request::~Request(void)
{
	cancel();
}

unsigned Request::id(void) const
{
	return mId;
}

const Identifier &Request::target(void) const
{
	return mTarget;
}

void Request::submit(void)
{
	if(!mId)
	{
		mId = Core::Instance->addRequest(this);
	}
}

void Request::cancel(void)
{
	if(mId)
	{
		Core::Instance->removeRequest(mId);
	}
}

void Request::addPending(void)
{
	lock();
	mPendingCount++;
	unlock();
}

void Request::removePending(void)
{
	lock();
	Assert(mPendingCount != 0);
	mPendingCount--;
	unlock();
}

}
