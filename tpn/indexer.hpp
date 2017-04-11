/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#include "tpn/include.hpp"
#include "tpn/interface.hpp"
#include "tpn/resource.hpp"
#include "tpn/database.hpp"
#include "tpn/request.hpp"
#include "tpn/network.hpp"

#include "pla/serializable.hpp"
#include "pla/map.hpp"
#include "pla/set.hpp"
#include "pla/array.hpp"
#include "pla/list.hpp"
#include "pla/alarm.hpp"
#include "pla/threadpool.hpp"

namespace tpn
{

class User;

class Indexer : public Network::Publisher, public Network::Subscriber, public HttpInterfaceable
{
public:
	Indexer(User *user);
	~Indexer(void);

	User *user(void) const;
	String userName(void) const;
	String prefix(void) const;

	void addDirectory(const String &name, String path, String remote, Resource::AccessLevel access = Resource::Public, bool nocommit = false);
	void removeDirectory(const String &name, bool nocommit = false);
	void getDirectories(Array<String> &array) const;
	String directoryRemotePath(const String &name) const;
	Resource::AccessLevel directoryAccessLevel(const String &name) const;
	bool moveFileToCache(String &fileName, String name = "");	// fileName is modified on success

	void save(void) const;
	void start(duration delay = duration(0.));

	bool process(String path, Resource &resource);
	bool get(String path, Resource &resource, Time *time = NULL);
	void notify(String path, const Resource &resource, const Time &time);

	// Publisher
	bool anounce(const Network::Link &link, const String &prefix, const String &path, List<BinaryString> &targets);

	// Subscriber
	virtual bool incoming(const Network::Link &link, const String &prefix, const String &path, const BinaryString &target);

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

		void setAccessLevel(Resource::AccessLevel access);
		void setFromSelf(bool isFromSelf = true);	// Sets the access level accordingly

		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;

	private:
		String mPath, mMatch;
		BinaryString mDigest;
		int mOffset, mCount;
		Resource::AccessLevel mAccess;

		friend class Indexer;
	};

	bool query(const Query &q, List<BinaryString> &targets);
	bool query(const Query &q, Set<Resource> &resources);
	bool query(const Query &q, Resource &resource);

private:
	static const String CacheDirectoryName;
	static const String UploadDirectoryName;

	bool prepareQuery(Database::Statement &statement, const Query &query, const String &fields);
	void sync(String path, const BinaryString &target, Time time);
	void update(String path = "/");
	String realPath(String path) const;
	bool isHiddenPath(String path) const;
	Resource::AccessLevel pathAccessLevel(String path) const;
	int64_t freeSpace(String path, int64_t maxSize, int64_t space = 0);

	struct Entry : public Serializable
	{
	public:
		Entry(void);
		Entry(const String &path, const String &remote, Resource::AccessLevel access = Resource::Public);
		~Entry(void);

		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;

		String path;
		String remote;
		Resource::AccessLevel access;
	};

	User *mUser;
	Database *mDatabase;
	String mFileName;
	String mBaseDirectory;
	Map<String, Entry> mDirectories;
	Alarm mRunAlarm;
	ThreadPool mPool, mSyncPool;
	bool mRunning;

	mutable std::mutex mMutex;
};

}

#endif
