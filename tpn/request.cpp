/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/request.h"
#include "tpn/core.h"
#include "tpn/user.h"
#include "tpn/addressbook.h"
#include "tpn/store.h"
#include "tpn/stripedfile.h"
#include "tpn/yamlserializer.h"
#include "tpn/scheduler.h"
#include "tpn/config.h"

namespace tpn
{

const int Request::Response::Finished = -1;
const int Request::Response::Success = 0;
const int Request::Response::Pending = 1;
const int Request::Response::Failed = 2;
const int Request::Response::NotFound = 3;
const int Request::Response::Empty = 4;
const int Request::Response::AlreadyResponded = 5;
const int Request::Response::Interrupted = 6;
const int Request::Response::ReadFailed = 7;

Request::Request(const String &target, bool data) :
		mIsData(false),
		mIsForwardable(true),		// forwardable by default
		mContentSink(NULL),
		mResponseSender(NULL),
		mId(0),				// 0 = invalid id
		mRemoteId(0),
		mCancelTask(this)
{
	setTarget(target, data);
}

Request::~Request(void)
{
	Scheduler::Global->remove(&mCancelTask);
	cancel();

	if(!hasContent())
		delete mContentSink;
	
	for(int i=0; i<mResponses.size(); ++i)
		delete mResponses[i];
}

unsigned Request::id(void) const
{
	Synchronize(this);
	return (mRemoteId ? mRemoteId : mId);
}

String Request::target(void) const
{
	Synchronize(this);
	return mTarget;
}

void Request::setContentSink(ByteStream *bs)
{
	Synchronize(this);
	mContentSink = bs;
}

void Request::setTarget(const String &target, bool data)
{
	Synchronize(this);
	mTarget = target;
	mIsData = data;
}

void Request::setParameters(StringMap &params)
{
	Synchronize(this);
	mParameters = params;
}

void Request::setParameter(const String &name, const String &value)
{
	Synchronize(this);
	mParameters.insert(name, value);
}

void Request::setNonReceiver(const Identifier &nonreceiver)
{
	Synchronize(this);
	mNonReceiver = nonreceiver;
}

void Request::setForwardable(bool forwardable)
{
	Synchronize(this);
	mIsForwardable = forwardable;
}

void Request::submit(double timeout)
{
	Synchronize(this);
	
	if(!mId) 
	{
		Desynchronize(this);
		Core::Instance->addRequest(this);	// mId set by Core
	}

	if(timeout > 0.) 
		Scheduler::Global->schedule(&mCancelTask, timeout);  
}

void Request::submit(const Identifier &receiver, double timeout)
{
	Synchronize(this);
	if(!mId)
	{
		mReceiver = receiver;
		Desynchronize(this);
		Core::Instance->addRequest(this);	// mId set by Core
	}

	if(timeout > 0.)
                Scheduler::Global->schedule(&mCancelTask, timeout);
}

void Request::cancel(void)
{
	Synchronize(this);
	if(mId)
	{
		Desynchronize(this);
		Core::Instance->removeRequest(mId);
	}
}

bool Request::forward(const Identifier &receiver, const Identifier &source)
{
	Synchronize(this);
	
	if(!mIsForwardable) return false;

	int hops = 1;
	if(mParameters.contains("hops"))
		hops = mParameters.get("hops").toInt();
	
	// Zeros means unlimited
	if(hops == 1) return false;
	
	double timeout = milliseconds(Config::Get("request_timeout").toInt());
	if(mParameters.contains("timeout"))
		timeout = mParameters.get("timeout").toDouble();
	timeout*= 0.5;	// TODO
	
	LogDebug("Request::forward", "Forwarding request " + String::number(mRemoteId));
	
	mParameters["access"] = "public";
	mParameters["hops"] = String::number((hops == 0 ? 0 : hops-1));
	mParameters["timeout"] = String::number(timeout);
	setNonReceiver(source);	// TODO: and we shouldn't send to self
	
	try {
		submit(receiver);
		wait(timeout);
	}
	catch(const Exception &e) 
	{
		LogWarn("Request::forward", e.what());
		return false;
	}
	
	return true;
}

bool Request::execute(User *user, bool isFromSelf)
{
	const String instanceName = Core::Instance->getName();
	
	Assert(user);
	Synchronize(this);

	AddressBook *addressBook = user->addressBook();
	Store *store = user->store();
	
	StringMap parameters = mParameters;
	String command;
	String argument;
	int pos = mTarget.find(':');
	if(pos != String::NotFound)
	{
		command  = mTarget.substr(0,pos);
		argument = mTarget.substr(pos+1);
	}
	else {
		if(mTarget.contains('/')) command  = "file";
		else command = "digest";
		argument = mTarget;
	}

	if(mParameters.contains("instance") && mParameters["instance"] != instanceName)
	{
		addResponse(new Response(Response::NotFound));
		return false;
	}
	
	if(command.empty() || command == "digest" || command == "peer")
	{
		Identifier identifier;
		try { argument >> identifier; }
		catch(const Exception &e) { identifier.clear(); }
	
		if(!identifier.empty())
		{
			if(command == "peer")
			{
				LogDebug("Request::execute", "Got request for peer (" + identifier.getName()+")");
				
				if(!mParameters.contains("adresses"))
					throw Exception("Missing addresses in peer request");
				
				AddressBook::Contact *contact = addressBook->getContact(identifier);
				const String &instance = identifier.getName();
				if(contact && !contact->isConnected(instance))
				{
					setForwardable(false);

					List<String> list;
					mParameters.get("adresses").explode(list, ',');
					
					if(mParameters.contains("port") && !mRemoteAddr.isNull()) 
						list.push_back(mRemoteAddr.host() + ':' + mParameters.get("port"));
					
					for(	List<String>::iterator it = list.begin();
						it != list.end();
						++it)
					try {
						Desynchronize(this);
						Address addr(*it);
						Socket *sock = new Socket(addr, 1000);	// TODO: timeout
						Core::Instance->addPeer(sock, identifier, true);	// async					
						LogDebug("Request::execute", "Socket connected to peer");						
						StringMap parameters;
						parameters["remote"] = contact->remotePeering().toString();
						Response *response = new Response(Response::Success, parameters);
						addResponse(response);
						return true;
					}
					catch(...)
					{

					}

					addResponse(new Response(Response::Failed));
					return false;
				}
			}
			else {
				Resource::Query query(store);
				query.setDigest(identifier);
				query.setFromSelf(isFromSelf);
	
				Resource resource;
				if(query.submitLocal(resource))
				{
					setForwardable(false);
					addResponse(createResponse(resource, parameters, store));
					return true;
				}
			}
		}
	}
	else if(command == "file")
	{
		Resource::Query query(store, argument);
		query.setFromSelf(isFromSelf);
		
		Set<Resource> resources;
		if(query.submitLocal(resources))
		{
			if(resources.empty())
			{
				addResponse(new Response(Response::Empty));
			}
			else {
				if(argument.empty() || argument[argument.size()-1] != '/')	// if not a directory listing
					setForwardable(false);

				for(Set<Resource>::iterator it = resources.begin();
					it != resources.end();
					++it)
				{
					addResponse(createResponse(*it, parameters, store));
				}
			}
		
			return true;
		}
	}
	else if(command == "search")
	{
		Resource::Query query(store);
		query.setFromSelf(isFromSelf);

		StringMap tmp(parameters);
		query.deserialize(tmp);
		if(!argument.empty() && argument != "*") query.setMatch(argument);
		
		Set<Resource> resources;
		if(query.submitLocal(resources))
		{
			if(resources.empty())
			{
				addResponse(new Response(Response::Empty));
			}
			else for(Set<Resource>::iterator it = resources.begin();
				it != resources.end();
				++it)
			{
				addResponse(createResponse(*it, parameters, store));
			}
			
			return true;
		}
	}
	
	addResponse(new Response(Response::NotFound));
	LogDebug("Request", "Target not Found");
	return false;
}

bool Request::executeDummy(void)
{
	Synchronize(this);
	addResponse(new Response(Response::NotFound));
	return false;
}

Request::Response *Request::createResponse(const Resource &resource, const StringMap &parameters, Store *store)
{
	StringMap rparameters;
	resource.serialize(rparameters);
	
	ByteStream *content = NULL;
	if(mIsData)
	{
		if(resource.isDirectory())
		{
			rparameters["processing"] = "none";
			rparameters["formatting"] = "YAML";
			
			Response *response = new Response(Response::Success, rparameters, new ByteString);
			
			Assert(store);
			Assert(response->content());
			
			// The trailing '/' means it's a directory listing
			String url = resource.url();
			if(url.empty() || url[url.size()-1] != '/') url+= '/';
		
			Resource::Query query(store, url);	
			SerializableSet<Resource> resources;
			if(query.submitLocal(resources))
			{
				YamlSerializer serializer(response->content());
				serializer.output(resources);
			}
					
			response->content()->close();	// no more content
			return response;
		}
		
		if(!parameters.contains("stripe")) 
		{
			content = resource.accessor();
			resource.dissociateAccessor();
			
			if(parameters.contains("position"))
			{
				int64_t position = 0;
				parameters.get("position").extract(position);
				content->seekRead(position);
				rparameters["position"] << position;
			}
			
			rparameters["processing"] = "none";
		}
		else {
			size_t blockSize;
			int stripesCount, stripe;
			parameters.get("block-size").extract(blockSize);
			parameters.get("stripes-count").extract(stripesCount);
			parameters.get("stripe").extract(stripe);
				
			Assert(blockSize > 0);
			Assert(stripesCount > 0);
			Assert(stripe >= 0);
				
			StripedFile *stripedFile = NULL;
						
			try {
				// TODO: Request should not be Resource's friend
				File *file = new File(resource.mPath, File::Read);
				stripedFile = new StripedFile(file, blockSize, stripesCount, stripe);
				
				size_t block = 0;
				size_t offset = 0;
				if(parameters.contains("block")) parameters.get("block").extract(block);
				if(parameters.contains("offset")) parameters.get("offset").extract(offset);
				stripedFile->seekRead(block, offset);
				
				content = stripedFile;
				
				rparameters["processing"] = "striped";
				//rparameters["size"].clear();
				//rparameters["size"] << TODO;
				rparameters.erase("size");
			}
			catch(...)
			{
				delete stripedFile;
				throw;
			}
		}
		
		Assert(content);
	}
	
	Response *response = new Response(Response::Success, rparameters, content, true);	// content is read only
	return response;	
}

Identifier Request::receiver(void) const
{
	Synchronize(this);
	return mReceiver; 
}

bool Request::isPending() const
{
	Synchronize(this);	
	return !mPending.empty();
}

void Request::addPending(const Identifier &peering)
{
	Synchronize(this);
	mPending.insert(peering);
}

void Request::removePending(const Identifier &peering)
{
	Synchronize(this);
	Set<Identifier>::iterator it = mPending.find(peering);
	if(it != mPending.end())
	{
		mPending.erase(it);
		if(mPending.empty()) notifyAll();
	}
}

int Request::responsesCount(void) const
{
	Synchronize(this);
	return mResponses.size();
}

int Request::addResponse(Response *response)
{
	Synchronize(this);
	Assert(response != NULL);
	Assert(!mResponses.contains(response));
	
	int hops = 0;
	if(response->mParameters.contains("hops"))
		hops = std::max(0, response->mParameters["hops"].toInt());
	
	if(mId && mRemoteId)	// if forwarded
	{
		// Add only successful responses
		if(response->error())
			return -1;

		// Remove existing negative responses
		for(int i=0; i<mResponses.size();)
		{
			if(mResponses[i]->error()) 
			{
				delete mResponses[i];
				mResponses.erase(i);
			}
			else ++i;
		}
		
		hops+= 1;
	}
	
	response->mParameters["hops"] = String::number(hops);
	
	mResponses.push_back(response);
	if(mResponseSender) mResponseSender->notify();
	return mResponses.size()-1;
}

Request::Response *Request::response(int num)
{
	Synchronize(this);
	return mResponses.at(num);
}

const Request::Response *Request::response(int num) const
{
	Synchronize(this);
	return mResponses.at(num);
}

bool Request::isSuccessful(void) const
{
	Synchronize(this);
  
	for(int i=0; i<responsesCount(); ++i)
		if(!response(i)->error())
			return true;
	
	return false;
}

bool Request::hasContent(void) const
{
	Synchronize(this);
  
	for(int i=0; i<responsesCount(); ++i)
		if(response(i)->content())
			return true;
	
	return false;
}

Request::Response::Response(int status) :
	mStatus(status),
	mContent(NULL),
	mChannel(0),
	mTransfertStarted(false),
	mTransfertFinished(false)
{
	Assert(status >= 0);
}

Request::Response::Response(int status, const StringMap &parameters, ByteStream *content, bool readOnly) :
	mStatus(status),
	mParameters(parameters),
	mContent(NULL),
	mChannel(0),
	mTransfertStarted(false),
	mTransfertFinished(false)
{
	Assert(status >= 0);
	if(content) mContent = new Pipe(content, readOnly);
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

String Request::Response::instance(void) const
{
	if(isLocal()) return Core::Instance->getName();
	else return mPeering.getName();
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

bool Request::Response::isLocal(void) const
{
	return mPeering == Identifier::Null;
}

int Request::Response::status(void) const
{
 	return mStatus;
}

bool Request::Response::error(void) const
{
	return (status() > 0 
		&& status() != Pending
		&& status() != Empty);
}

bool Request::Response::finished(void) const
{
	return status() < 0;
}

Request::CancelTask::CancelTask(Request *request) : 
	mRequest(request)
{
	
}

void Request::CancelTask::run(void) 
{
	if(mRequest->responsesCount()) mRequest->notifyAll(); 
	else mRequest->cancel();
}

}
