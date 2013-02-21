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

#include "splicer.h"
#include "request.h"
#include "pipe.h"
#include "config.h"

namespace tpot
{
	
Map<ByteString, Splicer::CacheEntry*> Splicer::Cache;
Mutex Splicer::CacheMutex;
  
Splicer::Splicer(const ByteString &target, int64_t begin, int64_t end) :
	mFirstBlock(0),
	mCurrentBlock(0),
	mOffset(0),
	mBegin(begin),
	mEnd(end),
	mLeft(0)
{
	{
		CacheMutex.lock();
		if(!Cache.get(target, mCacheEntry))
		{
			Log("Splicer", "No cached file, creating a new one");
			
			try {
				mCacheEntry = new CacheEntry(target);
			}
			catch(...)
			{
				CacheMutex.unlock();
				throw;
			}
			
			Cache.insert(target, mCacheEntry);
		}
		
		Map<ByteString, CacheEntry*>::iterator it = Cache.begin();
		while(it != Cache.end())
		{
			if(!it->second || Time::Now() - it->second->lastAccessTime() > 3600)
				Cache.erase(it++);
			else it++;
		}
		
		CacheMutex.unlock();
	}
	
	Set<Identifier> sources;
	mCacheEntry->getSources(sources);
	if(sources.empty()) throw Exception("No sources found for " + target.toString());
	
	// OK, the cache entry is initialized
	
	// Initialize variables
	mBegin = bounds(mBegin, int64_t(0), mCacheEntry->size());
	if(mEnd < 0) mEnd = mCacheEntry->size();
	else mEnd = bounds(mEnd, mBegin, mCacheEntry->size());
	mLeft = mEnd-mBegin;
	
	mCurrentBlock = mCacheEntry->block(mBegin);
	mFirstBlock = mCurrentBlock;
	while(mCacheEntry->isBlockFinished(mFirstBlock))
		++mFirstBlock;
	
	Log("Splicer", "Starting for " + mCacheEntry->name() + " [" + String::number(mBegin) + "," + String::number(mEnd) + "]");
	
	// Open file to read
	mFile = new File(mCacheEntry->fileName(), File::Read);
	mFile->seekRead(mCurrentBlock*mCacheEntry->blockSize());
	mOffset = mBegin - mCurrentBlock*mCacheEntry->blockSize();
	
	// Request stripes
	int nbStripes = std::max(1, int(sources.size()));	// TODO

	if(!finished())
	{
		mRequests.fill(NULL, nbStripes);
        	mStripes.fill(NULL, nbStripes);

		Set<Identifier>::iterator it = sources.begin();
		for(int i=0; i<nbStripes; ++i)
		{
			query(i, *it);
			++it;
			if(it == sources.end()) it = sources.begin();
		}
	
		//Log("Splicer", "Transfers launched successfully");
	}
}

Splicer::~Splicer(void)
{
	close();
	
	delete mFile;
}

const String &Splicer::name(void) const
{
	return mCacheEntry->name(); 
}

int64_t Splicer::size(void) const
{
	return mEnd - mBegin; 
}

int64_t Splicer::begin(void) const
{
	return mBegin; 
}

int64_t Splicer::end(void) const
{
	return mEnd;
}

bool Splicer::finished(void) const
{
	return mCacheEntry->finished(); 
}

int64_t Splicer::process(ByteStream *output)
{
	int64_t written = 0;

	mCacheEntry->setAccessTime();
	unsigned currentBlock = mCacheEntry->block(mCacheEntry->size());
	int nbPending = 0;
	for(int i=0; i<mRequests.size(); ++i)
	{
		Assert(mRequests[i]);
		Assert(mStripes[i]);
		Synchronize(mRequests[i]);
		
		if(mRequests[i]->responsesCount()) 
		{
			const Request::Response *response = mRequests[i]->response(0);
			Assert(response != NULL);
			if(response->finished()) continue;
		}
		
		//std::cout<<i<<" -> "<<mStripes[i]->tellWriteBlock()<<std::endl;
		currentBlock = std::min(currentBlock, mStripes[i]->tellWriteBlock());
		mStripes[i]->flush();
		++nbPending;
	}

	if(!nbPending) ++currentBlock;
	else if(currentBlock > 0) --currentBlock;
	
	if(mCurrentBlock < currentBlock)
	{
		while(mCurrentBlock < currentBlock)
		{
			mCacheEntry->markBlockFinished(mCurrentBlock);
			
			if(output && mLeft)
			{
				if(mOffset)
				{
					mFile->ignore(mOffset);
					mOffset = 0;
				}
			  
				size_t size = size_t(std::min(int64_t(mCacheEntry->blockSize()), mLeft));
				Assert(size = mFile->readBinary(*output, size));
				mLeft-= size;
				written+= size;
				
				if(!mLeft) return written;
			}
			
			++mCurrentBlock;
		}
	}
	
	std::vector<int>		onError;
	std::multimap<unsigned, int> 	byBlocks;
	
	for(int i=0; i<mRequests.size(); ++i)
	{
		Assert(mRequests[i]);
		Assert(mStripes[i]);
		Synchronize(mRequests[i]);
		
		byBlocks.insert(std::pair<unsigned,int>(mStripes[i]->tellWriteBlock(), i));
		
		if(mRequests[i]->responsesCount())
		{
			const Request::Response *response = mRequests[i]->response(0);
			Assert(response != NULL);
				
			if(response->error())
				onError.push_back(i);
		}
	}
	
	if(onError.empty())
	{
		if(mRequests.size() >= 2)
		{
			int slowest = byBlocks.begin()->second;
			int fastest = byBlocks.rbegin()->second;
			if(mRequests[fastest]->receiver() != mRequests[slowest]->receiver())
			{
				if((mStripes[fastest]->tellWriteBlock()-mFirstBlock) > 2*(mStripes[slowest]->tellWriteBlock()-mFirstBlock) + 2)
					query(slowest, mRequests[fastest]->receiver());
			}
		}
	}
	else for(int k=0; k<onError.size(); ++k)
	{
		int i = onError[k];
		Log("Splicer::process", String("Error on request ") + String::number(i));
		
		Identifier formerSource = mRequests[i]->receiver();
		Identifier source;
		Map<unsigned, int>::reverse_iterator it = byBlocks.rbegin();
		while(true)
		{
			source = mRequests[it->second]->receiver();
			if(source != formerSource) break;
			++it;
			if(it == byBlocks.rend())
			{
				Set<Identifier> sources;
				mCacheEntry->refreshSources();
				mCacheEntry->getSources(sources);
	
				if(sources.empty())
				{
					msleep(30000);
					return written;
				}

				if(sources.find(formerSource) != sources.end())
				{
					if(sources.size()> 1) sources.erase(formerSource);
					else msleep(30000);
				}
				
				Set<Identifier>::iterator jt = sources.begin();
				int r = rand() % sources.size();
				for(int i=0; i<r; ++i) jt++;
				source = *jt;
				break;
			}
		}
		
		query(i, source);
	}
	
	return written;
}

void Splicer::close(void)
{
	if(!mRequests.empty())
	{
		for(int i=0; i<mRequests.size(); ++i)
			delete mRequests[i];
			
		mRequests.clear();
	}
	
	// Deleting the requests deletes the responses
	// Deleting the responses deletes the contents
	// So the stripes are already deleted
	
	mStripes.clear();
}

void Splicer::query(int i, const Identifier &source)
{
	Assert(i < mRequests.size());
	Assert(i < mStripes.size());

	int nbStripes = mStripes.size();
	unsigned block = mFirstBlock;
	size_t offset = 0;

	if(mStripes[i])
	{
		block = std::max(block, mStripes[i]->tellWriteBlock());
		mStripes[i]->flush();
	}

	if(mRequests[i]) delete mRequests[i];
	mStripes[i] = NULL;
	mRequests[i] = NULL;
	
	File *file = new File(mCacheEntry->fileName(), File::ReadWrite);
	StripedFile *striped = new StripedFile(file, mCacheEntry->blockSize(), nbStripes, i);
	striped->seekWrite(block, offset);
	mStripes[i] = striped;
	
	StringMap parameters;
	parameters["block-size"] << mCacheEntry->blockSize();
	parameters["stripes-count"] << nbStripes;
	parameters["stripe"] << i;
	parameters["block"] << block;
	parameters["offset"] << offset;
	
	Request *request = new Request;
	request->setTarget(mCacheEntry->target().toString(),true);
	request->setParameters(parameters);
	request->setContentSink(striped);
	request->submit(source);
	mRequests[i] = request;
}

Splicer::CacheEntry::CacheEntry(const ByteString &target) :
	mTarget(target),
	mSize(0),
	mBlockSize(128*1024),	// TODO
	mTime(Time::Now())
{
	mFileName = File::TempName();
	File dummy(mFileName, File::TruncateReadWrite);
	dummy.close();
}

Splicer::CacheEntry::~CacheEntry(void)
{
	File::Remove(mFileName);
}

const ByteString &Splicer::CacheEntry::target(void) const
{
	Synchronize(this);
	return mTarget;
}

const String &Splicer::CacheEntry::fileName(void) const
{
	Synchronize(this);
	return mFileName;
}

const String &Splicer::CacheEntry::name(void) const
{
	Synchronize(this); 
	return mName;
}

int64_t Splicer::CacheEntry::size(void) const
{
	Synchronize(this); 
	return mSize;
}

size_t Splicer::CacheEntry::blockSize(void) const
{
	Synchronize(this); 
	return mBlockSize; 
}

bool Splicer::CacheEntry::finished(void) const
{
	Synchronize(this);
	if(mFinishedBlocks.size() < (mSize + mBlockSize - 1) / mBlockSize) return false;
	if(std::find(mFinishedBlocks.begin(), mFinishedBlocks.end(), false) != mFinishedBlocks.end()) return false;
	return true;
}

Time Splicer::CacheEntry::lastAccessTime(void) const
{
	Synchronize(this);
	return mTime;	
}

void Splicer::CacheEntry::setAccessTime(void)
{
	Synchronize(this);
	mTime = Time::Now();
}

unsigned Splicer::CacheEntry::block(int64_t position) const
{
	Synchronize(this); 
	return unsigned(position / int64_t(mBlockSize));  
}

bool Splicer::CacheEntry::getSources(Set<Identifier> &sources)
{
	Synchronize(this);
	if(mSources.empty()) refreshSources();
	sources = mSources;
	return !sources.empty();
}

void Splicer::CacheEntry::refreshSources(void)
{
	Synchronize(this);  

	Log("Splicer", "Requesting available sources...");
	
	const unsigned timeout = Config::Get("request_timeout").toInt();
	
	Request request(mTarget.toString(), false);
	request.submit();
	DesynchronizeStatement(this, request.wait(timeout));

	{
		Synchronize(&request);
		mSources.clear();
		for(int i=0; i<request.responsesCount(); ++i)
		{
			Request::Response *response = request.response(i);
			const StringMap &parameters = response->parameters();
			
			if(!response->error())
			{
				if(mName.empty() && parameters.contains("name")) 
					mName = parameters.get("name");
				
				// TODO: check size
				if(mSize == 0 && parameters.contains("size")) 
					parameters.get("size").extract(mSize);
				
				mSources.insert(response->peering());
			}
		}
	}
	
	if(mName.empty()) throw Exception("Unable to retrieve the file name");
	
	Log("Splicer", "Found " + String::number(int(mSources.size())) + " sources");
}

bool Splicer::CacheEntry::isBlockFinished(unsigned block) const
{
	Synchronize(this);

	if(block >= mFinishedBlocks.size()) return false;
	return mFinishedBlocks[block];
}

bool Splicer::CacheEntry::markBlockFinished(unsigned block)
{
	Synchronize(this);
  
	if(block >= mFinishedBlocks.size())
	{
		unsigned i = mFinishedBlocks.size();
		mFinishedBlocks.resize(block+1);
		while(i < mFinishedBlocks.size()-1)
		{
			mFinishedBlocks[i] = false;
			++i;
		}
	}
	
	mFinishedBlocks[block] = true;
}

}
