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

#ifndef TPOT_STORE_H
#define TPOT_STORE_H

#include "include.h"
#include "thread.h"
#include "synchronizable.h"
#include "identifier.h"
#include "file.h"
#include "map.h"
#include "http.h"
#include "interface.h"
#include "mutex.h"
#include "database.h"

namespace tpot
{

class User;
  
class Store : public Thread, protected Synchronizable, public HttpInterfaceable
{
public:
	struct Entry
	{
		Identifier 	hash;
		String		url;
		String		path;
		String		name;
		int		type;
		uint64_t	size;
		uint64_t	time;
	};
  
  	static bool GetResource(const Identifier &hash, Entry &entry);
	static const size_t ChunkSize;

	Store(User *user);
	~Store(void);
	
	User *user(void) const;
	String userName(void) const;
	
	void addDirectory(const String &name, const String &path);
	void removeDirectory(const String &name);

	void save(void) const;
	void update(void);

	bool queryEntry(const String &url, Entry &entry);
	bool queryList(const String &url, List<Entry> &list);
	bool queryResource(const Identifier &hash, Entry &entry);
	bool search(const String &keywords, List<Entry> &list);
	
	void http(const String &prefix, Http::Request &request);

private:  
	void updateDirectory(const String &dirUrl, const String &dirPath, int64_t dirId);
	String urlToPath(const String &url) const;
	void run(void);

	User *mUser;
	Database *mDatabase;
	String mFileName;
	StringMap mDirectories;
	
	static void keywords(String name, Set<String> &result);
	
	static Map<Identifier,String> Resources;	// path by data hash
	static Mutex ResourcesMutex;
};

}

#endif
