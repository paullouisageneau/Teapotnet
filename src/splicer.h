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

#ifndef TPOT_SPLICER_H
#define TPOT_SPLICER_H

#include "include.h"
#include "stripedfile.h"
#include "identifier.h"
#include "request.h"
#include "array.h"

namespace tpot
{

// "I'll wrap you in a sheet !"

class Splicer : public Synchronizable
{
public:
	Splicer(const ByteString &target, const String &filename, size_t blockSize, size_t firstBlock = 0);
	~Splicer(void);

	const String &name(void) const;
	uint64_t size(void) const;
	
	void process(void);
	bool finished(void) const;
	size_t finishedBlocks(void) const;
	void close(void);
	
private:
  	void search(Set<Identifier> &sources);
  	void query(int i, const Identifier &source);
  
	ByteString mTarget;
	String mFileName;
	size_t mBlockSize;
	size_t mFirstBlock;
	String mName;
	uint64_t mSize;
	int mNbStripes;
	Array<Request*> mRequests;
	Array<StripedFile*> mStripes;
};

}

#endif
