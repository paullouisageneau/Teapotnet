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

#include "tpn/block.h"

namespace tpn
{

bool Block::ProcessFile(File &file, Block &block)
{
	// TODO: clear and unregister
	
	block.mFilename = file.name();
	block.mOffset = file.tellRead();
	
	int64_t maxSize = MaxChunks*ChunkSize;
	int64_t size = Sha256().compute(file, maxSize, block.mDigest);
	if(!size) return false;
	
	block.save();
	return true;
}

Block::Block(void)
{

}

Block::Block(const BinaryString &digest)
{
	load(digest);
}

Block::~Block(void)
{
	Cache::Instance->unregisterBlock(mDigest, this);
}

void Block::load(const BinaryString &digest)
{
	if(!mDigest.empty()) 
		Cache::Instance->unregisterBlock(mDigest, this);
		
	mDigest = digest;
	Cache::Instance->registerBlock(mDigest, this);
}

void Block::save(void)
{
	// TODO
	computeDigest();
	Cache::Instance->registerBlock(mDigest, this);
}

Block::Block(File *file, bool refresh)
{
	Assert(file);
	mFile = file;
	
	if(mFile->mode() == File::Read) 
	{
		if(refresh) throw Exception("Unable to refresh fountain, file is read-only");
		mMapFile = NULL;
	}
	else {
		mMapFile = new File(mFile->name()+".map", (refresh ? File::TruncateReadWrite : File::ReadWrite));
	}
}

Block::~Block(void)
{
	delete mFile;
	delete mMapFile;
}

size_t Block::readChunk(int64_t offset, char *buffer, size_t size)
{
	Synchronize(this);
	if(!isWritten(offset)) return 0;
	mFile->seekRead(offset*ChunkSize);
	return mFile->readData(buffer, size);
}

void Block::writeChunk(int64_t offset, const char *data, size_t size)
{
	Synchronize(this);
	mFile->seekWrite(offset*ChunkSize);
	mFile->writeData(data, size);
	markWritten(offset);
	
	if(isComplete()) stopCalling();
}

bool Block::checkChunk(int64_t offset)
{
	Synchronize(this);
	return isWritten(offset);
}

bool Block::isWritten(int64_t offset)
{
	Synchronize(this);
	if(!mMapFile) return true;
	
	uint8_t byte = 0;
	uint8_t mask = 1 << (offset%8);
	offset/= 8;

	if(offset >= mMapFile->size())
		return false;

	mMapFile->seekRead(offset);
	mMapFile->readBinary(byte);
	return (byte & mask) != 0;
}

void Block::markWritten(int64_t offset)
{
	Synchronize(this);
	if(!mMapFile) return;

	uint8_t byte = 1 << (offset%8);
	offset/= 8;

	if(offset > mMapFile->size())
	{
		mMapFile->seekWrite(mMapFile->size());
		mMapFile->writeZero(offset - mMapFile->size());
	}

	mMapFile->seekWrite(offset);	
	mMapFile->writeBinary(byte);
	notifyAll();	
	return;
}

Block::Reader::Reader(Block *fountain) :
	mBlock(fountain)
{
	Assert(fountain);
}

Block::Reader::~Reader(void)
{
	
}

size_t Block::Reader::readData(char *buffer, size_t size)
{
	Synchronize(mBlock);
	
	int64_t offset = mReadPosition/ChunkSize;
	size = std::min(size, size_t(mReadPosition%ChunkSize));
	
	while(!mBlock->isWritten(offset))
	{
		mBlock->startCalling();
		mBlock->wait();
	}

	mBlock->mFile->seekRead(mReadPosition);
	size = mBlock->mFile->readData(buffer, size);
	mReadPosition+= size;
	return size;
}

void Block::Reader::writeData(const char *buffer, size_t size)
{
	throw Unsupported("Writing to Block::Reader");
}

void Block::Reader::seekRead(int64_t position)
{
	mReadPosition = position;
}

void Block::Reader::seekWrite(int64_t position)
{
	throw Unsupported("Writing to Block::Reader");
}

}
