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

#include "bytestream.h"
#include "serializable.h"

namespace arc
{

ByteStream::ByteStream(void)
{

}

ByteStream::~ByteStream(void)
{

}

size_t ByteStream::readBinary(ByteStream &s)
{
	char buffer[BufferSize];
	size_t total = 0;
	size_t size;
	while((size = readData(buffer,BufferSize)))
	{
		total+= size;
		s.writeData(buffer,size);
	}
	return total;
}

size_t ByteStream::readBinary(ByteStream &s, size_t max)
{
	char buffer[BufferSize];
	size_t left = max;
	size_t size;
	while(left && (size = readData(buffer,std::min(BufferSize,left))))
	{
		left-= size;
		s.writeData(buffer,size);
	}
	return max-left;
}

bool ByteStream::readBinary(Serializable &s)
{
	s.deserializeBinary(*this);
	return true;
}

bool ByteStream::readBinary(ByteString &s)
{
	s.clear();

	uint32_t count;
	if(!readBinary(count)) return false;

	char b;
	for(uint32_t i=0; i<count; ++i)
	{
		if(!readData(&b,1)) return false;
		s.push_back(b);
	}

	return true;
}

bool ByteStream::readBinary(sint8_t &i)
{
	uint8_t u;
	if(!readBinary(u)) return false;
	i = sint8_t(u);
	return true;
}

bool ByteStream::readBinary(sint16_t &i)
{
	uint16_t u;
	if(!readBinary(u)) return false;
	i = sint16_t(u);
	return true;
}

bool ByteStream::readBinary(sint32_t &i)
{
	uint32_t u;
	if(!readBinary(u)) return false;
	i = sint32_t(u);
	return true;
}

bool ByteStream::readBinary(sint64_t &i)
{
	uint64_t u;
	if(!readBinary(u)) return false;
	i = sint64_t(u);
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

bool ByteStream::readInt8(signed int &i)
{
	uint8_t val;
	if(!readBinary(val)) return false;
	i = (signed int)(val);
	return true;
}

bool ByteStream::readInt16(signed int &i)
{
	uint16_t val;
	if(!readBinary(val)) return false;
	i = (signed int)(val);
	return true;
}

bool ByteStream::readInt32(signed int &i)
{
	uint32_t val;
	if(!readBinary(val)) return false;
	i = (signed int)(val);
	return true;
}

bool ByteStream::readInt8(unsigned int &i)
{
	uint8_t val;
	if(!readBinary(val)) return false;
	i = (unsigned int)(val);
	return true;
}

bool ByteStream::readInt16(unsigned int &i)
{
	uint16_t val;
	if(!readBinary(val)) return false;
	i = (unsigned int)(val);
	return true;
}

bool ByteStream::readInt32(unsigned &i)
{
	uint32_t val;
	if(!readBinary(val)) return false;
	i = (unsigned int)(val);
	return true;
}

bool ByteStream::readFloat(float &f)
{
	return readBinary(f);
}

void ByteStream::writeBinary(ByteStream &s)
{
	s.readBinary(*this);
}

void ByteStream::writeBinary(const Serializable &s)
{
	s.serializeBinary(*this);
}

void ByteStream::writeBinary(const ByteString &s)
{
	uint32_t count(s.size());
	writeBinary(count);

	for(uint32_t i=0; i<count; ++i)
		writeData(&s.at(i),1);
}

void ByteStream::writeBinary(sint8_t i)
{
	writeBinary(uint8_t(i));
}

void ByteStream::writeBinary(sint16_t i)
{
	writeBinary(uint16_t(i));
}

void ByteStream::writeBinary(sint32_t i)
{
	writeBinary(uint32_t(i));
}

void ByteStream::writeBinary(sint64_t i)
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

void ByteStream::writeInt8(signed int i)
{
	writeBinary(sint8_t(i));
}

void ByteStream::writeInt16(signed int i)
{
	writeBinary(sint16_t(i));
}

void ByteStream::writeInt32(signed int i)
{
	writeBinary(sint32_t(i));
}

void ByteStream::writeInt8(unsigned int i)
{
	writeBinary(uint8_t(i));
}

void ByteStream::writeInt16(unsigned int i)
{
	writeBinary(uint16_t(i));
}

void ByteStream::writeInt32(unsigned int i)
{
	writeBinary(uint32_t(i));
}

void ByteStream::writeFloat(float f)
{
	writeBinary(f);
}

void ByteStream::clear(void)
{
	char dummy;
	while(readData(&dummy,1));
}

void ByteStream::ignore(int n)
{
	char dummy;
	for(int i=0; i<n; ++i)
		if(!readData(&dummy,1)) break;
}

void ByteStream::discard(void)
{
	char buffer[BufferSize];
	while(readData(buffer,BufferSize))
	{
		// do nothing
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

