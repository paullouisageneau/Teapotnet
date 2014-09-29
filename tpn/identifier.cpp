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

#include "pla/exception.h"
#include "pla/random.h"

namespace tpn
{

// Hardcoded identifier size, do not change
const size_t Identifier::DigestSize = 32;            // digest: 32 B
const size_t Identifier::Size = DigestSize + 8;      // total:  40 B
const Identifier Identifier::Null;

Identifier::Identifier(void)
{
	mDigest.writeZero(DigestSize);
	mNumber = 0;
}

Identifier::Identifier(const BinaryString &digest, uint64_t number)
{
	mDigest.writeData(digest.data(), std::min(digest.size(), DigestSize));
	if(DigestSize > digest.size())
		mDigest.writeZero(DigestSize - digest.size());
	
	mNumber = number;
}

Identifier::~Identifier(void)
{

}

const BinaryString &Identifier::digest(void) const
{
	return mDigest;  
}

uint64_t Identifier::number(void) const
{
	return mNumber; 
}
	
void Identifier::setDigest(const BinaryString &digest)
{
	mDigest = digest;  
}

void Identifier::setNumber(uint64_t number)
{
	mNumber = number; 
}

bool Identifier::empty(void) const
{
	for(int i=0; i<mDigest.size(); ++i)
		if(mDigest.at(i))
			return false;
	
	return true;
}

void Identifier::clear(void)
{
	mDigest.clear();
	mDigest.writeZero(DigestSize);
	mNumber = 0;
}

Identifier::operator BinaryString &(void)
{
	return mDigest; 
}

Identifier::operator const BinaryString &(void) const
{
	return mDigest;
}

void Identifier::serialize(Serializer &s) const
{
	Assert(mDigest.size() == DigestSize);
	for(int i=0; i<DigestSize; ++i)
		s.output(uint8_t(mDigest.at(i)));
	
	s.output(mNumber);
}

bool Identifier::deserialize(Serializer &s)
{
	mDigest.clear();
	mNumber = 0;
	
	uint8_t b;
	if(!s.input(b)) return false;
	mDigest.push_back(b);
	
	for(size_t i=1; i<DigestSize; ++i)
	{
		AssertIO(s.input(b));
		mDigest.push_back(b);
	}

	s.input(mNumber);
	return true;
}

void Identifier::serialize(Stream &s) const
{
	if(mNumber != 0) s.write(mDigest.toString() + ":" + String::hexa(mNumber));
	else s.write(mDigest.toString());
}

bool Identifier::deserialize(Stream &s)
{
	mDigest.clear();
	mNumber = 0;
	
	String str;
	if(!s.read(str)) return false;
	if(str.empty()) return true;
	
	String tmp = str.cut(':');
	tmp.hexaMode(true);
	tmp.read(mNumber);
	AssertIO(str.read(mDigest));
	return true;
}

bool Identifier::isNativeSerializable(void) const
{
        return false;
}

bool Identifier::isInlineSerializable(void) const
{
        return true;
}

bool operator < (const Identifier &i1, const Identifier &i2)
{
	return (i1.digest() < i2.digest()
		|| (i1.digest() == i2.digest()
		&& i1.number() && i2.number() && i1.number() < i2.number()));
}

bool operator > (const Identifier &i1, const Identifier &i2)
{
	return (i1.digest() > i2.digest()
		|| (i1.digest() == i2.digest()
		&& i1.number() && i2.number() && i1.number() > i2.number()));
}

bool operator == (const Identifier &i1, const Identifier &i2)
{
	return (i1.digest() == i2.digest()
		&& (!i1.number() || !i2.number() || i1.number() == i2.number()));
}

bool operator != (const Identifier &i1, const Identifier &i2)
{
	return !(i1 == i2);
}

}

