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

#include "tpn/resource.hpp"
#include "tpn/cache.hpp"
#include "tpn/store.hpp"
#include "tpn/config.hpp"

#include "pla/binaryserializer.hpp"
#include "pla/object.hpp"
#include "pla/jsonserializer.hpp"

namespace tpn
{

Resource::Resource(void) :
	mIndexBlock(NULL),
	mIndexRecord(NULL),
	mLocalOnly(false)
{

}

Resource::Resource(const Resource &resource) :
	mIndexBlock(NULL),
	mIndexRecord(NULL),
	mLocalOnly(false)
{
	*this = resource;
}

Resource::Resource(const BinaryString &digest, bool localOnly) :
	mIndexBlock(NULL),
	mIndexRecord(NULL),
	mLocalOnly(false)
{
	fetch(digest, localOnly);
}

Resource::~Resource(void)
{

}

void Resource::fetch(const BinaryString &digest, bool localOnly)
{
	mLocalOnly = localOnly;
	mIndexRecord.reset();
	mIndexBlock.reset();
	
	if(mLocalOnly && !Store::Instance->hasBlock(digest))
		throw Exception(String("Local resource not found: ") + digest.toString());
	
	//LogDebug("Resource::fetch", "Fetching resource " + digest.toString());
	
	try {
		mIndexBlock = std::make_shared<Block>(digest);
		if(mLocalOnly && !mIndexBlock->isLocallyAvailable())
			throw Exception("Block is not available locally");
		
		//LogDebug("Resource::fetch", "Reading index block for " + digest.toString());
		
		mIndexRecord = std::make_shared<IndexRecord>();
		AssertIO(BinarySerializer(mIndexBlock.get()) >> mIndexRecord);
		
		for(const BinaryString &digest : mIndexRecord->blockDigests)
			Store::Instance->hintBlock(digest, mIndexBlock->digest());
	}
	catch(const std::exception &e)
	{
		mIndexRecord.reset();
		mIndexBlock.reset();
		throw Exception(String("Unable to fetch resource index block: ") + e.what());
	}

}

void Resource::process(const String &filename, const String &name, const String &type, const String &secret, bool cache)
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
		Sha256().pbkdf2_hmac(digest, type, salt, 32, 100000);
		Assert(!salt.empty());
	}
	
	// Fill index record
	int64_t size = File::Size(filename);
	mIndexRecord = std::make_shared<Resource::IndexRecord>();
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
			
			// Generate IV
			BinaryString iv;
			Sha256().pbkdf2_hmac(salt, subsalt, iv, 16, 100);
			
			if(!Block::EncryptFile(file, subkey, iv, blockDigest))
				break;
			
			mIndexRecord->blockDigests.append(blockDigest);
			++i;
		}
	} 
	else {
		while(Block::ProcessFile(file, blockDigest, cache))
			mIndexRecord->blockDigests.append(blockDigest);
	}
	
	// Create index
	String tempFileName = File::TempName();
	File tempFile(tempFileName, File::Truncate);
	BinarySerializer serializer(&tempFile);
	serializer << mIndexRecord;
	tempFile.close();
	String indexFilePath = Cache::Instance->move(tempFileName);
	
	// Create index block
	mIndexBlock = std::make_shared<Block>(indexFilePath);
}

void Resource::cache(const String &filename, const String &name, const String &type, const String &secret)
{
	// Process in cache mode
	process(filename, name, type, "", true);
	
	// And remove the original file
	File::Remove(filename);
}

BinaryString Resource::digest(void) const
{
	if(mIndexBlock) return mIndexBlock->digest();
	else return BinaryString();
}

int Resource::blocksCount(void) const
{
	if(mIndexRecord) return int(mIndexRecord->blockDigests.size());
	else return 0;
}

int Resource::blockIndex(int64_t position, size_t *offset) const
{
	if(!mIndexBlock || position < 0 || (position > 0 && position >= mIndexRecord->size))
		throw OutOfBounds("Resource position out of bounds");
  
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

bool Resource::isSigned(void) const
{
	if(mIndexRecord) return !mIndexRecord->signature.empty();
	else return false;
}

bool Resource::check(const Rsa::PublicKey &pubKey) const
{
	// TODO: signature checking
	return false;
}

void Resource::serialize(Serializer &s) const
{
	if(!mIndexRecord) throw Unsupported("Serializing empty resource");
	
	s << Object()
		.insert("name", mIndexRecord->name)
		.insert("type", mIndexRecord->type)
		.insert("size", mIndexRecord->size)
		.insert("digest", mIndexBlock->digest());
}

bool Resource::deserialize(Serializer &s)
{
	throw Unsupported("Deserializing resource");
}

bool Resource::isInlineSerializable(void) const
{
	return false;
}

Resource &Resource::operator= (const Resource &resource)
{
	mIndexBlock = resource.mIndexBlock;
	mIndexRecord = resource.mIndexRecord;
	return *this;
}

bool operator< (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return true;
	if(!r1.isDirectory() && r2.isDirectory()) return false;
	return r1.name().toLower() < r2.name().toLower();
}

bool operator> (const Resource &r1, const Resource &r2)
{
	if(r1.isDirectory() && !r2.isDirectory()) return false;
	if(!r1.isDirectory() && r2.isDirectory()) return true;
	return r1.name().toLower() > r2.name().toLower();
}

bool operator==(const Resource &r1, const Resource &r2)
{
	if(r1.name() != r2.name()) return false;
	return r1.digest() == r2.digest() && r1.isDirectory() == r2.isDirectory();
}

bool operator!=(const Resource &r1, const Resource &r2)
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
	*static_cast<Resource::MetaRecord*>(&record) = *static_cast<Resource::MetaRecord*>(mIndexRecord.get());
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
		
		// Generate IV
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
	if(!(serializer >> record)) return false;
	
	Store::Instance->hintBlock(record.digest, mResource->digest());
	return true;
}

sptr<Block> Resource::Reader::createBlock(int index)
{
	if(index < 0 || index >= mResource->blocksCount()) return NULL;
	
	//LogDebug("Resource::Reader", "Creating block " + String::number(index) + " over " + String::number(mResource->blocksCount()));
	sptr<Block> block = std::make_shared<Block>(mResource->blockDigest(index), mResource->digest());
	if(mResource->mLocalOnly && !block->isLocallyAvailable())
		throw Exception("Block is not available locally");
	
	return block;
}

void Resource::MetaRecord::serialize(Serializer &s) const
{
	s << Object()
		.insert("name", name)
		.insert("type", type)
		.insert("size", size);
}

bool Resource::MetaRecord::deserialize(Serializer &s)
{
	return !!(s >> Object()
		.insert("name", name)
		.insert("type", type)
		.insert("size", size));
}

bool Resource::MetaRecord::isInlineSerializable(void) const
{
	return false;
}

void Resource::IndexRecord::serialize(Serializer &s) const
{
	Object object;
	object.insert("name", name)
	      .insert("type", type)
	      .insert("size", size)
	      .insert("digests", blockDigests);
	
	if(!signature.empty()) object.insert("signature", signature);
	if(!salt.empty()) object.insert("salt", salt);
	
	s << object;
}

bool Resource::IndexRecord::deserialize(Serializer &s)
{
	return !!(s >> Object()
		.insert("name", name)
		.insert("type", type)
		.insert("size", size)
		.insert("digests", blockDigests)
		.insert("signature", signature)
		.insert("salt", salt));
}

void Resource::DirectoryRecord::serialize(Serializer &s) const
{
	Object object;
	object.insert("name", name)
	      .insert("type", type)
	      .insert("size", size)
	      .insert("digest", digest);
	
	if(time != 0) object.insert("time", time);
	
	s << object;
}

bool Resource::DirectoryRecord::deserialize(Serializer &s)
{
	return !!(s >> Object()
		.insert("name", name)
		.insert("type", type)
		.insert("size", size)
		.insert("digest", digest)
		.insert("time", time));
}

Resource::ImportTask::ImportTask(Serializable *object, 
	const BinaryString &digest,
	const String &type,
	const BinaryString &secret) :
	mObject(object),
	mDigest(digest),
	mSecret(secret),
	mType(type)
{
	Assert(mObject);
}
		
void Resource::ImportTask::operator()(void)
{
	try {
		Resource resource(mDigest);
		if(!mType.empty() && resource.type() != mType)
			return;
		
		Reader reader(&resource, mSecret);
		JsonSerializer serializer(&reader);
		serializer >> *mObject;
	}
	catch(const std::exception &e)
	{
		LogWarn("Resource::ImportTask", e.what());
	}
}

}
