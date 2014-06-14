/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_INDEXER_H
#define TPN_INDEXER_H

#include "tpn/include.h"
#include "tpn/thread.h"
#include "tpn/synchronizable.h"
#include "tpn/serializable.h"
#include "tpn/resource.h"
#include "tpn/file.h"
#include "tpn/map.h"
#include "tpn/set.h"
#include "tpn/array.h"
#include "tpn/list.h"
#include "tpn/http.h"
#include "tpn/interface.h"
#include "tpn/mutex.h"
#include "tpn/database.h"

namespace tpn
{

class User;
  
class Indexer : public Task, protected Synchronizable, public HttpInterfaceable
{
public:
	Indexer(User *user);
	~Indexer(void);
	
	User *user(void) const;
	String userName(void) const;

	void addDirectory(const String &name, String path, Resource::AccessLevel level = Resource::Public);
	void removeDirectory(const String &name);
	void getDirectories(Array<String> &array) const;
	void setDirectoryAccessLevel(const String &name, Resource::AccessLevel level);
	Resource::AccessLevel directoryAccessLevel(const String &name) const; 
	bool moveFileToCache(String &fileName, String name = "");	// fileName is modified on success
	
	void save(void) const;
	void start(void);

	bool process(const String &path, Resource &resource);
	bool get(const String &path, Resource &resource, Time *time = NULL);
	void notify(const String &path, const Resource &resource);
	
	void http(const String &prefix, Http::Request &request);

	class Query : public Serializable
	{
	public:
		Query(const String &url = "");
		~Query(void);
		
		void setLocation(const String &url);
		void setDigest(const BinaryString &digest);
		void setMinAge(int seconds);
		void setMaxAge(int seconds);
		void setRange(int first, int last);
		void setLimit(int count);
		void setMatch(const String &match);
		
		void setAccessLevel(AccessLevel level);
		void setFromSelf(bool isFromSelf = true);	// Sets the access level accordingly
		
		void createRequest(Request &request) const;
		
		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;
		
	private:
		String mUrl, mMatch;
		BinaryString mDigest;
		int mMinAge, mMaxAge;	// seconds
		int mOffset, mCount;
		AccessLevel mAccessLevel;

		friend class Indexer;
	};

	bool query(const Query &query, Resource &resource);
	bool query(const Query &query, Set<Resource> &resources);
	
private:
	static const String CacheDirectoryName;
	static const String UploadDirectoryName;

	bool prepareQuery(Database::Statement &statement, const Query &query, const String &fields, bool oneRowOnly = false);
	void update(const String &url, String path = "", int64_t parentId = -1, bool computeDigests = true);
	String urlToDirectory(const String &url) const;
	String urlToPath(const String &url) const;
	String absolutePath(const String &path) const;
	bool isHiddenUrl(const String &url) const;
	Resource::AccessLevel urlAccessLevel(const String &url) const;
	int64_t freeSpace(String path, int64_t maxSize, int64_t space = 0);
	void run(void);
	
	User *mUser;
	Database *mDatabase;
	String mFileName;
	String mBasePath;
	StringMap mDirectories;
	Map<String, Resource::AccessLevel> mDirectoriesAccessLevel;
	bool mRunning;
};

}

#endif
