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

#include "tpn/pipe.h"
#include "tpn/binarystring.h"
#include "tpn/file.h"

namespace tpn
{

Pipe::Pipe(void)
{
	open(new BinaryString, false);
}

Pipe::Pipe(Stream *buffer, bool readOnly)
{
	open(buffer, readOnly);
}

Pipe::~Pipe(void)
{
	NOEXCEPTION(close());
	delete mReadBuffer;
}

void Pipe::open(Stream *buffer, bool readOnly)
{
	mReadBuffer = buffer;
	if(!readOnly) mWriteBuffer = buffer->pipeIn();
	else mWriteBuffer = NULL;
}

void Pipe::close(void)
{
	mMutex.lock();

	if(mWriteBuffer && mWriteBuffer != mReadBuffer)
		delete mWriteBuffer;

	mWriteBuffer = NULL;

	mSignal.launchAll();
	mMutex.unlock();
}

bool Pipe::is_open(void) const
{
	return (mWriteBuffer != NULL); 
}

size_t Pipe::readData(char *buffer, size_t size)
{
	mMutex.lock();
	
	size_t len;
	while((len = mReadBuffer->readData(buffer,size)) == 0)
	{
		if(!mWriteBuffer)	// if closed
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
	if(!mWriteBuffer) throw IOException("Pipe is closed, cannot write");
	
	mMutex.lock();
	mWriteBuffer->writeData(data, size);
	mWriteBuffer->flush();
	mSignal.launchAll();
	mMutex.unlock();
}


ReadOnlyPipe::ReadOnlyPipe(Stream *buffer)
{
	open(buffer, true);
}

ReadOnlyPipe::~ReadOnlyPipe(void)
{
  
}

}
