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
#include "store.h"
#include "stripedfile.h"

namespace arc
{

Request::Request(const String &target, bool data) :
		mId(0),				// 0 = invalid id
		mPendingCount(0),
		mResponseSender(NULL)
{
	setTarget(target, data);
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

void Request::setContentSink(ByteStream *bs)
{
	mContentSink = bs;
}

void Request::setTarget(const String &target, bool data)
{
	mTarget = target;
	mIsData = data;
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

void Request::submit(const Identifier &receiver)
{
	if(!mId)
	{
		mReceiver = receiver;
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

bool Request::execute(void)
{
	StringMap info;
	if(!Store::Instance->info(mTarget, info))
	{
		addResponse(new Response("KO"));
		return false;
	}

	ByteStream *bs = NULL;
	if(mIsData)
	{
		StringMap parameters = mParameters;

		if(parameters.contains("Stripe"))
		{
			size_t blockSize;
			int nbStripe, stripe;

			parameters["BlockSize"] >> blockSize;
			parameters["StripesCount"] >> nbStripe;
			parameters["Stripe"] >> stripe;

			File *file = Store::Instance->get(mTarget);
			if(file)
			{
				StripedFile *stripedFile = new StripedFile(file, blockSize, nbStripe, stripe);
				// TODO: seeking
				bs = stripedFile;
			}
		}
		else {
			File *file = Store::Instance->get(mTarget);
			// TODO: seeking
			bs = file;
		}
	}

	Response *response = new Response("OK", info, bs);
	if(response->content()) response->content()->close();	// no more content
	addResponse(response);
	return true;
}

void Request::addPending(void)
{
	mPendingCount++;
}

void Request::removePending(void)
{
	Assert(mPendingCount != 0);
	mPendingCount--;
	if(mPendingCount == 0) notifyAll();
}

int Request::responsesCount(void) const
{
	return mResponses.size();
}

int Request::addResponse(Response *response)
{
	Assert(response != NULL);
	mResponses.push_back(response);
	if(mResponseSender) mResponseSender->notify();
	return mResponses.size()-1;
}

Request::Response *Request::response(int num)
{
	return mResponses.at(num);
}

Request::Response::Response(const String &status) :
	mStatus(status),
	mContent(NULL),
	mIsSent(false)
{

}

Request::Response::Response(const String &status, const StringMap &parameters, ByteStream *content) :
	mStatus(status),
	mParameters(parameters),
	mIsSent(false)
{
	if(content) mContent = new Pipe(content);
	else mContent = NULL;
}

Request::Response::~Response(void)
{
	delete mContent;
}

const Identifier &Request::Response::peer(void) const
{
	return mPeer;
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
