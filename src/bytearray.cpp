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

#include "bytearray.h"
#include "exception.h"

namespace arc
{

ByteArray::ByteArray(int size) :
		mData(new char[size]),
		mSize(size),
		mLeft(0),
		mReadPos(0),
		mWritePos(0),
		mMustDelete(true)
{

}

ByteArray::ByteArray(char *data, int size) :
		mData(data),
		mSize(size),
		mLeft(0),
		mReadPos(0),
		mWritePos(0),
		mMustDelete(false)
{

}

ByteArray::ByteArray(const ByteArray &array) :
	mData(array.mData),
	mSize(array.mSize),
	mLeft(array.mLeft),
	mReadPos(array.mReadPos),
	mWritePos(array.mWritePos),
	mMustDelete(false)
{

}

ByteArray::~ByteArray(void)
{
	if(mMustDelete) delete mData;
}

const char *ByteArray::data(void) const
{
	return mData+mReadPos;
}

int ByteArray::size(void) const
{
	return mLeft;
}

void ByteArray::clear(void)
{
	mReadPos = 0;
	mWritePos = 0;
	mLeft = 0;
}

int ByteArray::readData(char *buffer, int size)
{
	if(mLeft <= 0) return 0;
	size = std::min(size,mLeft);
	std::copy(mData+mReadPos, mData+mReadPos+size, buffer);
	mReadPos+= size;
	mLeft-= size;
	return size;
}

void ByteArray::writeData(const char *data, int size)
{
	if(mWritePos+size > mSize) throw IOException();
	std::copy(data, data+size, mData+mWritePos+size);
	mWritePos+= size;
	if(mLeft< mWritePos) mLeft = mWritePos;
}

}
