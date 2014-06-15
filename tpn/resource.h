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

class Resource
{
public:
	enum AccessLevel { Public, Private, Personal };
	
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
		
	private:
		Block *createBlock(int index); 
	  
		Resource *mResource;
		int64_t mReadPosition;
		
		int mCurrentBlockIndex;
		Block *mCurrentBlock;
		Block *mNextBlock;
	};
	
	static int CreatePlaylist(const Set<Resource> &resources, Stream *output, String host = "");
	
	Resource(void);
	Resource(const BinaryString &digest);
	~Resource(void);
	
	void fetch(const BinaryString &digest);
	BinaryString digest(void) const;
	
	int blocksCount(void) const;
	int blockIndex(int64_t position) const;
	BinaryString blockDigest(int index) const;
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;
	
protected:  
	class MetaRecord : public Serializable
	{
	public:
		MetaRecord(void);
		virtual ~MetaRecord(void);

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
		IndexRecord(void);
		~IndexRecord(void);
		
		Array<BinaryString> blockDigests;
	};

	class DirectoryRecord: public MetaRecord
	{
	public:
		DirectoryRecord(void);
		~DirectoryRecord(void);

		BinaryString	digest;
		Time 		time;
	};
	
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

