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

#include "tpn/stripedfile.h"

namespace tpn
{

StripedFile::StripedFile(File *file, size_t blockSize, int nbStripes, int stripe) :
		mFile(file),
		mBlockSize(blockSize),
		mStripeSize(blockSize/nbStripes),
		mStripeOffset(stripe*mStripeSize)
{
	if(stripe == nbStripes-1)
		mStripeSize+= mBlockSize - nbStripes*mStripeSize;
  
	mFile->seekg(mStripeOffset, File::beg);
	mFile->seekp(mStripeOffset, File::beg);
	
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
	Synchronize(this);
	return uint64_t(tellReadBlock())*mStripeSize + uint64_t(tellReadOffset());
}

unsigned StripedFile::tellReadBlock(void) const
{
	Synchronize(this);
	return mReadBlock;
}

size_t StripedFile::tellReadOffset(void) const
{
	Synchronize(this);
	return mReadOffset;
}

uint64_t StripedFile::tellWrite(void) const
{
	Synchronize(this);
	return uint64_t(tellWriteBlock())*mStripeSize + uint64_t(tellWriteOffset());
}

unsigned StripedFile::tellWriteBlock(void) const
{
	Synchronize(this);
	return mWriteBlock;
}

size_t StripedFile::tellWriteOffset(void) const
{
	Synchronize(this);
	return mWriteOffset;
}

void StripedFile::seekRead(int64_t position)
{
	Synchronize(this);
	seekRead(position / mStripeSize, position % mStripeSize);
}

void StripedFile::seekRead(unsigned block, size_t offset)
{
	Synchronize(this);
  
	if(mReadBlock <= block)
	{
		mFile->seekg(File::streamoff(offset)-mReadOffset, File::cur);
		mReadOffset = offset;
	}
	else {
		mFile->seekg(mStripeOffset + offset, File::beg);
		mReadBlock = 0;
		mReadOffset = offset;
	}

	while(mReadBlock < block)
	{
		mFile->seekg(mBlockSize, File::cur);
		++mReadBlock;
	}
}

void StripedFile::seekWrite(int64_t position)
{
	Synchronize(this);
	seekWrite(position / mStripeSize, position % mStripeSize);
}

void StripedFile::seekWrite(unsigned block, size_t offset)
{
	Synchronize(this);
  
	// Flush buffers first
	flush();
	
	// WARNING: seekp() MUST allow seeking past the end of the file.
	// If it is not the case, this WILL NOT work correctly.

	if(mWriteBlock <= block)
	{
		mFile->seekp(File::streamoff(offset)-mWriteOffset, File::cur);
		mWriteOffset = offset;
	}
	else {
		mFile->seekp(mStripeOffset + offset, File::beg);
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

	Synchronize(this);
	
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
	
	Synchronize(this);
	
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

void StripedFile::flush(void)
{
  	Synchronize(this);
	mFile->flush(); 
}

}
