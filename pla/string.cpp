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

#include "pla/string.hpp"
#include "pla/exception.hpp"
#include "pla/array.hpp"
#include "pla/list.hpp"
#include "pla/set.hpp"
#include "pla/map.hpp"
#include "pla/binarystring.hpp"

namespace pla
{

String String::number(double d, int digits)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(digits) << d;
	return out.str();
}

String String::number(int n, int minDigits)
{
	std::ostringstream out;
	out << std::abs(n);
	return (n < 0 ? "-" : "") + String(std::max(0, int(minDigits-out.str().size())), '0') + out.str();
}

String String::number(unsigned int n, int minDigits)
{
	std::ostringstream out;
	out << n;
	return String(std::max(0, int(minDigits-out.str().size())), '0') + out.str();
}

String String::number64(uint64_t n, int minDigits)
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

String String::random(size_t nbr, Random::QualityLevel level)
{
	Random rnd(level);
	String result;
	while(nbr--)
	{
		int i = rnd.uniform(0, 26 + 26 + 10);

		if(i < 26) result+= char('a' + i);
		else {
			i-= 26;
			if(i < 26) result+= char('A' + i);
			else {
				i-= 26;
				result+= char('0' + i);
			}
		}
	}

	return result;
}

String String::hrSize(uint64_t size)
{
 	const char prefix[] = {'K','M','G','T'};
	const int base = 1024;

	if(size < base) return String::number(size) + " B";

	char symbol;
	double fsize(size);
	for(int i=0; i<4; ++i)
	{
		fsize/= base;
		symbol = prefix[i];
		if(fsize < base) break;
	}
	return String::number(fsize,1) + " " + symbol + "iB";
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

String::String(char chr) :
	std::string(&chr, size_t(1))
{

}

String::String(const char *str) :
	std::string(str)
{

}

String::String(const char *data, size_t size) :
	std::string(data, size)
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
	std::string(str, begin)
{

}

String::String(const String &str, int begin, int end) :
	std::string(str, begin, end)
{

}

String::String(const wchar_t *str)
{
	unsigned int codepoint = 0;
	while(*str != 0)
	{
		if (*str >= 0xd800 && *str <= 0xdbff)
			codepoint = ((*str - 0xd800) << 10) + 0x10000;
		else
		{
			if (*str >= 0xdc00 && *str <= 0xdfff)
				codepoint |= *str - 0xdc00;
			else
				codepoint = *str;

			if (codepoint <= 0x7f)
				append(1, static_cast<char>(codepoint));
			else if (codepoint <= 0x7ff)
			{
				append(1, static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
				append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
			}
			else if (codepoint <= 0xffff)
			{
				append(1, static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
				append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
				append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
			}
			else
			{
				append(1, static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
				append(1, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
				append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
				append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
			}
			codepoint = 0;
		}

		++str;
	}
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

void String::trimQuotes(void)
{
	int i = 0;
	while(i < size() && (at(i) == '\'' || at(i) == '\"')) ++i;
	if(i > 0) erase(0,i);

	i = size();
	while(i > 0 && (at(i-1) == '\'' || at(i-1) == '\"')) --i;
	if(i < size()) erase(i);
}

bool String::contains(char chr) const
{
	return (find(chr) != NotFound);
}

bool String::contains(const String &str) const
{
	return (find(str) != NotFound);
}

bool String::containsDigits(void) const
{
	for(int i=0; i<size(); ++i)
	{
		unsigned char c = (unsigned char)(at(i));
		if(c >= 48 && c <= 57) return true;
	}

	return false;
}

bool String::containsLetters(void) const
{
	for(int i=0; i<size(); ++i)
	{
		unsigned char c = (unsigned char)(at(i));
		if(c >= 65 && c <= 90) return true;
		if(c >= 97 && c <= 122) return true;
		if(c >= 128) return true;
	}

	return false;
}

bool String::isAlphanumeric(void) const
{
	for(int i=0; i<size(); ++i)
	{
		unsigned char c = (unsigned char)(at(i));
		if(c >= 48 && c <= 57) continue;	// digit
		if(c >= 65 && c <= 90) continue;
		if(c >= 97 && c <= 122) continue;
		if(c >= 128) continue;
		return false;
	}

	return true;
}

void String::remove(int pos, int n)
{
    std::string::size_type end = std::string::npos;
    if(n >= 0) end = pos + n;
    *this = String(this->substr(0, pos) + this->substr(end, std::string::npos));
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
			this->erase(i, 1);
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

bool String::replace(const String &a, const String &b)
{
	bool found = false;
	std::string::size_type pos = 0;
	while ((pos = std::string::find(a, pos)) != std::string::npos)
	{
	    std::string::replace(pos, a.length(), b);
	    pos += b.length();
	    found = true;
	}
	return found;
}

String String::mid(int pos, int n) const
{
    if(n >= 0) return this->substr(pos, n);
    else return this->substr(pos, std::string::npos);
}

String String::left(int n) const
{
    return this->substr(0, std::min(n,int(this->size())));
}

String String::right(int n) const
{
    return this->substr(std::max(int(this->size())-n,0), std::min(n,int(this->size())));
}

String String::after(char c) const
{
	int pos = this->indexOf(c);
	if(pos == NotFound) return *this;
	else return this->substr(pos+1);
}

String String::afterLast(char c) const
{
	int pos = this->lastIndexOf(c);
	if(pos == NotFound) return *this;
	else return this->substr(pos+1);
}

String String::before(char c) const
{
	int pos = this->indexOf(c);
	if(pos == NotFound) return *this;
	else return this->substr(0,pos);
}

String String::beforeLast(char c) const
{
	int pos = this->lastIndexOf(c);
	if(pos == NotFound) return *this;
	else return this->substr(0,pos);
}

String String::noAccents(void) const
{
	String s(*this);
	s.replace("é", "e"); s.replace("è", "e"); s.replace("ê", "e"); s.replace("ë", "e"); s.replace("ñ", "n");
	s.replace("ì", "i"); s.replace("í", "i"); s.replace("î", "i"); s.replace("ï", "i"); s.replace("ç", "c");
	s.replace("ù", "u"); s.replace("ú", "u"); s.replace("û", "u"); s.replace("ü", "u"); s.replace("ÿ", "y");
	s.replace("à", "a"); s.replace("á", "a"); s.replace("â", "a"); s.replace("ä", "a"); s.replace("ã", "a");
	s.replace("ò", "o"); s.replace("ó", "o"); s.replace("ô", "o"); s.replace("ö", "o"); s.replace("õ", "o");
	s.replace("É", "E"); s.replace("È", "E"); s.replace("Ê", "E"); s.replace("Ë", "E"); s.replace("Ñ", "N");
	s.replace("Ì", "I"); s.replace("Í", "I"); s.replace("Î", "I"); s.replace("Ï", "I"); s.replace("Ç", "C");
	s.replace("Ù", "U"); s.replace("Ú", "U"); s.replace("Û", "U"); s.replace("Ü", "U"); s.replace("Ÿ", "Y");
	s.replace("À", "A"); s.replace("Á", "A"); s.replace("Â", "A"); s.replace("Ä", "A"); s.replace("Ã", "A");
	s.replace("Ò", "O"); s.replace("Ó", "O"); s.replace("Ô", "O"); s.replace("Ö", "O"); s.replace("Õ", "O");
	return s;
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

String String::capitalizedFirst(void) const
{
	String s(*this);
	if(!s.empty())
		s[0] = toupper(s[0]);
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

// Quick and dirty Windows-1252 to UTF-8 conversion
// TODO: this is ugly and should be replaced by proper locale handling
String String::windowsDecode(void) const
{
	String result;

	for(int i=0; i<size(); ++i)
	{
		char chr = at(i);
		if(chr & 0x80)
		{
			result+= char((uint8_t(chr) >> 6) | uint8_t(0xC0));		// 1st byte
			result+= char((uint8_t(chr) & ~uint8_t(0xC0)) | uint8_t(0x80));	// 2nd byte
		}
		else {
			result+= chr;
		}
	}

	return result;
}

// Quick and dirty UTF-8 to Windows-1252 conversion
// TODO: this is ugly and should be replaced by proper locale handling
String String::windowsEncode(void) const
{
	String result;

	for(int i=0; i<size(); ++i)
	{
		char chr = at(i);
		if(chr & 0x80)		// 2 bytes
		{
			++i;
			if(chr & 0x20)	// 3 bytes
			{
				result+= '?';
				++i;
				if(chr & 0x10) ++i;	// 4 bytes
			}
			else {
				chr = char(uint8_t(chr) << 6);
				if(i != size())
					chr|= char(uint8_t(at(i)) & ~uint8_t(0xC0));
				result+= chr;
			}
		}
		else {
			result+= chr;
		}
	}

	return result;
}

String String::pathEncode(void) const
{
#ifdef WINDOWS
	return windowsEncode();
#else
	return *this;
#endif
}

String String::pathDecode(void) const
{
#ifdef WINDOWS
	return windowsDecode();
#else
	return *this;
#endif
}

String String::lineEncode(void) const
{
	String out;
	String tr = trimmed();
	for(int i=0; i<tr.size(); ++i)
	{
		char c = tr.at(i);
		if(c == '\r') continue;
		if(c == '\n') out+= "\\n";
		else out+= c;
	}
	return out;
}

String String::lineDecode(void) const
{
	String out;
	for(int i=0; i<size(); ++i)
	{
		char c = at(i);
 		if(c == '\\')
		{
			++i;
			if(i == size()) break;
			c = at(i);
			switch(c)
			{
			case 'b': 	out+='\b';	break;
			case 'f': 	out+='\f';	break;
			case 't': 	out+='\t';	break;
			case 'n': 	out+= Stream::NewLine;	break;
			}
		}
		else out+= c;
	}
	return out;
}

BinaryString String::base64Decode(void) const
{
	BinaryString out;
	int i = 0;
	while(i < size())
	{
		unsigned char tab[4];
		std::memset(tab, 0, 4);
		int j = 0;
		while(i < size() && j < 4)
		{
			char c = at(i);
			if(c == '=') break;

			if ('A' <= c && c <= 'Z') tab[j] = c - 'A';
			else if ('a' <= c && c <= 'z') tab[j] = c + 26 - 'a';
			else if ('0' <= c && c <= '9') tab[j] = c + 52 - '0';
			else if (c == '+' || c == '-') tab[j] = 62;
			else if (c == '/' || c == '_') tab[j] = 63;
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

unsigned String::dottedToInt(unsigned base) const
{
	List<String> l;
	explode(l, '.');
	if(l.size() <= 1) return 0;

	unsigned n = 0;
	unsigned b = 1;
	while(!l.empty())
	{
		try {
		  	unsigned i = 0;
			l.back().extract(i);
			n+= i*b;
			b*= base;
		}
		catch(...) { }

		l.pop_back();
	}

	return n;
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
	if(empty()) return false;
	String s(*this);
	return s.readBool();
}

void String::substrings(Set<String> &result, int minlength) const
{
	for(int s = std::max(minlength,0); s<=size(); ++s)
	{
		if(s == 0) result.insert(String());
		else for(int i=0; i<size()-s+1; ++i)
			 result.insert(substr(i,s));
	}
}

String::operator const char*(void) const
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

char *String::ptr(void)
{
	return reinterpret_cast<char*>(&at(0));
}

const char *String::ptr(void) const
{
	return reinterpret_cast<const char*>(&at(0));
}

void String::serialize(Serializer &s) const
{
	s << *static_cast<const std::string*>(this);
}

bool String::deserialize(Serializer &s)
{
	return s >> *static_cast<std::string*>(this);
}

void String::serialize(Stream &s) const
{
	s.writeData(data(), size());
}

bool String::deserialize(Stream &s)
{
	clear();

	char chr;
	if(!s.get(chr)) return false;

	while(Stream::BlankCharacters.contains(chr))
		if(!get(chr)) return false;

	while(!Stream::BlankCharacters.contains(chr))
	{
		push_back(chr);
		if(!s.get(chr)) break;
	}

	return true;
}

bool String::isNativeSerializable(void) const
{
	return true;
}

String String::toString(void) const
{
	return *this;
}

void String::fromString(String str)
{
	*this = str;
}

void String::clear(void)
{
	std::string::clear();
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
