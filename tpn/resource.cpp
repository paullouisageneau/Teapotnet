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
#include "tpn/store.h"
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

Resource::Resource(const BinaryString &digest, bool localOnly) :
	mIndexBlock(NULL),
	mIndexRecord(NULL)
{
	fetch(digest, localOnly);
}

Resource::~Resource(void)
{
	delete mIndexBlock;
	delete mIndexRecord;
}

void Resource::fetch(const BinaryString &digest, bool localOnly)
{
	delete mIndexBlock;
	delete mIndexRecord;
	mIndexRecord = NULL;
	mIndexBlock = NULL;
	
	if(localOnly && !Store::Instance->hasBlock(digest))
		throw Exception(String("Local resource not found: ") + digest.toString());
	
	//LogDebug("Resource::fetch", "Fetching resource " + digest.toString());
	
	try {
		mIndexBlock = new Block(digest);
		mIndexRecord = new IndexRecord;
		
		//LogDebug("Resource::fetch", "Reading index block for " + digest.toString());
		
		BinarySerializer serializer(mIndexBlock);
		AssertIO(static_cast<Serializer*>(&serializer)->input(mIndexRecord));
	}
	catch(const std::exception &e)
	{
		delete mIndexBlock;
		delete mIndexRecord;
		mIndexRecord = NULL;
		mIndexBlock = NULL;
		throw Exception(String("Unable to fetch resource index block: ") + e.what());
	}

}

void Resource::process(const String &filename, const String &name, const String &type, const String &secret)
{
	BinaryString salt;
	
	// If secret is not empty then this is an encrypted resource
	if(!secret.empty())
	{
		// Generate salt from plaintext
		// Because we need to be able to recognize data is identical even when encrypted
		BinaryString digest;
		File file(filename, File::Read);
		Sha256().compute(file, digest);
		Sha256().pbkdf2_hmac(salt, type, digest, 32, 100000);
	}
	
	// Fill index record
	delete mIndexRecord;
	int64_t size = File::Size(filename);
	mIndexRecord = new Resource::IndexRecord;
	mIndexRecord->name = name;
	mIndexRecord->type = type;
	mIndexRecord->size = size;
	mIndexRecord->salt = salt;
	mIndexRecord->blockDigests.reserve(size/Block::Size);
	
	// Process blocks
	File file(filename, File::Read);
	BinaryString blockDigest;
	
	if(!secret.empty())
	{
		BinaryString key;
		Sha256().pbkdf2_hmac(secret, salt, key, 32, 100000);
		
		uint64_t i = 0;
		while(true)
		{
			BinaryString subsalt;
			subsalt.writeBinary(i);
			
			// Generate subkey
			BinaryString subkey;
			Sha256().pbkdf2_hmac(key, subsalt, subkey, 32, 100);
			
			// Generate iv
			BinaryString iv;
			Sha256().pbkdf2_hmac(salt, subsalt, iv, 16, 100);
			
			if(!Block::EncryptFile(file, subkey, iv, blockDigest))
				break;
			
			mIndexRecord->blockDigests.append(blockDigest);
			++i;
		}
	} 
	else {
		while(Block::ProcessFile(file, blockDigest))
			mIndexRecord->blockDigests.append(blockDigest);
	}
	
	// Create index
	String tempFileName = File::TempName();
	File tempFile(tempFileName, File::Truncate);
	BinarySerializer serializer(&tempFile);
	serializer.write(mIndexRecord);
	tempFile.close();
	String indexFilePath = Cache::Instance->move(tempFileName);
	
	// Create index block
	delete mIndexBlock;
	mIndexBlock = new Block(indexFilePath);
}

void Resource::cache(const String &filename, const String &name, const String &type, const String &secret)
{
	if(!secret.empty())
	{
		// Blocks will be copied to cache since resource is encrypted
		process(filename, name, type, secret);
		File::Remove(filename);
	}
	else {
		// Move to cache and process
		process(Cache::Instance->move(filename), name, type, "");
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

int Resource::blockIndex(int64_t position, size_t *offset) const
{
	if(!mIndexBlock || position < 0 || (position > 0 && position >= mIndexRecord->size))
		throw OutOfBounds("Resource position out of bounds");
  
	// TODO: block size in record ?
	if(offset) *offset = size_t(position % Block::Size);
	return int(position/Block::Size);
}

BinaryString Resource::blockDigest(int index) const
{
	if(!mIndexBlock || index < 0 || index >= mIndexRecord->blockDigests.size())
		throw OutOfBounds("Block index out of bounds");
	
	return mIndexRecord->blockDigests.at(index);  
}

String Resource::name(void) const
{
	if(mIndexRecord) return mIndexRecord->name;
	else return "";
}

String Resource::type(void) const
{
	if(mIndexRecord) return mIndexRecord->type;
	else return "";
}

int64_t Resource::size(void) const
{
	if(mIndexRecord) return mIndexRecord->size;
	else return 0;
}

BinaryString Resource::salt(void) const
{
	if(mIndexRecord) return mIndexRecord->salt;
	else return "";
}

bool Resource::isDirectory(void) const
{
	return (type() == "directory");
}

bool Resource::isLocallyAvailable(void) const
{
	if(!mIndexRecord) return false;
	
	for(int i=0; i<mIndexRecord->blockDigests.size(); ++i)
		if(!Store::Instance->hasBlock(mIndexRecord->blockDigests[i]))
			return false;
	
	return true;
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
	mIndexBlock = new Block(*resource.mIndexBlock);
	mIndexRecord = new IndexRecord(*resource.mIndexRecord);
}

bool operator <  (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return true;
	if(!r1.isDirectory() && r2.isDirectory()) return false;
	return r1.name().toLower() < r2.name().toLower();
}

bool operator >  (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return false;
	if(!r1.isDirectory() && r2.isDirectory()) return true;
	return r1.name().toLower() > r2.name().toLower();
}

bool operator == (const Resource &r1, const Resource &r2)
{
	if(r1.name() != r2.name()) return false;
	return r1.digest() == r2.digest() && r1.isDirectory() == r2.isDirectory();
}

bool operator != (const Resource &r1, const Resource &r2)
{
	return !(r1 == r2);
}

Resource::IndexRecord Resource::getIndexRecord(void) const
{
	if(!mIndexRecord) throw Exception("No index record for the resource");
	return *mIndexRecord;
}

Resource::DirectoryRecord Resource::getDirectoryRecord(Time recordTime) const
{
	 if(!mIndexRecord) throw Exception("No index record for the resource");
	 
	Resource::DirectoryRecord record;
	*static_cast<Resource::MetaRecord*>(&record) = *static_cast<Resource::MetaRecord*>(mIndexRecord);
	record.digest = digest();
	record.time = recordTime;
	return record;
}

Resource::Reader::Reader(Resource *resource, const String &secret, bool nocheck) :
	mResource(resource),
	mReadPosition(0),
	mCurrentBlock(NULL),
	mNextBlock(NULL)
{
	Assert(mResource);
	
	if(!secret.empty())
	{
		if(!nocheck && mResource->salt().empty())
			throw Exception("Expected encrypted resource");
		
		Sha256().pbkdf2_hmac(secret, mResource->salt(), mKey, 32, 100000); 
	}
	else {
		if(!nocheck && !mResource->salt().empty())
			throw Exception("Expected non-encrypted resource");
	}
	
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
	
	if(!mKey.empty() && !mCurrentBlock->hasDecryption())
	{
		BinaryString subsalt;
		subsalt.writeBinary(uint64_t(mCurrentBlockIndex));
		
		// Generate subkey
		BinaryString subkey;
		Sha256().pbkdf2_hmac(mKey, subsalt, subkey, 32, 100);
		
		// Generate iv
		BinaryString iv;
		Sha256().pbkdf2_hmac(mResource->salt(), subsalt, iv, 16, 100);
		
		// Initialize decryption process
		mCurrentBlock->setDecryption(subkey, iv);
	}
	
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
	
	size_t offset = 0;
	mCurrentBlockIndex = mResource->blockIndex(position, &offset);
	mCurrentBlock	= createBlock(mCurrentBlockIndex);
	mNextBlock	= createBlock(mCurrentBlockIndex + 1);
	mReadPosition	= position;
	
	if(mCurrentBlock) 
		mCurrentBlock->seekRead(offset);
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

bool Resource::Reader::readDirectory(DirectoryRecord &record)
{
	BinarySerializer serializer(this);
	return serializer.read(record);
}

Block *Resource::Reader::createBlock(int index)
{
	if(index < 0 || index >= mResource->blocksCount()) return NULL;
	
	//LogDebug("Resource::Reader", "Creating block " + String::number(index) + " over " + String::number(mResource->blocksCount()));
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
	SerializableWrapper<int64_t> sizeWrapper(&size);
	
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
	mapping["salt"] = &salt;
	
	s.outputObject(mapping);
}

bool Resource::IndexRecord::deserialize(Serializer &s)
{
	SerializableWrapper<int64_t> sizeWrapper(&size);
	
	Serializer::ObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	mapping["digests"] = &blockDigests;
	mapping["salt"] = &salt;
	
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
	if(time != 0) mapping["time"] = &time;
	
	s.outputObject(mapping);
}

bool Resource::DirectoryRecord::deserialize(Serializer &s)
{
	SerializableWrapper<int64_t> sizeWrapper(&size);
	
	Serializer::ObjectMapping mapping;
	mapping["name"] = &name;
	mapping["type"] = &type;
	mapping["size"] = &sizeWrapper;
	mapping["digest"] = &digest;
	mapping["time"] = &time;
	
	return s.inputObject(mapping);
}

}
