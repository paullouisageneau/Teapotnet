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

#ifndef TPN_RESOURCE_H
#define TPN_RESOURCE_H

#include "tpn/include.h"
#include "tpn/serializable.h"
#include "tpn/string.h"
#include "tpn/binarystring.h"
#include "tpn/identifier.h"
#include "tpn/block.h"
#include "tpn/file.h"

namespace tpn
{

class Resource : public Chunk
{
public:
	enum AccessLevel { Public, Private, Personal };

	// TODO
	class Query : public Serializable
	{
	public:
		Query(Store *store = NULL, const String &url = "");
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
		BinaryString mDigest;
		int mMinAge, mMaxAge;	// seconds
		int mOffset, mCount;
		AccessLevel mAccessLevel;
		
		Store *mStore;

		friend class Store;
	};
	
	class Accessor : public Stream
	{
	public:
		// Stream
		virtual size_t readData(char *buffer, size_t size) = 0;
		virtual void writeData(const char *data, size_t size) = 0;
		virtual void seekRead(int64_t position) = 0;
		virtual void seekWrite(int64_t position) = 0;
		
		virtual int64_t size(void) = 0;
	};
	
	static int CreatePlaylist(const Set<Resource> &resources, Stream *output, String host = "");
	
	Resource(const String &filename);	// file will be read and hashed
	~Resource(void);
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;

protected:
	SerializableArray<Block*> mBlocks;
	
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
		
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		int64_t size(void);
		
	private:
		// TODO: simple fountain with one source only
	};
	
	class ContentAccessor : public Accessor
	{
	public:
		ContentAccessor(const BinaryString &digest, const Set<Identifier> &sources);
		~ContentAccessor(void);
		
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		int64_t size(void);
		
	private:
		const BinaryString mDigest;
		const Set<Identifier> mSources;
		
		int64_t mPosition;
		Fountain *mFountain;	// TODO: StreamFountain
	};
};

bool operator <  (const Resource &r1, const Resource &r2);
bool operator >  (const Resource &r1, const Resource &r2);
bool operator == (const Resource &r1, const Resource &r2);
bool operator != (const Resource &r1, const Resource &r2);

}

#endif

