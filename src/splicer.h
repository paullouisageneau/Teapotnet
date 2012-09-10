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

#ifndef ARC_SPLICER_H
#define ARC_SPLICER_H

#include "include.h"
#include "stripedfile.h"
#include "identifier.h"
#include "request.h"
#include "array.h"

namespace arc
{

// "I'll wrap you in a sheet !"

class Splicer : public Synchronizable
{
public:
	Splicer(const Identifier &target, const String &filename, size_t blockSize);
	~Splicer(void);

	const String &name(void);
	
	bool finished(void) const;
	size_t finishedBlocks(void) const;
	
private:
	Identifier mTarget;
	String mFileName;
	String mName;
	size_t mBlockSize;
	Array<Request*> mRequests;
	Array<StripedFile*> mStripes;
};

}

#endif
