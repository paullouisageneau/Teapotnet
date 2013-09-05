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

#include "tpn/bytestream.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"
#include "tpn/byteserializer.h"
#include "tpn/bytestring.h"

namespace tpn
{

ByteStream::ByteStream(void)
{

}

ByteStream::~ByteStream(void)
{

}

void ByteStream::seekRead(int64_t position)
{
	throw Unsupported("seekRead");
}

void ByteStream::seekWrite(int64_t position)
{
	throw Unsupported("seekWrite");
}

void ByteStream::clear(void)
{
	char dummy;
	while(readData(&dummy,1));
}

void ByteStream::flush(void)
{

}

bool ByteStream::ignore(size_t size)
{
	char buffer[BufferSize];
	size_t len;
	while(size && (len = readData(buffer, std::min(size, BufferSize))))
	{
		if(!len) return false;
		size-= len;
	}

	return true;
}

void ByteStream::discard(void)
{
	char buffer[BufferSize];
	while(readData(buffer,BufferSize))
	{
		// do nothing
	}
}

int64_t ByteStream::readBinary(ByteStream &s)
{
	char buffer[BufferSize];
	int64_t total = 0;
	size_t size;
	while((size = readData(buffer,BufferSize)))
	{
		total+= size;
		s.writeData(buffer,size);
	}
	return total;
}

int64_t ByteStream::readBinary(ByteStream &s, int64_t max)
{
	char buffer[BufferSize];
	int64_t left = max;
	size_t size;
	while(left && (size = readData(buffer,size_t(std::min(int64_t(BufferSize),left)))))
	{
		left-= size;
		s.writeData(buffer,size);
	}
	return max-left;
}

int64_t ByteStream::readBinary(ByteString &s)
{
	ByteStream &stream = s;
	return readBinary(stream);
}

bool ByteStream::readBinary(Serializable &s)
{
	ByteSerializer serializer(this);
	return s.deserialize(serializer);
}

bool ByteStream::readBinary(int8_t &i)
{
	uint8_t u;
	if(!readBinary(u)) return false;
	i = int8_t(u);
	return true;
}

bool ByteStream::readBinary(int16_t &i)
{
	uint16_t u;
	if(!readBinary(u)) return false;
	i = int16_t(u);
	return true;
}

bool ByteStream::readBinary(int32_t &i)
{
	uint32_t u;
	if(!readBinary(u)) return false;
	i = int32_t(u);
	return true;
}

bool ByteStream::readBinary(int64_t &i)
{
	uint64_t u;
	if(!readBinary(u)) return false;
	i = int64_t(u);
	return true;
}

bool ByteStream::readBinary(uint8_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),1) != 1) return false;
	return true;
}

bool ByteStream::readBinary(uint16_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),2) != 2) return false;
	i = fixEndianess(i);
	return true;
}

bool ByteStream::readBinary(uint32_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),4) != 4) return false;
	i = fixEndianess(i);
	return true;
}

bool ByteStream::readBinary(uint64_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),8) != 8) return false;
	i = fixEndianess(i);
	return true;
}

bool ByteStream::readBinary(float32_t &f)
{
	if(readData(reinterpret_cast<char*>(&f),4) != 4) return false;
	return true;
}

bool ByteStream::readBinary(float64_t &f)
{
	if(readData(reinterpret_cast<char*>(&f),8) != 8) return false;
	return true;
}

void ByteStream::writeBinary(ByteStream &s)
{
	s.readBinary(*this);
}

void ByteStream::writeBinary(const ByteString &s)
{
	ByteString tmp(s);
	ByteStream &stream = tmp;
	writeBinary(stream);
}

void ByteStream::writeBinary(const Serializable &s)
{
	ByteSerializer serializer(this);
	s.serialize(serializer);
}

void ByteStream::writeBinary(int8_t i)
{
	writeBinary(uint8_t(i));
}

void ByteStream::writeBinary(int16_t i)
{
	writeBinary(uint16_t(i));
}

void ByteStream::writeBinary(int32_t i)
{
	writeBinary(uint32_t(i));
}

void ByteStream::writeBinary(int64_t i)
{
	writeBinary(uint64_t(i));
}

void ByteStream::writeBinary(uint8_t i)
{
	writeData(reinterpret_cast<char*>(&i),1);
}

void ByteStream::writeBinary(uint16_t i)
{
	i = fixEndianess(i);
	writeData(reinterpret_cast<char*>(&i),2);
}

void ByteStream::writeBinary(uint32_t i)
{
	i = fixEndianess(i);
	writeData(reinterpret_cast<char*>(&i),4);
}

void ByteStream::writeBinary(uint64_t i)
{
	i = fixEndianess(i);
	writeData(reinterpret_cast<char*>(&i),8);
}

void ByteStream::writeBinary(float32_t f)
{
	writeData(reinterpret_cast<char*>(&f),4);
}

void ByteStream::writeBinary(float64_t f)
{
	writeData(reinterpret_cast<char*>(&f),8);
}

void ByteStream::writeZero(size_t size)
{
	char buffer[BufferSize];
	std::memset(buffer, 0, std::min(size, BufferSize));
	while(size)
	{
		size_t len = std::min(size, BufferSize);
		writeData(buffer,len);
		size-= len;
	}
}

void ByteStream::writeRandom(size_t size)
{
	char buffer[BufferSize];
	while(size)
	{
		size_t len = std::min(size, BufferSize);
		cryptrand(buffer, len);
		writeData(buffer,len);
		size-= len;
	}
}

ByteStream *ByteStream::pipeIn(void)
{
	return this;
}

uint16_t ByteStream::fixEndianess(uint16_t n)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(&n);
	return	(uint16_t(p[0]) << 8) |
		(uint16_t(p[1]));
}

uint32_t ByteStream::fixEndianess(uint32_t n)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(&n);
	return	(uint32_t(p[0]) << 24) |
		(uint32_t(p[1]) << 16) |
		(uint32_t(p[2]) <<  8) |
		(uint32_t(p[3]));
}

uint64_t ByteStream::fixEndianess(uint64_t n)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(&n);
	return	(uint64_t(p[0]) << 56) |
		(uint64_t(p[1]) << 48) |
		(uint64_t(p[2]) << 40) |
		(uint64_t(p[3]) << 32) |
		(uint64_t(p[4]) << 24) |
		(uint64_t(p[5]) << 16) |
		(uint64_t(p[6]) <<  8) |
		(uint64_t(p[7]));
}

}

