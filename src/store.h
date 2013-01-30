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
	static Store *GlobalInstance;
  
	class Query
	{
	public:
		Query(const String &url = "");
		
		void setLocation(const String &url);
		void setDigest(const ByteString &digest);
		void setAge(Time min, Time max);
		void setRange(int first, int last);
		void setLimit(int count);
	  	void setMatch(const String &match);
		void addType(const String &type);
		
	private:
		String mUrl, mMatch;
		ByteString mDigest;
		List<String> mTypes;
		Time mMinAge, mMaxAge;
		int mOffset, mCount;
		
		friend class Store;
	};
  
	struct Entry
	{
		ByteString 	digest;
		String		url;
		String		path;
		String		name;
		int		type;
		uint64_t	size;
		Time		time;
	};
  
  	static bool GetResource(const ByteString &digest, Entry &entry);
	static const size_t ChunkSize;

	Store(User *user);
	~Store(void);
	
	User *user(void) const;
	String userName(void) const;
	
	void addDirectory(const String &name, String path);
	void removeDirectory(const String &name);
	void getDirectories(Array<String> &array) const;
	
	void save(void) const;
	void update(void);

	bool queryEntry(const Query &query, Entry &entry);
	bool queryList(const Query &query, List<Entry> &list);
	
	void http(const String &prefix, Http::Request &request);

private:
  	bool prepareQuery(Database::Statement &statement, const Query &query, const String &fields, bool oneRowOnly = false);
	void updateRec(const String &url, const String &path, int64_t parentId, bool computeDigests);
	String urlToPath(const String &url) const;
	String absolutePath(const String &path) const;
	void run(void);

	User *mUser;
	Database *mDatabase;
	String mFileName;
	String mBasePath;
	StringMap mDirectories;
	
	static void keywords(String name, Set<String> &result);
	
	static Map<ByteString,String> Resources;	// absolute path by data hash
	static Mutex ResourcesMutex;
};

}

#endif
