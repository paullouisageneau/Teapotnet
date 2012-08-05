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

#include "string.h"
#include "exception.h"

namespace arc
{

String::String(void)
{

}

String::String(const char chr) :
		std::string(&chr,&chr+1)
{

}

String::String(const char *str) :
		std::string(str)
{

}

String::String(const std::string &str) :
		std::string(str)
{

}

String::String(const String &str, int begin) :
		std::string(str,begin)
{

}

String::String(const String &str, int begin, int end) :
		std::string(str,begin,end)
{

}

String::~String(void)
{

}

void String::explode(std::list<String> &strings, char separator) const
{
	strings.clear();
	strings.push_back(String());
	for(int i=0; i<size(); ++i)
	{
		if(at(i) == separator) strings.push_back(String());
		else strings.back()+= at(i);
	}
}

void String::implode(const std::list<String> &strings, char separator)
{
	clear();
	for(std::list<String>::const_iterator it = strings.begin(); it != strings.end(); ++it)
	{
		if(it != strings.begin()) (*this)+= separator;
		(*this)+= *it;
	}
}

String String::cut(char separator)
{
	int pos = find(separator);
	if(pos == NotFound) return String();
	String after = substr(pos+1);
	resize(pos);
	return after;
}

void String::trim(void)
{
	int i = 0;
	while(i < size() && (at(i)==' ' || at(i) == '\t')) ++i;
	if(i > 0) erase(0,i);

	i = size();
	while(i > 0 && (at(i-1)==' ' || at(i-1) == '\t')) --i;
	if(i < size()) erase(i);
}

bool String::contains(char chr) const
{
	return (find(chr) != NotFound);
}

void String::remove(int pos, int nb)
{
    *this = String(this->substr(0, pos) + this->substr(pos+nb, std::string::npos));
}

bool String::isEmpty() const
{
    return this->empty();
}

String String::number(int n)
{
    std::ostringstream out;
    out << n;
    return out.str();
}

String String::number(unsigned int n)
{
    std::ostringstream out;
    out << n;
    return out.str();
}

String String::number(double d, int significatif)
{
    std::ostringstream out;
    out << std::setprecision(significatif) << d;
    return out.str();
}

int String::indexOf(char c, int from) const
{
    int pos = this->find(c, from);
    if(pos == std::string::npos) pos = -1;
    return pos;
}

int String::indexOf(const char* c, int from) const
{
    int pos = this->find(c, from);
    if(pos == std::string::npos) pos = -1;
    return pos;
}

int String::lastIndexOf(char c) const
{
    int pos = this->find_last_of(c);
    if(pos == std::string::npos) pos = -1;
    return pos;
}

int String::lastIndexOf(const char* c) const
{
    int pos = this->find_last_of(c);
    if(pos == std::string::npos) pos = -1;
    return pos;
}

bool String::remove(char chr)
{
	bool found = false;
	for(int i=0; i<this->size();)
		if(at(i) == chr)
		{
			this->erase(i);
			found = true;
		}
		else ++i;
	return found;
}

String String::mid(int pos, int n) const
{
    return this->substr(pos, n);
}

String String::left(int n) const
{
    return this->substr(0, std::min(n,int(this->size())));
}

String String::right(int n) const
{
    return this->substr(std::max(int(this->size())-n,0), std::min(n,int(this->size())));
}

String String::toLower(void) const
{
	String s(*this);
	for(int i=0; i<s.size(); ++i)
		s[i] = tolower(s[i]);
	return s;
}

String String::toUpper(void) const
{
	String s(*this);
	for(int i=0; i<s.size(); ++i)
		s[i] = toupper(s[i]);
	return s;
}

String String::toTrimmed(void) const
{
	String s(*this);
	s.trim();
	return s;
}

double String::toDouble() const
{
	String s(*this);
    return s.readDouble();
}

float String::toFloat() const
{
	String s(*this);
	return s.readFloat();
}

int String::toInt() const
{
	String s(*this);
	return s.readInt();
}

bool String::toBool() const
{
	String s(*this);
	return s.readBool();
}

void String::serialize(Stream &s) const
{
	s.write(*this);
}

void String::deserialize(Stream &s)
{
	assertIO(s.read(*this));
}

void String::serializeBinary(ByteStream &s) const
{
	const char null = '\0';
	s.write(data(),size());
	s.write(&null,1);
}

void String::deserializeBinary(ByteStream &s)
{
	clear();
	char chr;
	assertIO(s.read(&chr,1));
	while(chr != '\0')
	{
		push_back(chr);
		if(!s.read(&chr,1)) break;
	}
}

int String::readData(char *buffer, int size)
{
	size = std::min(size,int(this->size()));
	std::copy(begin(),begin()+size,buffer);
	erase(begin(),begin()+size); // WARNING: linear with string size
	return size;
}

void String::writeData(const char *data, int size)
{
	insert(end(), data, data+size);
}

}
