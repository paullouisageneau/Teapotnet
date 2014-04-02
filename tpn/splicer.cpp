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

#include "tpn/splicer.h"
#include "tpn/request.h"
#include "tpn/pipe.h"
#include "tpn/config.h"
#include "tpn/task.h"
#include "tpn/scheduler.h"
#include "tpn/random.h"

namespace tpn
{
	
Map<BinaryString, Splicer::CacheEntry*> Splicer::Cache;
Mutex Splicer::CacheMutex;

Splicer::CacheEntry *Splicer::GetCacheEntry(const BinaryString &target)
{
	CacheEntry *entry = NULL;
	
	CacheMutex.lock();
	
	try {
		if(!Cache.get(target, entry))
		{
			LogDebug("Splicer", "No cached file, creating a new one");
			entry = new CacheEntry(target);
			Cache.insert(target, entry);
		}
		
		entry->setAccessTime();
		
		Map<BinaryString, CacheEntry*>::iterator it = Cache.begin();
		while(it != Cache.end())
		{
			if(!it->second || Time::Now() - it->second->lastAccessTime() > 3600)
				Cache.erase(it++);
			else it++;
		}
	}
	catch(...)
	{
		CacheMutex.unlock();
		throw;
	}
	
	CacheMutex.unlock();
	return entry;
}

void Splicer::Prefetch(const BinaryString &target)
{
	class PrefetchTask : public Task
	{
	public:
		PrefetchTask(const BinaryString &target, int64_t maxSize)
		{ 
			this->target = target; 
			this->maxSize = maxSize;
		}
		
		void run(void)
		{
			Resource dummy;
        		if(!Store::Get(target, dummy))
			{
				Splicer *splicer = NULL;
				try {
					splicer = new Splicer(target);
					if(splicer->size() <= maxSize*1024*1024) splicer->start(true);	// autodelete
					else delete splicer;
				}
				catch(const Exception &e)
				{
					delete splicer;
				}
			}

			delete this;	// autodelete task
		}
		
	private:
		BinaryString target;
		int64_t maxSize;
	};
	
	Resource dummy;
	if(Store::Get(target, dummy)) return;

	int64_t maxFileSize = 0;
	Config::Get("prefetch_max_file_size").extract(maxFileSize);	// MiB
	if(maxFileSize > 0)
	{
		double average = 1./milliseconds(Config::Get("prefetch_delay").toInt());
		double delay = -average*std::log(Random().uniform(0.,1.));	// exponential law
		
		LogDebug("Splicer::Prefetch", "Scheduling prefetching in " + String::number(int(delay)) + " seconds");
		PrefetchTask *task = new PrefetchTask(target, maxFileSize);
		Scheduler::Global->schedule(task, delay);
	}
}

void Splicer::Hint(const BinaryString &target, const String &name, const Set<Identifier> &sources, int64_t size)
{
	CacheEntry *entry = GetCacheEntry(target);
	entry->hintSources(sources);
	if(!name.empty()) entry->hintName(name);
	if(size >= 0) entry->hintSize(size);
}

Splicer::Splicer(const BinaryString &target, int64_t begin, int64_t end) :
	mFirstBlock(0),
	mCurrentBlock(0),
	mBegin(begin),
	mEnd(end),
	mPosition(0),
	mAutoDelete(false)
{
	mCacheEntry = GetCacheEntry(target);
	
	mCacheEntry->getSources(mSources);
	if(mSources.empty()) 
		throw Exception("No sources found for " + target.toString());
	
	LogDebug("Splicer", String::number(mSources.size()) + " source(s) found");
	
	// OK, the cache entry is initialized
	
	// Initialize variables
	mBegin = bounds(mBegin, int64_t(0), mCacheEntry->size());
	if(mEnd < 0) mEnd = mCacheEntry->size();
	else mEnd = bounds(mEnd, mBegin, mCacheEntry->size());
	mPosition = mBegin;
	
	mCurrentBlock = mCacheEntry->block(mBegin);
	mFirstBlock = mCurrentBlock;
}

Splicer::~Splicer(void)
{
	mAutoDelete = false;
	NOEXCEPTION(stop());
}

void Splicer::addSources(const Set<Identifier> &sources)
{
	Synchronize(this);
	
	mCacheEntry->hintSources(sources);
	
	for(Set<Identifier>::iterator it = sources.begin(); it != sources.end(); ++it)
		mSources.insert(*it);
}

int64_t Splicer::size(void) const
{
	Synchronize(this);
	return mEnd - mBegin; 
}

int64_t Splicer::begin(void) const
{
	Synchronize(this);
	return mBegin; 
}

int64_t Splicer::end(void) const
{
	Synchronize(this);
	return mEnd;
}

bool Splicer::finished(void) const
{
	Synchronize(this);
	return mCacheEntry->finished(); 
}

void Splicer::start(bool autoDelete)
{
	Synchronize(this);
	
	mAutoDelete = autoDelete;
	
	if(isStarted()) return;

	if(finished())
	{
		if(mAutoDelete) delete this;
		return;
	}
	
	LogDebug("Splicer", "Starting splicer (begin=" + String::number(mBegin) + ")");

	// Find first block
	while(mCacheEntry->isBlockFinished(mCurrentBlock)) ++mCurrentBlock;
	mFirstBlock = mCurrentBlock;

	if(mCacheEntry->isBlockDownloading(mCurrentBlock))
	{
		// Already dowloading
		if(autoDelete) delete this;
		return;
	}

	mCacheEntry->markBlockDownloading(mCurrentBlock, true);

	// Initialize variables
	int nbSources = std::max(int(mSources.size()), 1);
        int nbStripes = bounds(nbSources, 1, 8);
        mRequests.fill(NULL, nbStripes);
        mStripes.fill(NULL, nbStripes);
	
	// Query sources
	Set<Identifier>::iterator it = mSources.begin();
	int r = pseudorand() % nbSources;
	while(r--) ++it;
	
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
			
		Assert(mRequests[i]);
		Assert(mStripes[i]);
		
		++i; ++it;
		if(it == mSources.end()) it = mSources.begin();
	}
	
	Scheduler::Global->repeat(this, milliseconds(200));
	
	LogDebug("Splicer", "Transfers launched successfully");
}

void Splicer::stop(void)
{
	Synchronize(this);
	
	if(!isStarted()) return;
	
	LogDebug("Splicer", "Stopping splicer");

	mCacheEntry->markBlockDownloading(mCurrentBlock, false);
	Scheduler::Global->remove(this);
	
	for(int i=0; i<mRequests.size(); ++i)
		delete mRequests[i];
	
	mRequests.clear();
	
	// Deleting the requests deletes the responses
	// Deleting the responses deletes the contents
	// So the stripes are already deleted
	mStripes.clear();
	
	if(mAutoDelete) delete this;
}

bool Splicer::isStarted(void) const
{
	Synchronize(this);
	return !mRequests.empty();
}

size_t Splicer::readData(char *buffer, size_t size)
{
	Synchronize(this);
	
	mCacheEntry->setAccessTime();
	if(!size || mPosition >= mEnd)
		return 0;

	unsigned block = mCacheEntry->block(mPosition);
	
	if(!mCacheEntry->finished())
	{
		if(!mCacheEntry->isDownloading()) start();

		while(!mCacheEntry->isBlockFinished(block))
		{
			//LogDebug("Splicer::readData", "Waiting for block " + String::number(block) + "...");
			
			if(isStarted() || mCacheEntry->isBlockDownloading(block)) 
			{
				Desynchronize(this);
				mCacheEntry->wait(milliseconds(100));
			}
			else {
				mCurrentBlock = block;
				start();
			}
		}
	}
	
	//LogDebug("Splicer::readData", "Reading block " + String::number(block));
	
	size = std::min(size, size_t(std::min(int64_t((block+1)*mCacheEntry->blockSize())-mPosition, mEnd-mPosition)));
	Assert(size <= mCacheEntry->blockSize());
	
	// Open file to read
	File file(mCacheEntry->fileName(), File::Read);
	file.seekRead(mPosition);
	size = file.readData(buffer, size);
	file.close();
	
	if(!size) throw Exception("Internal synchronization fault in splicer");
	mPosition+= size;
	
	//double progress = double(mPosition-mBegin) / double(mEnd-mBegin);
	//LogDebug("Splicer::readData", "Reading progress: " + String::number(progress*100,2) + "%");
	
	return size;
}

void Splicer::writeData(const char *data, size_t size)
{
	throw Unsupported("Writing to Splicer");
}

bool Splicer::query(int i, const Identifier &source)
{
	Synchronize(this);
	
	Request *request = NULL;
	StripedFile *stripe = NULL;
	
	try {
		Assert(i < mRequests.size());
		Assert(i < mStripes.size());

		const double timeout = milliseconds(Config::Get("request_timeout").toInt());
		const int hops = 3;
		
		int nbStripes = mStripes.size();
		unsigned block = mFirstBlock;
		size_t offset = 0;

		if(mStripes[i])
		{
			block = std::max(block, mStripes[i]->tellWriteBlock());
			mStripes[i]->flush();
		}
	
		StringMap parameters;
		parameters["hops"] << hops;
		parameters["timeout"] << timeout;
		parameters["block-size"] << mCacheEntry->blockSize();
		parameters["stripes-count"] << nbStripes;
		parameters["stripe"] << i;
		parameters["block"] << block;
		parameters["offset"] << offset;
		
		File *file = new File(mCacheEntry->fileName(), File::ReadWrite);
		stripe = new StripedFile(file, mCacheEntry->blockSize(), nbStripes, i);
		stripe->seekWrite(block, offset);

		request = new Request;
		request->setTarget(mCacheEntry->target().toString(),true);
		request->setParameters(parameters);
		request->setContentSink(stripe);
		request->submit(source, timeout);
	}
	catch(const Exception &e)
	{
		delete stripe;
		delete request;
		LogDebug("Splicer::query", e.what());
		return false;
	}
	
	delete mRequests[i];
	mRequests[i] = request;
	mStripes[i] = stripe;
	return true;
}

void Splicer::run(void)
{
	Synchronize(this);
	
	mCacheEntry->setAccessTime();
	if(!isStarted()) return;
		       
	try {
		//LogDebug("Splicer::run", "Processing splicer...");

		std::vector<int>		onError;
		std::multimap<unsigned, int> 	byBlocks;
		
		unsigned lastBlock = mCacheEntry->block(mCacheEntry->size());
		unsigned currentBlock = lastBlock;
		
		Assert(!mRequests.empty());
		Assert(mStripes.size() == mRequests.size());
		
		int nbPending = mRequests.size();
		for(int i=0; i<mRequests.size(); ++i)
		{
			Assert(mRequests[i]);
			Assert(mStripes[i]);
			Synchronize(mRequests[i]);
			
			if(mRequests[i]->responsesCount())
			{
				const Request::Response *response = mRequests[i]->response(0);
				Assert(response != NULL);
				
				if(response->error()) 
				{
					onError.push_back(i);
				} 
				else {
					byBlocks.insert(std::pair<unsigned,int>(mStripes[i]->tellWriteBlock(), i));
					if(response->finished()) --nbPending;
				}
			}
			else {
				if(!mRequests[i]->isPending())
					onError.push_back(i);
			}

			currentBlock = std::min(currentBlock, mStripes[i]->tellWriteBlock());
		}

		if(!nbPending) ++currentBlock;
		
		//if(mCurrentBlock < currentBlock)
		//{
		//	double progress = double(currentBlock) / double(lastBlock+1);
		//	LogDebug("Splicer::run", "Download position: " + String::number(progress*100,2) + "%");
		//}
		
		if(mCurrentBlock < currentBlock)
		{
			bool blockFinished = mCacheEntry->markBlockDownloading(currentBlock, true);
			
			while(mCurrentBlock < currentBlock)
			{
				//LogDebug("Splicer::run", "Block finished: " + String::number(mCurrentBlock));
				mCacheEntry->markBlockFinished(mCurrentBlock);
				++mCurrentBlock;
			}
			
			if(blockFinished)
			{
				// Block is finished
				stop();
				return;	// Warning: the splicer can be autodeleted
			}
		}
		
		if(byBlocks.size() >= 2)
		{
			int slowest = byBlocks.begin()->second;
			int fastest = byBlocks.rbegin()->second;
			if(mRequests[fastest]->receiver() != mRequests[slowest]->receiver())
			{
				LogDebug("Splicer::run", "Switching, source "+String::number(slowest)+" is too slow...");
				if((mStripes[fastest]->tellWriteBlock()-mFirstBlock) > 2*(mStripes[slowest]->tellWriteBlock()-mFirstBlock) + 2)
					query(slowest, mRequests[fastest]->receiver());
			}
		}
		
		else for(int k=0; k<onError.size(); ++k)
		{
			int i = onError[k];
			LogDebug("Splicer::run", "Stripe " + String::number(i) + ": Request is in error state");
			
			Identifier formerSource = mRequests[i]->receiver();
			Identifier source;
			Map<unsigned, int>::reverse_iterator it = byBlocks.rbegin();
			while(true)
			{
				if(it == byBlocks.rend())
				{
					Set<Identifier> sources;
					mCacheEntry->refreshSources();
					mCacheEntry->getSources(sources);
		
					if(sources.empty())
					{
						LogDebug("Splicer::run", "No sources found");
						Scheduler::Global->schedule(this, 30.);
						return;
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

				Assert(it->second < mRequests.size());
                                source = mRequests[it->second]->receiver();
                                if(source != formerSource) break;
                                ++it;
			}
			
			LogDebug("Splicer::run", "Stripe " + String::number(i) + ": Sending new request");
			query(i, source);
		}
		
		//LogDebug("Splicer::run", "Processing finished");
		
		if(finished())
		{
			stop();
			return;	// Warning: the splicer can be autodeleted
		}
	}
	catch(const Exception &e)
	{
		LogWarn("Splicer::run", e.what());
		stop();
		return;	// Warning: the splicer can be autodeleted
	}
}

Splicer::CacheEntry::CacheEntry(const BinaryString &target) :
	mTarget(target),
	mIsFileInCache(false),
	mSize(-1),
	mBlockSize(128*1024),	// TODO
	mTime(Time::Now())
{
	
}

Splicer::CacheEntry::~CacheEntry(void)
{
	// If finished the file is in the cache
	if(!finished() && !mFileName.empty())
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

String Splicer::CacheEntry::name(void) const
{
	Synchronize(this);
	return mName;
}

BinaryString Splicer::CacheEntry::target(void) const
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

void Splicer::CacheEntry::hintName(const String &name)
{
	if(!name.empty() && (mName.empty() || name.size() < mName.size()))
		mName = name;
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

	LogDebug("Splicer::CacheEntry", "Requesting available sources...");
	
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
					String url;
					if(response->parameter("url", url))
						hintName(url.afterLast('/'));
					
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
	
	LogDebug("Splicer::CacheEntry", "Found " + String::number(int(mSources.size())) + " sources");
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

	mDownloading.erase(block);
	
	if(isBlockFinished(block)) return true;

	if(block >= mFinishedBlocks.size())
	{
		// TODO: check against mSize if mSize >= 0

		unsigned i = mFinishedBlocks.size();
		mFinishedBlocks.resize(block+1);
		while(i < mFinishedBlocks.size()-1)
		{
			mFinishedBlocks[i] = false;
			++i;
		}
	}
	
	mFinishedBlocks[block] = true;
	
	if(!mIsFileInCache && finished())
	{
		try {
			// Note: modify mFileName
			mIsFileInCache = Store::GlobalInstance->moveFileToCache(mFileName, mName);
		}
		catch(const Exception &e)
		{
			LogWarn("Splicer::CacheEntry", String("Unable to move the file to cache: ") + e.what());
		}
	}

	notifyAll();
	return false;
}

bool Splicer::CacheEntry::isBlockDownloading(unsigned block) const
{
	return mDownloading.contains(block);
}

bool Splicer::CacheEntry::markBlockDownloading(unsigned block, bool state)
{
	if(isBlockFinished(block))
	{
		mDownloading.erase(block);
		return true;
	}
	else {
		if(state) mDownloading.insert(block);
		else mDownloading.erase(block);
		return false;
	}
}

bool Splicer::CacheEntry::isDownloading(void) const
{
	return (!mDownloading.empty());
}

}
