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

#ifndef TPN_SPLICER_H
#define TPN_SPLICER_H

#include "tpn/include.h"
#include "tpn/synchronizable.h"
#include "tpn/stripedfile.h"
#include "tpn/identifier.h"
#include "tpn/request.h"
#include "tpn/time.h"
#include "tpn/array.h"
#include "tpn/set.h"

namespace tpn
{

// "I'll wrap you in a sheet !"

class Splicer : protected Synchronizable, public Task, public ByteStream
{
public:
	static void Prefetch(const ByteString &target);
	static void Hint(const ByteString &target, const String &name, const Set<Identifier> &sources, int64_t size = -1);
	
	Splicer(const ByteString &target, int64_t begin = 0, int64_t end = -1);
	~Splicer(void);
	
	void addSources(const Set<Identifier> &sources);
	
	int64_t size(void) const;	// range size
	int64_t begin(void) const;
	int64_t end(void) const;
	bool finished(void) const;	// true if the whole file is downloaded
	
	void start(bool autoDelete = false);
	void stop(void);
	bool isStarted(void) const;
	
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	
private:
	bool query(int i, const Identifier &source);
	void run(void);
	
	Array<Request*> mRequests;
	Array<StripedFile*> mStripes;
	unsigned mFirstBlock, mCurrentBlock;
	int64_t mBegin, mEnd, mPosition;
	bool mAutoDelete;
	
	class CacheEntry : public Synchronizable
	{
	public:
		CacheEntry(const ByteString &target);
		~CacheEntry(void);
		
		String fileName(void);
		String name(void) const;
		ByteString target(void) const;
		int64_t size(void) const;
		size_t blockSize(void) const;
		bool finished(void) const;	// true if the whole file is finished
		
		Time lastAccessTime(void) const;
		void setAccessTime(void);
		
		unsigned block(int64_t position) const;
		
		void hintName(const String &name);
		void hintSize(int64_t size);
		void hintSources(const Set<Identifier> &sources);
		bool getSources(Set<Identifier> &sources);
		void refreshSources(void);
		
		bool isBlockFinished(unsigned block) const;
		bool markBlockFinished(unsigned block);	// true if block was finished
		
		bool isBlockDownloading(unsigned block) const;
		bool markBlockDownloading(unsigned block, bool state = true); // true if block was finished
		bool isDownloading(void) const;
		
	private:
		ByteString mTarget;
		String mFileName;
		bool mIsFileInCache;
		String mName;
		int64_t mSize;
		unsigned mBlockSize;
		Time mTime;
	  
		Set<Identifier> mSources;
		Array<bool> mFinishedBlocks;
		Set<unsigned> mDownloading;
	};
	
	CacheEntry *mCacheEntry;
	Set<Identifier> mSources;
	
	static Splicer::CacheEntry *GetCacheEntry(const ByteString &target);
	static Map<ByteString, CacheEntry*> Cache;
	static Mutex CacheMutex;
};

}

#endif
