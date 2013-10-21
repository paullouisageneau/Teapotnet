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

#include "tpn/store.h"
#include "tpn/user.h"
#include "tpn/directory.h"
#include "tpn/sha512.h"
#include "tpn/html.h"
#include "tpn/lineserializer.h"
#include "tpn/jsonserializer.h"
#include "tpn/config.h"
#include "tpn/time.h"
#include "tpn/mime.h"

namespace tpn
{

Store *Store::GlobalInstance = NULL;
Map<ByteString,String> Store::Resources;
Mutex Store::ResourcesMutex;
const String Store::CacheDirectoryName = "_cache";
const String Store::UploadDirectoryName = "_upload";  

bool Store::Get(const ByteString &digest, Resource &resource)
{
	String path;
	ResourcesMutex.lock();
	bool found = Resources.get(digest, path);
	ResourcesMutex.unlock();
	if(!found) return false;
	
	LogDebug("Store::Get", "Requested " + digest.toString());  
	
	if(!File::Exist(path))
	{
		ResourcesMutex.lock();
		Resources.erase(digest);
		ResourcesMutex.unlock();
		return false;
	}
	
	resource.clear();
	resource.mDigest = digest;
	resource.mPath = path;
	resource.mUrl = path.afterLast(Directory::Separator);	// this way the name is available
	resource.mSize = File::Size(path);
	resource.mTime = File::Time(path);
	resource.mStore = Store::GlobalInstance;
	return true;
}

Store::Store(User *user) :
	mUser(user),
	mRunning(false)
{
	if(mUser) mDatabase = new Database(mUser->profilePath() + "files.db");
	else mDatabase = new Database("files.db");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS files\
	(id INTEGER PRIMARY KEY AUTOINCREMENT,\
	parent_id INTEGER,\
	url TEXT UNIQUE,\
	digest BLOB,\
	name_rowid INTEGER,\
	size INTEGER(8),\
	time INTEGER(8),\
	type INTEGER(1),\
	seen INTEGER(1))");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS digest ON files (digest)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS parent_id ON files (parent_id)");
	mDatabase->execute("CREATE VIRTUAL TABLE IF NOT EXISTS names USING FTS3(name)");
	
	// Fix: "IF NOT EXISTS" is not available for virtual tables with old sqlite3 versions
	//Database::Statement statement = mDatabase->prepare("select DISTINCT tbl_name from sqlite_master where tbl_name = 'names'");
	//if(!statement.step()) mDatabase->execute("CREATE VIRTUAL TABLE names USING FTS3(name)");	
	//statement.finalize();
	//

	if(mUser) 
	{
		mFileName = mUser->profilePath() + "directories";
		
		const String folder = Config::Get("shared_dir");
		if(!Directory::Exist(folder))
			Directory::Create(folder);
		
		const String subfolder = folder + Directory::Separator + mUser->name();
		if(!Directory::Exist(subfolder))
			Directory::Create(subfolder);
		
		mBasePath = subfolder + Directory::Separator;
	}
	else {
		mFileName = "directories.txt";
		mBasePath = "";
	}
	
	if(File::Exist(mFileName))
	{
		try {
	  		File file(mFileName, File::Read);
	  		LineSerializer serializer(&file);
	  		serializer.input(mDirectories);
	  		file.close();
			
			StringMap::iterator it = mDirectories.begin();
			while(it != mDirectories.end())
			{
				String &path = it->second;
				if(path.empty() || path == "/")
				{
					mDirectories.erase(it++);
					continue;
				}

				if(path[path.size()-1] == '/')
					path.resize(path.size()-1);
				
				++it;
			}
		}
		catch(...) {}
	}
	
	// Special upload directory
	if(mUser) addDirectory(UploadDirectoryName, UploadDirectoryName);
	
	save();
	
	Scheduler::Global->schedule(this, 60.);		// 1 min
	Scheduler::Global->repeat(this, 6*60*60.);	// 6h
	
	if(mUser)
	{
		Interface::Instance->add("/"+mUser->name()+"/files", this);
		Interface::Instance->add("/"+mUser->name()+"/myself/files", this);	// overridden by self contact if it exists
		Interface::Instance->add("/"+mUser->name()+"/explore", this);
	}
}

Store::~Store(void)
{
	Scheduler::Global->remove(this);
	
	if(mUser)
	{
		Interface::Instance->remove("/"+mUser->name()+"/files");
		Interface::Instance->remove("/"+mUser->name()+"/myself/files");
		Interface::Instance->remove("/"+mUser->name()+"/explore");
	}
}

User *Store::user(void) const
{
	return mUser; 
}

String Store::userName(void) const
{
	if(mUser) return mUser->name();
	else return "";
}

void Store::addDirectory(const String &name, String path)
{
	Synchronize(this);
  
	if(!path.empty() && path[path.size()-1] == '/')
		path.resize(path.size()-1);
	
	if(path.empty())	// "/" matches here too
		throw Exception("Invalid directory");
	
	String absPath = absolutePath(path);
	if(!Directory::Exist(absPath))
	{
		if(absPath != path) Directory::Create(absPath);
		else throw Exception("The directory does not exist: " + absPath);
	}
	
	Directory test(absPath);
	test.close();
	
	String oldPath;
	if(!mDirectories.get(name, oldPath) || oldPath != path)
	{
		mDirectories.insert(name, path);
		save();
		start();
	}
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

void Store::getDirectories(Array<String> &array) const
{
	Synchronize(this);
	mDirectories.getKeys(array);
	array.remove(CacheDirectoryName);
	array.remove(UploadDirectoryName);
}

bool Store::moveFileToCache(String &fileName, String name)
{
	Synchronize(this);
	
	// Check file size
	int64_t fileSize = File::Size(fileName);
	int64_t maxCacheFileSize = 0;
	Config::Get("cache_max_file_size").extract(maxCacheFileSize);	// MiB
	if(fileSize > maxCacheFileSize*1024*1024)
	{
		LogDebug("Store", "File is too large for cache: " + name);
		return false;
	}
	
	if(!mDirectories.contains(CacheDirectoryName))
	{
		// Create cache directory
        	if(mUser) addDirectory(CacheDirectoryName, CacheDirectoryName);
        	else {
			String cacheDir = Config::Get("cache_dir");
			Directory::Create(cacheDir);
			addDirectory(CacheDirectoryName, cacheDir);
		}
	}

	// Free some space
	int64_t maxCacheSize = 0;
	Config::Get("cache_max_size").extract(maxCacheSize);	// MiB
	if(freeSpace(CacheDirectoryName, maxCacheSize*1024*1024, fileSize) < fileSize)
	{
		// This is not normal
		LogWarn("Store", "Not enough free space in cache for " + name);
		return false;
	}
	
	if(name.empty()) name = fileName.afterLast(Directory::Separator);
	LogInfo("Store", "Moving to cache: " + name);
	
	// Find a unique name
	int count = 0;
	String url, path;
	do {
		url = "/" + CacheDirectoryName + "/" + name.beforeLast('.');
		if(++count >= 2) url+= "_" + String::number(count);
		if(name.contains('.')) url+= "." + name.afterLast('.');
		path = urlToPath(url);
	} 
	while(File::Exist(path));

	// Copy the file
	File placeholder(path, File::Write);
	placeholder.close();

	try {
		Desynchronize(this);
		File::Rename(fileName, path);
	}
	catch(...)
	{
		File::Remove(path);
		throw;
	}

	update(url, path);
	fileName = path;
	return true;
}

void Store::save(void) const
{
  	Synchronize(this);
  
	File file(mFileName, File::Write);
	LineSerializer serializer(&file);
	serializer.output(mDirectories);
	file.close();
}

void Store::start(void)
{
	Synchronize(this);
	if(!mRunning) Scheduler::Global->schedule(this);
}

bool Store::query(const Resource::Query &query, Resource &resource)
{
	Synchronize(this);
	
	if(query.mUrl == "/")	// root directory
	{
		resource.clear();
		resource.mUrl = "/";
		resource.mType = 0;	// directory
		resource.mTime = Time::Now();
		resource.mStore = this;
		return true;
	}
	
	const String fields = "url, digest, type, size, time";
	Database::Statement statement;
	if(prepareQuery(statement, query, fields, true))
	{
		while(statement.step())
		{
			resource.clear();
			statement.retrieve(resource); 
			resource.mPath = urlToPath(resource.mUrl); 
			resource.mStore = this;
			statement.finalize();
			return true;
		}
		
		statement.finalize();
	}
	
	if(this != GlobalInstance) return GlobalInstance->query(query, resource);
	else return false;
}

bool Store::query(const Resource::Query &query, Set<Resource> &resources)
{
	Synchronize(this);

	const String fields = "url, digest, type, size, time";
	Database::Statement statement;
	bool success = prepareQuery(statement, query, fields, false);
	if(success)
	{
		while(statement.step())
		{
			Resource resource;
			statement.retrieve(resource);
			if(query.mUrl == "/" && isHiddenUrl(resource.mUrl)) continue; 
			resource.mPath = urlToPath(resource.mUrl);
			resource.mStore = this;
			resources.insert(resource);
		}

		statement.finalize();
	}
	
	if(this != GlobalInstance) success|= GlobalInstance->query(query, resources);
	return success;
}

void Store::http(const String &prefix, Http::Request &request)
{
	const String &url = request.url;
	if(mUser) mUser->setOnline();

	Synchronize(this);
	
	try {
		if(prefix.afterLast('/') == "explore")
		{
			if(url != "/") throw 404;
			if(this == GlobalInstance)
			{
				if(!request.sock->getRemoteAddress().isLocal())
					throw 401;
			}
			else {
				if(!Config::Get("user_global_shares").toBool())
				  	throw 404;
			}
			
			String path;
			if(!request.get.get("path", path)) 
			{
#ifdef WINDOWS
				DWORD disks = GetLogicalDrives();
				if(!disks) throw 500;

				Http::Response response(request,200);
				response.send();
							
				Html page(response.sock);
				page.header(path);
				
				page.open("div",".box");
				page.open("table",".files");
				for(int i=0; i<25; ++i)
				{
					if(disks & (0x1 << i)) 
					{
						char letter = 0x41+i;
						String name = String(letter) + ":\\";
						String hrName = String(letter) + String(" drive");
						String link = prefix + "/?path=" + name.urlEncode();
						
						page.open("tr");
						page.open("td",".icon");
						page.image("/dir.png");
						page.close("td");
						page.open("td",".filename");
						page.link(link, hrName);
						page.close("td");
						page.open("td",".add");
						page.link(link+"&add=1", "share");
						page.close("td");
						page.close("tr");
					}
				}
				page.close("table");
				page.close("div");
				page.footer();
				return;
#else
				path = "/";
#endif
			}
			
			if(path[path.size()-1] != Directory::Separator)
				path+= Directory::Separator;
		  
			if(!Directory::Exist(path)) throw 404;
			
			if(request.get.contains("add")) 
			{
				path.resize(path.size()-1);
				String name = path.afterLast(Directory::Separator);
				name.remove(':');
				
				if(request.method == "POST")
				{
					if(!user()->checkToken(request.post["token"], "directory_add")) 
						throw 403;
					
					request.post.get("name", name);
					
					try {
						if(name.empty()
							|| name.contains('/') || name.contains('\\') 
							|| name.find("..") != String::NotFound)
								throw Exception("Invalid directory name");

						addDirectory(name, path);
					}
					catch(const Exception &e)
					{
						Http::Response response(request,200);
						response.send();
						
						Html page(response.sock);
						page.header("Error", false, prefix + url);
						page.text(e.what());
						page.footer();
						return;
					}
					
					Http::Response response(request,303);
					response.headers["Location"] = "/"+mUser->name()+"/files/";
					response.send();
					return;
				}
	  
				Http::Response response(request,200);
				response.send();
				  
				Html page(response.sock);
				page.header("Add directory");
				page.openForm(prefix + url + "?path=" + path.urlEncode() + "&add=1", "post");
				page.input("hidden", "token", user()->generateToken("directory_add"));
				page.openFieldset("Add directory");
				page.label("", "Path"); page.text(path + Directory::Separator); page.br();
				page.label("name","Name"); page.input("text","name", name); page.br();
				page.label("add"); page.button("add","Add directory");
				page.closeFieldset();
				page.closeForm();
				page.footer();
				return;
			}
			
			Directory dir;
			try { 
				dir.open(path); 
			}
			catch(...)
			{
				throw 401;
			}
			
			Set<String> folders;
			while(dir.nextFile())
				if(dir.fileIsDir() && dir.fileName().at(0) != '.')
					folders.insert(dir.fileName());
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header("Share directory");
			
			page.open("div",".box");
			if(folders.empty()) page.text("No subdirectories");
			else {
			  	Array<String> existingPaths;
			  	mDirectories.getValues(existingPaths);
				Set<String> existingPathsSet;
				existingPathsSet.insert(existingPaths.begin(), existingPaths.end());
				
				page.open("table",".files");
				for(Set<String>::iterator it = folders.begin();
					it != folders.end();
					++it)
				{
					const String &name = *it; 
					String childPath = path + name;
					String link = prefix + "/?path=" + childPath.urlEncode();
					
					page.open("tr");
					page.open("td",".icon");
					page.image("/dir.png");
					page.close("td");
					
					if(existingPathsSet.find(childPath) == existingPathsSet.end())
					{
						page.open("td",".filename");
						page.link(link, name);
						page.close("td");
						page.open("td",".add");
						page.link(link+"&add=1", "share");
						page.close("td");
					}
					else {
						page.open("td",".filename");
						page.text(name);
						page.close("td");
						page.open("td",".add");
						page.close("td");
					}
					
					page.close("tr");
				}
				page.close("table");
			}
			page.close("div");
			page.footer();
			return;
			
		} // prefix == "explore"

		if(request.method != "POST" && (request.get.contains("json")  || request.get.contains("playlist")))
		{
			// Query resources
			Resource::Query query(this, url);

			SerializableSet<Resource> resources;
			if(!query.submitLocal(resources)) throw 404;
			
			if(request.get.contains("json"))
			{
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();
				JsonSerializer json(response.sock);
				json.output(resources);
			}
			else {
				Http::Response response(request, 200);
				response.headers["Content-Disposition"] = "attachment; filename=\"playlist.m3u\"";
				response.headers["Content-Type"] = "audio/x-mpegurl";
				response.send();
				
				String host;
				request.headers.get("Host", host);
				Resource::CreatePlaylist(resources, response.sock, host);
			}
			return;
		}
		
		// TODO: the following code should use JSON
		
		if(url == "/")
		{
		  	if(this != GlobalInstance && request.method == "POST")
			{
				if(!user()->checkToken(request.post["token"], "directory")) 
					throw 403;
				
				String command = request.post["command"];
			  	if(command == "delete")
				{
					String name = request.post["argument"];
					if(!name.empty() && name != UploadDirectoryName) 
					{
						removeDirectory(name);
						// TODO: delete files recursively
					}
				}
			  	else if(request.post.contains("name"))
				{
					String name = request.post["name"];
					try {
						if(name.empty()
							|| name.contains('/') || name.contains('\\') 
							|| name.find("..") != String::NotFound)
								throw Exception("Invalid directory name");

						String dirname = name.toLower();
						dirname.replace(' ','_');
						// TODO: sanitize dirname
						
						Assert(!dirname.empty());
					 	addDirectory(name, dirname);
					}
					catch(const Exception &e)
					{
						Http::Response response(request,200);
						response.send();
						
						Html page(response.sock);
						page.header("Error", false, prefix + "/");
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
		  
		  	String action;
		  	if(request.get.get("action", action))
			{
				String redirect = prefix + url;
			  	request.get.get("redirect", redirect);
				
				if(action == "refresh")
				{
					GlobalInstance->start();
					start();
					
					Http::Response response(request, 200);
					response.send();
					
					Html page(response.sock);
					page.header("Refreshing in background...", true, redirect);
					page.open("div", "notification");
					page.openLink("/");
					page.image("/refresh.png", "Refresh");
					page.closeLink();
					page.br();
					page.open("h1",".huge");
					page.text("Refreshing in background...");
					page.close("h1");
					page.close("div");
					page.footer();
					return;
				}
				
				Http::Response response(request,303);
				response.headers["Location"] = redirect;
				response.send();
				return;
			}
		
			Http::Response response(request,200);
			response.send();

			Html page(response.sock);
			page.header("Shared folders");
			
			Array<String> directories;
			getDirectories(directories);
			directories.prepend(UploadDirectoryName);
			
			if(!directories.empty())
			{
				page.open("div",".box");
				page.open("table",".files");
				
				for(int i=0; i<directories.size(); ++i)
				{
					String name;
					if(directories[i] == UploadDirectoryName) name = "Sent files";
					else name = directories[i];
					
					page.open("tr");
					page.open("td",".icon");
					page.image("/dir.png");
					page.close("td");
					page.open("td",".filename");
					page.link(directories[i], name);
					if(isHiddenUrl(directories[i])) page.text(" (not visible)");
					page.close("td");
					
					if(this != GlobalInstance)
					{
						page.open("td",".delete");
						if(directories[i] != UploadDirectoryName) page.image("/delete.png", "Delete");
						page.close("td");
					}
					
					page.close("tr");
				}
				
				page.close("table");
				page.close("div");

				if(this != GlobalInstance)
                                {
                                        page.openForm(prefix+url, "post", "executeForm");
                                        page.input("hidden", "token", user()->generateToken("directory"));
					page.input("hidden", "command");
                                        page.input("hidden", "argument");
                                        page.closeForm();

                                        page.javascript("function deleteDirectory(name) {\n\
                                                if(confirm('Do you really want to delete the shared directory '+name+' ?')) {\n\
                                                        document.executeForm.command.value = 'delete';\n\
                                                        document.executeForm.argument.value = name;\n\
                                                        document.executeForm.submit();\n\
                                                }\n\
                                        }");

                                        page.javascript("$('td.delete').css('cursor', 'pointer').click(function(event) {\n\
                                                event.stopPropagation();\n\
                                                var fileName = $(this).closest('tr').find('td.filename a').text();\n\
                                                deleteDirectory(fileName);\n\
                                        });");
                                }
			}
			
			if(this != GlobalInstance)
			{
				page.openForm(prefix+"/","post");
				page.input("hidden", "token", user()->generateToken("directory_create"));
				page.openFieldset("New directory");
				page.label("name","Name"); page.input("text","name"); page.br();
				page.label("add"); page.button("add","Create directory");
				page.label(""); page.link("/"+mUser->name()+"/explore/", "Add existing directory", ".button"); 
				page.br();
				page.closeFieldset();
				page.closeForm();
			}
			
			page.footer();
		}
		else {
			String path = urlToPath(url);
			
			if(path.empty())
			{
				if(this == GlobalInstance) throw 404;
				GlobalInstance->http(prefix, request);
				return;
			}
			
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
				
				if(this != GlobalInstance && request.method == "POST")
				{
					if(!user()->checkToken(request.post["token"], "directory")) 
						throw 403;
					
					String command = request.post["command"];
			  		if(command == "delete")
					{
						String fileName = request.post["argument"];
						if(!fileName.empty())
						{
							String filePath = path + Directory::Separator + fileName;
							
							if(Directory::Exist(filePath))
                                                        {
								// TODO: recursively delete directories
								if(Directory::Remove(filePath))
								{
									Database::Statement statement = mDatabase->prepare("DELETE FROM files WHERE url = ?1");
                                                                	statement.bind(1, url);
                                                                	statement.execute();
								}
                                                        }
							else if(File::Exist(filePath))
							{
								if(File::Remove(filePath))
								{
									Database::Statement statement = mDatabase->prepare("DELETE FROM files WHERE url = ?1");
									statement.bind(1, url);
									statement.execute();
								}
							}
						}
					}
					else {
						SerializableSet<Resource> resources;
						
						for(Map<String,TempFile*>::iterator it = request.files.begin();
						it != request.files.end();
						++it)
						{
							String fileName;
							if(!request.post.get(it->first, fileName)) continue;
							Assert(!fileName.empty());
							
							VAR(fileName);
							
							if(fileName.contains('/') || fileName.contains('\\') 
									|| fileName.find("..") != String::NotFound)
										throw Exception("Invalid file name");
								
							TempFile *tempFile = it->second;
							tempFile->close();
							
							String filePath = path + Directory::Separator + fileName;
							File::Rename(tempFile->name(), filePath);
							
							LogInfo("Store::Http", String("Uploaded: ") + fileName);
							
							try {
								String fileUrl = url + fileName;
								update(fileUrl);
								
								Resource res; 
								Resource::Query qry;
								qry.setLocation(fileUrl);
								qry.setFromSelf(true);
								if(!query(qry, res)) throw Exception("Query failed for " + fileUrl);
									
								resources.insert(res);
							}
							catch(const Exception &e)
							{
								LogWarn("Store::Http", String("Unable to get resource after upload: ") + e.what());
							}
						}
					
						if(request.get.contains("json"))
						{
							Http::Response response(request, 200);
							response.headers["Content-Type"] = "application/json";
							response.send();
							JsonSerializer json(response.sock);
							json.output(resources);
							return;
						}
						
						start();
					}
					
					Http::Response response(request,303);
					response.headers["Location"] = prefix+url;
					response.send();
					return;
				}

				Http::Response response(request, 200);
				response.send();

				Html page(response.sock);
				
				String directory = url.substr(1);
				directory.cut('/');
				
				String title;
				if(directory == CacheDirectoryName) title = "Cached files";
				else if(directory == UploadDirectoryName) title = "Sent files";
				else title = "Shared folder: " + request.url.substr(1,request.url.size()-2);
					
				page.header(title);

				if(this != GlobalInstance)
				{
					page.openForm(prefix+url,"post", "uploadForm", true);
					page.input("hidden", "token", user()->generateToken("directory"));
					page.openFieldset("Add a file");
					page.label("file"); page.file("file", "Select a file"); page.br();
					page.label("send"); page.button("send","Send");
					page.closeFieldset();
					page.closeForm();
					page.div("","uploadMessage");

					page.javascript("document.uploadForm.send.style.display = 'none';\n\
					$(document.uploadForm.file).change(function() {\n\
						if(document.uploadForm.file.value != '') {\n\
							document.uploadForm.style.display = 'none';\n\
							$('#uploadMessage').html('<div class=\"box\">Uploading the file, please wait...</div>');\n\
							document.uploadForm.submit();\n\
						}\n\
					});");
				}
	
				Map<String, StringMap> files;
				Directory dir(path);
				StringMap info;
				while(dir.nextFile())
				{
					if(dir.fileName() == ".directory" 
						|| dir.fileName().toLower() == "thumbs.db"
						|| dir.fileName().substr(0,7) == ".Trash-")
						continue;
					
					dir.getFileInfo(info);
					if(info.get("type") == "directory") files.insert("0"+info.get("name").toLower()+info.get("name"),info);
					else files.insert("1"+info.get("name").toLower()+info.get("name"),info);
				}
				
				page.open("div", ".box");
				
				String desc;
				desc << files.size() << " files";
				page.span(desc, ".button");
					
				if(request.url[request.url.size()-1] == '/') page.openLink("..", ".button");
				else page.openLink(".", ".button");
				page.image("/arrow_up.png", "Parent");
				page.closeLink();
					
				page.br();
				
				if(files.empty()) page.div("No files", ".files");
				else {
					page.open("table", ".files");
					for(Map<String, StringMap>::iterator it = files.begin();
						it != files.end();
						++it)
					{
						StringMap &info = it->second;
						String name = info.get("name");
						String link = name;
						
						page.open("tr");
						page.open("td",".icon");
						if(info.get("type") == "directory") page.image("/dir.png");
						else page.image("/file.png");
						page.close("td");
						page.open("td",".filename");
						if(info.get("type") != "directory" && name.contains('.'))
							page.span(name.afterLast('.').toUpper(), ".type");
						page.link(link,name);
						page.close("td");
						page.open("td",".size"); 
						if(info.get("type") == "directory") page.text("directory");
						else page.text(String::hrSize(info.get("size"))); 
						page.close("td");
						page.open("td",".actions");
						if(info.get("type") != "directory")
						{
							page.openLink(Http::AppendGet(link,"download"));
							page.image("/down.png", "Download");
							page.closeLink();
							
							if(Mime::IsAudio(name) || Mime::IsVideo(name))
							{
								page.openLink(Http::AppendGet(link,"play"));
								page.image("/play.png", "Play");
								page.closeLink();
							}
						}
						page.close("td");
						
						if(this != GlobalInstance)
						{
							page.open("td",".delete");
							page.image("/delete.png", "Delete");
							page.close("td");
						}
						
						page.close("tr");
					}
					page.close("table");
					page.close("div");

					if(this != GlobalInstance)
					{
						page.openForm(prefix+url, "post", "executeForm");
						page.input("hidden", "token", user()->generateToken("directory"));
						page.input("hidden", "command");
						page.input("hidden", "argument");
						page.closeForm();

						page.javascript("function deleteFile(name) {\n\
							if(confirm('Do you really want to delete '+name+' ?')) {\n\
								document.executeForm.command.value = 'delete';\n\
								document.executeForm.argument.value = name;\n\
								document.executeForm.submit();\n\
							}\n\
						}");

						page.javascript("$('td.delete').css('cursor', 'pointer').click(function(event) {\n\
							event.stopPropagation();\n\
							var fileName = $(this).closest('tr').find('td.filename a').text();\n\
							deleteFile(fileName);\n\
						});");
					}
				}
				
				page.footer();
			}
			else if(File::Exist(path))
			{
				if(request.get.contains("play"))
				{
					String host;
					if(!request.headers.get("Host", host))
					host = String("localhost:") + Config::Get("interface_port");
					
					Http::Response response(request, 200);
					response.headers["Content-Disposition"] = "attachment; filename=\"stream.m3u\"";
					response.headers["Content-Type"] = "audio/x-mpegurl";
					response.send();
					
					response.sock->writeLine("#EXTM3U");
					response.sock->writeLine(String("#EXTINF:-1, ") + path.afterLast(Directory::Separator));
					response.sock->writeLine("http://" + host + prefix + request.url);
					return;
				}
			  
				Http::Response response(request,200);
				if(request.get.contains("download")) response.headers["Content-Type"] = "application/octet-stream";
				else response.headers["Content-Type"] = Mime::GetType(path);
				response.headers["Content-Length"] << File::Size(path);
				response.headers["Last-Modified"] = File::Time(path).toHttpDate();
				response.send();

				File file(path, File::Read);
				response.sock->writeBinary(file);
			}
			else throw 404;
		}
	}
	catch(const std::exception &e)
	{
		LogWarn("Store::http", e.what());
		throw 500;	// Httpd handles integer exceptions
	}
}

bool Store::prepareQuery(Database::Statement &statement, const Resource::Query &query, const String &fields, bool oneRowOnly)
{
	String url = query.mUrl;
	int count = query.mCount;
	if(oneRowOnly) count = 1;
	
	// Limit for security purposes
	if(!query.mMatch.empty() && (count <= 0 || count > 200)) count = 200;	// TODO: variable
	
	// If multiple rows are expected and url finishes with '/', this is a directory listing
	int64_t parentId = -1;
	if(!oneRowOnly && !url.empty() && url[url.size()-1] == '/')
	{
		url.resize(url.size()-1);
		
		// Do not allow listing hidden directories for others
		if(!query.mFromSelf && isHiddenUrl(url)) return false;
		
		if(url.empty()) parentId = 0;
		else {
			statement = mDatabase->prepare("SELECT id, type FROM files WHERE url = ?1");
			statement.bind(1, url);
			
			if(!statement.step())
			{
				statement.finalize();
				return false;
			}
			
			int type;
			statement.value(0, parentId);
			statement.value(1, type);
			statement.finalize();

			if(type != 0) return false;
		}
	}
	
	String sql;
	sql<<"SELECT "<<fields<<" FROM files ";
	if(!query.mMatch.empty()) sql<<"JOIN names ON names.rowid = name_rowid ";
	sql<<"WHERE url NOT NULL ";
	if(parentId >= 0)				sql<<"AND parent_id = ? ";
	else if(!url.empty())				sql<<"AND url = ? ";
	if(!query.mDigest.empty())			sql<<"AND digest = ? ";
	else if(url.empty() || !query.mFromSelf)	sql<<"AND url NOT LIKE '/\\_%' ESCAPE '\\' ";		// hidden files
	if(!query.mMatch.empty())			sql<<"AND names.name MATCH ? ";
	
	if(query.mMinAge > 0) sql<<"AND time <= ? "; 
	if(query.mMaxAge > 0) sql<<"AND time >= ? ";
	
	sql<<"ORDER BY time DESC "; // Newer files first
	
	if(count > 0)
	{
		sql<<"LIMIT "<<String::number(count)<<" ";
		if(query.mOffset > 0) sql<<"OFFSET "<<String::number(query.mOffset)<<" ";
	}

	statement = mDatabase->prepare(sql);
	int parameter = 0;
	if(parentId >= 0)		statement.bind(++parameter, parentId);
	else if(!url.empty())		statement.bind(++parameter, url);
	if(!query.mDigest.empty())	statement.bind(++parameter, query.mDigest);
	if(!query.mMatch.empty())	
	{
		String match = query.mMatch;
		match.replace('*', '%');
		statement.bind(++parameter, match);
	}
	
	if(query.mMinAge > 0)	statement.bind(++parameter, int64_t(Time::Now()-double(query.mMinAge)));
	if(query.mMaxAge > 0)	statement.bind(++parameter, int64_t(Time::Now()-double(query.mMaxAge)));
	
	return true;
}

void Store::update(const String &url, String path, int64_t parentId, bool computeDigests)
{
	Synchronize(this);

	try {
		Unprioritize(this);
	  
		if(url.empty()) return;
		
		if(path.empty())
		{
			path = url;
			path.replace('/', Directory::Separator);
			if(path[0] == Directory::Separator) path.ignore();
		}
		
		String absPath = absolutePath(path);
		
		int64_t time = File::Time(absPath);
		int64_t size = 0;
		int type = 0;
		if(!Directory::Exist(absPath)) 
		{
			type = 1;
			size = File::Size(absPath);
		}
	
		if(parentId < 0)
		{
			Database::Statement statement = mDatabase->prepare("SELECT id FROM files WHERE url = ?1");
                	statement.bind(1, url.beforeLast('/'));

			if(statement.step()) statement.value(0, parentId);
			statement.finalize();
		}
	
		Database::Statement statement = mDatabase->prepare("SELECT id, digest, size, time, type FROM files WHERE url = ?1");
		statement.bind(1, url);
		
		int64_t id;
		ByteString digest;
		
		if(statement.step())	// entry already exists
		{
			int64_t dbSize;
			int64_t dbTime;
			int     dbType;
			statement.value(0, id);
			statement.value(1, digest);
			statement.value(2, dbSize);
			statement.value(3, dbTime);
			statement.value(4, dbType);
			statement.finalize();
			
			if(time == dbTime && size == dbSize && type == dbType && !(type && digest.empty()))
			{
				statement = mDatabase->prepare("UPDATE files SET parent_id=?2, seen=1 WHERE id=?1");
				statement.bind(1, id);
				statement.bind(2, parentId);
				statement.execute();
			}
			else {	// file has changed
				  
			  	if(computeDigests) LogInfo("Store", String("Processing: ") + path);
			  
				if(type && computeDigests)
				{
					Desynchronize(this);
					digest.clear();
					File data(absPath, File::Read);
					Sha512::Hash(data, digest);
					data.close();
				}
				
				statement = mDatabase->prepare("UPDATE files SET parent_id=?2, digest=?3, size=?4, time=?5, type=?6, seen=1 WHERE id=?1");
				statement.bind(1, id);
				statement.bind(2, parentId);
				if(!digest.empty()) statement.bind(3, digest);
				else statement.bindNull(3);
				statement.bind(4, size);
				statement.bind(5, time);
				statement.bind(6, type);
				statement.execute();
			}
		}
		else {
			statement.finalize();
			if(computeDigests) LogInfo("Store", String("Processing new: ") + path);
			else LogInfo("Store", String("Indexing: ") + path);
			
			if(type && computeDigests)
			{
				Desynchronize(this);
				digest.clear();
				File data(absPath, File::Read);
				Sha512::Hash(data, digest);
				data.close();
			}
			
			String name = url.afterLast('/');
			statement = mDatabase->prepare("INSERT INTO names (name) VALUES (?1)");
			statement.bind(1, name);
			statement.execute();
			
			// This seems bogus
			//int64_t nameRowId = mDatabase->insertId();
			
			// Dirty way to get the correct rowid
			int64_t nameRowId = 0;
			statement = mDatabase->prepare("SELECT rowid FROM names WHERE name = ?1 LIMIT 1");
			statement.bind(1, name);
			if(statement.step()) statement.value(0, nameRowId);
			statement.finalize();
			//
			
			statement = mDatabase->prepare("INSERT INTO files (parent_id, url, digest, size, time, type, name_rowid, seen)\
							VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 1)");
			statement.bind(1, parentId);
			statement.bind(2, url);
			if(!digest.empty()) statement.bind(3, digest);
			else statement.bindNull(3);
			statement.bind(4, size);
			statement.bind(5, time);
			statement.bind(6, type);
			statement.bind(7, nameRowId);
			statement.execute();
				
			id = mDatabase->insertId();
		}
			
		if(!type)	// directory
		{
			Desynchronize(this);
			Directory dir(absPath);
			while(dir.nextFile())
			{
				if(dir.fileName() == ".directory" 
					|| dir.fileName().toLower() == "thumbs.db"
					|| dir.fileName().substr(0,7) == ".Trash-")
					continue;
				
				String childPath = path + Directory::Separator + dir.fileName();
				String childUrl  = url + '/' + dir.fileName();
				update(childUrl, childPath, id, computeDigests);
			}
		}
		else {		// file
		  
			Desynchronize(this);
			ResourcesMutex.lock();
			Resources.insert(digest, absPath);
			ResourcesMutex.unlock();
		}
	}
	catch(const Exception &e)
	{
		LogWarn("Store", String("Processing failed for ") + path + ": " + e.what());

	}
}

String Store::urlToPath(const String &url) const
{
	if(url.empty() || url[0] != '/') throw Exception("Invalid URL");
	Synchronize(this);
  
	String dir(url.substr(1));
	String path = dir.cut('/');

	// Do not accept the parent directory symbol as a security protection
	if(path == ".." || path.find("../") != String::NotFound || path.find("/..") != String::NotFound)
		throw Exception("Invalid URL");

	String dirPath;
	if(!mDirectories.get(dir, dirPath)) return "";

	path.replace('/', Directory::Separator);

	return absolutePath(dirPath + Directory::Separator + path);
}

String Store::absolutePath(const String &path) const
{
	if(path.empty()) throw Exception("Empty path");
	Synchronize(this);
	
#ifdef WINDOWS
	bool absolute = (path.size() >= 2 && path[1] == ':' && (path.size() == 2 || path[2] == '\\'));
#else
	bool absolute = (path[0] == '/');
#endif

	if(absolute) return path;
	else return mBasePath + path;
}

bool Store::isHiddenUrl(const String &url) const
{
	if(url.empty()) return false;
	if(url[0] == '_') return true;
	if(url.size() >= 2 && url[0] == '/' && url[1] == '_') return true;
	return false;
}

int64_t Store::freeSpace(String path, int64_t maxSize, int64_t space)
{
	int64_t totalSize = 0;

	try {
		Assert(!path.empty());
		if(path[path.size()-1] == Directory::Separator)
			path.resize(path.size()-1);

		StringList list;
		Directory dir(absolutePath(path));
		while(dir.nextFile())
			if(!dir.fileIsDir())
			{
				list.push_back(dir.fileName());
				totalSize+= dir.fileSize();
			}
		
		if(maxSize > totalSize)
		{
			int64_t freeSpace = Directory::GetFreeSpace(path);
			int64_t margin = 1024*1024;	// 1 MiB
			freeSpace = std::max(freeSpace - margin, int64_t(0));
			maxSize = totalSize + std::min(maxSize-totalSize, freeSpace);
		}
		
		space = std::min(space, maxSize);
		
		while(!list.empty() && totalSize > maxSize - space)
		{
			int r = uniform(0, int(list.size()));
			StringList::iterator it = list.begin();
			while(r--) ++it;
			
			String filePath = path + Directory::Separator + *it;
			
			Database::Statement statement = mDatabase->prepare("DELETE FROM files WHERE path = ?1");
                	statement.bind(1, filePath);
			statement.execute();
			
			// TODO: delete in Resources

			String absFilePath = absolutePath(filePath);
			totalSize-= File::Size(absFilePath);
			File::Remove(absFilePath);
			list.erase(it);
		}
	}
	catch(const Exception &e)
	{
		throw Exception(String("Unable to free space: ") + e.what());
	}

	return std::max(maxSize - totalSize, int64_t(0));
}

void Store::run(void)
{
	Synchronize(this);
	mRunning = true;
	
	// DO NOT return without switching mRunning to false
	try {
		LogDebug("Store::run", "Started");
		
		mDatabase->execute("UPDATE files SET seen=0 WHERE url IS NOT NULL");
		
		Array<String> names;
		mDirectories.getKeys(names);
		
		for(int i=0; i<names.size(); ++i)
		try {
			String name = names[i];
			String path = mDirectories.get(name);
			String absPath = absolutePath(path);
			String url = String("/") + name;
			
			if(!Directory::Exist(absPath))
				Directory::Create(absPath);
			
			update(url, path, 0, false);
		}
		catch(const Exception &e)
		{
			LogWarn("Store", String("Update failed for directory ") + names[i] + ": " + e.what());
		}
		
		mDatabase->execute("DELETE FROM files WHERE seen=0");	// TODO: delete from names
		
		for(int i=0; i<names.size(); ++i)
		try {
			String name = names[i];
			String path = mDirectories.get(name);
			String absPath = absolutePath(path);
			String url = String("/") + name;
			
			update(url, path, 0, true);
		}
		catch(const Exception &e)
		{
			LogWarn("Store", String("Hashing failed for directory ") + names[i] + ": " + e.what());

		}
		
		LogDebug("Store::run", "Finished");
	}
	catch(const Exception &e)
	{
		LogWarn("Store::run", e.what());
	}
	
	mRunning = false;
}

/*
void Store::keywords(String name, Set<String> &result)
{
	const int minLength = 3;
  
	result.clear();
	
	for(int i=0; i<name.size(); ++i)
		if(!std::isalnum(name[i]) && !std::isdigit(name[i]))
			name[i] = ' ';
	
	List<String> lst;
	name.explode(lst, ' ');
	
	for(List<String>::iterator it = lst.begin(); it != lst.end(); ++it)
		it->substrings(result, minLength);
}
*/

}
