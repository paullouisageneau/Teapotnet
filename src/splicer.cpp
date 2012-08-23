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

#include "splicer.h"

namespace arc
{

Splicer::Splicer(const String &filename, size_t blockSize, int nbStripes) :
	mBlockSize(blockSize)
{
	mBlock = 0;

	for(int i=0; i<nbStripes; ++i)
	{
		File *file = new File(filename, File::Write);
		StripedFile *striped = new StripedFile(file, blockSize, nbStripes, i);
		mStripes.push_back(striped);

		// TODO: request object !
	}
}

Splicer::~Splicer(void)
{
	for(int i=0; i<mStripes.size(); ++i)
		delete mStripes[i];
}

}
