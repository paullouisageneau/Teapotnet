/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "store.h"
#include "user.h"
#include "directory.h"
#include "sha512.h"
#include "html.h"

namespace tpot
{

Map<Identifier,String> Store::Resources;
Mutex Store::ResourcesMutex;
  
const size_t Store::ChunkSize = 256*1024;		// 256 Kio

bool Store::GetResource(const Identifier &hash, Entry &entry, bool content)
{
	entry.content = NULL;
	entry.info.clear();
	entry.hash = hash;
	entry.url.clear();
	entry.path.clear();

	String entryName;
	ResourcesMutex.lock();
	if(Resources.get(hash, entryName))	// Hash is on content
	{
		ResourcesMutex.unlock();
		try {
			Log("Store::GetResource", "Requested " + hash.toString());  

			if(!File::Exist(entryName))
			{
				Log("Store", "WARNING: No entry for " + hash.toString());
				ResourcesMutex.lock();
				Resources.erase(hash);
				ResourcesMutex.unlock();
				return false;
			}
				
			File file(entryName, File::Read);
			file.readLine(entry.path);
			file.read(entry.info);
			if(entry.url.empty()) entry.info.get("url", entry.url);
		}
		catch(const Exception &e)
		{
			Log("Store", "Corrupted entry for \""+hash.toString()+"\": "+e.what());
			return false;
		}
		
		if(content && entry.info.get("type") != "directory") 
			entry.content = new File(entry.path, File::Read);	// content = file

		return true;
	}
	
	ResourcesMutex.unlock();
	return false;
}

Store::Store(User *user) :
	mUser(user)
{
  	Assert(mUser != NULL);
	mFileName = mUser->profilePath() + "directories";
	mDatabasePath = mUser->profilePath() + "db" + Directory::Separator;
	
	if(!Directory::Exist(mDatabasePath))
		Directory::Create(mDatabasePath);
	
	Interface::Instance->add("/"+mUser->name()+"/files", this);
	
	try {
	  File file(mFileName, File::Read);
	  file.read(mDirectories);
	  file.close();
	  start();
	}
	catch(...)
	{
	  
	}
}

Store::~Store(void)
{
	Interface::Instance->remove("/"+mUser->name()+"/files");
}

User *Store::user(void) const
{
	return mUser; 
}

String Store::userName(void) const
{
	return mUser->name(); 
}

void Store::addDirectory(const String &name, const String &path)
{
	Synchronize(this);
  
	if(!Directory::Exist(path)) throw Exception("The directory does not exist: "+path);
	Directory test(path);
	test.close();
	
	mDirectories.insert(name, path);
	save();
	start();
}

void Store::removeDirectory(const String &name)
{
  	Synchronize(this);
  
	if(mDirectories.contains(name))
	{
  		mDirectories.erase(name);
		save();
		start();
	}
}

void Store::save(void) const
{
  	Synchronize(this);
  
	File file(mFileName, File::Write);
	file.write(mDirectories);
	file.close();
}

void Store::update(void)
{
	Synchronize(this);
	
	mUpdateMutex.lock();
	Log("Store::update", "Started");
	
	Identifier hash;
	Sha512::Hash("/", hash);
	String entryName = mDatabasePath + hash.toString();
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
		updateDirectory("/" + it->first, it->second);
		
		StringMap info;
		info["name"] = it->first;
		info["type"] = "directory";
		info["url"]  = "/" + it->first; 
		info["time"] << time(NULL);
		dirEntry.write(info);
	}
	
	Log("Store::update", "Finished");
	mUpdateMutex.unlock();
}

bool Store::get(const Identifier &identifier, Entry &entry, bool content)
{
	Synchronize(this);
  
	entry.content = NULL;
	entry.info.clear();
	
	if(entry.hash != identifier)
	{
		entry.hash = identifier;
		entry.url.clear();
	}

	String entryName = mDatabasePath + identifier.toString();
	if(File::Exist(entryName))
	{
		try {
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
	}

	return GetResource(identifier, entry, content);
}

bool Store::get(const String &url, Entry &entry, bool content)
{
	Synchronize(this);
	
  	entry.content = NULL;
	entry.url = url;
	entry.hash.clear();
	entry.info.clear();
	
  	if(url.empty()) return false;
	
	try {
		if(url.find('/') == String::NotFound)	// url is a hash
		{
			String s(url);
			s >> entry.hash;
		}
		else {
			Sha512::Hash(url, entry.hash);
		}
	}
	catch(...)
	{
		return false;
	}

	return get(entry.hash, entry, content);
}

void Store::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	
	try {
		const String &url = request.url;

		if(request.url == "/")
		{
		  	if(request.method == "POST")
			{
				String name = request.post["name"];
				String path = request.post["path"];
				
				if(!path.empty())
				{
				  	if(name.empty()) 
					{
						name = path;
						name = name.cutLast(Directory::Separator);
						name = name.cutLast('/');
						name = name.cutLast('\\');
					}
				  
					try {
					 	addDirectory(name, path);
					}
					catch(const Exception &e)
					{
						Http::Response response(request,200);
						response.send();
						
						Html page(response.sock);
						page.header("Error", prefix + "/");
						page.open("h1");
						page.text("Error");
						page.close("h1");
						page.text(e.what());
						page.footer();
						return;
					}
				}
				
				Http::Response response(request,303);
				response.headers["Location"] = prefix + "/";
				response.send();
				return;
			}
		  
			Http::Response response(request,200);
			response.send();

			Html page(response.sock);
			page.header("Shared files");
			page.open("h1");
			page.text("Shared files");
			page.close("h1");

			for(StringMap::iterator it = mDirectories.begin();
						it != mDirectories.end();
						++it)
			{
				page.link(it->first, it->first);
				page.br();
			}

			page.openForm(prefix+"/","post");
			page.openFieldset("New directory");
			page.label("path","Path"); page.input("text","path"); page.br();
			page.label("name","Name"); page.input("text","name"); page.br();
			page.label("add"); page.button("add","Share directory");
			page.closeFieldset();
			page.closeForm();
			
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

void Store::updateDirectory(const String &dirUrl, const String &dirPath)
{
	Synchronize(this);
	Log("Store", String("Refreshing directory: ")+dirUrl);

	Identifier hash;
	Sha512::Hash(dirUrl, hash);
	String entryName = mDatabasePath + hash.toString();
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
		String entryName = mDatabasePath + urlHash.toString();

		if(!dir.fileIsDir() && File::Exist(entryName))
		{
			try {
				File file(entryName, File::Read);

				String path;
				StringMap header;
				file.readLine(path);
				file.read(header);
				
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
				 	ResourcesMutex.lock();
					Resources.insert(hash, entryName);
					ResourcesMutex.unlock();
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
				
				updateDirectory(url, dir.filePath());
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
				
				ResourcesMutex.lock();
				Resources.insert(hash, entryName);
				ResourcesMutex.unlock();

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
					
					UnPrioritize(this);
				}
			}
		}
		catch(Exception &e)
		{
			Log("Store", String("Processing failed for ")+dir.fileName()+": "+e.what());
			File::Remove(entryName);
		}
		
		UnPrioritize(this);
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

void Store::run(void)
{
	update(); 
}

}
