/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "stripedfile.h"

namespace arc
{

StripedFile::StripedFile(File *file, size_t blockSize, int nbStripes, int stripe) :
		mFile(file),
		mBlockSize(blockSize),
		mStripeSize(blockSize/nbStripes),
		mStripe(stripe)
{
	mFile->seekg(stripe*mStripeSize, File::beg);
	mReadBlock = mWriteBlock = 0;
	mReadOffset = mWriteOffset = 0;
}

StripedFile::~StripedFile(void)
{
	mFile->close();
	delete mFile;
}

uint64_t StripedFile::tellRead(void) const
{
	return uint64_t(tellReadBlock())*mStripeSize + uint64_t(tellReadOffset());
}

size_t StripedFile::tellReadBlock(void) const
{
	return mReadBlock;
}

size_t StripedFile::tellReadOffset(void) const
{
	return mReadOffset;
}

uint64_t StripedFile::tellWrite(void) const
{
	return uint64_t(tellWriteBlock())*mStripeSize + uint64_t(tellWriteOffset());
}

size_t StripedFile::tellWriteBlock(void) const
{
	return mWriteBlock;
}

size_t StripedFile::tellWriteOffset(void) const
{
	return mWriteOffset;
}

void StripedFile::seekRead(uint64_t position)
{
	seekRead(position / mStripeSize, position % mStripeSize);
}

void StripedFile::seekRead(size_t block, size_t offset)
{
	if(mReadBlock <= block)
	{
		mFile->seekg(offset-mReadOffset, File::cur);
		mReadOffset = offset;
	}
	else {
		mFile->seekg(mStripe*mStripeSize + offset, File::beg);
		mReadBlock = 0;
		mReadOffset = offset;
	}

	while(mReadBlock < block)
	{
		mFile->seekg(mBlockSize, File::cur);
		++mReadBlock;
	}
}

void StripedFile::seekWrite(uint64_t position)
{
	seekWrite(position / mStripeSize, position % mStripeSize);
}

void StripedFile::seekWrite(size_t block, size_t offset)
{
	// WARNING: seekp() MUST allow seeking past the end of the file.
	// If it is not the case, this WILL NOT work.
  
	if(mWriteBlock <= block)
	{
		mFile->seekp(offset-mWriteOffset, File::cur);
		mWriteOffset = offset;
	}
	else {
		mFile->seekp(mStripe*mStripeSize + offset, File::beg);
		mWriteBlock = 0;
		mWriteOffset = offset;
	}

	while(mWriteBlock < block)
	{
		mFile->seekp(mBlockSize, File::cur);
		++mWriteBlock;
	}
}

size_t StripedFile::readData(char *buffer, size_t size)
{
	if(!size) return 0;

	const size_t left = std::min(mStripeSize - mReadOffset, size);
	const size_t len = mFile->readData(buffer, left);
	mReadOffset+= len;

	if(mReadOffset == mStripeSize)
	{
		// Move to the beginning of the stripe on the next block
		seekRead(mReadBlock+1, 0);
		return len + readData(buffer+len, size-len);
	}

	return len;
}

void StripedFile::writeData(const char *buffer, size_t size)
{
	if(!size) return;

	const size_t len = std::min(mStripeSize - mWriteOffset, size);

	mFile->writeData(buffer, len);
	mWriteOffset+= len;

	if(mWriteOffset == mStripeSize)
	{
		// Move to the beginning of the stripe on the next block
		seekWrite(mWriteBlock+1, 0);
		writeData(buffer+len, size-len);
	}
}

}
