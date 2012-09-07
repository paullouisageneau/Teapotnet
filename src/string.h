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

#ifndef ARC_STRING_H
#define ARC_STRING_H

#include "include.h"
#include "stream.h"
#include "serializable.h"

namespace arc
{

class String : public Stream, public Serializable, public std::string
{
public:
	static const String Empty;

	String(void);
	String(const char chr);
	String(const char *str);
	String(const std::string &str),
	String(const String &str, int begin = 0);
	String(const String &str, int begin, int end);
	virtual ~String(void);

	static String number(int n);
	static String number(unsigned int n);
	static String number(double d, int significatif = 4);
	static String hexa(unsigned int n);
	
	void explode(std::list<String> &strings, char separator) const;
	void implode(const std::list<String> &strings, char separator);
	String cut(char separator);
	void trim(void);

	bool contains(char chr) const;
	void remove(int pos, int nb = String::npos);
	bool isEmpty() const;

	int indexOf(char c, int from = 0) const;
	int indexOf(const char* c, int from = 0) const;
	int lastIndexOf(char c) const;
	int lastIndexOf(const char* c) const;

	bool remove(char chr);
	bool replace(char a, char b);

	String mid(int pos, int n = String::npos) const;
	String left(int pos) const;
	String right(int pos) const;

	String toLower(void) const;
	String toUpper(void) const;
	String toTrimmed(void) const;
	String urlEncode(void) const;
	String urlDecode(void) const;
	String base64Encode(void) const;
	String base64Decode(void) const;
	
	double toDouble() const;
	float toFloat() const;
	int toInt() const;
	bool toBool() const;

	operator const char*(void);

	// Serializable
	virtual void serialize(Stream &s) const;
	virtual void deserialize(Stream &s);
	virtual void serializeBinary(ByteStream &s) const;
	virtual void deserializeBinary(ByteStream &s);

	static const size_type NotFound = npos;

protected:
	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
};


// Templates functions from Stream using String are defined here

template<typename T> bool Stream::readLine(T &output)
{
	String line;
	if(!readLine(line)) return false;
	return line.read(output);
}

template<typename T> bool Stream::writeLine(const T &input)
{
	write(input);
	write(NewLine);
}

}

#endif
