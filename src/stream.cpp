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

#include "stream.h"
#include "serializable.h"
#include "exception.h"
#include "bytestream.h"

namespace arc
{

const String Stream::IgnoredCharacters = "\r\0";
const String Stream::BlankCharacters = " \t";
const String Stream::FieldDelimiters = ",;\n";	// Must NOT contain '.', '=', and ':'
const char Stream::NewLine = '\n';
const char Stream::Space = ' ';

Stream::Stream(void) :
	mLast(0),
	mHexa(false)
{

}

Stream::~Stream(void)
{

}

size_t Stream::readData(ByteStream &s, size_t max)
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

size_t Stream::writeData(ByteStream &s, size_t max)
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
	if(readData(&mLast,1))
	{
		chr = mLast;
		return true;
	}
	else return false;
}

void Stream::put(char chr)
{
	writeData(&chr,1);
}

char Stream::last(void) const
{
	return mLast;
}

bool Stream::ignore(int n)
{
	if(!get(mLast)) return false;
	while(--n) if(!get(mLast)) return false;
	return true;
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

Stream &Stream::operator<<(Stream &s)
{
	write(s);
	return (*this);
}

size_t Stream::read(Stream &s)
{
	char buffer[BufferSize];
	size_t total = 0;
	size_t size;
	while((size = readData(buffer,BufferSize)))
	{
		total+= size;
		mLast = buffer[size-1];
		s.writeData(buffer,size);
	}
	return total;
}

size_t Stream::read(Stream &s, size_t max)
{
	char buffer[BufferSize];
	size_t left = max;
	size_t size;
	while(left && (size = readData(buffer,std::min(BufferSize,left))))
	{
		left-=size;
		mLast = buffer[size-1];
		s.writeData(buffer,size);
	}
	return max-left;
}

bool Stream::read(Serializable &s)
{
	// TODO: end of stream
	s.deserialize(*this);
	return true;
}

bool Stream::read(String &s)
{
	s.clear();
	if(!ignoreWhile(BlankCharacters)) return false;
	s+= last();
	readString(s,FieldDelimiters+BlankCharacters,false);
	return true;
}

bool Stream::read(bool &b)
{
	String s;
	if(!read(s)) return false;
	s.trim();
	String str = s.toLower();
	if(str == "true" || str == "yes" || str == "1") b = true;
	else if(str == "false"|| str == "no"  || str == "0" || str == "") b = false;
	else throw Exception("Invalid boolean value: \""+s+"\"");
	return true;
}

double Stream::readDouble(void)
{
	double value;
	if(!read(value)) throw Exception("No value to read");
	return value;
}

float Stream::readFloat(void)
{
	float value;
	if(!read(value)) throw Exception("No value to read");
	return value;
}

int Stream::readInt(void)
{
	int value;
	if(!read(value)) throw Exception("No value to read");
	return value;
}

bool Stream::readBool(void)
{
	bool value;
	if(!read(value)) throw Exception("No value to read");
	return value;
}

void Stream::write(Stream &s)
{
	s.read(*this);
}

void Stream::write(const Serializable &s)
{
	s.serialize(*this);
}

void Stream::write(const String &s)
{
	writeData(s.data(), s.size());
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

bool Stream::readUntil(Stream &output, char delimiter)
{
	if(!get(mLast)) return false;
	while(mLast != delimiter)
	{
		if(!IgnoredCharacters.contains(mLast)) output.write(mLast);
		if(!get(mLast)) break;
	}
	return true;
}

bool Stream::readUntil(Stream &output, const String &delimiters)
{
	if(!get(mLast)) return false;
	while(!delimiters.contains(mLast))
	{
		if(!IgnoredCharacters.contains(mLast)) output.write(mLast);
		if(!get(mLast)) break;
	}
	return true;
}

bool Stream::readString(Stream &output, const String &delimiters, bool skipBefore)
{
	if(!get(mLast)) return false;

	if(skipBefore)
	{
		while(delimiters.contains(mLast))
			if(!get(mLast)) return false;
	}

	while(!delimiters.contains(mLast))
	{
		if(!IgnoredCharacters.contains(mLast)) output.write(mLast);
		if(!get(mLast)) break;
	}
	return true;
}

bool Stream::readString(Stream &output)
{
	return readString(output, BlankCharacters, true);
}

bool Stream::readField(Stream &output)
{
	return readString(output,FieldDelimiters,false);
}

bool Stream::readLine(Stream &output)
{
	return readString(output,String(NewLine),false);
}

bool Stream::readLine(String &output)
{
	return readString(output,String(NewLine),false);
}

bool Stream::readStdString(std::string &output)
{
	String str;
	if(!read(str)) return false;
	output+= str;
	return true;
}

void Stream::error(void)
{
	throw IOException();
}

}
