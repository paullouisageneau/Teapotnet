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

#ifndef TPN_FOUNTAIN_H
#define TPN_FOUNTAIN_H

#include "tpn/include.h"
#include "tpn/binarystring.h"

namespace tpn
{

class Fountain : protected Synchronizable
{
public:
	static void Prefetch(const BinaryString &target);
	static void Hint(const BinaryString &target, const String &name, const Set<Identifier> &sources, int64_t size = -1);
	
	Fountain(void);
	virtual ~Fountain(void);
	
	void addSources(const Set<Identifier> &sources);
	
	int64_t size(void) const;	// range size
	int64_t begin(void) const;
	int64_t end(void) const;
	bool finished(void) const;	// true if the whole file is downloaded
	
	void start(bool autoDelete = false);
	void stop(void);
	bool isStarted(void) const;
	
	struct Combination
	{
		Combination(void) { this->offset = 0; }
		Combination(int64_t first) { this->first = first; this->coeffs.append(1); }
		~Combination(void);

		int64_t first;
		Array<uint8_t> coeffs;
		BinaryString data;
	};

	void generate(int64_t first, int64_t last, Combination &c);
	void generate(int64_t offset, Combination &c);	
	void solve(const Combination &c);

protected:
	virtual size_t readBlock(int64_t offset, char *buffer, size_t size) = 0;
	virtual void writeBlock(int64_t offset, const char *data, size_t size) = 0;
	virtual size_t hashBlock(int64_t offset, BinaryString &digest);

private:
	bool mAutoDelete;
	
	// TODO

	class CacheEntry : public Synchronizable
	{
	public:
		CacheEntry(const BinaryString &target);
		~CacheEntry(void);
		
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
