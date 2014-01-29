/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_RESOURCE_H
#define TPN_RESOURCE_H

#include "tpn/include.h"
#include "tpn/serializable.h"
#include "tpn/string.h"
#include "tpn/bytestring.h"
#include "tpn/identifier.h"
#include "tpn/stream.h"
#include "tpn/bytestream.h"
#include "tpn/time.h"
#include "tpn/set.h"
#include "tpn/map.h"

namespace tpn
{

class Store;
class File;
class Request;
class Splicer;
	
class Resource : public Serializable
{
public:
	enum AccessLevel { Public, Private, Personal };

	class Query : public Serializable
	{
	public:
		Query(Store *store = NULL, const String &url = "");
		~Query(void);
		
		void setLocation(const String &url);
		void setDigest(const ByteString &digest);
		void setMinAge(int seconds);
		void setMaxAge(int seconds);
		void setRange(int first, int last);
		void setLimit(int count);
		void setMatch(const String &match);
		
		void setAccessLevel(AccessLevel level);
		void setFromSelf(bool isFromSelf = true);	// Sets the access level accordingly
		
		bool submitLocal(Resource &result);
		bool submitLocal(Set<Resource> &result);
		bool submitRemote(Set<Resource> &result, const Identifier &peering = Identifier::Null);
		bool submit(Set<Resource> &result, const Identifier &peering = Identifier::Null, bool forceLocal = false);

		void createRequest(Request &request) const;
		
		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;
		
	private:
		String mUrl, mMatch;
		ByteString mDigest;
		int mMinAge, mMaxAge;	// seconds
		int mOffset, mCount;
		AccessLevel mAccessLevel;
		
		Store *mStore;

		friend class Store;
	};
	
	class Accessor : public Stream, public ByteStream
	{
	public:
		// ByteStream
		virtual size_t readData(char *buffer, size_t size) = 0;
		virtual void writeData(const char *data, size_t size) = 0;
		virtual void seekRead(int64_t position) = 0;
		virtual void seekWrite(int64_t position) = 0;
		
		virtual int64_t size(void) = 0;
		virtual size_t hashData(ByteString &digest, size_t size);
	};
	
	static int CreatePlaylist(const Set<Resource> &resources, Stream *output, String host = "");
	
	Resource(const Identifier &peering, const String &url, Store *store = NULL);
	Resource(const ByteString &digest, Store *store = NULL);
	Resource(Store *store = NULL);
	~Resource(void);

	void clear(void);
	void fetch(bool forceLocal = false);
	void refresh(bool forceLocal = false);
	
	ByteString 	digest(void) const;
	Time		time(void) const;
	int64_t		size(void) const;
	int		type(void) const;

	String		url(void) const;
	String		name(void) const;
	bool		isDirectory(void) const;
	
	Identifier	peering(void) const;	// null if local
	int		hops(void) const;
	
	Accessor *accessor(void) const;
	void dissociateAccessor(void) const;
	
	void setPeering(const Identifier &peering);
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;

private:
	static Map<ByteString, Resource> Cache;
	static Mutex CacheMutex;
	
	void merge(const Resource &resource);
	void createQuery(Query &query) const;
	
	ByteString 	mDigest;
	String		mUrl;
	Time		mTime;
	int64_t		mSize;
	int		mType;
	
	Identifier 	mPeering;
	int 		mHops;
	String 		mPath;		// if local
	Set<Identifier>	mSources;
	
	Store 		*mStore;
	
	mutable Accessor *mAccessor;
	
	class LocalAccessor : public Accessor
	{
	public:
		LocalAccessor(const String &path);
		~LocalAccessor(void);
		
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		int64_t size(void);
		
	private:
		File *mFile;
	};
	
	class RemoteAccessor : public Accessor
	{
	public:
		RemoteAccessor(const Identifier &peering, const String &url);
		~RemoteAccessor(void);
		
		size_t hashData(ByteString &digest, size_t size);
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		int64_t size(void);
		
	private:
		void initRequest(void);
		void clearRequest(void);
		
		Identifier mPeering;
		String mUrl;

		int64_t mPosition;
		int64_t mSize;
		Request *mRequest;
		ByteStream *mByteStream;
	};
	
	class SplicerAccessor : public Accessor
	{
	public:
		SplicerAccessor(const ByteString &digest, const Set<Identifier> &sources);
		~SplicerAccessor(void);
		
		size_t hashData(ByteString &digest, size_t size);
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		int64_t size(void);
		
	private:
		const ByteString mDigest;
		const Set<Identifier> mSources;
		
		int64_t mPosition;
		Splicer *mSplicer;
	};
	
	friend class Store;
	friend class Request; // TODO: should not
};

bool operator <  (const Resource &r1, const Resource &r2);
bool operator >  (const Resource &r1, const Resource &r2);
bool operator == (const Resource &r1, const Resource &r2);
bool operator != (const Resource &r1, const Resource &r2);

}

#endif

