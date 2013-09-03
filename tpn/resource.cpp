/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/resource.h"
#include "tpn/config.h"
#include "tpn/sha512.h"
#include "tpn/store.h"
#include "tpn/file.h"
#include "tpn/request.h"
#include "tpn/splicer.h"
#include "tpn/directory.h"
#include "tpn/pipe.h"
#include "tpn/thread.h"

namespace tpn
{

Resource::Resource(const Identifier &peering, const String &url, Store *store) :
		mUrl(url),
	mTime(0),
	mSize(0),
	mType(1),
	mPeering(peering),
	mStore(Store::GlobalInstance),
	mAccessor(NULL)
{
	if(store) mStore = store;
}

Resource::Resource(const ByteString &digest, Store *store) :
	mDigest(digest),
	mTime(0),
	mSize(0),
	mType(1),
	mStore(Store::GlobalInstance),
	mAccessor(NULL)
{
	if(store) mStore = store;
}

Resource::Resource(Store *store) :
	mTime(0),
	mSize(0),
	mType(1),
	mStore(Store::GlobalInstance),
	mAccessor(NULL)
{
	if(store) mStore = store;
}

Resource::~Resource(void)
{
	delete mAccessor;
}

void Resource::clear(void)
{
	mDigest.clear();
	mUrl.clear();
	mPath.clear();
	mTime = Time::Now();
	mSize = 0;
	mType = 1;
	mPath.clear();
	mPeering = Identifier::Null;
	
	if(mAccessor)
	{
		delete mAccessor;
		mAccessor = NULL;
	}
}

void Resource::refresh(bool forceLocal)
{
	mSources.clear();

	Query query;
	createQuery(query);
	Set<Resource> result;
	query.submit(result, mStore, mPeering, forceLocal);

	if(result.empty())
		throw Exception("Resource not found");
	
	for(Set<Resource>::iterator it = result.begin(); it != result.end(); ++it)
	{
		// Merge resources
		merge(*it);
		
		// Add peering as source if remote
		Identifier peering = it->peering();
		if(peering != Identifier::Null)
			mSources.insert(peering);
	}
	
	// Hints for the splicer system
	if(!mDigest.empty())
		Splicer::Hint(mDigest, mSources, mSize);
}

ByteString Resource::digest(void) const
{
	return mDigest;
}

Time Resource::time(void) const
{
	return mTime;
}

int64_t Resource::size(void) const
{
	return mSize;
}
	
int Resource::type(void) const
{
	return mType;
}

Identifier Resource::peering(void) const
{
	return mPeering;
}

String Resource::url(void) const
{
	return mUrl;
}

String Resource::name(void) const
{
	if(!mUrl.empty()) return mUrl.afterLast('/');
	if(!mPath.empty()) return mPath.afterLast(Directory::Separator);
	else return mDigest.toString();
}

bool Resource::isDirectory(void) const
{
	return (mType == 0);
}

Resource::Accessor *Resource::accessor(void) const
{
	if(isDirectory()) return NULL;
	
	if(!mAccessor)
	{
		Query query;
		createQuery(query);
		Resource dummy;		// TODO: rather stupid
		if(query.submitLocal(dummy, mStore))
		{
			Assert(!dummy.mPath.empty());
			mAccessor = new LocalAccessor(dummy.mPath);
		}
		else if(!mDigest.empty())
		{
			mAccessor = new SplicerAccessor(mDigest, mSources);
		}
		else if(!mUrl.empty()) {
			mAccessor = new RemoteAccessor(mPeering, mUrl);
		}
		else throw Exception("Unable to query resource");
	}
	
	return mAccessor;
}

void Resource::dissociateAccessor(void) const
{
	mAccessor = NULL;
}

void Resource::setPeering(const Identifier &peering)
{
	mPeering = peering;
}

void Resource::merge(const Resource &resource)
{
	if(resource.mTime > mTime)
	{
		mTime = resource.mTime;
		mSize = resource.mSize;
		mDigest = resource.mDigest;
		mType = resource.mType;
		
		if(mUrl.empty())
		{
			mUrl = resource.mUrl;
			mPeering = resource.mPeering;
		}
	}
}

void Resource::createQuery(Query &query) const
{
	if(!mDigest.empty()) query.setDigest(mDigest);
	else {
		Assert(!mUrl.empty());
		query.setLocation(mUrl);
	}
}

void Resource::serialize(Serializer &s) const
{
	// TODO: WARNING: This will break if serialized to database (wrong type field and digest is called hash)

	String strType = (mType == 0 ? "directory" : "file");
	String tmpName = name();
	ConstSerializableWrapper<int64_t> sizeWrapper(mSize);
	
	// Note path is not serialized
	Serializer::ConstObjectMapping mapping;
	if(!mDigest.empty()) mapping["hash"] = &mDigest;	// backward compatibility, should be digest
	if(!mUrl.empty()) mapping["url"] = &mUrl;
	mapping["name"] = &tmpName;
	mapping["time"] = &mTime;
	mapping["type"] = &strType;
	mapping["size"] = &sizeWrapper;

	s.outputObject(mapping);
}

bool Resource::deserialize(Serializer &s)
{
	clear();
	
	String strType;
	String tmpName;
	SerializableWrapper<int64_t> sizeWrapper(&mSize);

	// Note path is not deserialized
	Serializer::ObjectMapping mapping;
	mapping["digest"] = &mDigest;
	mapping["hash"] = &mDigest;	// backward compatibility
	mapping["url"] = &mUrl;
	mapping["name"] = &tmpName;
	mapping["time"] = &mTime;
	mapping["type"] = &strType;
	mapping["size"] = &sizeWrapper;
	
	if(!s.inputObject(mapping)) return false;
	
	if(strType.containsLetters())
	{
		if(strType == "directory") mType = 0;
		else mType = 1;
	}
	else {
		// Compatibility with database
		strType.extract(mType);
	}

	if(mUrl.empty()) mUrl = tmpName;
	
	return true;
}

bool Resource::isInlineSerializable(void) const
{
	return false;
}

bool operator <  (const Resource &r1, const Resource &r2)
{
	return r1.name() < r2.name();
}

bool operator >  (const Resource &r1, const Resource &r2)
{
	return r1.name() > r2.name();
}

bool operator == (const Resource &r1, const Resource &r2)
{
	if(r1.name() != r2.name()) return false;
	return r1.digest() == r2.digest();
}

bool operator != (const Resource &r1, const Resource &r2)
{
	return !(r1 == r2);
}

Resource::Query::Query(const String &url) :
	mUrl(url),
	mMinAge(0), mMaxAge(0),
	mOffset(0), mCount(-1)
{
  
}

Resource::Query::~Query(void)
{
	
}

void Resource::Query::setLocation(const String &url)
{
	mUrl = url;
}

void Resource::Query::setDigest(const ByteString &digest)
{
	mDigest = digest;
}

void Resource::Query::setAge(Time min, Time max)
{
	mMinAge = min;
	mMaxAge = max;
}

void Resource::Query::setRange(int first, int last)
{
	mOffset = std::max(first,0);
	mCount  = std::max(last-mOffset,0);
}

void Resource::Query::setLimit(int count)
{
	mCount = count;
}
	  
void Resource::Query::setMatch(const String &match)
{
	mMatch = match;
}

bool Resource::Query::submitLocal(Resource &result, Store *store)
{
	if(!store) store = Store::GlobalInstance;
	
	if(!mDigest.empty())
	{
		if(Store::Get(mDigest, result))
			return true;
		// TODO: if Get was reliable at startup calling query would not be necessary here
	}
	
	return store->query(*this, result);
}

bool Resource::Query::submitLocal(Set<Resource> &result, Store *store)
{
	if(!store) store = Store::GlobalInstance;

	if(!mDigest.empty())
	{
		Resource resource;
		if(Store::Get(mDigest, resource))
		{
			result.insert(resource);
			return true;
		}
		// TODO: if Get was reliable at startup calling query would not be necessary here
	}
	
	return store->query(*this, result);
}

bool Resource::Query::submitRemote(Set<Resource> &result, const Identifier &peering)
{
	const double timeout = milliseconds(Config::Get("request_timeout").toInt());

	Request request;
	createRequest(request);
        request.submit(peering);
	request.wait(timeout);
	
	Synchronize(&request);
	for(int i=0; i<request.responsesCount(); ++i)
	{
		const Request::Response *response = request.response(i);		
                if(response->error()) continue;

		try
		{
			Resource resource;
			StringMap parameters = response->parameters();
			resource.deserialize(parameters);
			resource.setPeering(response->peering());
                        result.insert(resource);
                }
		catch(const Exception &e)
		{
			LogWarn("Resource::Query::submit", String("Dropping invalid response: ") + e.what());
		}
	}

	return (request.responsesCount() != 0);
}

bool Resource::Query::submit(Set<Resource> &result, Store *store, const Identifier &peering, bool forceLocal)
{
	if(!store) store = Store::GlobalInstance;
	
	bool success = false;
	if(forceLocal || peering == Identifier::Null) 
	{
		success = submitLocal(result, store);
		if(!mDigest.empty() && success) return true;
	}
	
	success|= submitRemote(result, peering);
	return success;
}

void Resource::Query::createRequest(Request &request) const
{
	StringMap parameters;
	serialize(parameters);
	
	if(!mDigest.empty()) 
	{
		request.setTarget(mDigest.toString(), false);
		parameters.erase("digest");
	}
	else if(!mUrl.empty()) 
	{
		request.setTarget(mUrl, false);
		parameters.erase("url");
	}
	else {
		if(!mMatch.empty()) request.setTarget("search:"+mMatch, false);
		else request.setTarget("search:*", false);
		parameters.erase("match");
	}
	
	request.setParameters(parameters);
}

void Resource::Query::serialize(Serializer &s) const
{
	ConstSerializableWrapper<int> offsetWrapper(mOffset);
	ConstSerializableWrapper<int> countWrapper(mCount);
	
	Serializer::ConstObjectMapping mapping;
	if(!mUrl.empty())     mapping["url"] = &mUrl;
	if(!mMatch.empty())    mapping["match"] = &mMatch;
	if(!mDigest.empty())  mapping["digest"] = &mDigest;
	if(mMinAge > Time(0)) mapping["minAge"] = &mMinAge;
	if(mMaxAge > Time(0)) mapping["maxAge"] = &mMaxAge;
	if(mOffset > 0)       mapping["offset"] = &offsetWrapper;
	if(mCount > 0)        mapping["count"] = &countWrapper;

	s.outputObject(mapping);
}

bool Resource::Query::deserialize(Serializer &s)
{
	SerializableWrapper<int> offsetWrapper(&mOffset);
	SerializableWrapper<int> countWrapper(&mCount);

	Serializer::ObjectMapping mapping;
	mapping["url"] = &mUrl;
	mapping["match"] = &mMatch;
	mapping["digest"] = &mDigest;
	mapping["minAge"] = &mMinAge;
	mapping["maxAge"] = &mMaxAge;
	mapping["offset"] = &offsetWrapper;
	mapping["count"] = &countWrapper;
	
	return s.inputObject(mapping);
}

bool Resource::Query::isInlineSerializable(void) const
{
	return false;
}

size_t Resource::Accessor::hashData(ByteString &digest, size_t size)
{
	// Default implementation
	if(size >= 0) return Sha512::Hash(*this, size, digest);
	else return Sha512::Hash(*this, digest);
}

Resource::LocalAccessor::LocalAccessor(const String &path)
{
	Assert(!path.empty());
	mFile = new File(path, File::Read);
}

Resource::LocalAccessor::~LocalAccessor(void)
{
	delete mFile;
}

size_t Resource::LocalAccessor::readData(char *buffer, size_t size)
{
	return mFile->readData(buffer, size);
}

void Resource::LocalAccessor::writeData(const char *data, size_t size)
{
	// TODO: check if we have right to modify this resource
	if(mFile->mode() == File::Read)
		mFile->reopen(File::ReadWrite);
	
	mFile->writeData(data, size);	
}
		
void Resource::LocalAccessor::seekRead(int64_t position)
{
	mFile->seekRead(position);
}

void Resource::LocalAccessor::seekWrite(int64_t position)
{
	// TODO: check if we have right to modify this resource
	if(mFile->mode() == File::Read)
		mFile->reopen(File::ReadWrite);
	
	mFile->seekWrite(position);
}

Resource::RemoteAccessor::RemoteAccessor(const Identifier &peering, const String &url) :
	mPeering(peering),
	mUrl(url),
	mPosition(0),
	mRequest(NULL),
	mByteStream(NULL)
{
	Assert(!mUrl.empty());
}

Resource::RemoteAccessor::~RemoteAccessor(void)
{
	clearRequest();
}

size_t Resource::RemoteAccessor::hashData(ByteString &digest, size_t size)
{
	// TODO
	return Accessor::hashData(digest, size);
}

size_t Resource::RemoteAccessor::readData(char *buffer, size_t size)
{
	if(!mByteStream) initRequest();
	size = mByteStream->readData(buffer, size);
	mPosition+= size;
	return size;
}

void Resource::RemoteAccessor::writeData(const char *data, size_t size)
{
	throw Unsupported("Writing to remote resource");
}

void Resource::RemoteAccessor::seekRead(int64_t position)
{
	mPosition = position;
	clearRequest();
}

void Resource::RemoteAccessor::seekWrite(int64_t position)
{
	throw Unsupported("Writing to remote resource");
}

void Resource::RemoteAccessor::initRequest(void)
{
	const double timeout = milliseconds(Config::Get("request_timeout").toInt());
	
	clearRequest();
	mRequest = new Request(mUrl, true);
	StringMap parameters;
	parameters["position"] << mPosition;
	mRequest->setParameters(parameters);
	mRequest->setContentSink(new TempFile);
	mRequest->submit(mPeering);
	mRequest->wait(timeout);
	
	for(int i=0; i<mRequest->responsesCount(); ++i)
	{
		Request::Response *response = mRequest->response(i);
		if(response->error()) continue;

		/*Resource resource;
		parameters = response->parameters();
		parameters.input(resource);
		merge(resource);*/
		
		if(!mByteStream)
		{
			mByteStream = response->content();
			mPeering = response->peering();
		}
		else {
			if(response->content())
				response->content()->close();
		}
	}
	
	if(!mByteStream) 
	{
		delete mRequest;
		throw IOException("Unable to access resource: " + mUrl);
	}
}

void Resource::RemoteAccessor::clearRequest(void)
{
	delete mRequest;
	mRequest = NULL;
	mByteStream = NULL;	// deleted by the request
}

Resource::SplicerAccessor::SplicerAccessor(const ByteString &digest, const Set<Identifier> &sources) :
	mDigest(digest),
	mSources(sources),
	mPosition(0),
	mSplicer(NULL)
{
	Assert(!mDigest.empty());
}

Resource::SplicerAccessor::~SplicerAccessor(void)
{
	delete mSplicer;
}

size_t Resource::SplicerAccessor::hashData(ByteString &digest, size_t size)
{
	// TODO
        return Accessor::hashData(digest, size);
}

size_t Resource::SplicerAccessor::readData(char *buffer, size_t size)
{
	if(!mSplicer)
	{
		mSplicer = new Splicer(mDigest, mPosition);
		mSplicer->addSources(mSources);
		mSplicer->start();
	}

	while(mSplicer->process())
	{
		size_t s;
		if(s = mSplicer->read(buffer, size))
		{
			mPosition+= s;
			return s;
		}
		
		Thread::Sleep(milliseconds(10));
	}
	
	return 0;
}

void Resource::SplicerAccessor::writeData(const char *data, size_t size)
{
	throw Unsupported("Writing to remote resource");
}

void Resource::SplicerAccessor::seekRead(int64_t position)
{
	mPosition = position;
	delete mSplicer;
	mSplicer = NULL;
}

void Resource::SplicerAccessor::seekWrite(int64_t position)
{
	throw Unsupported("Writing to remote resource");
}

}
