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
#include "lineserializer.h"
#include "config.h"
#include "time.h"
#include "mime.h"

namespace tpot
{

Store *Store::GlobalInstance = NULL;
Map<ByteString,String> Store::Resources;
Mutex Store::ResourcesMutex;
  
bool Store::GetResource(const ByteString &digest, Entry &entry)
{
	String path;
	ResourcesMutex.lock();
	bool found = Resources.get(digest, path);
	ResourcesMutex.unlock();
	if(!found) return false;
	
	//Log("Store::GetResource", "Requested " + digest.toString());  
	
	if(!File::Exist(path))
	{
		ResourcesMutex.lock();
		Resources.erase(digest);
		ResourcesMutex.unlock();
		return false;
	}
	
	entry.digest = digest;
	entry.path = path;
	entry.url.clear();	// no url
	entry.type = 1;		// file
	entry.size = File::Size(path);
	entry.time = File::Time(path);
	entry.name = path.afterLast(Directory::Separator);

	return true;
}

Store::Store(User *user) :
	mUser(user)
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
	//mDatabase->execute("CREATE VIRTUAL TABLE IF NOT EXISTS names USING FTS3(name)");
	
	// Fix: "IF NOT EXISTS" is not available for virtual tables with old sqlite3 versions
	Database::Statement statement = mDatabase->prepare("select DISTINCT tbl_name from sqlite_master where tbl_name = 'names'");
	if(!statement.step()) mDatabase->execute("CREATE VIRTUAL TABLE names USING FTS3(name)");	
	statement.finalize();
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
			
	  		start();
		}
		catch(...) {}
	}
	
	save();
	
	if(mUser)
	{
		Interface::Instance->add("/"+mUser->name()+"/files", this);
		Interface::Instance->add("/"+mUser->name()+"/explore", this);
	}
}

Store::~Store(void)
{
	if(mUser) 
	{
		Interface::Instance->remove("/"+mUser->name()+"/files");
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
		throw Exception("The directory does not exist: " + absPath);
	
	Directory test(absPath);
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

void Store::getDirectories(Array<String> &array) const
{
	Synchronize(this);
	mDirectories.getKeys(array);
}

void Store::save(void) const
{
  	Synchronize(this);
  
	File file(mFileName, File::Write);
	LineSerializer serializer(&file);
	serializer.output(mDirectories);
	file.close();
}

void Store::update(void)
{
	Synchronize(this);
	//Log("Store::update", "Started");
	
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
		
		updateRec(url, path, 0, false);
	}
	catch(const Exception &e)
	{
		Log("Store", String("Update failed for directory ") + names[i] + ": " + e.what());
	}
	
	mDatabase->execute("DELETE FROM files WHERE seen=0");	// TODO: delete from names
	
	for(int i=0; i<names.size(); ++i)
	try {
		String name = names[i];
		String path = mDirectories.get(name);
		String absPath = absolutePath(path);
		String url = String("/") + name;
		
		updateRec(url, path, 0, true);
	}
	catch(const Exception &e)
	{
		Log("Store", String("Hashing failed for directory ") + names[i] + ": " + e.what());

	}
	
	//Log("Store::update", "Finished");
}

bool Store::queryEntry(const Store::Query &query, Store::Entry &entry)
{
	Synchronize(this);
	
	if(query.mUrl == "/")	// root directory
	{
		entry.path = "";
		entry.url = "/";
		entry.type = 0;
		entry.size = 0;
		entry.time = Time::Now();
		entry.digest.clear();
		return true;
	}
	
	const String fields = "url, digest, type, size, time";
	Database::Statement statement;
	if(prepareQuery(statement, query, fields, true))
	{
		if(statement.step())
		{
			statement.value(0, entry.url);
			statement.value(1, entry.digest);
			statement.value(2, entry.type);
			statement.value(3, entry.size);
			
			int64_t time;
			statement.value(4, time);
			entry.time = time;
			
			entry.path = urlToPath(entry.url);
			entry.name = entry.url.afterLast('/');
			
			statement.finalize();
			return true;
		}
		
		statement.finalize();
	}
	
	if(this != GlobalInstance) return GlobalInstance->queryEntry(query, entry);
	else return false;
}

bool Store::queryList(const Store::Query &query, List<Store::Entry> &list)
{
	Synchronize(this);
	
	bool success = false;
	const String fields = "url, digest, type, size, time";
	Database::Statement statement;
	if(prepareQuery(statement, query, fields, false))
	{
		while(statement.step())
		{
			Entry entry;
			statement.value(0, entry.url);
			statement.value(1, entry.digest);
			statement.value(2, entry.type);
			statement.value(3, entry.size);
			
			int64_t time;
			statement.value(4, time);
			entry.time = time;
			
			entry.path = urlToPath(entry.url);
			entry.name = entry.url.afterLast('/');
			
			list.push_back(entry);
		}
		
		statement.finalize();
		success = true;
	}
	
	if(this != GlobalInstance) success|= GlobalInstance->queryList(query, list);
	return success;
}

void Store::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	const String &url = request.url;
	
	try {
		user()->setOnline();
		
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
						String hrName = String("Drive ") + letter;
						String link = prefix + "/?path=" + name.urlEncode();
						
						page.open("tr");
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
					
					if(existingPathsSet.find(childPath) == existingPathsSet.end())
					{
						page.open("tr");
						page.open("td",".dirname");
						page.link(link, name);
						page.close("td");
						page.open("td",".add");
						page.link(link+"&add=1", "share");
						page.close("td");
						page.close("tr");
					}
					else {
						page.open("tr");
						page.open("td",".dirname");
						page.text(name);
						page.close("td");
						page.open("td",".add");
						page.close("td");
						page.close("tr");
					}
				}
				page.close("table");
			}
			page.close("div");
			page.footer();
			return;
			
		} // prefix == "explore"

		if(url == "/")
		{
		  	if(this != GlobalInstance && request.method == "POST")
			{
				String command = request.post["command"];
			  	if(command == "delete")
				{
					String name = request.post["argument"];
					if(!name.empty()) 
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
						String path = mBasePath + dirname;
						if(!Directory::Exist(path))
							Directory::Create(path);
					
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
		  
			Http::Response response(request,200);
			response.send();

			Html page(response.sock);
			page.header("Shared folders");
			
			if(!mDirectories.empty())
			{
				page.open("div",".box");
				
				if(this != GlobalInstance)
				{
					page.openForm(prefix+url, "post", "executeForm");
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
				}
				
				page.open("table",".files");
				for(StringMap::iterator it = mDirectories.begin();
							it != mDirectories.end();
							++it)
				{
					page.open("tr");
					page.open("td",".filename");
					page.link(it->first, it->first);
					page.close("td");
					
					if(this != GlobalInstance)
					{
						page.open("td",".delete");
						page.openLink("javascript:deleteDirectory('"+it->first+"')");
						page.image("/delete.png", "Delete");
						page.closeLink();
						page.close("td");
					}
					
					page.close("tr");
				}
				
				page.close("table");
				page.close("div");
			}
			
			if(this != GlobalInstance)
			{
				page.openForm(prefix+"/","post");
				page.openFieldset("New directory");
				page.label("name","Name"); page.input("text","name"); page.br();
				page.label("add"); page.button("add","Create directory"); page.br();
				page.br();
				page.label(""); page.link("/"+mUser->name()+"/explore/", "Add existing directory"); page.br();
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
					String command = request.post["command"];
			  		if(command == "delete")
					{
						String fileName = request.post["argument"];
						if(!fileName.empty())
						{
							String filePath = path + Directory::Separator + fileName;
							if(File::Exist(filePath))
							{
				  				File::Remove(filePath);
								Database::Statement statement = mDatabase->prepare("DELETE FROM files WHERE url = ?1");
								statement.bind(1, url);
								statement.execute();
							}
							// TODO: recursively delete repertories
						}
					}
					else for(Map<String,TempFile*>::iterator it = request.files.begin();
						it != request.files.end();
						++it)
					{
						String fileName;
						if(!request.post.get(it->first, fileName)) continue;
						Assert(!fileName.empty());
						
						if(fileName.contains('/') || fileName.contains('\\') 
								|| fileName.find("..") != String::NotFound)
									throw Exception("Invalid file name");
							
						TempFile *tempFile = it->second;
						tempFile->close();
						
						String filePath = path + Directory::Separator + fileName;
						File::Rename(tempFile->name(), filePath);
						
						Log("Store::Http", String("Uploaded: ") + fileName);
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
				page.header(String("Shared folder: ") + request.url.substr(1,request.url.size()-2));

				if(this != GlobalInstance)
				{
					page.openForm(prefix+url,"post", "uploadForm", true);
					page.openFieldset("Upload a file");
					page.label("file"); page.file("file"); page.br();
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
					if(dir.fileName() == ".directory" || dir.fileName().toLower() == "thumbs.db")
						continue;
					
					dir.getFileInfo(info);
					if(info.get("type") == "directory") files.insert("0"+info.get("name"),info);
					else files.insert("1"+info.get("name"),info);
				}
				
				if(!info.empty())
				{
					page.open("div", ".box");
					
					if(this != GlobalInstance)
					{
						page.openForm(prefix+url, "post", "executeForm");
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
					}
					
					page.open("table", ".files");
					for(Map<String, StringMap>::iterator it = files.begin();
						it != files.end();
						++it)
					{
						StringMap &info = it->second;
						page.open("tr");
						page.open("td",".filename"); page.link(info.get("name"),info.get("name")); page.close("td");
						page.open("td",".size"); 
						if(info.get("type") == "directory") page.text("directory");
						else page.text(String::hrSize(info.get("size"))); 
						page.close("td");
						
						if(this != GlobalInstance)
						{	
							page.open("td",".delete");
							page.openLink("javascript:deleteFile('"+info.get("name")+"')");
							page.image("/delete.png", "Delete");
							page.closeLink();
							page.close("td");
						}
						
						page.close("tr");
					}
					page.close("table");
					page.close("div");
				}
				
				page.footer();
			}
			else if(File::Exist(path))
			{
				Http::Response response(request,200);
				response.headers["Content-Type"] = Mime::GetType(path);
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
		Log("Store::http", e.what());
		throw 500;	// Httpd handles integer exceptions
	}
}

bool Store::prepareQuery(Database::Statement &statement, const Store::Query &query, const String &fields, bool oneRowOnly)
{
	String url = query.mUrl;
	int count = query.mCount;
	if(oneRowOnly) count = 1;
	
	// If multiple rows are expected and url finishes with '/', this is a directory listing
	int64_t parentId = -1;
	if(!oneRowOnly && !url.empty() && url[url.size()-1] == '/')
	{
		url.resize(url.size()-1);
		
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
	if(parentId >= 0)		sql<<"AND parent_id = ? ";
	else if(!url.empty())		sql<<"AND url = ? ";
	if(!query.mDigest.empty())	sql<<"AND digest = ? ";
	if(!query.mMatch.empty())	sql<<"AND names.name MATCH ? ";
	
	/*if(!query.mTypes.empty())
	{
		sql<<"AND (";
		for(List<String>::iterator it = query.mTypes.begin(); it != query.mTypes.end(); ++it)
		{
			if(it != query.mTypes.begin()) sql<<"OR ";
			sql<<"type = ?";
		}
		sql<<") ";
	}*/
	
	if(query.mMinAge.toUnixTime() > 0) sql<<"AND time >= ? "; 
	if(query.mMaxAge.toUnixTime() > 0) sql<<"AND time <= ? ";
	
	sql<<"ORDER BY time DESC "; // Newer files first
	
	if(count  > 0)
	{
		sql<<"LIMIT "<<String::number(count)<<" ";
		if(query.mOffset > 0) sql<<"OFFSET "<<String::number(query.mOffset)<<" ";
	}
	
	statement = mDatabase->prepare(sql);
	int parameter = 0;
	if(parentId >= 0)		statement.bind(++parameter, parentId);
	else if(!url.empty())		statement.bind(++parameter, url);
	if(!query.mDigest.empty())	statement.bind(++parameter, query.mDigest);
	if(!query.mMatch.empty())	statement.bind(++parameter, query.mMatch);
	
	/*if(!query.mTypes.empty())
	{
		for(List<String>::iterator it = query.mTypes.begin(); it != query.mTypes.end(); ++it)
		statement.bind(++parameter, *it);
	}*/
	
	if(query.mMinAge.toUnixTime() > 0)	statement.bind(++parameter, int64_t(Time::Now()-query.mMinAge));
	if(query.mMaxAge.toUnixTime() > 0)	statement.bind(++parameter, int64_t(Time::Now()-query.mMaxAge));
	
	return true;
}

void Store::updateRec(const String &url, const String &path, int64_t parentId, bool computeDigests)
{
	Synchronize(this);

	try {
		UnPrioritize(this);
	  
		String absPath = absolutePath(path);
		
		int type = 0;
		if(!Directory::Exist(absPath)) type = 1;
		uint64_t size = File::Size(absPath);
		int64_t  time = File::Time(absPath);
		
		Database::Statement statement = mDatabase->prepare("SELECT id, digest, size, time, type FROM files WHERE url = ?1");
		statement.bind(1, url);
		
		int64_t id;
		ByteString digest;
		
		if(statement.step())	// Entry already exists
		{
			uint64_t dbSize;
			int64_t  dbTime;
			int      dbType;
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
				  
			  	if(computeDigests) Log("Store", String("Processing: ") + path);
			  
				if(type && computeDigests)
				{
					digest.clear();
					Desynchronize(this);
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
			if(computeDigests) Log("Store", String("Processing new: ") + path);
			else Log("Store", String("Indexing: ") + path);
			
			if(type && computeDigests)
			{
				digest.clear();
				Desynchronize(this);
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
			Directory dir(absPath);
			while(dir.nextFile())
			{
				if(dir.fileName() == ".directory" || dir.fileName().toLower() == "thumbs.db")
					continue;
				
				String childPath = path + Directory::Separator + dir.fileName();
				String childUrl  = url + '/' + dir.fileName();
				updateRec(childUrl, childPath, id, computeDigests);
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
		Log("Store", String("Processing failed for ") + path + ": " + e.what());

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
	if(!mDirectories.get(dir,dirPath)) return "";

	path.replace('/',Directory::Separator);

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

void Store::run(void)
{
	// TODO
	while(true)
	{
		update();
		if(this != GlobalInstance) break;
		msleep(12*60*60*1000);	// 12h
	}
}

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


Store::Query::Query(const String &url) :
	mUrl(url),
	mMinAge(0), mMaxAge(0),
	mOffset(0), mCount(-1)
{
  
}
		
void Store::Query::setLocation(const String &url)
{
	mUrl = url;
}

void Store::Query::setDigest(const ByteString &digest)
{
	mDigest = digest;
}

void Store::Query::setAge(Time min, Time max)
{
	mMinAge = min;
	mMaxAge = max;
}

void Store::Query::setRange(int first, int last)
{
	mOffset = std::max(first,0);
	mCount  = std::max(last-mOffset,0);
}

void Store::Query::setLimit(int count)
{
	mCount = count;
}
	  
void Store::Query::setMatch(const String &match)
{
	mMatch = match;
}

void Store::Query::addType(const String &type)
{
	mTypes.push_back(type);
}

}
