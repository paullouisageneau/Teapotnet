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

#ifndef PLA_STRING_H
#define PLA_STRING_H

#include "pla/include.hpp"
#include "pla/binarystring.hpp"
#include "pla/random.hpp"

#include <cwchar>

namespace pla
{

template<typename T> class Array;
template<typename T> class Set;
template<typename K, typename V> class Map;

class String : public BinaryString
{
public:
	template<typename T> static String number(T n);
	static String number(double d, int digits = 4);
	static String number(int n, int minDigits = 1);
	static String number(unsigned int n, int minDigits = 1);
	static String number64(uint64_t n, int minDigits = 1);
	static String hexa(unsigned int n, int minDigits = 1);

	static String random(size_t nbr, Random::QualityLevel level = Random::Nonce);

	static String hrSize(uint64_t size);
	static String hrSize(const String &size);

	static const String Empty;
	static const size_type NotFound = npos;

	String(void);
	String(char chr);
	String(const char *str);
	String(const char *data, size_t size);
	String(const std::string &str);
	String(size_t n, char chr);
	String(const String &str, int begin = 0);
	String(const String &str, int begin, int end);
	String(const wchar_t *str);	// UTF-16
	template <class InputIterator> String(InputIterator first, InputIterator last) : BinaryString(first, last) {}
	virtual ~String(void);

	void explode(std::list<String> &strings, char separator) const;
	void implode(const std::list<String> &strings, char separator);
	String cut(char separator);
	String cutLast(char separator);
	void trim(void);
	void trimQuotes(void);

	bool contains(char chr) const;
	bool contains(const String &str) const;
	bool containsDigits(void) const;
	bool containsLetters(void) const;
	bool isAlphanumeric(void) const;
	void remove(int pos, int n = -1);

	int indexOf(char c, int from = 0) const;
	int indexOf(const char* c, int from = 0) const;
	int lastIndexOf(char c) const;
	int lastIndexOf(const char* c) const;

	bool remove(char chr);
	bool replace(char a, char b);
	bool replace(const String &a, const String &b);

	String mid(int pos, int n = -1) const;
	String left(int pos) const;
	String right(int pos) const;

	String after(char c) const;
	String afterLast(char c) const;
	String before(char c) const;
	String beforeLast(char c) const;

	String noAccents(void) const;
	String toLower(void) const;
	String toUpper(void) const;
	String capitalized(void) const;
	String capitalizedFirst(void) const;
	String trimmed(void) const;
	String urlEncode(void) const;
	String urlDecode(void) const;
	String windowsEncode(void) const;  // To Windows-1252
	String windowsDecode(void) const;  // From Windows-1252
	String pathEncode(void) const;
	String pathDecode(void) const;
	String lineEncode(void) const;
	String lineDecode(void) const;
	unsigned dottedToInt(unsigned base = 256) const;

	double toDouble() const;
	float toFloat() const;
	int toInt() const;
	bool toBool() const;

	template<typename T> void extract(T &value) const;

	void substrings(Set<String> &result, int minlength = 0) const;

	operator const char*(void) const;
	char &operator [](int pos);
	const char &operator [](int pos) const;

	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual void serialize(Stream &s) const;
	virtual bool deserialize(Stream &s);
	virtual bool isNativeSerializable(void) const;
	virtual String toString(void) const;
	virtual void fromString(String str);

protected:
	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
};

template<typename T> String String::number(T n)
{
	std::ostringstream out;
	out << n;
	return out.str();
}

template<typename T> void String::extract(T &value) const
{
	String tmp(*this);
	tmp.read(value);
}

// Templates functions from Stream using String are defined here

template<typename T> bool Stream::readLine(T &output)
{
	String line;
	if(!readLine(line)) return false;
	return line.read(output);
}

template<typename T> void Stream::writeLine(const T &input)
{
	write(input);
	write(NewLine);
}

}

#endif
