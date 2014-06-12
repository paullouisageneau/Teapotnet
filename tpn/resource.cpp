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

bool Resource::Process(const String &path, Resource &resource, BinaryString &digest)
{
	// Sanitize path
	if(path.empty()) return false;
	if(path[path.size() - 1] == Directory::Separator)
		path.resize(path.size() - 1);
	
	// Get name
	String name = path.afterLast(Directory::Separator);
	
	// Recursively process if it's a directory 
	bool isDirectory;
	if((isDirectory = Directory::Exist(path)))
	{
		String tempFileName = File::TempName();
		File tempFile(tempFileName);
		
		BinarySerializer serializer(&tempFile);
		Directory dir(path);
		while(dir.nextFile())
		{
			Resource subResource;
			BinaryString subDigest;
			Process(dir.filePath(), subResource, subDigest);
		  
			DirectoryRecord record;
			record = subResource.mIndexRecord;
			record.digest = subDigest;
			record.time = dir.fileTime();
			
			serializer.output(record);
		}
		
		tempFile.close();
		path = Cache::Instance->move(tempFileName);
		size = 0;
	}
	else {
		if(!File::Exist())
			return false;
	}
	
	int64_t size = File::Size(path);
	
	// Fill index record
	delete resource.mIndexRecord;
	resource.mIndexRecord = new IndexRecord;
	resource.mIndexRecord->name = name;
	resource.mIndexRecord->type = (isDirectory ? "directory" : "file");
	resource.mIndexRecord->size = size;
	resource.mIndexRecord->blockDigests.reserve(size/Block::Size);
	
	// Process blocks
	File file(path, File::Read);
	BinaryString digest;
	while(Block::ProcessFile(file, digest))
		resource.mIndexRecord->blockDigests.append(digest);
	
	// Create index
	String tempFileName = File::TempName();
	File tempFile(tempFileName);
	BinarySerializer serializer(&tempFile);
	serializer.output(resource.mIndexRecord);
	tempFile.close();
	String indexFilePath = Cache::Instance->move(tempFileName);
	
	// Create index block
	delete resource.mIndexBlock;
	resource.mIndexBlock = new Block(indexFilePath);
	
	return true;
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

Resource::Resource(const String &path) :
	mIndexBlock(NULL),
	mIndexRecord(NULL)
{
	if(!Process(path, *this, mDigest))
		throw Exception("Unable to process resource from path: " + path);
}

Resource::~Resource(void)
{
	delete mIndexBlock;
	delete mIndexRecord;
}

BinaryString Resource::digest(void) const
{
	return mDigest;
}

void Resource::serialize(Serializer &s) const
{
	// TODO
}

bool Resource::deserialize(Serializer &s)
{
	// TODO
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
