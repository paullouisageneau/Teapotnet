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
#include "user.h"
#include "addressbook.h"
#include "store.h"
#include "stripedfile.h"
#include "yamlserializer.h"

namespace tpot
{

Request::Request(const String &target, bool data) :
		mId(0),				// 0 = invalid id
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

bool Request::execute(User *user)
{
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
		else command == "digest";
		argument = mTarget; 
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
				Log("Request::execute", "Got request for peer " + identifier.toString());
				
				if(!mParameters.contains("adresses"))
					throw Exception("Missing addresses in peer request");
				
				AddressBook::Contact *contact = addressBook->getContact(identifier);
				const String &instance = identifier.getName();
				if(contact && !contact->isConnected(instance))
				{
					List<String> list;
					mParameters.get("adresses").explode(list, ',');
					
					if(mParameters.contains("port") && !mRemoteAddr.isNull()) 
						list.push_back(mRemoteAddr.host() + ':' + mParameters.get("port"));
					
					for(	List<String>::iterator it = list.begin();
						it != list.end();
						++it)
					try {
						Address addr(*it);
						Socket *sock = new Socket(addr, 1000);	// TODO: timeout
						Core::Instance->addPeer(sock, identifier, true);	// async					
						Log("Request::execute", "Socket connected to peer");						
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
				Store::Entry entry;
				if(Store::GetResource(identifier, entry))
				{
					addResponse(createResponse(entry, parameters, store));
					return true;
				}
			}
		}
	}
	else if(command == "file")
	{
		List<Store::Entry> list;
		if(store->queryList(Store::Query(argument), list))
		{
			for(List<Store::Entry>::iterator it = list.begin();
				it != list.end();
				++it)
			{
				addResponse(createResponse(*it, parameters, store));
			}
			
			return true;
		}
	}
	else if(command == "search")
	{
		Store::Query query;
		query.setMatch(argument);
		
		List<Store::Entry> list;
		if(store->queryList(query, list))
		{		
			for(List<Store::Entry>::iterator it = list.begin();
				it != list.end();
				++it)
			{
				addResponse(createResponse(*it, parameters, store));
			}
			
			return true;
		}
	}
	
	addResponse(new Response(Response::NotFound));
	Log("Request", "Target not Found");
	return false;
}

Request::Response *Request::createResponse(Store::Entry &entry, const StringMap &parameters, Store *store)
{
	StringMap rparameters;
	rparameters["name"] << entry.name;
	rparameters["size"] << entry.size;
	rparameters["time"] << entry.time;
	if(!entry.url.empty()) rparameters["path"] = entry.url;	// Warning: path in parameters is url in store
	if(!entry.type) rparameters["type"] = "directory";
	else {
		rparameters["type"] = "file";
		rparameters["hash"] << entry.digest;
	}
		  
	if(!mIsData) return new Response(Response::Success, rparameters, NULL);
		
	if(!entry.type)	// directory
	{
		rparameters["processing"] = "none";
		rparameters["formatting"] = "YAML";
		
		Response *response = new Response(Response::Success, rparameters, new ByteString);
		
		Assert(store);
		Assert(response->content());
		
		// The trailing '/' means it's a directory listing
		String url = entry.url;
		if(url.empty() || url[url.size()-1] != '/') url+= '/';
		
		List<Store::Entry> list;
		if(store->queryList(Store::Query(url), list))
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
				if(!entry.type) map["type"] = "directory";
				else {
					map["type"] = "file";
					map["hash"] << entry.digest;
				}
				serializer.output(map);
			}
		}
				
		response->content()->close();	// no more content
		return response;
	}
	
	ByteStream *content = NULL;
	if(!parameters.contains("stripe")) 
	{
		content = new File(entry.path);
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
			
		File *file = NULL;
		StripedFile *stripedFile = NULL;
					
		try {
			file = new File(entry.path);
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
			delete file;
			delete stripedFile;
			throw;
		}
	}
	
	Assert(content);
	Response *response = new Response(Response::Success, rparameters, content);
	if(response->content()) response->content()->close();	// no more content
	return response;	
	
}

const Identifier &Request::receiver(void) const
{
	return mReceiver; 
}

bool Request::isPending() const
{
	return !mPending.empty();
}

void Request::addPending(const Identifier &peering)
{
	mPending.insert(peering);
}

void Request::removePending(const Identifier &peering)
{
	Set<Identifier>::iterator it = mPending.find(peering);
	if(it != mPending.end())
	{
		mPending.erase(it);
		if(mPending.empty()) notifyAll();
	}
}

int Request::responsesCount(void) const
{
	return mResponses.size();
}

int Request::addResponse(Response *response)
{
	Assert(response != NULL);
	//Assert(!mResponses.contains(response));
	
	mResponses.push_back(response);
	if(mResponseSender) mResponseSender->notify();
	return mResponses.size()-1;
}

Request::Response *Request::response(int num)
{
	return mResponses.at(num);
}

const Request::Response *Request::response(int num) const
{
	return mResponses.at(num);
}

bool Request::isSuccessful(void) const
{
	for(int i=0; i<responsesCount(); ++i)
		if(!response(i)->error())
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

Request::Response::Response(int status, const StringMap &parameters, ByteStream *content) :
	mStatus(status),
	mParameters(parameters),
	mContent(NULL),
	mTransfertStarted(false),
	mTransfertFinished(false)
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
	return (status() > 0) && (status() != Pending); 
}

bool Request::Response::finished(void) const
{
	return status() < 0; 
}

}
