/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef TPN_CACHE_H
#define TPN_CACHE_H

#include "tpn/include.h"
#include "tpn/filefountain.h"

namespace tpn
{

class Cache
{
public:
	Cache(void);
	~Cache(void);
	
	void prefetch(const BinaryString &target);
	void hint(const BinaryString &target, const String &name, const Set<Identifier> &sources, int64_t size = -1);


private:
	class Entry : public FountainFile
	{
	public:
		Entry(const BinaryString &target);
		~Entry(void);
		
		String fileName(void);
		String name(void) const;
		BinaryString target(void) const;
		int64_t size(void) const;
		bool finished(void) const;	// true if the whole file is finished
		
		Time lastAccessTime(void) const;
		void setAccessTime(void);
		
		unsigned block(int64_t position) const;
		
		void hintName(const String &name);
		void hintSize(int64_t size);
	
		bool getSources(Set<Identifier> &sources);
		void refreshSources(void);
		
	private:
		BinaryString mTarget;
		String mFileName;
		String mMapFileName;
		bool mIsFileInCache;
		String mName;
		int64_t mSize;
		Time mTime;
	  	
		Set<Identifier> mSources;
		Array<bool> mFinishedBlocks;
		Set<unsigned> mDownloading;
	};
	
	CacheEntry *mCacheEntry;
	
	static Splicer::CacheEntry *GetCacheEntry(const BinaryString &target);
	static Map<BinaryString, CacheEntry*> Cache;
	static Mutex CacheMutex;
};

}

#endif
