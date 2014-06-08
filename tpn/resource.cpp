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

#include "tpn/resource.h"
#include "tpn/config.h"
#include "tpn/crypto.h"
#include "tpn/random.h"
#include "tpn/store.h"
#include "tpn/file.h"
#include "tpn/request.h"
#include "tpn/Fountain.h"
#include "tpn/directory.h"
#include "tpn/pipe.h"
#include "tpn/thread.h"
#include "tpn/mime.h"
#include "tpn/addressbook.h"
#include "tpn/user.h"

namespace tpn
{

void Resource::Process(const String &path, Resource &resource)
{
	File *file;
	
	if(Directory::Exist(path))
	{
		file = new TempFile;
		
		// TODO: serialize to file
		Directory dir(path);
		while(dir.nextFile())
		{
			
		}
	}
	else {
		file = new File(path, File::Read);
	}
	
	Sha256().compute(*file, resource.mDigest);
	file->seekRead(0);
	
	// Move file to cache if directory
	
	// TODO: Fill resource fields
	
	// Process blocks
	resource.mBlocks.clear();
	resource.mBlocks.reserve(file->size()/(Block::MaxChunks*Block::ChunkSize));
	
	Block block;
	while(Block::ProcessFile(*file, block))
	{
		resource.mBlocks.append(block.digest());
	}
}

// TODO
int Resource::CreatePlaylist(const Set<Resource> &resources, Stream *output, String host)
{
	if(host.empty()) host = String("localhost:") + Config::Get("interface_port");
	
	int count = 0;
	output->writeLine("#EXTM3U");
	for(Set<Resource>::iterator it = resources.begin(); it != resources.end(); ++it)
	{
		const Resource &resource = *it;
		if(resource.isDirectory() || resource.digest().empty()) continue;
		if(!Mime::IsAudio(resource.name()) && !Mime::IsVideo(resource.name())) continue;
		String link = "http://" + host + "/" + resource.digest().toString();
		output->writeLine("#EXTINF:-1," + resource.name().beforeLast('.'));
		output->writeLine(link);
		++count;
	}
	
	return count;
}

Resource::Resource(const String &path)
{
	
}

Resource::~Resource(void)
{
	for(int i=0; i<mBlocks.size(); ++i)
		delete mBlocks[i];
}

// TODO
void Resource::clear(void)
{
	mDigest.clear();
	mUrl.clear();
	mPath.clear();
	mTime = Time::Now();
	mSize = 0;
	mType = 1;
	mHops = 0;
	mPath.clear();
	mPeering = Identifier::Null;
	
	delete mAccessor;
	mAccessor = NULL;
}

void Resource::fetch(bool forceLocal)
{
	if(!mDigest.empty())
	{
		CacheMutex.lock();
		bool success = Cache.get(mDigest, *this);
		CacheMutex.unlock();
		if(success) return;
	}
	
	refresh(forceLocal);
}
	
void Resource::refresh(bool forceLocal)
{
	mSources.clear();

	Query query(mStore);
	createQuery(query);
	Set<Resource> result;
	query.submit(result, mPeering, forceLocal);

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
	
	// If remote and accessed by digest, cache the resource
	if(!mSources.empty() && !mDigest.empty())
	{
		CacheMutex.lock();
		while(Cache.size() >= 1000)
		{
			Map<BinaryString, Resource>::iterator it = Cache.begin();
			int r = Random().uniform(0, int(Cache.size())-1);
			while(r--) it++;
			Cache.erase(it);
		}
		Cache.insert(mDigest, *this);
		CacheMutex.unlock();
		
		// Hints for the Fountain system
		Fountain::Hint(mDigest, name(), mSources, mSize);
	}
}

BinaryString Resource::digest(void) const
{
	return mDigest;
}

Time Resource::time(void) const
{
	return mTime;
}

int64_t Resource::size(void) const
{
	if(mAccessor) return mAccessor->size();
	else return mSize;
}
	
int Resource::type(void) const
{
	return mType;
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

Identifier Resource::peering(void) const
{
	return mPeering;
}

int Resource::hops(void) const
{
	return mHops;
}

Resource::Accessor *Resource::accessor(void) const
{
	if(isDirectory()) return NULL;
	
	if(!mAccessor)
	{
		Query query(mStore);
		createQuery(query);
		Resource dummy;		// TODO: rather stupid
		if(query.submitLocal(dummy))
		{
			Assert(!dummy.mPath.empty());
			mAccessor = new LocalAccessor(dummy.mPath);
		}
		else if(!mDigest.empty())
		{
			mAccessor = new ContentAccessor(mDigest, mSources);
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

void Resource::addHop(void)
{
	++mHops;
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
	// TODO: WARNING: This will break if serialized to database (wrong type field)
	// + hops field

	String strType = (mType == 0 ? "directory" : "file");
	String tmpName = name();
	String tmpContact;

	if(mPeering != Identifier::Null)
	{
		const User *user = mStore->user();
		if(user)
		{
			const AddressBook::Contact *contact = user->addressBook()->getContact(mPeering);
			if(contact) tmpContact = contact->uniqueName();
		}
	}
	
	ConstSerializableWrapper<int64_t> sizeWrapper(mSize);
	ConstSerializableWrapper<int> hopsWrapper(mHops);
	
	// Note path is not serialized
	Serializer::ConstObjectMapping mapping;
	if(!mDigest.empty()) mapping["digest"] = &mDigest;
	if(!mUrl.empty()) mapping["url"] = &mUrl;
	mapping["name"] = &tmpName;
	mapping["time"] = &mTime;
	mapping["type"] = &strType;
	mapping["size"] = &sizeWrapper;
	mapping["hops"] = &hopsWrapper;
	
	if(!tmpContact.empty()) mapping["contact"] = &tmpContact;
	
	s.outputObject(mapping);
}

bool Resource::deserialize(Serializer &s)
{
	clear();
	
	String strType;
	String tmpName;
	String tmpContact;
	SerializableWrapper<int64_t> sizeWrapper(&mSize);
	SerializableWrapper<int> hopsWrapper(&mHops);
	
	// Note path is not deserialized
	Serializer::ObjectMapping mapping;
	mapping["digest"] = &mDigest;
	mapping["url"] = &mUrl;		
	mapping["name"] = &tmpName;
	mapping["time"] = &mTime;
	mapping["type"] = &strType;
	mapping["size"] = &sizeWrapper;
	mapping["hops"] = &hopsWrapper;
	
	mapping["contact"] = &tmpContact;
	
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

	if(!tmpContact.empty())
	{
		const User *user = mStore->user();
		if(user)
		{
			const AddressBook::Contact *contact = user->addressBook()->getContactByUniqueName(tmpContact);
			if(contact) mPeering = contact->peering();
		}
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
	if(r1.isDirectory() && !r2.isDirectory()) return true;
	if(!r1.isDirectory() && r2.isDirectory()) return false;
	return r1.url().toLower() < r2.url().toLower();
}

bool operator >  (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return false;
	if(!r1.isDirectory() && r2.isDirectory()) return true;
	return r1.url().toLower() > r2.url().toLower();
}

bool operator == (const Resource &r1, const Resource &r2)
{
	if(r1.url() != r2.url()) return false;
	return r1.digest() == r2.digest() && r1.isDirectory() == r2.isDirectory();
}

bool operator != (const Resource &r1, const Resource &r2)
{
	return !(r1 == r2);
}

Resource::Query::Query(Store *store, const String &url) :
	mUrl(url),
	mStore(store),
	mMinAge(0), mMaxAge(0),
	mOffset(0), mCount(-1),
	mAccessLevel(Private)
{
  	if(!mStore) mStore = Store::GlobalInstance;
}

Resource::Query::~Query(void)
{
	
}

void Resource::Query::setLocation(const String &url)
{
	mUrl = url;
}

void Resource::Query::setDigest(const BinaryString &digest)
{
	mDigest = digest;
}

void Resource::Query::setMinAge(int seconds)
{
	mMinAge = seconds;
}

void Resource::Query::setMaxAge(int seconds)
{
	mMaxAge = seconds;
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

void Resource::Query::setAccessLevel(AccessLevel level)
{
	mAccessLevel = level;
}

void Resource::Query::setFromSelf(bool isFromSelf)
{
	if(isFromSelf)
	{
		if(mAccessLevel == Private)
			mAccessLevel = Personal;
	}
	else {
		if(mAccessLevel == Personal) 
			mAccessLevel = Private;
	}
}

bool Resource::Query::submitLocal(Resource &result)
{
	if(!mDigest.empty())
	{
		return Store::Get(mDigest, result);
	}
	
	return mStore->query(*this, result);
}

bool Resource::Query::submitLocal(Set<Resource> &result)
{
	if(!mDigest.empty())
	{
		Resource resource;
		if(!Store::Get(mDigest, resource)) return false;
		result.insert(resource);
		return true;
	}
	
	return mStore->query(*this, result);
}

bool Resource::Query::submitRemote(Set<Resource> &result, const Identifier &peering)
{
	const double timeout = milliseconds(Config::Get("request_timeout").toInt());
	const int hops = 3;	// Must be less or equals to Fountain requests
	
	Request request;
	createRequest(request);
	
	if(peering == Identifier::Null)
	{
		request.setParameter("hops", String::number(hops));
		request.setParameter("timeout", String::number(timeout));	// Hint for neighbors
	}
	
	try {
		request.submit(peering);
		request.wait(timeout);
	}
	catch(const Exception &e)
	{
		return false;
	}

	Synchronize(&request);
	bool success = false;
	for(int i=0; i<request.responsesCount(); ++i)
	{
		const Request::Response *response = request.response(i);		
                if(response->error()) continue;
		success = true;
		
		if(response->status() != Request::Response::Empty)
		{
			try {
				Resource resource(mStore);
				StringMap parameters = response->parameters();
				resource.deserialize(parameters);
				resource.setPeering(response->peering());
				resource.addHop();
				result.insert(resource);
			}
			catch(const Exception &e)
			{
				LogWarn("Resource::Query::submit", String("Dropping invalid response: ") + e.what());
			}
		}
	}

	return success;
}

bool Resource::Query::submit(Set<Resource> &result, const Identifier &peering, bool forceLocal)
{
	bool success = false;
	if(forceLocal || peering == Identifier::Null) 
	{
		int oldSize = result.size();
		success|= submitLocal(result);
		if(!mDigest.empty() && result.size() > oldSize) return true;
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
	ConstSerializableWrapper<int> minAgeWrapper(mMinAge);
	ConstSerializableWrapper<int> maxAgeWrapper(mMaxAge);
	ConstSerializableWrapper<int> offsetWrapper(mOffset);
	ConstSerializableWrapper<int> countWrapper(mCount);
	String strAccessLevel = (mAccessLevel == Private || mAccessLevel == Personal ? "private" : "public");
	
	Serializer::ConstObjectMapping mapping;
	if(!mUrl.empty())	mapping["url"] = &mUrl;
	if(!mMatch.empty())	mapping["match"] = &mMatch;
	if(!mDigest.empty())	mapping["digest"] = &mDigest;
	if(mMinAge > 0)		mapping["minage"] = &minAgeWrapper;
	if(mMaxAge > 0)		mapping["maxage"] = &maxAgeWrapper;
	if(mOffset > 0)		mapping["offset"] = &offsetWrapper;
	if(mCount > 0)		mapping["count"] = &countWrapper;
	mapping["access"] = &strAccessLevel;

	s.outputObject(mapping);
}

bool Resource::Query::deserialize(Serializer &s)
{
	SerializableWrapper<int> minAgeWrapper(&mMinAge);
	SerializableWrapper<int> maxAgeWrapper(&mMaxAge);
	SerializableWrapper<int> offsetWrapper(&mOffset);
	SerializableWrapper<int> countWrapper(&mCount);
	String strAccessLevel;
	
	Serializer::ObjectMapping mapping;
	mapping["url"] = &mUrl;
	mapping["match"] = &mMatch;
	mapping["digest"] = &mDigest;
	mapping["minage"] = &minAgeWrapper;
	mapping["maxage"] = &maxAgeWrapper;
	mapping["offset"] = &offsetWrapper;
	mapping["count"] = &countWrapper;
	mapping["access"] = &strAccessLevel;
	
	if(strAccessLevel == "private") mAccessLevel = Private;
	else mAccessLevel = Public;
	
	return s.inputObject(mapping);
}

bool Resource::Query::isInlineSerializable(void) const
{
	return false;
}

Resource::Reader::Reader(Resource *resource) :
	mResource(resource),
	mReadPosition(0)
	mCurrentBlock(NULL),
	mNextBlock(NULL)
{
	Assert(mResource);
	seekRead(0);	// Initialize positions
}

Resource::Reader::~Reader(void)
{
	delete mCurrentBlock;
	delete mNextBlock;
}
	  
size_t Resource::Reader::readData(char *buffer, size_t size)
{
	if(!mCurrentBlock) return 0;	// EOF
  
	size_t ret;
	if((ret = mCurrentBlock->readData(buffer, size)))
	{
		mReadPosition+= ret;
		return ret;
	}
	
	delete mCurrentBlock;
	++mCurrentBlockIndex;
	mCurrentBlock = mNextBlock;
	mNextBlock = createBlock(mCurrentBlockIndex + 1);
	return readData(buffer, size);
}

void Resource::Reader::writeData(const char *data, size_t size)
{
	throw Unsupported("Writing to Resource::Reader");
}

void Resource::Reader::seekRead(int64_t position)
{
	delete mCurrentBlock;
	delete mNextBlock;
	
	mCurrentBlockIndex = mResource->blockIndex(position);
	mCurrentBlock	= createBlock(mCurrentBlockIndex);
	mNextBlock	= createBlock(mCurrentBlockIndex + 1);
	mReadPosition	= position;
}

void Resource::Reader::seekWrite(int64_t position)
{
	throw Unsupported("Writing to Resource::Reader");
}

int64_t Resource::Reader::tellRead(void) const
{
	return mReadPosition;  
}

int64_t Resource::Reader::tellWrite(void) const
{
	return 0;  
}

Block *Resource::Reader::createBlock(int index)
{
	if(index < 0 || index >= mResource->blocksCount()) return NULL;
	return new Block(mResource->blockDigest(index)); 
}

}
