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

Request::Request(const String &target) :
		mId(0),				// 0 = invalid id
		mPendingCount(0)
{
	setTarget(target);
}

Request::~Request(void)
{
	cancel();

	for(int i=0; i<mResponses.size(); ++i)
		delete mResponses[i];
}

unsigned Request::id(void) const
{
	return mId;
}

const String &Request::target(void) const
{
	return mTarget;
}

bool Request::isPending() const
{
	return (mPendingCount != 0);
}

void Request::setLocal(void)
{

}

void Request::setBroadcast(void)
{

}

void Request::setMulticast(const Identifier &group)
{

}

void Request::setUnicast(const Identifier &destination)
{

}

void Request::setContentSink(ByteStream *bs)
{
	mContentSink = bs;
}

void Request::setTarget(const String &target)
{
	mTarget = target;
}

void Request::setParameters(StringMap &params)
{
	mParameters = params;
}

void Request::setParameter(const String &name, const String &value)
{
	mParameters.insert(name, value);
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

Request::Response::Response(const String &status, const StringMap &parameters, ByteStream *sink) :
	mStatus(status),
	mParameters(parameters)
{
	mContent = new Pipe(sink);
}

Request::Response::~Response(void)
{
	delete mContent;
}

const String &Request::Response::status(void) const
{
	return mStatus;
}

const StringMap &Request::Response::parameters(void) const
{
	return mParameters;
}

String Request::Response::parameter(const String &name) const
{
	String value;
	if(mParameters.contains(name)) return value;
	else return "";
}

bool Request::Response::parameter(const String &name, String &value) const
{
	return mParameters.get(name,value);
}

Pipe *Request::Response::content(void) const
{
	return mContent;
}

}
