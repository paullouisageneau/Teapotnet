/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/identifier.h"
#include "tpn/exception.h"
#include "tpn/random.h"

namespace tpn
{

const Identifier Identifier::Null;

Identifier::Identifier(void)
{
	mData.writeZeros(Size);
}

Identifier::Identifier(const BinaryString &digest)
{
	mData.writeData(digest.data(), std::min(digest.size(), Size));
	if(Size > digest.size())
		mData.writeZeros(Size - digest.size());
}

Identifier::~Identifier(void)
{

}

const char *data(void) const
{
	return mData.data(); 
}

size_t size(void) const
{
	Assert(mData.size() == Size);
	return Size; 
}

bool Identifier::empty(void) const
{
 	return mData.empty(); 
}

void Identifier::clear(void)
{
	mData.clear();
	mData.writeZeros(Size);
}

BinaryString Identifier::toBinaryString(void) const
{
	return mData;
}

Identifier::operator BinaryString &(void)
{
	return mData; 
}

Identifier::operator const BinaryString &(void) const
{
	return mData;
}

void Identifier::serialize(Serializer &s) const
{
	Assert(mData.size() == Size);
	for(int i=0; i<Size; ++i)
		s.output(uint8_t(mData.at(i)));
}

bool Identifier::deserialize(Serializer &s)
{
	mData.clear();
	mData.reserve(Size);
	
	uint8_t b;
	if(!s.input(b)) return false;
	mData.push_back(b);
	
	for(size_t i=1; i<Size; ++i)
	{
		AssertIO(s.input(b));
		mData.push_back(b);
	}

	return true;
}

void Identifier::serialize(Stream &s) const
{
	mData.serialize(s);
}

bool Identifier::deserialize(Stream &s)
{
	bool ret = mData.deserialize(s);
	
	if(digest.size() >= Size) mData.resize(Size);
	else mData.writeZeros(Size - digest.size());
	
	return ret;
}

bool operator < (const Identifier &i1, const Identifier &i2)
{
	return (i1.toBinaryString() < i2.toBinaryString());
}

bool operator > (const Identifier &i1, const Identifier &i2)
{
	return (i1.toBinaryString() > i2.toBinaryString());
}

bool operator == (const Identifier &i1, const Identifier &i2)
{
	return (i1.toBinaryString() == i2.toBinaryString());
}

bool operator != (const Identifier &i1, const Identifier &i2)
{
	return !(i1 == i2);
}

}

