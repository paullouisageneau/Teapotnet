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

#include "bytearray.h"
#include "exception.h"

namespace tpot
{

ByteArray::ByteArray(size_t size) :
		mArray(new char[size]),
		mLength(size),
		mLeft(0),
		mReadPos(0),
		mWritePos(0),
		mMustDelete(true)
{

}

ByteArray::ByteArray(char *array, size_t length) :
		mArray(array),
		mLength(length),
		mLeft(length),
		mReadPos(0),
		mWritePos(0),
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
	if(mMustDelete) delete mArray;
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
	return mArray+mReadPos;
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
	if(mWritePos+size > mLength) throw IOException();
	std::copy(data, data+size, mArray+mWritePos);
	mWritePos+= size;
	mLeft+= size;
}

}
