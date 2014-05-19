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

#include "tpn/filefountain.h"

namespace tpn
{

FileFountain::FileFountain(File *file, bool refresh)
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

FileFountain::~FileFountain(void)
{
	delete mFile;
	delete mMapFile;
}

size_t FileFountain::readBlock(int64_t offset, char *buffer, size_t size)
{
	Synchronize(this);
	if(!isWritten(offset)) return 0;
	mFile->seekRead(offset*BlockSize);
	return mFile->readData(buffer, size);
}

void FileFountain::writeBlock(int64_t offset, const char *data, size_t size)
{
	Synchronize(this);
	mFile->seekWrite(offset*BlockSize);
	mFile->writeData(data, size);
	markWritten(offset);
}

bool FileFountain::checkBlock(int64_t offset)
{
	Synchronize(this);
	return isWritten(offset);
}

bool FileFountain::isWritten(int64_t offset)
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

void FileFountain::markWritten(int64_t offset)
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

FileFountain::Reader::Reader(FileFountain *fountain) :
	mFileFountain(fountain)
{
	Assert(fountain);
}

FileFountain::Reader::~Reader(void)
{
	
}

size_t FileFountain::Reader::readData(char *buffer, size_t size)
{
	Synchronize(mFileFountain);
	
	int64_t offset = mReadPosition/BlockSize;
	size = std::min(size, size_t(mReadPosition%BlockSize));
	
	while(!mFileFountain->isWritten(offset))
		mFileFountain->wait();

	mFileFountain->mFile->seekRead(mReadPosition);
	size = mFileFountain->mFile->readData(buffer, size);
	mReadPosition+= size;
	return size;
}

void FileFountain::Reader::writeData(const char *buffer, size_t size)
{
	throw Unsupported("Writing to FileFountain::Reader");
}

void FileFountain::Reader::seekRead(int64_t position)
{
	mReadPosition = position;
}

void FileFountain::Reader::seekWrite(int64_t position)
{
	throw Unsupported("Writing to FileFountain::Reader");
}

}

