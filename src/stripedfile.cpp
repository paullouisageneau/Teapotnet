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
	mBlock = 0;
	mOffset = 0;
}

StripedFile::~StripedFile(void)
{
	mFile->close();
	delete mFile;
}

uint64_t StripedFile::tell(void) const
{
	return uint64_t(mBlock)*mStripeSize + uint64_t(mOffset);
}

size_t StripedFile::tellBlock(void) const
{
	return mBlock;
}

size_t StripedFile::tellOffset(void) const
{
	return mOffset;
}

void StripedFile::seek(uint64_t position)
{
	seek(position / mStripeSize, position % mStripeSize);
}

void StripedFile::seek(size_t block, size_t offset)
{
	if(mBlock <= block)
	{
		mFile->seekg(offset-mOffset, File::cur);
		mOffset = offset;
	}
	else {
		mFile->seekg(mStripe*mStripeSize + offset, File::beg);
		mBlock = 0;
		mOffset = offset;
	}

	while(mBlock < block)
	{
		mFile->seekg(mBlockSize, File::cur);
		++mBlock;
	}
}

size_t StripedFile::readData(char *buffer, size_t size)
{
	if(!size) return 0;

	const size_t left = std::min(mStripeSize - mOffset, size);
	const size_t len = mFile->readData(buffer, left);
	mOffset+= len;

	if(mOffset == mStripeSize)
	{
		// Move to the beginning of the stripe on the next block
		seek(mBlock+1, 0);
		return len + readData(buffer+len, size-len);
	}

	return len;
}

void StripedFile::writeData(const char *buffer, size_t size)
{
	if(!size) return;

	const size_t len = std::min(mStripeSize - mOffset, size);

	mFile->writeData(buffer, len);
	mOffset+= len;

	if(mOffset == mStripeSize)
	{
		// Move to the beginning of the stripe on the next block
		seek(mBlock+1, 0);
		writeData(buffer+len, size-len);
	}
}

}
