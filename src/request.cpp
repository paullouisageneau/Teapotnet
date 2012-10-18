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

#include "request.h"
#include "core.h"
#include "store.h"
#include "stripedfile.h"
#include "yamlserializer.h"

namespace tpot
{

Request::Request(const String &target, bool data) :
		mId(0),				// 0 = invalid id
		mPendingCount(0),
		mResponseSender(NULL),
		mContentSink(NULL)
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

bool Request::execute(Store *store)
{
	Synchronize(this);  
	
	StringMap parameters = mParameters;

	bool success = false;
	Store::Entry entry;
	if(mTarget.contains('/'))
	{
		if(store) success = store->queryEntry(mTarget, entry);
	}
	else {
	  try {
	    	Identifier identifier;
		mTarget >> identifier;
	  	if(store) success = store->queryResource(identifier, entry);
		else success = Store::GetResource(identifier, entry);
	  }
	  catch(const Exception &e) {}
	}
	
	if(!success)
	{
		addResponse(new Response(Response::NotFound));
		Log("Request", "Target not Found");
		return false;
	}

	StringMap rparameters;
	rparameters["name"] << entry.name;
	rparameters["size"] << entry.size;
	rparameters["time"] << entry.time;
	if(entry.type) rparameters["type"] = "file";
	else rparameters["type"] = "directory";
	
	if(mIsData)
	{
		if(entry.type)	// file
		{
			ByteStream *content = NULL;
			if(!parameters.contains("Stripe")) content = new File(entry.path);
			else {
				size_t blockSize;
				int stripesCount, stripe;

				parameters["block-size"] >> blockSize;
				parameters["stripes-count"] >> stripesCount;
				parameters["stripe"] >> stripe;

				Assert(blockSize > 0);
				Assert(stripesCount > 0);
				
				File *file = NULL;
				StripedFile *stripedFile = NULL;
				
				try {
					file = new File(entry.path);
					stripedFile = new StripedFile(file, blockSize, stripesCount, stripe);
					
					size_t block = 0;
					size_t offset = 0;
					if(parameters.contains("block")) parameters["block"] >> block;
					if(parameters.contains("offset")) parameters["offset"] >> offset;
					stripedFile->seekRead(block, offset);
					
					content = stripedFile;
					entry.size/= stripesCount;
				}
				catch(...)
				{
					delete file;
					delete stripedFile;
					throw;
				}
			}
			
			Response *response = new Response(Response::Success, rparameters, content);
			if(response->content()) response->content()->close();	// no more content
			addResponse(response);
		}
		else {	// directory
		 
			Response *response = new Response(Response::Success, rparameters, new ByteString);
			addResponse(response);
		  
			List<Store::Entry> list;
			if(store->queryList(mTarget, list))
			{
				YamlSerializer serializer(response->content());
				 
				for(List<Store::Entry>::iterator it = list.begin();
					it != list.end();
					++it)
				{
					const Store::Entry &entry = *it;
					StringMap map;
					map["name"] = entry.name;
					map["size"] << entry.size;
					map["time"] << entry.time;
					if(entry.type) map["type"] = "directory";
					else map["type"] = "files";
					serializer.output(map);
				}
			}
			
			response->content()->close();	// no more content
		}
	}
	else {
		Response *response = new Response(Response::Success, rparameters, NULL);
		addResponse(response);
	}

	Log("Request", "Finished execution");
	return true;
}

const Identifier &Request::receiver(void) const
{
	return mReceiver; 
}

bool Request::isPending() const
{
	return (mPendingCount == 0);
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

Request::Response::Response(int status) :
	mStatus(status),
	mContent(NULL),
	mIsSent(false),
	mPendingCount(0)
{
	Assert(status >= 0);
}

Request::Response::Response(int status, const StringMap &parameters, ByteStream *content) :
	mStatus(status),
	mParameters(parameters),
	mContent(NULL),
	mIsSent(false),
	mPendingCount(0)
{
	Assert(status >= 0);
	if(content) mContent = new Pipe(content);
	else mContent = NULL;
}

Request::Response::~Response(void)
{
	delete mContent;
}

const Identifier &Request::Response::peering(void) const
{
	return mPeering;
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

int Request::Response::status(void) const
{
 	return mStatus;
}

bool Request::Response::error(void) const
{
	return status() > 0; 
}

bool Request::Response::finished(void) const
{
	return status() < 0; 
}

}
