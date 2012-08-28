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
#include "html.h"

namespace arc
{

const String Store::DatabaseDirectory = "db";
const size_t Store::ChunkSize = 256*1024;		// 256 Kio

Store *Store::Instance = new Store;

Store::Store(void)
{

}

Store::~Store(void)
{

}

void Store::addDirectory(const String &name, const String &path)
{
	mDirectories.insert(name, path);
}

void Store::removeDirectory(const String &name)
{
	mDirectories.erase(name);
}

void Store::refresh(void)
{
	mFiles.clear();

	for(StringMap::iterator it = mDirectories.begin();
			it != mDirectories.end();
			++it)
	{
		refreshDirectory(it->first, it->second);
	}
}

File *Store::get(const Identifier &identifier)
{
	String url;
	if(mFiles.get(identifier,url)) return NULL;
	return get(url);
}

File *Store::get(const String &url)
{
	String path(urlToPath(url));
	if(!File::Exist(path)) return NULL;
	return new File(path);
}

bool Store::info(const Identifier &identifier, StringMap &map)
{
	String url;
	if(mFiles.get(identifier,url)) return false;
	return info(url, map);
}

bool Store::info(const String &url, StringMap &map)
{
	ByteString hash;
	Sha512::Hash(url, hash);
	String entry = DatabaseDirectory+Directory::Separator+url;
	if(!File::Exist(entry)) return false;

	try {
		File file(entry, File::Read);

		String path;
		SerializableMap<String,String> header;
		file.readLine(path);
		file.read(map);
	}
	catch(Exception &e)
	{
		Log("Store", String("Corrupted entry for ")+url+": "+e.what());
		return false;
	}

	return true;
}

void Store::http(Httpd::Request &request)
{
	try {
		const String &url = request.file;

		if(request.file == "/")
		{
			Httpd::Response response(request,200);
			response.send();

			Html page(response.sock);
			page.header(request.file);
			page.open("h1");
			page.text(request.file);
			page.close("h1");

			for(StringMap::iterator it = mDirectories.begin();
						it != mDirectories.end();
						++it)
			{
				page.link(it->first, it->first);
				page.br();
			}

			page.footer();
		}
		else {
			String path = urlToPath(url);
			if(path[path.size()-1] == Directory::Separator) path.resize(path.size()-1);

			if(Directory::Exist(path))
			{
				if(url[url.size()-1] != '/')
				{
					Httpd::Response response(request, 301);	// Moved Permanently
					response.headers["Location"] = url+"/";
					response.send();
					return;
				}

				Httpd::Response response(request, 200);
				response.send();

				Html page(response.sock);
				page.header(request.file);
				page.open("h1");
				page.text(request.file);
				page.close("h1");

				Directory dir(path);
				while(dir.nextFile())
				{
					page.link(dir.fileName(), dir.fileName());
					page.br();
				}

				page.footer();
			}
			else if(File::Exist(path))
			{
				Httpd::Response response(request,200);
				response.headers["Content-Type"] = "application/octet-stream";	// TODO
				response.send();

				File file(path, File::Read);
				response.sock->writeBinary(file);
			}
			else throw Exception("File not found");
		}
	}
	catch(...)
	{
		// TODO: Log
		throw 404;	// Httpd handles integer exceptions
	}
}

void Store::refreshDirectory(const String &dirUrl, const String &dirPath)
{
	Log("Store", String("Refreshing directory: ")+dirUrl);

	Directory dir(dirPath);
	while(dir.nextFile())
	{
		String url(dirUrl + Directory::Separator + dir.fileName());

		ByteString urlHash;
		Sha512::Hash(url, urlHash);
		String entry = DatabaseDirectory+Directory::Separator+urlHash.toString();

		if(File::Exist(entry))
		{
			try {
				File file(entry, File::Read);

				String path;
				StringMap header;
				file.readLine(path);
				file.read(header);
				
				time_t time;
				header["time"] >> time;
				if(!header.contains("type")) throw Exception("Missing type field");				

				if(header["type"] == "directory") 
				{
					continue;
				}
				else {
					size_t size;
					String hash;
					size_t chunkSize;
					unsigned chunkCount;
					header["size"] >> size;
					header["chunk-size"] >> chunkSize;
					header["chunk-count"] >> chunkCount;
					header["hash"] >> hash;

					// If the file has not changed, don't hash it again
					if(size == dir.fileSize() && time == dir.fileTime())
					{
						mFiles.insert(Identifier(hash), url);
						continue;
					}
				}
			}
			catch(Exception &e)
			{
				Log("Store", String("Corrupted entry for ")+dir.fileName()+": "+e.what());
			}
		}

		Log("Store", String("Processing: ")+dir.fileName());

		try {
			StringMap header;
			header["time"] << dir.fileTime();

			if(dir.fileIsDir())
                	{
				header["type"] << "directory";
                	
				File file(entry, File::Write);
                                file.writeLine(dir.filePath());
                                file.write(header);
				file.close();

                                refreshDirectory(url, dir.filePath());
			}
			else {
				ByteString dataHash;
				File data(dir.filePath(), File::Read);
				Sha512::Hash(data, dataHash);
				data.close();

				size_t   chunkSize = ChunkSize;
                        	unsigned chunkCount = dir.fileSize()/ChunkSize + 1;

				header["type"] << "file";
				header["size"] << dir.fileSize();
				header["chunk-size"] << chunkSize;
				header["chunk-count"] << chunkCount;
				header["hash"] << dataHash;

				mFiles.insert(Identifier(dataHash), url);
			
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
			}
		}
		catch(Exception &e)
		{
			Log("Store", String("Hashing failed for ")+dir.fileName()+": "+e.what());
			File::Remove(entry);
		}
	}
}

String Store::urlToPath(const String &url) const
{
	if(url.empty() || url[0] != '/') throw Exception("Invalid URL");

	String dir(url.substr(1));
	String path = dir.cut('/');

	// Do not accept the parent directory symbol as a security protection
	if(path.find("..") != String::NotFound) throw Exception("Invalid URL");

	String dirPath;
	if(!mDirectories.get(dir,dirPath)) throw Exception("Directory does not exists");

	path.replace('/',Directory::Separator);

	return dirPath + Directory::Separator + path;
}

}
