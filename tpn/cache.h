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
#include "tpn/synchronizable.h"

namespace tpn
{

class Cache : public Synchronizable
{
public:
	Cache(void);
	~Cache(void);
	
	void prefetch(const BinaryString &target);
	void sync(const BinaryString &target, const String &filename);
	
	void push(const BinaryString &target, ByteArray &input);
	bool pull(const BinaryString &target, int64_t begin, int64_t end, ByteArray &result);
	
private:
	class Entry
	{
	public:
		Entry(Cache *cache, const BinaryString &target);
		~Entry(void);
		
		Fountain *fountain(void);
		Stream *stream(void);
		
	private:
		Cache *mCache;
		BinaryString mTarget;
		String mFileName;
		FileFountain *mFountain;	// Null if unused
		Map<int, BinaryString> mChunks;
		
		// TODO: filefoutain should be deleted if unused
	};
	
	Entry *getEntry(const BinaryString &target);
	Map<BinaryString, Entry*> mEntries;
};

}

#endif
