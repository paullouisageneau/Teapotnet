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
#include "tpn/interface.h"
#include "tpn/resource.h"
#include "tpn/database.h"
#include "tpn/request.h"
#include "tpn/core.h"

#include "pla/synchronizable.h"
#include "pla/serializable.h"
#include "pla/task.h"
#include "pla/map.h"
#include "pla/set.h"
#include "pla/array.h"
#include "pla/list.h"

namespace tpn
{

class User;
  
class Indexer : protected Synchronizable, public Task, public Core::Publisher, public HttpInterfaceable
{
public:
	Indexer(User *user);
	~Indexer(void);
	
	User *user(void) const;
	String userName(void) const;
	String prefix(void) const;
	
	void addDirectory(const String &name, String path, Resource::AccessLevel level = Resource::Public);
	void removeDirectory(const String &name);
	void getDirectories(Array<String> &array) const;
	void setDirectoryAccessLevel(const String &name, Resource::AccessLevel level);
	Resource::AccessLevel directoryAccessLevel(const String &name) const; 
	bool moveFileToCache(String &fileName, String name = "");	// fileName is modified on success
	
	void save(void) const;
	void start(void);
	
	bool process(String path, Resource &resource);
	bool get(String path, Resource &resource, Time *time = NULL);
	void notify(String path, const Resource &resource, const Time &time);
	
	// Publisher
	bool anounce(const Identifier &peer, const String &prefix, const String &path, List<BinaryString> &targets);
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);

	class Query : public Serializable
	{
	public:
		Query(const String &path = "");
		~Query(void);
		
		void setPath(const String &path);
		void setDigest(const BinaryString &digest);
		void setRange(int first, int last);
		void setLimit(int count);
		void setMatch(const String &match);
		
		void setAccessLevel(Resource::AccessLevel level);
		void setFromSelf(bool isFromSelf = true);	// Sets the access level accordingly
		
		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;
		
	private:
		String mPath, mMatch;
		BinaryString mDigest;
		int mOffset, mCount;
		Resource::AccessLevel mAccessLevel;

		friend class Indexer;
	};

	bool query(const Query &query, Resource &resource);
	bool query(const Query &query, Set<Resource> &resources);
	
private:
	static const String CacheDirectoryName;
	static const String UploadDirectoryName;
	
	bool prepareQuery(Database::Statement &statement, const Query &query, const String &fields);
	void update(String path = "/");
	String realPath(String path) const;
	bool isHiddenPath(const String &path) const;
	Resource::AccessLevel pathAccessLevel(const String &path) const;
	int64_t freeSpace(String path, int64_t maxSize, int64_t space = 0);
	
	// Task
	void run(void);
	
	User *mUser;
	Database *mDatabase;
	String mFileName;
	String mBaseDirectory;
	StringMap mDirectories;
	Map<String, Resource::AccessLevel> mDirectoriesAccessLevel;
	bool mRunning;
};

}

#endif
