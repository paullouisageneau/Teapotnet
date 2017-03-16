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

#include "pla/bytearray.hpp"
#include "pla/exception.hpp"
#include "pla/string.hpp"

namespace pla
{

ByteArray::ByteArray(size_t size) :
	mArray(new char[size]),
	mLength(size),
	mLeft(0),
	mMustDelete(true)
{

}

ByteArray::ByteArray(char *array, size_t length) :
	mArray(array),
	mLength(length),
	mLeft(length),
	mMustDelete(false)
{

}

ByteArray::ByteArray(byte *array, size_t length) :
	mArray(reinterpret_cast<char*>(array)),
	mLength(length),
	mLeft(length),
	mMustDelete(false)
{

}

ByteArray::ByteArray(const ByteArray &array) :
	mArray(array.mArray),
	mLength(array.mLength),
	mLeft(array.mLeft),
	mReadPos(array.mReadPos),
	mWritePos(array.mWritePos),
	mMustDelete(false)
{

}

ByteArray::~ByteArray(void)
{
	if(mMustDelete)
		delete[] mArray;
}

char *ByteArray::array(void)
{
	return mArray;
}

const char *ByteArray::array(void) const
{
	return mArray;
}

size_t ByteArray::length(void) const
{
	return mLength;
}

const char *ByteArray::data(void) const
{
	return mArray + mReadPos;
}

const byte *ByteArray::bytes(void) const
{
	return reinterpret_cast<const byte*>(data());
}

size_t ByteArray::size(void) const
{
	return mLeft;
}

void ByteArray::clear(void)
{
	mReadPos = 0;
	mWritePos = 0;
	mLeft = 0;
}

void ByteArray::reset(void)
{
	mLeft+= mReadPos;
	mReadPos = 0;
}

void ByteArray::serialize(Serializer &s) const
{
	// implemented in Serializer::output(const BinaryString &)
	s << *this;
}

bool ByteArray::deserialize(Serializer &s)
{
	// implemented in Serializer::input(BinaryString &)
	return !!(s >> *this);
}

void ByteArray::serialize(Stream &s) const
{
	String str;
	for(int i=0; i<size(); ++i)
	{
		std::ostringstream oss;
		oss.width(2);
		oss.fill('0');
		oss<<std::hex<<std::uppercase<<unsigned(uint8_t(*(data()+i)));
		s<<oss.str();
	}
}

bool ByteArray::deserialize(Stream &s)
{
	clear();

	String str;
	if(!s.read(str)) return false;
	if(str.empty()) return true;

	int count = (str.size()+1)/2;
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

		char chr(value % 256);
		writeData(&chr, 1);
	}

	return true;
}

bool ByteArray::isNativeSerializable(void) const
{
        return false;
}

size_t ByteArray::readData(char *buffer, size_t size)
{
	if(mLeft <= 0) return 0;
	size = std::min(size,mLeft);
	std::copy(mArray+mReadPos, mArray+mReadPos+size, buffer);
	mReadPos+= size;
	mLeft-= size;
	return size;
}

void ByteArray::writeData(const char *data, size_t size)
{
	if(mWritePos+size > mLength) throw Exception("ByteArray overflow");
	std::copy(data, data+size, mArray+mWritePos);
	mWritePos+= size;
	mLeft+= size;
}

}
