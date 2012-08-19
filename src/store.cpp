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

Store::Store(void)
{

}

Store::~Store(void)
{
	// TODO: delete resources
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
			File file(entry, File::Read);
			String path;
			time_t time;
			size_t size;
			String hash;
			file.readLine(path);
			file.readLine(time);
			file.readLine(size);
			file.readLine(hash);

			// If the file has not changed, don't hash it again
			if(size == dir.fileSize() && time == dir.fileTime())
			{
				//mResources.insert(Identifier(hash));
				continue;
			}
		}

		Log("Store", String("Hashing file: ")+dir.fileName());

		ByteString dataHash;
		File data(dir.filePath(), File::Read);

		Sha512::Hash(data, dataHash);
		File file(entry, File::Write);
		file.writeLine(dir.filePath());
		file.writeLine(dir.fileTime());
		file.writeLine(dir.fileSize());
		file.writeLine(dataHash);
	}
}

Resource *Store::get(const Identifier &identifier)
{
	Resource *resource;
	if(mResources.get(identifier,resource)) return resource;
	else return NULL;
}

}
