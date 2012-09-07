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

Store *Store::Instance = NULL;

Store::Store(void)
{
	Interface::Instance->add("/files", this);
}

Store::~Store(void)
{
	Interface::Instance->remove("/files");
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

bool Store::get(const Identifier &identifier, Entry &entry, bool content)
{
	entry.content = NULL;

	try {
		String entryName = DatabaseDirectory+Directory::Separator+identifier.toString();
		String url;
		if(mFiles.get(identifier,url))	// Hash is on content
		{
			Identifier hash;
			Sha512::Hash(url, hash);
			entryName = DatabaseDirectory+Directory::Separator+hash.toString();

			if(File::Exist(entryName))		// TODO: force reindexation
			{
				File file(entryName, File::Read);
				file.readLine(entry.path);
				file.read(entry.info);

				Assert(entry.info.get("type") != "directory");
				if(content) entry.content = new File(entry.path, File::Read);	// content = file
			}
		}
		else if(File::Exist(entryName))		// Hash is on URL
		{
			entry.content = new File(entryName, File::Read);	// content = meta
			entry.content->readLine(entry.path);
			entry.content->read(entry.info);

			if(!content)
			{
				delete entry.content;
				entry.content = NULL;
			}
		}
	}
	catch(Exception &e)
	{
		if(entry.content)
		{
			delete entry.content;
			entry.content = NULL;
		}

		Log("Store", String("Corrupted entry for ")+identifier.toString()+": "+e.what());
		return false;
	}

	return true;
}

bool Store::get(const String &url, Entry &entry, bool content)
{
	Identifier hash;

	try {
		if(url.find('/') == String::NotFound)	// url is a hash
		{
			String s(url);
			s >> hash;
		}
		else {
			Sha512::Hash(url, hash);
		}
	}
	catch(...)
	{
		// TODO: Log
		return false;
	}

	return get(hash, entry, content);
}

void Store::http(const String &prefix, Http::Request &request)
{
	try {
		const String &url = request.url;

		if(request.url == "/")
		{
			Http::Response response(request,200);
			response.send();

			Html page(response.sock);
			page.header(request.url);
			page.open("h1");
			page.text(request.url);
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
					Http::Response response(request, 301);	// Moved Permanently
					response.headers["Location"] = prefix+url+"/";
					response.send();
					return;
				}

				Http::Response response(request, 200);
				response.send();

				Html page(response.sock);
				page.header(request.url);
				page.open("h1");
				page.text(request.url);
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
				Http::Response response(request,200);
				response.headers["Content-Type"] = "application/octet-stream";	// TODO
				response.send();

				File file(path, File::Read);
				response.sock->writeBinary(file);
			}
			else throw Exception("File not found");
		}
	}
	catch(const Exception &e)
	{
		Log("Store::http",e.what());
		throw 404;	// Httpd handles integer exceptions
	}
}

void Store::refreshDirectory(const String &dirUrl, const String &dirPath)
{
	Log("Store", String("Refreshing directory: ")+dirUrl);

	Identifier hash;
	Sha512::Hash(dirUrl, hash);
	String entryName = DatabaseDirectory+Directory::Separator+hash.toString();
	File dirEntry(entryName, File::Write);
	dirEntry.writeLine(dirPath);

	StringMap header;
	header["type"] = "directory";
	header["time"] << time(NULL);
	dirEntry.write(header);

	Directory dir(dirPath);
	while(dir.nextFile())
	{
		String url(dirUrl + Directory::Separator + dir.fileName());
		ByteString urlHash;
		Sha512::Hash(url, urlHash);
		String entryName = DatabaseDirectory+Directory::Separator+urlHash.toString();

		if(!dir.fileIsDir() && File::Exist(entryName))
		{
			try {
				File file(entryName, File::Read);

				String path;
				StringMap header;
				file.readLine(path);
				file.read(header);
				
				StringMap origHeader(header);

				time_t time;
				header["time"] >> time;
				if(!header.contains("type")) throw Exception("Missing type field");				
				if(header["type"] == "directory") throw Exception("Invalid type for file");
				
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
					dirEntry.write(origHeader);
					continue;
				}
			}
			catch(Exception &e)
			{
				Log("Store", String("Corrupted entry for ")+dir.fileName()+": "+e.what());
			}
		}

		Log("Store", String("Processing: ")+dir.fileName());

		try {

			if(dir.fileIsDir())
			{
				refreshDirectory(url, dir.filePath());
			}
			else {
				ByteString dataHash;
				File data(dir.filePath(), File::Read);
				Sha512::Hash(data, dataHash);
				data.close();

				size_t   chunkSize = ChunkSize;
				unsigned chunkCount = dir.fileSize()/ChunkSize + 1;

				StringMap header;
				dir.getFileInfo(header);
				header["type"] = "file";
				header["chunk-size"] << chunkSize;
				header["chunk-count"] << chunkCount;
				header["hash"] << dataHash;
				
				mFiles.insert(Identifier(dataHash), url);

				File file(entryName, File::Write);
				file.writeLine(dir.filePath());
				file.write(header);
				dirEntry.write(header);

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
			Log("Store", String("Processing failed for ")+dir.fileName()+": "+e.what());
			File::Remove(entryName);
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
