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

#include "pipe.h"
#include "bytestring.h"

namespace arc
{

Pipe::Pipe(void) :
	mBuffer(new ByteString),
	mIsClosed(false)
{

}

Pipe::Pipe(ByteStream *buffer) :
		mBuffer(buffer),
		mIsClosed(false)
{

}

Pipe::~Pipe(void)
{
	close();
	delete mBuffer;
}

void Pipe::close(void)
{
	mMutex.lock();
	mIsClosed = true;
	mSignal.launchAll();
	mMutex.unlock();
}

size_t Pipe::readData(char *buffer, size_t size)
{
	mMutex.lock();
	size_t len;
	while((len = mBuffer->read(buffer,size)) == 0)
	{
		if(mIsClosed)
		{
			mMutex.unlock();
			return 0;
		}

		mSignal.wait(mMutex);
	}

	mMutex.unlock();
	return len;
}

void Pipe::writeData(const char *data, size_t size)
{
	if(size == 0) return;
	if(mIsClosed) throw IOException("Pipe is closed, cannot write");

	mMutex.lock();
	mBuffer->write(data,size);
	mMutex.unlock();
	mSignal.launchAll();
}

}
