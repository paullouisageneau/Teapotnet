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

#ifndef TPOT_BYTESTREAM_H
#define TPOT_BYTESTREAM_H

#include "include.h"

namespace tpot
{

class Serializable;
class ByteString;
class Pipe;

class ByteStream
{
public:
	ByteStream(void);
	virtual ~ByteStream(void);

	virtual size_t readData(char *buffer, size_t size) = 0;
	virtual void writeData(const char *data, size_t size) = 0;

	size_t	readBinary(ByteStream &s);
	size_t	readBinary(ByteStream &s, size_t max);
	bool	readBinary(Serializable &s);
	bool	readBinary(ByteString &s);
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
	void	writeBinary(const Serializable &s);
	void	writeBinary(const ByteString &s);
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

	virtual void clear(void);
	virtual void flush(void);
	virtual void ignore(int n = 1);
	virtual void discard(void);

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
