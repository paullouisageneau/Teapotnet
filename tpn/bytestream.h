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

#ifndef TPN_BYTESTREAM_H
#define TPN_BYTESTREAM_H

#include "tpn/include.h"
#include "tpn/task.h"

namespace tpn
{

class Serializable;
class ByteString;
class String;
class Pipe;

class ByteStream
{
public:
	static void Transfer(ByteStream *bs1, ByteStream *bs2); // Warning: performance issue: uses 2 threads

	ByteStream(void);
	virtual ~ByteStream(void);

	virtual size_t readData(char *buffer, size_t size) = 0;
	virtual void writeData(const char *data, size_t size) = 0;
	virtual void seekRead(int64_t position);
	virtual void seekWrite(int64_t position);
	virtual void clear(void);
	virtual void flush(void);
	virtual void discard(void);
	virtual bool ignore(size_t size = 1);
	
	int64_t	readBinary(ByteStream &s);
	int64_t	readBinary(ByteStream &s, int64_t max);
	int64_t	readBinary(ByteString &s);
	bool	readBinary(char *data, size_t size);	// blocks until size bytes are read
	bool    readBinary(String &str);
	bool	readBinary(Serializable &s);
	bool	readBinary(int8_t &i);
	bool	readBinary(int16_t &i);
	bool	readBinary(int32_t &i);
	bool	readBinary(int64_t &i);
	bool	readBinary(uint8_t &i);
	bool	readBinary(uint16_t &i);
	bool	readBinary(uint32_t &i);
	bool	readBinary(uint64_t &i);
	bool	readBinary(float32_t &f);
	bool	readBinary(float64_t &f);

	template<class T> bool readBinary(T *ptr);

	void	writeBinary(ByteStream &s);
	void	writeBinary(const ByteString &s);
	void    writeBinary(char *data, size_t size);	// alias for writeData
	void    writeBinary(const String &str);
	void	writeBinary(const Serializable &s);
	void	writeBinary(int8_t i);
	void	writeBinary(int16_t i);
	void	writeBinary(int32_t i);
	void	writeBinary(int64_t i);
	void	writeBinary(uint8_t i);
	void	writeBinary(uint16_t i);
	void	writeBinary(uint32_t i);
	void	writeBinary(uint64_t i);
	void	writeBinary(float32_t f);
	void	writeBinary(float64_t f);

	template<class T> void writeBinary(const T *ptr);

	void writeZero(size_t size = 1);	
	void writeRandom(size_t size = 1);

	// Handy task to schedule flushing
	class FlushTask : public Task
        {
        public:
                FlushTask(ByteStream *bs) { this->bs = bs; }
                void run(void) { bs->flush(); }
        private:
                ByteStream *bs;
        };

private:
	virtual ByteStream *pipeIn(void);	// return the write end for a pipe

	uint16_t fixEndianess(uint16_t n);
	uint32_t fixEndianess(uint32_t n);
	uint64_t fixEndianess(uint64_t n);

	friend class Pipe;
};

template<class T>
bool ByteStream::readBinary(T *ptr)
{
	return readBinary(*ptr);
}

template<class T>
void ByteStream::writeBinary(const T *ptr)
{
	writeBinary(*ptr);
}

}

#endif
