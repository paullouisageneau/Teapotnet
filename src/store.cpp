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
#include "yamlserializer.h"

namespace tpot
{

Map<Identifier,String> Store::Resources;
Mutex Store::ResourcesMutex;
  
bool Store::GetResource(const Identifier &hash, Entry &entry)
{
	String path;
	ResourcesMutex.lock();
	bool found = Resources.get(hash, path);
	ResourcesMutex.unlock();
	if(!found) return false;
	
	Log("Store::GetResource", "Requested " + hash.toString());  

	if(!File::Exist(path))
	{
		ResourcesMutex.lock();
		Resources.erase(hash);
		ResourcesMutex.unlock();
		return false;
	}
	
	entry.hash = hash;
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
  	Assert(mUser != NULL);
	
	mDatabase = new Database(mUser->profilePath() + "files.db");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS files\
	(id INTEGER PRIMARY KEY AUTOINCREMENT,\
	parent_id INTEGER,\
	path TEXT UNIQUE,\
	url TEXT UNIQUE,\
	hash BLOB,\
	name_rowid INTEGER,\
	size INTEGER(8),\
	time INTEGER(8),\
	type INTEGER(1),\
	seen INTEGER(1))");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS hash ON files (hash)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS parent_id ON files (parent_id)");
	//mDatabase->execute("CREATE VIRTUAL TABLE IF NOT EXISTS names USING FTS3(name)");
	
	// Fix: "IF NOT EXISTS" is not available for virtual tables with old ns-3 versions
	Database::Statement statement = mDatabase->prepare("select DISTINCT tbl_name from sqlite_master where tbl_name = 'names'");
	if(!statement.step()) mDatabase->execute("CREATE VIRTUAL TABLE names USING FTS3(name)");	
	statement.finalize();
	//

	mFileName = mUser->profilePath() + "directories";
	
	try {
	  File file(mFileName, File::Read);
	  YamlSerializer serializer(&file);
	  serializer.input(mDirectories);
	  file.close();
	  start();
	}
	catch(...)
	{
	  
	}
	
	Interface::Instance->add("/"+mUser->name()+"/files", this);
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
	YamlSerializer serializer(&file);
	serializer.output(mDirectories);
	file.close();
}

void Store::update(void)
{
	Synchronize(this);
	Log("Store::update", "Started");
	
	mDatabase->execute("UPDATE files SET seen=0");
	
	for(StringMap::iterator it = mDirectories.begin();
			it != mDirectories.end();
			++it)
	try {
		const String &name = it->first;
		const String &path = it->second;
		String url = "/" + name;

		Database::Statement statement = mDatabase->prepare("SELECT id FROM files WHERE path=?1");
		statement.bind(1, path);
		
		// Entry already exists
		if(statement.step())
		{
			int64_t id;
			statement.value(1, id);
			statement.finalize();
			
			statement = mDatabase->prepare("UPDATE files SET parent_id=0, url=?2, size=0, time=?3, type=0, seen=1 WHERE id=?1");
			statement.bind(1, id);
			statement.bind(2, url);
			statement.bind(3, int64_t(time(NULL)));
			statement.execute();
			
			updateDirectory(url, path, id);
		}
		else {
		  	statement.finalize();
		  	Log("Store", String("Processing: ")+name);
			
			statement = mDatabase->prepare("INSERT INTO names (name) VALUES (?1)");
			statement.bind(1, name);
			statement.execute();
			
			statement = mDatabase->prepare("INSERT INTO files (path, url, time, name_rowid, parent_id, hash, size, type, seen)\
				VALUES (?1, ?2, ?3, ?4, 0, NULL, 0, 0, 1)");
			statement.bind(1, path);
			statement.bind(2, url);
			statement.bind(3, int64_t(time(NULL)));
			statement.bind(4, mDatabase->insertId());
			statement.execute();
			
			updateDirectory(url, path, mDatabase->insertId());
		}
	}
	catch(const Exception &e)
	{
		Log("Store", String("Processing failed for ")+it->first+": "+e.what());

	}
	
	//mDatabase->execute("DELETE FROM files WHERE seen=0");	// TODO: delete from names
	
	Log("Store::update", "Finished");
}

bool Store::queryEntry(const String &url, Entry &entry)
{
	Synchronize(this);
	
	if(url == "/")
	{
		entry.path = "";
		entry.url = "/";
		entry.hash.clear();
		entry.size = 0;
		entry.time = time(NULL);
		entry.type = 0;
		return true;
	}
	
	Database::Statement statement = mDatabase->prepare("SELECT id, path, url, hash, size, time, type FROM files WHERE url = ?1");
	statement.bind(1, url);
	
	if(!statement.step())
	{
		statement.finalize();
		return false;
	}
	
	int64_t id;
	statement.value(0, id);
	statement.value(1, entry.path);
	statement.value(2, entry.url);
	statement.value(3, entry.hash);
	statement.value(4, entry.size);
	statement.value(5, entry.time);
	statement.value(6, entry.type);
	statement.finalize();

	entry.name = entry.url.afterLast('/');
	return true;
}

bool Store::queryList(const String &url, List<Store::Entry> &list)
{
	Synchronize(this);
	list.clear();
	
	int64_t id = 0;
	if(url != "/")
	{
		Database::Statement statement = mDatabase->prepare("SELECT id, type FROM files WHERE url = ?1");
		statement.bind(1, url);
		
		if(!statement.step())
		{
			statement.finalize();
			return false;
		}
		
		int type;
		statement.value(0, id);
		statement.value(1, type);
		statement.finalize();

		if(type != 0) return false;
	}
	
	Database::Statement statement = mDatabase->prepare("SELECT path, url, hash, size, time, type FROM files WHERE parent_id = ?1");
	statement.bind(1, id);
			
	while(statement.step())
	{
		Entry entry;
		statement.value(0, entry.path);
		statement.value(1, entry.url);
		statement.value(2, entry.hash);
		statement.value(3, entry.size);
		statement.value(4, entry.time);
		statement.value(5, entry.type);
					
		entry.name = entry.url.afterLast('/');
		
		list.push_back(entry);
	}
	
	statement.finalize(); 
	return true;
}

bool Store::queryResource(const Identifier &hash, Entry &entry)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("SELECT id, path, url, size, time, type FROM files WHERE hash = ?1 ORDER BY time DESC LIMIT 1");
	statement.bind(1, hash);
	
	if(statement.step())
	{
		int64_t id;
		statement.value(0, id);
		statement.value(1, entry.path);
		statement.value(2, entry.url);
		statement.value(3, entry.size);
		statement.value(4, entry.time);
		statement.value(5, entry.type);
		statement.finalize();
		
		entry.hash = hash;
		entry.name = entry.url.afterLast('/');
		
		Assert(entry.type != 0);	// the result cannot be a directory
		return true;
	}
	
	statement.finalize();
	return GetResource(hash, entry);
}

bool Store::search(const String &keywords, List<Entry> &list)
{
	return false; 
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
						page.header("Error", false, prefix + "/");
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

				Map<String, StringMap> files;
				Directory dir(path);
				StringMap info;
				while(dir.nextFile())
				{
					dir.getFileInfo(info);
					if(info.get("type") == "directory") files.insert("0"+info.get("name"),info);
					else files.insert("1"+info.get("name"),info);
				}
				
				page.open("table");
				for(Map<String, StringMap>::iterator it = files.begin();
					it != files.end();
					++it)
				{
					StringMap &info = it->second;
					page.open("tr");
					page.open("td"); page.link(info.get("name"),info.get("name")); page.close("td");
					page.open("td"); 
					if(info.get("type") == "directory") page.text("directory");
					else page.text(String::hrSize(info.get("size"))); 
					page.close("td");
					page.open("tr");
				}

				page.close("table");
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

void Store::updateDirectory(const String &dirUrl, const String &dirPath, int64_t dirId)
{
	Synchronize(this);
	Log("Store", String("Refreshing directory: ")+dirUrl);
	
	ByteString hash;
	Directory dir(dirPath);
	while(dir.nextFile()) 
	try {
		String fileUrl = dirUrl + '/' + dir.fileName();
	
		Database::Statement statement = mDatabase->prepare("SELECT id, url, hash, size, time, type FROM files WHERE path = ?1");
		statement.bind(1, dir.filePath());
		
		int64_t id;
		if(statement.step())	// Entry already exists
		{
			int64_t size, time;
			int type;
			String url;
			statement.value(1, id);
			statement.value(2, url);
			statement.value(3, hash);
			statement.value(4, size);
			statement.value(5, time);
			statement.value(6, type);
			statement.finalize();
			
			if(url != fileUrl)
			{
				statement = mDatabase->prepare("UPDATE files SET url=?2 WHERE id=?1");
				statement.bind(1, id);
				statement.bind(2, fileUrl);
				statement.execute();
			}
			
			if(size == dir.fileSize() && time == dir.fileTime())
			{
				statement = mDatabase->prepare("UPDATE files SET parent_id=?2, seen=1 WHERE id=?1");
				statement.bind(1, id);
				statement.bind(2, dirId);
				statement.execute();
			}
			else {	// file has changed
			  
				if(dir.fileIsDir()) type = 0;
				else {
					Desynchronize(this);
					type = 1;
					File data(dir.filePath(), File::Read);
					hash.clear();
					Sha512::Hash(data, hash);
					data.close();
				}
				
				size = dir.fileSize();
				time = dir.fileTime();
				
				statement = mDatabase->prepare("UPDATE files SET parent_id=?2, hash=?3, size=?4, time=?5, type=?6, seen=1 WHERE id=?1");
				statement.bind(1, id);
				statement.bind(2, dirId);
				statement.bind(3, hash);
				statement.bind(4, size);
				statement.bind(5, time);
				statement.bind(6, type);
				statement.execute();
			}
		}
		else {
			statement.finalize();
		  	Log("Store", String("Processing: ")+dir.fileName());
		  
			int type;
			if(dir.fileIsDir()) type = 0;
			else {
			  	Desynchronize(this);
				type = 1;
				File data(dir.filePath(), File::Read);
				hash.clear();
				Sha512::Hash(data, hash);
				data.close();
			}
			
			statement = mDatabase->prepare("INSERT INTO names (name) VALUES (?1)");
			statement.bind(1, dir.fileName());
			statement.execute();
			
			statement = mDatabase->prepare("INSERT INTO files (parent_id, path, url, hash, size, time, type, name_rowid, seen)\
				VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 1)");
			statement.bind(1, dirId);
			statement.bind(2, dir.filePath());
			statement.bind(3, fileUrl);
			if(dir.fileIsDir()) statement.bindNull(4);
			else statement.bind(4, hash);
			statement.bind(5, int64_t(dir.fileSize()));
			statement.bind(6, int64_t(dir.fileTime()));
			statement.bind(7, type);
			statement.bind(8, mDatabase->insertId());
			statement.execute();
			
			id = mDatabase->insertId();
		}
		
		if(dir.fileIsDir()) updateDirectory(fileUrl, dir.filePath(), id);
		else {
			ResourcesMutex.lock();
			Resources.insert(hash, dir.filePath());
			ResourcesMutex.unlock();
		}
	}
	catch(const Exception &e)
	{
		Log("Store", String("Processing failed for ")+dir.fileName()+": "+e.what());

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

}
