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

#ifndef ARC_STRIPEDFILE_H
#define ARC_STRIPEDFILE_H

#include "include.h"
#include "file.h"

namespace arc
{

class StripedFile : public ByteStream
{
public:
	StripedFile(File *file, size_t blockSize, int nbStripes, int stripe);
	~StripedFile(void);

	uint64_t tell(void) const;
	size_t tellBlock(void) const;
	size_t tellOffset(void) const;
	
	void seek(uint64_t position);
	void seek(size_t block, size_t offset);

	size_t readData(char *buffer, size_t size);
	void writeData(const char *buffer, size_t size);

private:
	File *mFile;
	size_t mBlockSize;
	size_t mStripeSize;
	int mStripe;

	size_t mBlock;		// Current block
	size_t mOffset;		// Current position inside the current stripe
};

}

#endif
