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

	Identifier hash;
	Sha512::Hash("/", hash);
	String entryName = DatabaseDirectory+Directory::Separator+hash.toString();
	SafeWriteFile dirEntry(entryName);
	dirEntry.writeLine("");

	StringMap header;
	header["name"] = "/";
	header["type"] = "directory";
	header["url"] = "/";
	header["time"] << time(NULL);
	dirEntry.write(header);
	
	for(StringMap::iterator it = mDirectories.begin();
			it != mDirectories.end();
			++it)
	{	
		refreshDirectory("/" + it->first, it->second);
		
		StringMap info;
		info["name"] = it->first;
		info["type"] = "directory";
		info["url"]  = "/" + it->first; 
		info["time"] << time(NULL);
		dirEntry.write(info);
	}
}

bool Store::get(const Identifier &identifier, Entry &entry, bool content)
{
	entry.content = NULL;
	entry.info.clear();
	
	if(entry.identifier != identifier)
	{
		entry.identifier = identifier;
		entry.url.clear();
	}

	try {
		String entryName = DatabaseDirectory+Directory::Separator+identifier.toString();
		String url;
		if(mFiles.get(identifier,url))	// Hash is on content
		{
			Log("Store", "Requested \"" + url + "\" from data hash");  
		
			Identifier hash;
			Sha512::Hash(url, hash);
			entryName = DatabaseDirectory+Directory::Separator+hash.toString();

			if(!File::Exist(entryName))
			{
				Log("Store", "WARNING: No entry for " + hash.toString());
				return false;		// TODO: force reindexation
			}
			
			File file(entryName, File::Read);
			file.readLine(entry.path);
			file.read(entry.info);
			if(entry.url.empty()) entry.info.get("url", entry.url);
			
			if(content && entry.info.get("type") != "directory") 
				entry.content = new File(entry.path, File::Read);	// content = file

			return true;
		}
		else if(File::Exist(entryName))		// Hash is on URL
		{
			entry.content = new File(entryName, File::Read);	// content = meta
			entry.content->readLine(entry.path);
			entry.content->read(entry.info);
			if(entry.url.empty()) entry.info.get("url", entry.url);

			Log("Store", "Requested \"" + entry.info.get("name") + "\" from url");
			
			if(!content)
			{
				delete entry.content;
				entry.content = NULL;
			}
			
			return true;
			
		}
	}
	catch(const Exception &e)
	{
		if(entry.content)
		{
			delete entry.content;
			entry.content = NULL;
		}

		Log("Store", "Corrupted entry for \""+identifier.toString()+"\": "+e.what());
		return false;
	}

	return false;
}

bool Store::get(const String &url, Entry &entry, bool content)
{
  	entry.content = NULL;
	entry.url = url;
	entry.identifier.clear();
	entry.info.clear();
	
  	if(url.empty()) return false;
	
	try {
		if(url.find('/') == String::NotFound)	// url is a hash
		{
			String s(url);
			s >> entry.identifier;
		}
		else {
			Sha512::Hash(url, entry.identifier);
		}
	}
	catch(...)
	{
		return false;
	}

	return get(entry.identifier, entry, content);
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
			else throw 404;
		}
	}
	catch(const std::exception &e)
	{
		Log("Store::http", e.what());
		throw 500;	// Httpd handles integer exceptions
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
	header["url"] = dirUrl;
	header["name"] = dirUrl.substr(dirUrl.lastIndexOf('/')+1);
	header["type"] = "directory";
	header["time"] << time(NULL);
	dirEntry.write(header);

	Directory dir(dirPath);
	while(dir.nextFile())
	{
		String url(dirUrl + '/' + dir.fileName());
		Identifier urlHash;
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
				
				Assert(path == dir.filePath());	
				Assert(header.get("type") != "directory");
				Assert(header.get("url") == url);
				
				StringMap origHeader(header);
				
				time_t time;
				size_t size;
				Identifier hash;
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
					mFiles.insert(hash, url);
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
			  	StringMap info;
				dir.getFileInfo(info);
				info["type"] = "directory";
				info["url"] = url;
				dirEntry.write(info);
				
				refreshDirectory(url, dir.filePath());
			}
			else {
				Identifier dataHash;
				File data(dir.filePath(), File::Read);
				Sha512::Hash(data, dataHash);
				data.close();

				size_t   chunkSize = ChunkSize;
				unsigned chunkCount = dir.fileSize()/ChunkSize + 1;

				StringMap header;
				dir.getFileInfo(header);
				header["type"] = "file";
				header["url"] = url;
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
