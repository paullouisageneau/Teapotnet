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

#include "tpn/cache.hpp"
#include "tpn/config.hpp"
#include "tpn/resource.hpp"
#include "tpn/store.hpp"

#include "pla/file.hpp"
#include "pla/directory.hpp"
#include "pla/random.hpp"
#include "pla/exception.hpp"

namespace tpn
{

Cache *Cache::Instance = NULL;

Cache::Cache(void) :
	mScheduler(2)
{
	mDirectory = Config::Get("cache_dir");

	if(!Directory::Exist(mDirectory))
		Directory::Create(mDirectory);
}

Cache::~Cache(void)
{
	
}

bool Cache::prefetch(const BinaryString &target)
{
	// Test local availability
	if(Store::Instance->hasBlock(target))
	{
		Resource resource(target, true);	// local only
		if(resource.isLocallyAvailable())
			return true;
	}
	
	mScheduler.schedule(Scheduler::clock::now(), [target]() {
		try {
			Resource resource(target);
			Resource::Reader reader(&resource);
			reader.discard();		// read everything
		}
		catch(const Exception &e)
		{
			LogWarn("Cache::prefetch", "Prefetching failed for " + target.toString());
		}
	});

	return false;
}

String Cache::move(const String &filename, BinaryString *fileDigest)
{
	// Check file size
	int64_t fileSize = File::Size(filename);
	int64_t maxCacheFileSize = 0;
	Config::Get("cache_max_file_size").extract(maxCacheFileSize);	// MiB
	if(fileSize > maxCacheFileSize*1024*1024)
		throw Exception("File is too large for cache: " + filename);
	
	// Free some space
	int64_t maxCacheSize = 0;
	Config::Get("cache_max_size").extract(maxCacheSize);	// MiB
	if(freeSpace(mDirectory, maxCacheSize*1024*1024, fileSize) < fileSize)
		throw Exception("Not enough free space in cache for " + filename);
	
	BinaryString digest;
	File file(filename);
	Sha256().compute(file, digest);
	file.close();
	
	if(fileDigest) *fileDigest = digest;
	
	String destination = path(digest);
	File::Rename(filename, destination);
	return destination;
}

String Cache::path(const BinaryString &digest) const
{
	Assert(!digest.empty());
	return mDirectory + Directory::Separator + digest.toString();
}

int64_t Cache::freeSpace(const String &path, int64_t maxSize, int64_t space)
{
	int64_t totalSize = 0;

	try {
		StringList list;
		Directory dir(path);
		while(dir.nextFile())
		{
			if(!dir.fileIsDir())
			{
				list.push_back(dir.fileName());
				totalSize+= dir.fileSize();
			}
		}
		
		if(maxSize > totalSize)
		{
			int64_t freeSpace = Directory::GetAvailableSpace(path);
			int64_t margin = 1024*1024;	// 1 MiB
			freeSpace = std::max(freeSpace - margin, int64_t(0));
			maxSize = totalSize + std::min(maxSize-totalSize, freeSpace);
		}
		
		space = std::min(space, maxSize);
		
		while(!list.empty() && totalSize > maxSize - space)
		{
			int r = Random().uniform(0, int(list.size()));
			StringList::iterator it = list.begin();
			while(r--) ++it;
			
			String filePath = path + Directory::Separator + *it;
			
			// Delete file
			if(!File::Remove(filePath))
				continue;
			
			// Notify Store
			Store::Instance->notifyFileErasure(filePath);
			
			totalSize-= File::Size(filePath);
			list.erase(it);
		}
	}
	catch(const Exception &e)
	{
		throw Exception(String("Unable to free space: ") + e.what());
	}

	return std::max(maxSize - totalSize, int64_t(0));
}

}
