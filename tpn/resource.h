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
#include "tpn/identifier.h"
#include "tpn/block.h"

#include "pla/serializable.h"
#include "pla/string.h"
#include "pla/binarystring.h"
#include "pla/file.h"

namespace tpn
{

class Resource : public Serializable
{
public:
	enum AccessLevel { Public, Private, Personal };
	
	Resource(void);
	Resource(const Resource &resource);
	Resource(const BinaryString &digest);
	~Resource(void);
	
	void fetch(const BinaryString &digest, bool localOnly = false);
	BinaryString digest(void) const;
	
	int blocksCount(void) const;
	int blockIndex(int64_t position) const;
	BinaryString blockDigest(int index) const;
	
	String  name(void) const;
	String  type(void) const;
	int64_t size(void) const;
	bool isDirectory(void) const;
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;
	
	Resource &operator = (const Resource &resource);
	
	class MetaRecord : public Serializable
	{
	public:
		MetaRecord(void)		{ size = 0; }
		virtual ~MetaRecord(void)	{}

		// Serializable
		virtual void serialize(Serializer &s) const;
		virtual bool deserialize(Serializer &s);
		virtual bool isInlineSerializable(void) const;
		
		String 		name;
		String		type;
		int64_t		size;
	};

	class IndexRecord : public MetaRecord
	{
	public:
		IndexRecord(void)	{}
		~IndexRecord(void)	{}
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		SerializableArray<BinaryString> blockDigests;
	};

	class DirectoryRecord: public MetaRecord
	{
	public:
		DirectoryRecord(void)	{}
		~DirectoryRecord(void)	{}
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
		BinaryString	digest;
		Time 		time;
	};
	
	IndexRecord getIndexRecord(void) const;
	DirectoryRecord getDirectoryRecord(Time recordTime = 0) const;
	
	class Reader : public Stream
	{
	public:
		Reader(Resource *resource);
		~Reader(void);
	  
		// Stream
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		int64_t tellRead(void) const;
		int64_t tellWrite(void) const;
		
		bool readDirectory(DirectoryRecord &record);
		
	private:
		Block *createBlock(int index); 
	  
		Resource *mResource;
		int64_t mReadPosition;
		
		int mCurrentBlockIndex;
		Block *mCurrentBlock;
		Block *mNextBlock;
	};
	
protected:
	Block *mIndexBlock;
	IndexRecord *mIndexRecord;
	
	friend class Indexer;
};

bool operator <  (const Resource &r1, const Resource &r2);
bool operator >  (const Resource &r1, const Resource &r2);
bool operator == (const Resource &r1, const Resource &r2);
bool operator != (const Resource &r1, const Resource &r2);

}

#endif

