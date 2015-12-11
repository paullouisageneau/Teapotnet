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

#include "tpn/cache.h"
#include "tpn/config.h"
#include "tpn/resource.h"
#include "tpn/store.h"

#include "pla/file.h"
#include "pla/directory.h"

namespace tpn
{

Cache *Cache::Instance = NULL;
  
Cache::Cache(void)
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
	
	class PrefetchTask : public Task
	{
	public:
		PrefetchTask(const BinaryString &target) { this->target = target; }
		
		void run(void)
		{
			try {
				Resource resource(target);
				Resource::Reader reader(&resource);
				reader.discard();		// read everything
			}
			catch(const Exception &e)
			{
				LogWarn("Cache::prefetch", "Prefetching failed for " + target.toString());
			}
			
			delete this;	// autodelete
		}
		
	private:
		BinaryString target;
	};
	
	PrefetchTask *task = new PrefetchTask(target);
	Scheduler::Global->schedule(task);
	return false;
}

String Cache::move(const String &filename)
{
	BinaryString digest;
	File file(filename);
	Sha256().compute(file, digest);
	file.close();
	
	String destination = path(digest);
	File::Rename(filename, destination);
	return destination;
}

String Cache::path(const BinaryString &digest) const
{
	Assert(!digest.empty());
	return mDirectory + Directory::Separator + digest.toString();
}

}
