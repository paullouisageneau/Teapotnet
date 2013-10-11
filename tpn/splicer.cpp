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

#include "tpn/splicer.h"
#include "tpn/request.h"
#include "tpn/pipe.h"
#include "tpn/config.h"

namespace tpn
{
	
Map<ByteString, Splicer::CacheEntry*> Splicer::Cache;
Mutex Splicer::CacheMutex;

Splicer::CacheEntry *Splicer::GetCacheEntry(const ByteString &target)
{
	CacheEntry *entry = NULL;
	
	CacheMutex.lock();
	if(!Cache.get(target, entry))
	{
		LogDebug("Splicer", "No cached file, creating a new one");
		
		try {
			entry = new CacheEntry(target);
		}
		catch(...)
		{
			CacheMutex.unlock();
			throw;
		}
		
		Cache.insert(target, entry);
	}
	
	entry->setAccessTime();
	
	Map<ByteString, CacheEntry*>::iterator it = Cache.begin();
	while(it != Cache.end())
	{
		if(!it->second || Time::Now() - it->second->lastAccessTime() > 3600)
			Cache.erase(it++);
		else it++;
	}
	CacheMutex.unlock();
	
	return entry;
}

void Splicer::Hint(const ByteString &target, const Set<Identifier> &sources, int64_t size)
{
	CacheEntry *entry = GetCacheEntry(target);
	entry->hintSources(sources);
	if(size >= 0) entry->hintSize(size);
}

Splicer::Splicer(const ByteString &target, int64_t begin, int64_t end) :
	mFirstBlock(0),
	mCurrentBlock(0),
	mBegin(begin),
	mEnd(end),
	mPosition(0)
{
	mCacheEntry = GetCacheEntry(target);
	
	mCacheEntry->getSources(mSources);
	if(mSources.empty()) 
		throw Exception("No sources found for " + target.toString());
	
	LogDebug("Splicer", String::number(mSources.size()) + " sources found");
	
	// OK, the cache entry is initialized
	
	// Initialize variables
	mBegin = bounds(mBegin, int64_t(0), mCacheEntry->size());
	if(mEnd < 0) mEnd = mCacheEntry->size();
	else mEnd = bounds(mEnd, mBegin, mCacheEntry->size());
	mPosition = mBegin;
	
	mCurrentBlock = mCacheEntry->block(mBegin);
	mFirstBlock = mCurrentBlock;
	while(mCacheEntry->isBlockFinished(mFirstBlock))
		++mFirstBlock;
}

Splicer::~Splicer(void)
{
	stop();
}

void Splicer::addSources(const Set<Identifier> &sources)
{
	mCacheEntry->hintSources(sources);
	
	for(Set<Identifier>::iterator it = sources.begin(); it != sources.end(); ++it)
		mSources.insert(*it);
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

void Splicer::start(void)
{
	stop();
	if(finished()) return;

	LogDebug("Splicer", "Starting splicer [" + String::number(mBegin) + "," + String::number(mEnd) + "]");
	
	int nbStripes = std::max(1, int(mSources.size()));	// TODO

	mRequests.fill(NULL, nbStripes);
        mStripes.fill(NULL, nbStripes);

	Set<Identifier>::iterator it = mSources.begin();
	int i = 0;
	while(i<nbStripes)
	{
		if(mSources.empty() || !query(i, *it))
		{
			mCacheEntry->refreshSources();
			mCacheEntry->getSources(mSources);
			it = mSources.begin();
				
			if(mSources.empty())
				throw Exception("No available sources found");
				
			continue;
		}
			
		++i; ++it;
		if(it == mSources.end()) it = mSources.begin();
	}
	
	LogDebug("Splicer", "Transfers launched successfully");
}

void Splicer::stop(void)
{
	if(!mRequests.empty())
	{
		LogDebug("Splicer", "Stopping splicer");
	  
		for(int i=0; i<mRequests.size(); ++i)
			delete mRequests[i];
			
		mRequests.clear();
	}
	
	// Deleting the requests deletes the responses
	// Deleting the responses deletes the contents
	// So the stripes are already deleted
	
	mStripes.clear();
}

bool Splicer::process(void)
{
	mCacheEntry->setAccessTime();
	
	if(!finished() && mRequests.empty()) throw Exception("Splicer is not started");
	
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
			if(mRequests[fastest]->receiver() != mRequests[slowest]->receiver() 
				|| mRequests[fastest]->receiver().getName() != mRequests[slowest]->receiver().getName())
			{
				if((mStripes[fastest]->tellWriteBlock()-mFirstBlock) > 2*(mStripes[slowest]->tellWriteBlock()-mFirstBlock) + 2)
					query(slowest, mRequests[fastest]->receiver());
			}
		}
	}
	else for(int k=0; k<onError.size(); ++k)
	{
		int i = onError[k];
		LogDebug("Splicer::process", "Stripe " + String::number(i) + ": Request is in error state");
		
		Identifier formerSource = mRequests[i]->receiver();
		Identifier source;
		Map<unsigned, int>::reverse_iterator it = byBlocks.rbegin();
		while(true)
		{
			Assert(it->second < mRequests.size());
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
					LogDebug("Splicer::process", "No sources found, waiting...");
					Thread::Sleep(30.);
					return (mPosition < mEnd);
				}

				if(sources.size() > 1 && sources.contains(formerSource))
					sources.erase(formerSource);
				
				Assert(!sources.empty());
				Set<Identifier>::iterator jt = sources.begin();
				if(sources.size() > 1)
				{
					int r = pseudorand() % sources.size();
					while(r--) ++jt;
				}
				source = *jt;
				break;
			}
		}
		
		LogDebug("Splicer::process", "Stripe " + String::number(i) + ": Sending new request");
		query(i, source);
	}
	
	return (mPosition < mEnd);
}

size_t Splicer::read(char *buffer, size_t size)
{
	mCacheEntry->setAccessTime();
	
	if(!finished() && mRequests.empty()) throw Exception("Splicer is not started");
	if(!size) return 0;
	
	unsigned lastBlock = mCacheEntry->block(mCacheEntry->size());
	unsigned currentBlock = lastBlock;
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
			if(!response->finished())
				++nbPending;
		}
		
		//std::cout<<i<<" -> "<<mStripes[i]->tellWriteBlock()<<std::endl;
		
		currentBlock = std::min(currentBlock, mStripes[i]->tellWriteBlock());
		mStripes[i]->flush();
	}
	
	if(!nbPending) ++currentBlock;
	
	//if(mCurrentBlock < currentBlock)
	//{
	//	double progress = double(currentBlock) / double(lastBlock+1);
	//	LogDebug("Splicer", "Download position: " + String::number(progress*100,2) + "%");
	//}
	
	while(mCurrentBlock < currentBlock)
	{
		mCacheEntry->markBlockFinished(mCurrentBlock);
		++mCurrentBlock;
	}
	
	unsigned block = mCacheEntry->block(mPosition);
	if(mPosition < mEnd && block < currentBlock)
	{
		size = std::min(size, size_t(std::min(int64_t((block+1)*mCacheEntry->blockSize())-mPosition, mEnd-mPosition)));
		Assert(size <= mCacheEntry->blockSize());
		
		// Open file to read
		File file(mCacheEntry->fileName(), File::Read);
		file.seekRead(mPosition);
		size = file.readData(buffer, size);
		file.close();

		mPosition+= size;
			
		//if(output)
		//{
		//	double progress = double(mPosition-mBegin) / double(mEnd-mBegin);
		//	LogDebug("Splicer", "Reading progress: " + String::number(progress*100,2) + "%");
		//}
		
		return size;
	}
	
	return 0;
}

bool Splicer::query(int i, const Identifier &source)
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
	
	try {
		request->submit(source);
	}
	catch(...)
	{
		return false;
	}
	
	mRequests[i] = request;
	return true;
}

Splicer::CacheEntry::CacheEntry(const ByteString &target) :
	mTarget(target),
	mSize(-1),
	mBlockSize(128*1024),	// TODO
	mTime(Time::Now())
{
	
}

Splicer::CacheEntry::~CacheEntry(void)
{
	if(!mFileName.empty())
		File::Remove(mFileName);
}

String Splicer::CacheEntry::fileName(void)
{
	Synchronize(this);
	
	if(mFileName.empty())
	{
		mFileName = File::TempName();
		File dummy(mFileName, File::TruncateReadWrite);
		dummy.close();
	}
	
	return mFileName;
}

ByteString Splicer::CacheEntry::target(void) const
{
	Synchronize(this);
	return mTarget;
}

int64_t Splicer::CacheEntry::size(void) const
{
	Synchronize(this); 
	return std::max(mSize, int64_t(0));
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
	if(mFinishedBlocks.contains(false)) return false;
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

void Splicer::CacheEntry::hintSize(int64_t size)
{
	mSize = std::max(mSize, size);
}

void Splicer::CacheEntry::hintSources(const Set<Identifier> &sources)
{
	for(Set<Identifier>::iterator it = sources.begin(); it != sources.end(); ++it)
		mSources.insert(*it);
}

bool Splicer::CacheEntry::getSources(Set<Identifier> &sources)
{
	Synchronize(this);
	if(mSources.empty() || mSize < 0) refreshSources();
	sources = mSources;
	return !sources.empty();
}

void Splicer::CacheEntry::refreshSources(void)
{
	Synchronize(this);  

	LogDebug("Splicer", "Requesting available sources...");
	
	const double timeout = milliseconds(Config::Get("request_timeout").toInt());
	
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
				try {
					String tmp;
					if(response->parameter("size", tmp))
					{
						int64_t size = -1;
						tmp.extract(size);
						hintSize(size);
					}
				}
				catch(...) {}
				
				mSources.insert(response->peering());
			}
		}
	}
	
	LogDebug("Splicer", "Found " + String::number(int(mSources.size())) + " sources");
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
	return true;
}

}
