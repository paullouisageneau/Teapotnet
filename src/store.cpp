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

Map<ByteString,String> Store::Resources;
Mutex Store::ResourcesMutex;
  
bool Store::GetResource(const ByteString &digest, Entry &entry)
{
	String path;
	ResourcesMutex.lock();
	bool found = Resources.get(digest, path);
	ResourcesMutex.unlock();
	if(!found) return false;
	
	Log("Store::GetResource", "Requested " + digest.toString());  

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
  	Assert(mUser != NULL);
	
	mDatabase = new Database(mUser->profilePath() + "files.db");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS files\
	(id INTEGER PRIMARY KEY AUTOINCREMENT,\
	parent_id INTEGER,\
	path TEXT UNIQUE,\
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
			statement.value(0, id);
			statement.finalize();
			
			statement = mDatabase->prepare("UPDATE files SET parent_id=0, url=?2, size=0, time=?3, type=0, seen=1 WHERE id=?1");
			statement.bind(1, id);
			statement.bind(2, url);
			statement.bind(3, time(NULL));
			statement.execute();
			
			updateDirectory(url, path, id);
		}
		else {
		  	statement.finalize();
		  	Log("Store", String("Processing: ")+name);
			
			statement = mDatabase->prepare("INSERT INTO names (name) VALUES (?1)");
			statement.bind(1, name);
			statement.execute();
			
			statement = mDatabase->prepare("INSERT INTO files (path, url, time, name_rowid, parent_id, digest, size, type, seen)\
				VALUES (?1, ?2, ?3, ?4, 0, NULL, 0, 0, 1)");
			statement.bind(1, path);
			statement.bind(2, url);
			statement.bind(3, time(NULL));
			statement.bind(4, mDatabase->insertId());
			statement.execute();
			
			updateDirectory(url, path, mDatabase->insertId());
		}
	}
	catch(const Exception &e)
	{
		Log("Store", String("Processing failed for ")+it->first+": "+e.what());

	}
	
	mDatabase->execute("DELETE FROM files WHERE seen=0");	// TODO: delete from names
	
	Log("Store::update", "Finished");
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
		entry.time = time(NULL);
		entry.digest.clear();
		return true;
	}
	
	const String fields = "path, url, digest, size, time, type";
	Database::Statement statement;
	if(!prepareQuery(statement, query, fields, true)) return false;
	
	if(statement.step())
	{
		statement.value(0, entry.path);
		statement.value(1, entry.url);
		statement.value(2, entry.digest);
		statement.value(3, entry.size);
		statement.value(4, entry.time);
		statement.value(5, entry.type);
		
		statement.finalize();
		return true;
	}

	statement.finalize();
	return false;
}

bool Store::queryList(const Store::Query &query, List<Store::Entry> &list)
{
	Synchronize(this);
	
	const String fields = "path, url, digest, size, time, type";
	Database::Statement statement;
	if(!prepareQuery(statement, query, fields, false)) return false;
	
	list.clear();
	while(statement.step())
	{
		Entry entry;
		statement.value(0, entry.path);
		statement.value(1, entry.url);
		statement.value(2, entry.digest);
		statement.value(3, entry.size);
		statement.value(4, entry.time);
		statement.value(5, entry.type);
					
		entry.name = entry.url.afterLast('/');
		
		list.push_back(entry);
	}
	
	statement.finalize();
	return !list.empty();
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
	if(!query.mMatch.empty()) sql<<"JOIN names ON names.rowid = names_rowid ";
	sql<<"WHERE url NOT NULL ";
	if(parentId >= 0)		sql<<"AND parent_id = ? ";
	else if(!url.empty())		sql<<"AND url = ? ";
	if(!query.mDigest.empty())	sql<<"AND digest = ? ";
	if(!query.mMatch.empty())	sql<<"AND names.name MATCH ?";
	
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
	
	if(query.mMinAge > 0) sql<<"AND time >= ? "; 
	if(query.mMaxAge > 0) sql<<"AND time <= ? ";
	
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
	
	if(query.mMinAge > 0)	statement.bind(++parameter, time(NULL)-query.mMinAge);
	if(query.mMaxAge > 0)	statement.bind(++parameter, time(NULL)-query.mMaxAge);
	
	return true;
}

void Store::updateDirectory(const String &dirUrl, const String &dirPath, int64_t dirId)
{
	Synchronize(this);
	Log("Store", String("Refreshing directory: ")+dirUrl);
	
	ByteString digest;
	Directory dir(dirPath);
	while(dir.nextFile()) 
	try {
		String fileUrl = dirUrl + '/' + dir.fileName();
	
		Database::Statement statement = mDatabase->prepare("SELECT id, url, digest, size, time, type FROM files WHERE path = ?1");
		statement.bind(1, dir.filePath());
		
		int64_t id;
		if(statement.step())	// Entry already exists
		{
			int type;
		  	int64_t size;
			time_t time;
			String url;
			statement.value(0, id);
			statement.value(1, url);
			statement.value(2, digest);
			statement.value(3, size);
			statement.value(4, time);
			statement.value(5, type);
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
					digest.clear();
					Sha512::Hash(data, digest);
					data.close();
				}
				
				size = dir.fileSize();
				time = dir.fileTime();
				
				statement = mDatabase->prepare("UPDATE files SET parent_id=?2, digest=?3, size=?4, time=?5, type=?6, seen=1 WHERE id=?1");
				statement.bind(1, id);
				statement.bind(2, dirId);
				statement.bind(3, digest);
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
				digest.clear();
				Sha512::Hash(data, digest);
				data.close();
			}
			
			statement = mDatabase->prepare("INSERT INTO names (name) VALUES (?1)");
			statement.bind(1, dir.fileName());
			statement.execute();
			
			statement = mDatabase->prepare("INSERT INTO files (parent_id, path, url, digest, size, time, type, name_rowid, seen)\
				VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 1)");
			statement.bind(1, dirId);
			statement.bind(2, dir.filePath());
			statement.bind(3, fileUrl);
			if(dir.fileIsDir()) statement.bindNull(4);
			else statement.bind(4, digest);
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
			Resources.insert(digest, dir.filePath());
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

void Store::Query::setAge(time_t min, time_t max)
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
