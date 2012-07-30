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
		mreadPos(0),
		mwritePos(0),
		mMustDelete(true)
{

}

ByteArray::ByteArray(char *data, int size) :
		mData(data),
		mSize(size),
		mLeft(0),
		mreadPos(0),
		mwritePos(0),
		mMustDelete(false)
{

}

ByteArray::ByteArray(const ByteArray &array) :
	mData(array.mData),
	mSize(array.mSize),
	mLeft(array.mLeft),
	mreadPos(array.mreadPos),
	mwritePos(array.mwritePos),
	mMustDelete(false)
{

}

ByteArray::~ByteArray(void)
{
	if(mMustDelete) delete mData;
}

const char *ByteArray::data(void) const
{
	return mData+mreadPos;
}

int ByteArray::size(void) const
{
	return mLeft;
}

void ByteArray::clear(void)
{
	mreadPos = 0;
	mwritePos = 0;
	mLeft = 0;
}

int ByteArray::readData(char *buffer, int size)
{
	if(mLeft <= 0) return 0;
	size = std::min(size,mLeft);
	std::copy(mData+mreadPos, mData+mreadPos+size, buffer);
	mreadPos+= size;
	mLeft-= size;
	return size;
}

void ByteArray::writeData(const char *data, int size)
{
	if(mwritePos+size > mSize) throw IOException();
	std::copy(data, data+size, mData+mwritePos+size);
	mwritePos+= size;
	if(mLeft< mwritePos) mLeft = mwritePos;
}

}
