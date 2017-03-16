/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "pla/stream.hpp"
#include "pla/string.hpp"
#include "pla/serializable.hpp"
#include "pla/exception.hpp"

namespace pla
{

const String Stream::IgnoredCharacters = "\r\0";
const String Stream::BlankCharacters = " \t\r\n";
const String Stream::NewLine = "\r\n";
const char Stream::Space = ' ';

bool Stream::waitData(duration timeout)
{
	return true;
}

void Stream::seekRead(int64_t position)
{
	throw Unsupported("seekRead");
}

void Stream::seekWrite(int64_t position)
{
	throw Unsupported("seekWrite");
}

int64_t Stream::tellRead(void) const
{
	return 0;
}

int64_t Stream::tellWrite(void) const
{
	return 0;
}

bool Stream::nextRead(void)
{
	return false;	// no sub-streams
}

bool Stream::nextWrite(void)
{
	return false;	// no sub-streams
}

void Stream::clear(void)
{
	char buffer[BufferSize];
	size_t len;
	while((len = readData(buffer,BufferSize)) > 0)
		mLast = buffer[len-1];
}

void Stream::flush(void)
{
	// do nothing
}

void Stream::close(void)
{
	// do nothing
}

bool Stream::ignore(size_t size)
{
	char buffer[BufferSize];
	size_t len;
	while(size && (len = readData(buffer, std::min(size, BufferSize))))
	{
		if(!len) return false;
		size-= len;
		mLast = buffer[len-1];
	}

	return true;
}

bool Stream::skipMark(void)
{
	// do nothing
	return false;
}

bool Stream::isDatagram(void) const
{
	return false;
}

size_t Stream::readData(Stream &s, size_t max)
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

size_t Stream::writeData(Stream &s, size_t max)
{
	char buffer[BufferSize];
	size_t left = max;
	size_t size;
	while(left && (size = s.readData(buffer,std::min(BufferSize,left))))
	{
		left-= size;
		writeData(buffer,size);
	}
	return max-left;
}

bool Stream::hexaMode(void)
{
	return mHexa;
}

bool Stream::hexaMode(bool enabled)
{
	bool old = mHexa;
	mHexa = enabled;
	return old;
}

bool Stream::get(char &chr)
{
	skipMark();

	while(readData(&mLast,1))
	{
		mEnd = false;
		if(!IgnoredCharacters.contains(mLast))
		{
			chr = mLast;
			return true;
		}
	}

	mEnd = true;
	return false;
}

void Stream::put(char chr)
{
	writeData(&chr,1);
}

void Stream::space(void)
{
	put(Space);
}

void Stream::newline(void)
{
	write(NewLine);
}

char Stream::last(void) const
{
	return mLast;
}

bool Stream::atEnd(void) const
{
	return mEnd;
}

bool Stream::ignoreUntil(char delimiter)
{
	if(!get(mLast)) return false;
	while(mLast != delimiter)
		if(!get(mLast)) return false;
	return true;
}

bool Stream::ignoreUntil(const String &delimiters)
{
	if(!get(mLast)) return false;
	while(!delimiters.contains(mLast))
		if(!get(mLast)) return false;
	return true;
}

bool Stream::ignoreWhile(const String &chars)
{
	if(!get(mLast)) return false;
	while(chars.contains(mLast))
		if(!get(mLast)) return false;
	return true;
}

bool Stream::readUntil(Stream &output, char delimiter)
{
	const int maxCount = 10240;	// 10 Ko for security reasons

	int left = maxCount;
	char chr;
	if(!get(chr)) return false;
	while(chr != delimiter)
	{
		output.write(chr);
		--left;
		if(!left || !get(chr)) break;
	}
	return true;
}

bool Stream::readUntil(Stream &output, const String &delimiters)
{
	const int maxCount = 10240;	// 10 Ko for security reasons

	int left = maxCount;
	char chr;
	if(!get(chr)) return false;
	while(!delimiters.contains(chr))
	{
		output.write(chr);
		--left;
		if(!left || !get(chr)) break;
	}
	return true;
}

int64_t Stream::read(Stream &s)
{
	char buffer[BufferSize];
	int64_t total = 0;
	size_t size;
	while((size = readData(buffer,BufferSize)))
	{
		total+= size;
		mLast = buffer[size-1];
		s.writeData(buffer,size);
	}
	mEnd = true;
	return total;
}

int64_t Stream::read(Stream &s, int64_t max)
{
	char buffer[BufferSize];
	int64_t left = max;
	size_t size;
	while(left && (size = readData(buffer,size_t(std::min(int64_t(BufferSize),left)))))
	{
		left-=size;
		mLast = buffer[size-1];
		s.writeData(buffer,size);
	}
	mEnd = (left != 0);
	return max-left;
}

bool Stream::read(Serializable &s)
{
	return s.deserialize(*this);
}

bool Stream::read(std::string &str)
{
	String tmp;
	if(!read(tmp)) return false;
	str = tmp;
	return true;
}

bool Stream::read(bool &b)
{
	String s;
	if(!read(s)) return false;
	s.trim();
	String str = s.toLower();
	if(str == "true" || str == "yes" || str == "on" || str == "1") b = true;
	else if(str.empty() || str == "false" || str == "no" || str == "off"  || str == "0") b = false;
	else throw InvalidData("Invalid boolean value: \""+s+"\"");
	return true;
}

bool Stream::read(BinaryString &str)
{
	return read(*static_cast<Serializable*>(&str));
}

bool Stream::read(String &str)
{
	return read(*static_cast<Serializable*>(&str));
}

double Stream::readDouble(void)
{
	double value;
	if(read(value)) return value;
	else return 0.;
}

float Stream::readFloat(void)
{
	float value;
	if(read(value)) return value;
	else return 0.f;
}

int Stream::readInt(void)
{
	int value;
	if(read(value)) return value;
	else return 0;
}

bool Stream::readBool(void)
{
	bool value;
	if(read(value)) return value;
	else return false;
}

int64_t Stream::write(Stream &s)
{
	return s.read(*this);
}

int64_t Stream::write(Stream &s, int64_t max)
{
	return s.read(*this, max);
}

void Stream::write(const Serializable &s)
{
	s.serialize(*this);
}

void Stream::write(const char *s)
{
	if(s) write(String(s));
}

void Stream::write(const std::string &s)
{
	write(String(s));
}

void Stream::write(bool b)
{
	if(b) write("true");
	else write("false");
}

void Stream::write(const BinaryString &str)
{
	write(*static_cast<const Serializable*>(&str));
}
void Stream::write(const String &str)
{
	write(*static_cast<const Serializable*>(&str));
}

Stream &Stream::operator<<(Stream &s)
{
	write(s);
	return (*this);
}

bool Stream::assertChar(char chr)
{
	char tmp;
	if(!readChar(tmp)) return false;
	AssertIO(tmp == chr);
	return true;
}

bool Stream::readChar(char &chr)
{
	if(!ignoreWhile(BlankCharacters)) return false;
	chr = last();
	return true;
}

bool Stream::readLine(String &str)
{
	str.clear();
	return readUntil(str, '\n');
}

bool Stream::readString(String &str)
{
	return read(str);
}

bool Stream::readStdString(std::string &output)
{
	String str;
	if(!read(str)) return false;
	output+= str;
	return true;
}

int64_t Stream::readBinary(char *data, size_t size)
{
	size_t left = size;
	size_t r;
	while(left && (r = readData(data, left)))
	{
		data+= r;
		left-= r;
	}

	return size - left;
}

bool Stream::readBinary(BinaryString &str)
{
	return read(*static_cast<Stream*>(&str)) != 0;
}

bool Stream::readBinary(int8_t &i)
{
	uint8_t u;
	if(!readBinary(u)) return false;
	i = int8_t(u);
	return true;
}

bool Stream::readBinary(int16_t &i)
{
	uint16_t u;
	if(!readBinary(u)) return false;
	i = int16_t(u);
	return true;
}

bool Stream::readBinary(int32_t &i)
{
	uint32_t u;
	if(!readBinary(u)) return false;
	i = int32_t(u);
	return true;
}

bool Stream::readBinary(int64_t &i)
{
	uint64_t u;
	if(!readBinary(u)) return false;
	i = int64_t(u);
	return true;
}

bool Stream::readBinary(uint8_t &i)
{
	if(!readBinary(reinterpret_cast<char*>(&i),1)) return false;
	return true;
}

bool Stream::readBinary(uint16_t &i)
{
	if(!readBinary(reinterpret_cast<char*>(&i),2)) return false;
	i = fixEndianess(i);
	return true;
}

bool Stream::readBinary(uint32_t &i)
{
	if(!readBinary(reinterpret_cast<char*>(&i),4)) return false;
	i = fixEndianess(i);
	return true;
}

bool Stream::readBinary(uint64_t &i)
{
	if(!readBinary(reinterpret_cast<char*>(&i),8)) return false;
	i = fixEndianess(i);
	return true;
}

bool Stream::readBinary(float32_t &f)
{
	return readBinary(reinterpret_cast<char*>(&f),4);
}

bool Stream::readBinary(float64_t &f)
{
	return readBinary(reinterpret_cast<char*>(&f),8);
}

void Stream::writeBinary(const BinaryString &str)
{
	writeData(str.data(), str.size());
}

void Stream::writeBinary(int8_t i)
{
	writeBinary(uint8_t(i));
}

void Stream::writeBinary(int16_t i)
{
	writeBinary(uint16_t(i));
}

void Stream::writeBinary(int32_t i)
{
	writeBinary(uint32_t(i));
}

void Stream::writeBinary(int64_t i)
{
	writeBinary(uint64_t(i));
}

void Stream::writeBinary(uint8_t i)
{
	writeData(reinterpret_cast<char*>(&i),1);
}

void Stream::writeBinary(uint16_t i)
{
	i = fixEndianess(i);
	writeData(reinterpret_cast<char*>(&i),2);
}

void Stream::writeBinary(uint32_t i)
{
	i = fixEndianess(i);
	writeData(reinterpret_cast<char*>(&i),4);
}

void Stream::writeBinary(uint64_t i)
{
	i = fixEndianess(i);
	writeData(reinterpret_cast<char*>(&i),8);
}

void Stream::writeBinary(float32_t f)
{
	writeData(reinterpret_cast<char*>(&f),4);
}

void Stream::writeBinary(float64_t f)
{
	writeData(reinterpret_cast<char*>(&f),8);
}

void Stream::writeZero(size_t size)
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

Stream *Stream::pipeIn(void)
{
	return this;
}

uint16_t Stream::fixEndianess(uint16_t n)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(&n);
	return	(uint16_t(p[0]) << 8) |
		(uint16_t(p[1]));
}

uint32_t Stream::fixEndianess(uint32_t n)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(&n);
	return	(uint32_t(p[0]) << 24) |
		(uint32_t(p[1]) << 16) |
		(uint32_t(p[2]) <<  8) |
		(uint32_t(p[3]));
}

uint64_t Stream::fixEndianess(uint64_t n)
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
