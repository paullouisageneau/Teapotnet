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
#include "tpn/store.h"

namespace tpn
{

bool Block::ProcessFile(File &file, BinaryString &digest)
{
	int64_t offset = file.tellRead();
	int64_t size = Sha256().compute(file, Size, digest);
	
	if(size)
	{
		Store::Instance->notifyBlock(digest, file.name(), offset, size);
		return true;
	}
	
	return false;
}

bool Block::ProcessFile(File &file, Block &block)
{
	int64_t offset = file.tellRead();
  
	if(!ProcessFile(file, block.mDigest))
		return false;
	
	block.mFile = new File(file.name(), File::Read);
	block.mOffset = offset;
	block.mSize = Size;
	block.mFile->seekRead(offset);
	
	// store is already notified
}
 
Block::Block(const Block &block)
{
	mFile = NULL;
	mOffset = 0;
	mSize = 0;
	*this = block;
}
 
Block::Block(const BinaryString &digest) :
	mDigest(digest)
{
	mFile = Store::Instance->getBlock(digest, mSize);
	if(mFile)
	{
		mOffset = mFile->tellRead();
	}
	else {
		mOffset = 0;
		mSize = -1;
	}
}

Block::Block(const BinaryString &digest, const String &filename, int64_t offset, int64_t size) :
	mDigest(digest)
{
	mFile = new File(filename, File::Write);
	mOffset = offset;
	mSize = size;
	
	notifyStore();
}

Block::Block(const String &filename, int64_t offset, int64_t size)
{
	mFile = new File(filename, File::Read);
	mOffset = offset;
	
	mFile->seekRead(mOffset);
	if(size >= 0) mSize = Sha256().compute(*mFile, size, mDigest);
	else mSize = Sha256().compute(*mFile, mDigest);
	
	mFile->seekRead(mOffset);
	notifyStore();
}

Block::~Block(void)
{
	delete mFile;
}

BinaryString Block::digest(void) const
{
	return mDigest; 
}

size_t Block::readData(char *buffer, size_t size)
{
	waitContent();
	int64_t left = mSize - tellRead();
	if(left <= 0) return 0;
	size = size_t(std::min(left, int64_t(size)));
	
	return mFile->readData(buffer, size);
}

void Block::writeData(const char *data, size_t size)
{
	throw Unsupported("Writing to Block");
}

bool Block::waitData(double &timeout)
{
	if(!waitContent(timeout)) return false;
	return mFile->waitData(timeout);
}

void Block::seekRead(int64_t position)
{
	waitContent();
	mFile->seekRead(mOffset + position);
}

void Block::seekWrite(int64_t position)
{
	throw Unsupported("Writing to Block");
}

int64_t Block::tellRead(void) const
{
	return mFile->tellRead() - mOffset;
}

int64_t Block::tellWrite(void) const
{
	return 0;
}

Block &Block::operator = (const Block &block)
{
	mDigest = block.mDigest;
	mOffset = block.mOffset;
	mSize = block.mSize;
	mFile = new File(block.mFile->name());
}

void Block::waitContent(void) const
{
	if(!mFile)
	{
		Store::Instance->waitBlock(mDigest);
		mFile = Store::Instance->getBlock(mDigest, mSize);
		Assert(mFile);
		mOffset = mFile->tellRead();
	}
	else if(mFile->openMode() == File::Write)
	{
		Store::Instance->waitBlock(mDigest);
		File *source = Store::Instance->getBlock(mDigest, mSize);
		Assert(mFile);
		mFile->seekWrite(mOffset);
		mFile->write(*source);
		mFile->reopen(File::Read);
		mFile->seekRead(mOffset);
		delete source;
		
		// TODO: remove block from cache
	}
	else {
		// Content is available, don't wait for Store !
	}
}

bool Block::waitContent(double &timeout) const
{
	if(!Store::Instance->waitBlock(mDigest, timeout))
		return false;
	
	waitContent();
	return true;
}

bool Block::waitContent(const double &timeout) const
{
	double dummy = timeout;
	return waitContent(dummy);
}

void Block::notifyStore(void) const
{
	Assert(mFile);
	Assert(mFile->openMode() == File::Read);
	
	Store::Instance->notifyBlock(mDigest, mFile->name(), mOffset, mSize);
}


}
