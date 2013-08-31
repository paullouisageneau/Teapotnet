/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

namespace tpn
{

class Store;
class File;
class Request;
class Splicer;
	
class Resource : public Serializable
{
public:
	class Query : public Serializable
	{
	public:
		Query(const String &url = "");
		~Query(void);
		
		void setLocation(const String &url);
		void setDigest(const ByteString &digest);
		void setAge(Time min, Time max);
		void setRange(int first, int last);
		void setLimit(int count);
		void setMatch(const String &match);

		bool submitLocal(Resource &result, Store *store = NULL);
		bool submitLocal(Array<Resource> &result, Store *store = NULL);
		bool submitRemote(Array<Resource> &result, const Identifier &peering = Identifier::Null);
		bool submit(Array<Resource> &result, Store *store = NULL, const Identifier &peering = Identifier::Null);

		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;
		
	private:
		String mUrl, mMatch;
		ByteString mDigest;
		Time mMinAge, mMaxAge;
		int mOffset, mCount;
		
		friend class Store;
	};
	
	class Accessor : public Stream, public ByteStream
	{
	public:
		virtual size_t hashData(ByteString &digest, size_t size = -1);
		
		// ByteStream
		virtual size_t readData(char *buffer, size_t size) = 0;
		virtual void writeData(const char *data, size_t size) = 0;
		virtual void seekRead(int64_t position) = 0;
		virtual void seekWrite(int64_t position) = 0;
	};
	
	Resource(const Identifier &peering, const String &url);
	Resource(const ByteString &digest);
	Resource(void);
	~Resource(void);

	void clear(void);
	void refresh(void);
	
	ByteString 	digest(void) const;
	Time		time(void) const;
	int64_t		size(void) const;
	int		type(void) const;

	Identifier	peering(void) const;	// null if local
	String		url(void) const;
	String		name(void) const;
	bool		isDirectory(void) const;
	
	Accessor *accessor(void);
	void dissociateAccessor(void);
	
	void setPeering(const Identifier &peering);
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;

private:
	void merge(const Resource &resource);
	void createQuery(Query &query) const;
	
	ByteString 	mDigest;
	String		mUrl;
	Time		mTime;
	int64_t		mSize;
	int		mType;
	
	Identifier 	mPeering;
	String 		mPath;		// if local
	Set<Identifier>	mSources;
	
	Store 		*mStore;
	Accessor	*mAccessor;
	
	class LocalAccessor : public Accessor
	{
	public:
		LocalAccessor(const String &path);
		~LocalAccessor(void);
		
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		
	private:
		File *mFile;
	};
	
	class RemoteAccessor : public Accessor
	{
	public:
		RemoteAccessor(const Identifier &peering, const String &url);
		~RemoteAccessor(void);
		
		size_t hashData(ByteString &digest, size_t size = -1);
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		
	private:
		void initRequest(void);
		void clearRequest(void);
		
		Identifier mPeering;
		String mUrl;

		int64_t mPosition;
		Request *mRequest;
		ByteStream *mByteStream;
	};
	
	class SplicerAccessor : public Accessor
	{
	public:
		SplicerAccessor(const ByteString &digest, const Set<Identifier> &sources);
		~SplicerAccessor(void);
		
		size_t hashData(ByteString &digest, size_t size = -1);
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		
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

