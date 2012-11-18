/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPOT_STRIPEDFILE_H
#define TPOT_STRIPEDFILE_H

#include "include.h"
#include "file.h"

namespace tpot
{

class StripedFile : public ByteStream
{
public:
	StripedFile(File *file, size_t blockSize, int nbStripes, int stripe);
	~StripedFile(void);

	uint64_t tellRead(void) const;
	size_t tellReadBlock(void) const;
	size_t tellReadOffset(void) const;
	
	uint64_t tellWrite(void) const;
	size_t tellWriteBlock(void) const;
	size_t tellWriteOffset(void) const;
	
	void seekRead(uint64_t position);
	void seekRead(size_t block, size_t offset);

	void seekWrite(uint64_t position);
	void seekWrite(size_t block, size_t offset);
	
	size_t readData(char *buffer, size_t size);
	void writeData(const char *buffer, size_t size);

	void flush(void);
	
private:
	File *mFile;
	size_t mBlockSize;
	size_t mStripeSize;
	size_t mStripeOffset;

	size_t mReadBlock,  mWriteBlock;	// Current block
	size_t mReadOffset, mWriteOffset;	// Current position inside the current stripe
};

}

#endif
