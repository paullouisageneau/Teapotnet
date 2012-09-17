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

#ifndef TPOT_PIPE_H
#define TPOT_PIPE_H

#include "stream.h"
#include "bytestream.h"
#include "mutex.h"
#include "signal.h"

namespace tpot
{

class Pipe : public Stream, public ByteStream
{
public:
	using ByteStream::ignore;

	Pipe(void);
	Pipe(ByteStream *buffer);	// buffer destroyed at deletion
	virtual ~Pipe(void);

	void close(void);		// closes then write end
	bool is_open(void) const;	// true is write end is open
	
	// Stream, ByteStream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

private:
	void open(ByteStream *buffer);

	ByteStream *mReadBuffer;
	ByteStream *mWriteBuffer;
	Mutex mMutex;
	Signal mSignal;
};

}

#endif
