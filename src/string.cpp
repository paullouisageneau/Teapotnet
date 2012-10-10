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

#include "string.h"
#include "exception.h"

namespace tpot
{

String String::number(double d, int digits)
{
    std::ostringstream out;
    out << std::setprecision(digits) << d;
    return out.str();
}

String String::number(unsigned int n, int minDigits)
{
    std::ostringstream out;
    out << n;
    return String(std::max(0, int(minDigits-out.str().size())), '0') + out.str();
}

String String::hexa(unsigned int n, int minDigits)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << n;
    return String(std::max(0, int(minDigits-out.str().size())), '0') + out.str();
}
  
String String::hrSize(uint64_t size)
{
 	const char prefix[] = {'K','M','G','T'};
	const unsigned base = 1024;
	
	if(size < base) return String::number(size) + " B";
	
	char symbol;
	for(int i=0; i<4; ++i)
	{
		size/= base;
		symbol = prefix[i];
		if(size < base) break; 
	}
	return String::number(size) + " " + symbol + "iB";
}

String String::hrSize(const String &size)
{
	String tmp(size);
	uint64_t i;
	tmp >> i;
	return hrSize(i);
}
  
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

String::String(size_t n, char chr) :
		std::string(n, chr)
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

String String::cutLast(char separator)
{
	int pos = find_last_of(separator);
	if(pos == NotFound) return String();
	String after = substr(pos+1);
	resize(pos);
	return after;
}

void String::trim(void)
{
	int i = 0;
	while(i < size() && BlankCharacters.contains(at(i))) ++i;
	if(i > 0) erase(0,i);

	i = size();
	while(i > 0 && BlankCharacters.contains(at(i-1))) --i;
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
		if(this->at(i) == chr)
		{
			this->erase(i);
			found = true;
		}
		else ++i;
	return found;
}

bool String::replace(char a, char b)
{
	bool found = false;
	for(int i=0; i<this->size(); ++i)
		if(this->at(i) == a)
		{
			this->at(i) = b;
			found = true;
		}
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

String String::capitalized(void) const
{
	String s(*this);
	bool cap = true;
	for(int i=0; i<s.size(); ++i)
	{
		if(cap)
		{
			s[i] = toupper(s[i]);
			cap = false;
		}
		else {
			if(s[i] == ' ' || s[i] == '-') cap=true;
			else s[i] = tolower(s[i]);
		}
	}
	return s;
}

String String::trimmed(void) const
{
	String s(*this);
	s.trim();
	return s;
}

String String::urlEncode(void) const
{
	String out;
	for(int i=0; i<size(); ++i)
	{
		char c = at(i);
		if(std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out+= c;
    		else if(c == ' ') out+= '+';
    		else {
		  out+= '%';
		  out+= String::hexa(unsigned((unsigned char)(c)));
		}
	}
	return out;
}

String String::urlDecode(void) const
{
	String out;
	for(int i=0; i<size(); ++i)
	{
		char c = at(i);
 		if(c == '%') 
		{
			String h(substr(i+1,2));

			unsigned value;
			try {
				h.hexaMode(true);
				h>>value;
			}
			catch(...)
			{
				out+= '%';
				continue;
			}
			out+= char(value);
			i+=2;
    		} 
    		else if(c == '+') out+= ' ';
		else out+= c;
	}
	return out;
}

String String::base64Encode(void) const
{
	static char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	String out;
	int i = 0;
	while (size()-i >= 3)
	{
		out+= tab[uint8_t(at(i)) >> 2];
		out+= tab[((uint8_t(at(i)) & 3) << 4) | (uint8_t(at(i+1)) >> 4)];
		out+= tab[((uint8_t(at(i+1)) & 0x0F) << 2) | (uint8_t(at(i+2)) >> 6)];
		out+= tab[uint8_t(at(i+2)) & 0x3F];
		i+= 3;
	}

	int left = size()-i;
	if(left)
	{
		out+= tab[at(i) >> 2];
		if (left == 1)
		{
			out+= tab[(uint8_t(at(i)) & 3) << 4];
			out+= '=';
		}
		else {
			out+= tab [((uint8_t(at(i)) & 3) << 4) | (uint8_t(at(i+1)) >> 4)];
			out+= tab [(uint8_t(at(i+1)) & 0x0F) << 2];
		}
		out+= '=';
	}
	
	return out;
}

String String::base64Decode(void) const
{
	String out;
	int i = 0;
	while(i < size())
	{
		unsigned char tab[4];
		int j = 0;
		while(i < size() && j < 4)
		{
			char c = at(i);
			if(c == '=') break;
			
			if ('A' <= c && c <= 'Z') tab[j] = c - 'A';
			else if ('a' <= c && c <= 'z') tab[j] = c + 26 - 'a';
			else if ('0' <= c && c <= '9') tab[j] = c + 52 - '0';
			else if (c == '+') tab[j] = 62;
			else if (c == '/') tab[j] = 63;
			else throw IOException("Invalid character");
			
			++i; ++j;
		}

		if(j)
		{
			out+= (tab[0] << 2) | (tab[1] >> 4);
			if (j > 2)
			{
				out+= (tab[1] << 4) | (tab[2] >> 2);
				if (j > 3) out+= (tab[2] << 6) | (tab[3]);
			}
		}
		
		if(i < size() && at(i) == '=') break;
	}

	return out;
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

String::operator const char*(void)
{
	return this->c_str(); 
}

char &String::operator [](int pos)
{
	return this->at(pos);
}

const char &String::operator [](int pos) const
{
	return this->at(pos); 
}

void String::serialize(Serializer &s) const
{
	s.output(*this);
}

bool String::deserialize(Serializer &s)
{
	return s.input(*this);
}

void String::serialize(Stream &s) const
{
	s.write(*this);
}

bool String::deserialize(Stream &s)
{
	s.read(*this); 
}

size_t String::readData(char *buffer, size_t size)
{
	size = std::min(size,this->size());
	std::copy(begin(),begin()+size,buffer);
	erase(begin(),begin()+size); // WARNING: linear with string size
	return size;
}

void String::writeData(const char *data, size_t size)
{
	insert(end(), data, data+size);
}

}
