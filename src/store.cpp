/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "store.h"
#include "directory.h"
#include "sha512.h"
#include "file.h"

namespace arc
{

const String Store::DatabaseDirectory = "db";
const size_t Store::ChunkSize = 256*1024;		// 256 Kio

Store::Store(void)
{

}

Store::~Store(void)
{

}

void Store::addDirectory(const String &path)
{
	mDirectories.insert(path);
}

void Store::removeDirectory(const String &path)
{
	mDirectories.erase(path);
}

void Store::refresh(void)
{
	for(Set<String>::iterator it = mDirectories.begin();
			it != mDirectories.end();
			++it)
	{
		refreshDirectory(*it);
	}
}

void Store::refreshDirectory(const String &directoryPath)
{
	Log("Store", String("Refreshing directory: ")+directoryPath);

	Directory dir(directoryPath);
	while(dir.nextFile())
	{
		if(dir.fileIsDir())
		{
			refreshDirectory(dir.filePath());
			continue;
		}

		ByteString pathHash;
		Sha512::Hash(dir.filePath(), pathHash);
		String entry = DatabaseDirectory+Directory::Separator+pathHash.toString();

		if(File::Exist(entry))
		{
			try {
				File file(entry, File::Read);

				String path;
				SerializableMap<String,String> header;
				file.readLine(path);
				file.read(header);

				time_t time;
				size_t size;
				String hash;
				size_t chunkSize;
				unsigned chunkCount;
				header["time"] >> time;
				header["size"] >> size;
				header["chunk-size"] >> chunkSize;
				header["chunk-count"] >> chunkCount;
				header["hash"] >> hash;

				// If the file has not changed, don't hash it again
				if(size == dir.fileSize() && time == dir.fileTime())
				{
					mResources.insert(Identifier(dataHash),dir.filePath());
					continue;
				}
			}
			catch(Exception &e)
			{
				Log("Store", String("Corrupted entry for ")+dir.fileName()+": "+e.what());
			}
		}

		Log("Store", String("Hashing file: ")+dir.fileName());

		try {
			ByteString dataHash;
			File data(dir.filePath(), File::Read);
			Sha512::Hash(data, dataHash);
			data.close();

			size_t   chunkSize = ChunkSize;
			unsigned chunkCount = dir.fileSize()/ChunkSize + 1;

			StringMap header;
			header["time"] << dir.fileTime();
			header["size"] << dir.fileSize();
			header["chunk-size"] << chunkSize;
			header["chunk-count"] << chunkCount;
			header["hash"] << dataHash;

			File file(entry, File::Write);
			file.writeLine(dir.filePath());
			file.write(header);

			data.open(dir.filePath(), File::Read);

			for(unsigned i=0; i<chunkCount; ++i)
			{
				dataHash.clear();
				AssertIO(Sha512::Hash(data, ChunkSize, dataHash));
				file.writeLine(dataHash);
			}

			data.close();

			mResources.insert(Identifier(dataHash),dir.filePath());
		}
		catch(Exception &e)
		{
			Log("Store", String("Hashing failed for ")+dir.fileName()+": "+e.what());
			File::Remove(entry);
		}
	}
}

ByteStream *Store::get(const Identifier &identifier)
{
	String url;
	if(mResources.get(identifier,url)) return false;
	return get(url);
}

ByteStream *Store::url(const String &url)
{
	if(!File::exists(url)) return NULL;
	return new File(url);
}

}
