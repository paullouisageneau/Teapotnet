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

#include "stream.h"
#include "serializable.h"
#include "exception.h"

namespace arc
{

// TODO: It depends on the OS (This is OK for Unix and Windows)
const String Stream::OutputLineDelimiter = "\r\n";
const String Stream::LineDelimiters = "\n";
const String Stream::IgnoredCharacters = "\r\0";
const String Stream::FieldDelimiters = " ,;\t\n";	// MUST contain ' ' and ',' and NOT '.' and ':'
const char Stream::Space = ' ';

Stream::Stream(void)
{

}

Stream::~Stream(void)
{

}

bool Stream::get(char &chr)
{
	return readData(&chr,1);
}

void Stream::put(char chr)
{
	writeData(&chr,1);
}

bool Stream::ignore(int n)
{
	char chr;
	if(!get(chr)) return false;
	while(--n) if(!get(chr)) return false;
	return true;
}

bool Stream::ignoreUntil(char delimiter)
{
	char chr;
	if(!get(chr)) return false;
	while(chr != delimiter)
		if(!get(chr)) return false;
	return true;
}

bool Stream::ignoreUntil(const String &delimiters, char *found)
{
	char chr;
	if(found) *found = '\0';
	if(!get(chr)) return false;
	while(!delimiters.contains(chr))
		if(!get(chr)) return false;
	if(found) *found = chr;
	return true;
}

bool Stream::ignoreWhile(const String &chars, char *found)
{
	char chr;
	if(found) *found = '\0';
	if(!get(chr)) return false;
	while(chars.contains(chr))
		if(!get(chr)) return false;
	if(found) *found = chr;
	return true;
}

Stream &Stream::operator<<(Stream &s)
{
	write(s);
	return (*this);
}

void Stream::read(Stream &s)
{
	char buffer[BufferSize];
	int size = readData(buffer,BufferSize);
	if(size <= 0) return;
	while(size > 0)
	{
		s.writeData(buffer,size);
		size = readData(buffer,BufferSize);
	}
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
	return readString(s,FieldDelimiters,IgnoredCharacters,true);
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
	char buffer[BufferSize];
	int size;
	while((size = s.readData(buffer,BufferSize)) > 0)
	{
		writeData(buffer,size);
	}
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

bool Stream::readLine(Stream &output)
{
	return readString(output,LineDelimiters,IgnoredCharacters,false);
}

bool Stream::writeLine(Stream &input)
{
	write(input);
	write(OutputLineDelimiter);
}

bool Stream::readUntil(Stream &output, char delimiter)
{
	char chr;
	if(!get(chr)) return false;
	while(chr != delimiter)
	{
		output.write(chr);
		if(!get(chr)) break;
	}
	return true;
}

bool Stream::readUntil(Stream &output, const String &delimiters, char *found)
{
	char chr;
	if(found) *found = '\0';
	if(!get(chr)) return false;
	while(!delimiters.contains(chr))
	{
		output.write(chr);
		if(!get(chr)) break;
	}
	if(found) *found = chr;
	return true;
}

bool Stream::readString(Stream &output, const String &delimiters, const String &ignored, bool skipBefore)
{
	char chr;
	if(!get(chr)) return false;

	if(skipBefore)
	{
		while(delimiters.contains(chr))
			if(!get(chr)) return false;
	}

	while(!delimiters.contains(chr))
	{
		if(!ignored.contains(chr)) output.write(chr);
		if(!get(chr)) break;
	}
	return true;
}

bool Stream::readField(Stream &output)
{
	return readString(output,FieldDelimiters,IgnoredCharacters,true);
}

bool Stream::parseField(Stream &output, const String &delimiters, char *found)
{
	String str;
	char chr;
	bool before = true;

	if(found) *found = '\0';
	if(!get(chr)) return false;
	while(!delimiters.contains(chr))
	{
		if(before)
		{
			if(!FieldDelimiters.contains(chr))
			{
				before = false;
				if(!IgnoredCharacters.contains(chr)) str+= chr;
			}
		}
		else {
			if(LineDelimiters.contains(chr)) throw InvalidData("Unable to parse field in stream");
			if(!IgnoredCharacters.contains(chr)) str+= chr;
		}

		if(!get(chr)) return true;
	}

	if(found) *found = chr;

	while(!str.empty())
	{
		chr = str[str.size()-1];
		if(FieldDelimiters.contains(chr)) str.erase(str.end()-1);
		else break;
	}

	output.write(str);
	return true;
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
