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

#include "tpn/resource.h"
#include "tpn/config.h"

#include "pla/binaryserializer.h"

namespace tpn
{

Resource::Resource(void) :
	mIndexBlock(NULL),
	mIndexRecord(NULL)
{

}

Resource::Resource(const Resource &resource) :
	mIndexBlock(NULL),
	mIndexRecord(NULL)
{
	*this = resource;
}

Resource::Resource(const BinaryString &digest) :
	mIndexBlock(NULL),
	mIndexRecord(NULL)
{
	fetch(digest);
}

Resource::~Resource(void)
{
	delete mIndexBlock;
	delete mIndexRecord;
}

void Resource::fetch(const BinaryString &digest)
{
	delete mIndexBlock;
	delete mIndexRecord;
	mIndexRecord = NULL;
	mIndexBlock = NULL;
	
	try {
		mIndexBlock = new Block(digest);
		mIndexRecord = new IndexRecord;
	
		BinarySerializer serializer(mIndexBlock);
		AssertIO(serializer.input(*mIndexRecord));
	}
	catch(...)
	{
		delete mIndexBlock;
		delete mIndexRecord;
		mIndexRecord = NULL;
		mIndexBlock = NULL;
		throw;
	}

}

BinaryString Resource::digest(void) const
{
	if(!mIndexBlock) return BinaryString();
	return mIndexBlock->digest();
}

int Resource::blocksCount(void) const
{
	if(!mIndexRecord) return 0;
	return int(mIndexRecord->blockDigests.size());
}

int Resource::blockIndex(int64_t position) const
{
	if(!mIndexBlock || position < 0 || position >= mIndexRecord->size)
		throw OutOfBounds("Resource position out of bounds");
  
	// TODO: block size in record ?
	return int(position/Block::Size);
}

BinaryString Resource::blockDigest(int index) const
{
	if(!mIndexBlock || index < 0 || index >= mIndexRecord->blockDigests.size())
		throw OutOfBounds("Block index out of bounds");
	return mIndexRecord->blockDigests.at(index);  
}

void Resource::serialize(Serializer &s) const
{
	if(!mIndexRecord) throw Unsupported("Serializing empty resource");
  
	ConstSerializableWrapper<int64_t> sizeWrapper(mIndexRecord->size);
	BinaryString digest(mIndexBlock->digest());
	
	Serializer::ConstObjectMapping mapping;
	mapping["name"] = &mIndexRecord->name;
        mapping["type"] = &mIndexRecord->type;
	mapping["size"] = &sizeWrapper;
	mapping["digest"] = &digest;

	s.outputObject(mapping);
}

bool Resource::deserialize(Serializer &s)
{
	throw Unsupported("Deserializing resource");
	return false;
}

bool Resource::isInlineSerializable(void) const
{
	return false;
}

Resource &Resource::operator = (const Resource &resource)
{
	delete mIndexBlock;
	delete mIndexRecord;
	mIndexBlock = new Block(resource.mIndexBlock);
	mIndexRecord = new IndexRecord(resource.mIndexRecord);
}

bool operator <  (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return true;
	if(!r1.isDirectory() && r2.isDirectory()) return false;
	return r1.url().toLower() < r2.url().toLower();
}

bool operator >  (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return false;
	if(!r1.isDirectory() && r2.isDirectory()) return true;
	return r1.url().toLower() > r2.url().toLower();
}

bool operator == (const Resource &r1, const Resource &r2)
{
	if(r1.url() != r2.url()) return false;
	return r1.digest() == r2.digest() && r1.isDirectory() == r2.isDirectory();
}

bool operator != (const Resource &r1, const Resource &r2)
{
	return !(r1 == r2);
}

Resource::Reader::Reader(Resource *resource) :
	mResource(resource),
	mReadPosition(0)
	mCurrentBlock(NULL),
	mNextBlock(NULL)
{
	Assert(mResource);
	seekRead(0);	// Initialize positions
}

Resource::Reader::~Reader(void)
{
	delete mCurrentBlock;
	delete mNextBlock;
}
	  
size_t Resource::Reader::readData(char *buffer, size_t size)
{
	if(!mCurrentBlock) return 0;	// EOF
  
	size_t ret;
	if((ret = mCurrentBlock->readData(buffer, size)))
	{
		mReadPosition+= ret;
		return ret;
	}
	
	delete mCurrentBlock;
	++mCurrentBlockIndex;
	mCurrentBlock = mNextBlock;
	mNextBlock = createBlock(mCurrentBlockIndex + 1);
	return readData(buffer, size);
}

void Resource::Reader::writeData(const char *data, size_t size)
{
	throw Unsupported("Writing to Resource::Reader");
}

void Resource::Reader::seekRead(int64_t position)
{
	delete mCurrentBlock;
	delete mNextBlock;
	
	mCurrentBlockIndex = mResource->blockIndex(position);
	mCurrentBlock	= createBlock(mCurrentBlockIndex);
	mNextBlock	= createBlock(mCurrentBlockIndex + 1);
	mReadPosition	= position;
}

void Resource::Reader::seekWrite(int64_t position)
{
	throw Unsupported("Writing to Resource::Reader");
}

int64_t Resource::Reader::tellRead(void) const
{
	return mReadPosition;  
}

int64_t Resource::Reader::tellWrite(void) const
{
	return 0;  
}

Block *Resource::Reader::createBlock(int index)
{
	if(index < 0 || index >= mResource->blocksCount()) return NULL;
	return new Block(mResource->blockDigest(index)); 
}

void Resource::MetaRecord::serialize(Serializer &s) const
{
	ConstSerializableWrapper<int64_t> sizeWrapper(size);
	
	Serializer::ConstObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	
	s.outputObject(mapping);
}

bool Resource::MetaRecord::deserialize(Serializer &s)
{
	SerializableWrapper<int64_t> sizeWrapper(size);
	
	Serializer::ObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	
	return s.inputObject(mapping);
}

bool Resource::MetaRecord::isInlineSerializable(void) const
{
	return false;
}

void Resource::IndexRecord::serialize(Serializer &s) const
{
	ConstSerializableWrapper<int64_t> sizeWrapper(size);
	
	Serializer::ConstObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	mapping["digests"] = &blockDigests;
	
	s.outputObject(mapping);
}

bool Resource::IndexRecord::deserialize(Serializer &s)
{
	SerializableWrapper<int64_t> sizeWrapper(size);
	
	Serializer::ObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	mapping["digests"] = &blockDigests;
	
	return s.inputObject(mapping);
}

void Resource::DirectoryRecord::serialize(Serializer &s) const
{
	ConstSerializableWrapper<int64_t> sizeWrapper(size);
	
	Serializer::ConstObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	mapping["digest"] = &digest;
	mapping["time"] = &time;
	
	s.outputObject(mapping);
}

bool Resource::DirectoryRecord::deserialize(Serializer &s)
{
	SerializableWrapper<int64_t> sizeWrapper(size);
	
	Serializer::ObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	mapping["digest"] = &digest;
	mapping["time"] = &time;
	
	return s.inputObject(mapping);
}

}
