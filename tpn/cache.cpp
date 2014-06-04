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

Cache::Cache(void)
{
	
}

Cache::~Cache(void)
{
	
}

void Cache::prefetch(const BinaryString &target)
{
	
}

void Cache::sync(const BinaryString &target, const String &filename)
{
	
}

void Cache::push(const BinaryString &target, ByteArray &input)
{
	
}

bool Cache::pull(const BinaryString &target, ByteArray &output)
{
	
}

void Cache::registerBlock(Block *block)
{
	const BinaryString &target = block->digest();
	mBlocks[target].insert(block);
	
	// TODO: if data is available, push it to the block
}

void Cache::unregisterBlock(Block *block)
{
	Map<BinaryString, Set<Blocks*> >::iterator it = mBlocks.find(target);
	if(it != mBlocks.end())
	{
		Set<Blocks*> &set = it->second;
		set.erase(block);
		if(set.empty()) mBlocks.erase(it);
	}
}

Block *Cache::getBlock(const BinaryString &target)
{
	Map<BinaryString, Set<Blocks*> >::iterator it = mBlocks.find(target);
	if(it != mBlocks.end())
	{
		Set<Blocks*> &set = it->second;
		if(!set.empty())
		{
			return *set.begin();
		}
	}
	
	Block *block = new Block(target);
	mTempBlocks.insert(target, block);
	return block;
}

}
