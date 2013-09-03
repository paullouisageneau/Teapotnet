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

#ifndef TPN_PIPE_H
#define TPN_PIPE_H

#include "tpn/stream.h"
#include "tpn/bytestream.h"
#include "tpn/mutex.h"
#include "tpn/signal.h"

namespace tpn
{

class Pipe : public Stream, public ByteStream
{
public:
	using ByteStream::ignore;

	Pipe(void);
	Pipe(ByteStream *buffer, bool readOnly = false);	// buffer destroyed at deletion
	virtual ~Pipe(void);

	void close(void);		// closes the write end
	bool is_open(void) const;	// true if the write end is open
	
	// Stream, ByteStream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

protected:
	void open(ByteStream *buffer, bool readOnly = false);

private:
	ByteStream *mReadBuffer;
	ByteStream *mWriteBuffer;
	Mutex mMutex;
	Signal mSignal;
};

class ReadOnlyPipe : public Pipe
{
 	ReadOnlyPipe(ByteStream *buffer);	// buffer destroyed at deletion
	virtual ~ReadOnlyPipe(void);
};

}

#endif
