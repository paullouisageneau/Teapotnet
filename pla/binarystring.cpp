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

#include "pla/binarystring.hpp"
#include "pla/exception.hpp"
#include "pla/string.hpp"

namespace pla
{

const BinaryString BinaryString::Empty;

BinaryString::BinaryString(void)
{

}

BinaryString::BinaryString(const char *str) :
		std::string(str)
{

}

BinaryString::BinaryString(const char *data, size_t size) :
		std::string(data, data+size)
{

}

BinaryString::BinaryString(const std::string &str) :
		std::string(str)
{

}

BinaryString::BinaryString(size_t n, char chr) :
		std::string(n, chr)
{

}


BinaryString::BinaryString(const BinaryString &str, int begin) :
		std::string(str,begin)
{

}

BinaryString::BinaryString(const BinaryString &str, int begin, int end) :
		std::string(str,begin,end)
{

}

BinaryString::~BinaryString(void)
{

}

char *BinaryString::ptr(void)
{
	return reinterpret_cast<char*>(&at(0));
}

const char *BinaryString::ptr(void) const
{
	return reinterpret_cast<const char*>(&at(0));
}

byte *BinaryString::bytes(void)
{
	return reinterpret_cast<byte*>(&at(0));
}

const byte *BinaryString::bytes(void) const
{
	return reinterpret_cast<const byte*>(&at(0));
}

BinaryString BinaryString::base64Encode(bool safeMode) const
{
	// safeMode is RFC 4648 'base64url' encoding

	static const char standardTab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static const char safeTab[]     = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	const char *tab = (safeMode ? safeTab : standardTab);

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
		out+= tab[uint8_t(at(i)) >> 2];
		if(left == 1)
		{
			out+= tab[(uint8_t(at(i)) & 3) << 4];
			if(!safeMode) out+= '=';
		}
		else {	// left == 2
			out+= tab [((uint8_t(at(i)) & 3) << 4) | (uint8_t(at(i+1)) >> 4)];
			out+= tab [(uint8_t(at(i+1)) & 0x0F) << 2];
		}
		if(!safeMode) out+= '=';
	}

	return out;
}

BinaryString BinaryString::base64Decode(void) const
{
	String out;
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

void BinaryString::serialize(Serializer &s) const
{
	s << uint32_t(size());

	for(int i=0; i<size(); ++i)
		s << uint8_t(at(i));
}

bool BinaryString::deserialize(Serializer &s)
{
	clear();

	uint32_t count;
	if(!(s >> count)) return false;

	uint8_t b;
	for(uint32_t i=0; i<count; ++i)
	{
		AssertIO(s >> b);
		push_back(b);
	}

	return true;
}

void BinaryString::serialize(Stream &s) const
{
	for(int i=0; i<size(); ++i)
	{
		std::ostringstream oss;
		oss.width(2);
		oss.fill('0');
		oss << std::hex << std::uppercase << unsigned(uint8_t(at(i)));
		s << oss.str();
	}
}

bool BinaryString::deserialize(Stream &s)
{
	clear();

	String str;
	if(!s.read(str)) return false;
	if(str.empty()) return true;

	int count = (str.size()+1)/2;
	reserve(count);
	for(int i=0; i<count; ++i)
	{
		std::string byte;
		byte+= str[i*2];
		if(i*2+1 != str.size()) byte+= str[i*2+1];
		else byte+= '0';
		std::istringstream iss(byte);

		unsigned value = 0;
		iss>>std::hex;
		if(!(iss>>value))
			throw InvalidData("Invalid hexadecimal representation");

		push_back(uint8_t(value % 256));
	}

	return true;
}

bool BinaryString::isNativeSerializable(void) const
{
	return false;
}

bool BinaryString::isInlineSerializable(void) const
{
	return true;
}

size_t BinaryString::readData(char *buffer, size_t size)
{
	size = std::min(size,this->size());
	std::copy(begin(),begin()+size,buffer);
	erase(begin(),begin()+size); // WARNING: linear with string size
	return size;
}

void BinaryString::writeData(const char *data, size_t size)
{
	insert(end(), data, data+size);
}

void BinaryString::clear(void)
{
	std::string::clear();
}

BinaryString operator ^ (const BinaryString &a, const BinaryString &b)
{
	BinaryString result;
	if(a.size() > b.size())
	{
		result = a;
		if(!b.empty()) memxor(result.ptr(), b.ptr(), b.size());
	}
	else {
		result = b;
		if(!a.empty()) memxor(result.ptr(), a.ptr(), a.size());
	}
	return result;

}

}
