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

#ifndef TPN_PIPE_H
#define TPN_PIPE_H

#include "tpn/stream.h"
#include "tpn/stream.h"
#include "tpn/mutex.h"
#include "tpn/signal.h"

namespace tpn
{

class Pipe : public Stream
{
public:
	Pipe(void);
	Pipe(Stream *buffer, bool readOnly = false);	// buffer destroyed at deletion
	virtual ~Pipe(void);

	void close(void);		// closes the write end
	bool is_open(void) const;	// true if the write end is open
	
	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	bool waitData(double &timeout);
	
protected:
	void open(Stream *buffer, bool readOnly = false);

private:
	Stream *mReadBuffer;
	Stream *mWriteBuffer;
	Mutex mMutex;
	Signal mSignal;
};

class ReadOnlyPipe : public Pipe
{
 	ReadOnlyPipe(Stream *buffer);	// buffer destroyed at deletion
	virtual ~ReadOnlyPipe(void);
};

}

#endif
