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
		size = readData(buffer,BufferSize);
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
	if(readData(reinterpret_cast<char*>(&i),1) != 1) return false;
	return true;
}

bool ByteStream::readBinary(sint16_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),2) != 2) return false;
	fixEndianess16(reinterpret_cast<char*>(i));
	return true;
}

bool ByteStream::readBinary(sint32_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),4) != 4) return false;
	fixEndianess32(reinterpret_cast<char*>(i));
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
	fixEndianess16(reinterpret_cast<char*>(i));
	return true;
}

bool ByteStream::readBinary(uint32_t &i)
{
	if(readData(reinterpret_cast<char*>(&i),4) != 4) return false;
	fixEndianess32(reinterpret_cast<char*>(i));
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

bool ByteStream::readTime(double &t)
{
	uint32_t ms;
	if(!readBinary(ms)) return false;
	t = double(ms)*0.001;
	return true;
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
	writeData(reinterpret_cast<char*>(&i),1);
}

void ByteStream::writeBinary(sint16_t i)
{
	fixEndianess16(reinterpret_cast<char*>(i));
	writeData(reinterpret_cast<char*>(&i),2);
}

void ByteStream::writeBinary(sint32_t i)
{
	fixEndianess32(reinterpret_cast<char*>(i));
	writeData(reinterpret_cast<char*>(&i),4);
}


void ByteStream::writeBinary(uint8_t i)
{
	writeData(reinterpret_cast<char*>(&i),1);
}

void ByteStream::writeBinary(uint16_t i)
{
	fixEndianess16(reinterpret_cast<char*>(i));
	writeData(reinterpret_cast<char*>(&i),2);
}

void ByteStream::writeBinary(uint32_t i)
{
	fixEndianess32(reinterpret_cast<char*>(i));
	writeData(reinterpret_cast<char*>(&i),4);
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

void ByteStream::writeTime(double time)
{
	uint32_t ms = uint32_t(time*1000.);
	writeBinary(ms);
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

ByteStream *ByteStream::pipeIn(void)
{
	return this;
}

void ByteStream::fixEndianess16(char *data)
{
	// TODO
}

void ByteStream::fixEndianess32(char *data)
{
	// TODO
}

}

