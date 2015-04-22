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
const size_t Identifier::DigestSize = 16;
const size_t Identifier::Size = 2*DigestSize;      // total:  32 B
const Identifier Identifier::Null;

Identifier::Identifier(void)
{
	mDigest.writeZero(DigestSize);
	mNumber = 0;
}

Identifier::Identifier(const BinaryString &user, const BinaryString &node)
{
	mUser = user;
	if(user.size() < DigestSize) mUser.writeZero(DigestSize - user.size());
	else mUser.resize(DigestSize);

	mNode = node;
	if(node.size() < DigestSize) mNode.writeZero(DigestSize - node.size());
	else mNode.resize(DigestSize);
}

Identifier::~Identifier(void)
{

}

const BinaryString &Identifier::user(void) const
{
	return mUser;  
}

const BinaryString &Identifier::node(void) const
{
	return mNode;  
}
	
void Identifier::setUser(const BinaryString &user)
{
	mUser = user;  
}

void Identifier::setNode(const BinaryString &node)
{
	mNode = node; 
}

bool Identifier::hasUser(void) const
{
	for(int i=0; i<mUser.size(); ++i)
		if(mUser.at(i))
			return true;
	
	return false;
}

bool Identifier::hasNode(void) const
{
	for(int i=0; i<mNode.size(); ++i)
		if(mNode.at(i))
			return true;
	
	return false;
}

bool Identifier::empty(void) const
{
	return !hasUser();
}

void Identifier::clear(void)
{
	mUser.clear();
	mUser.writeZero(DigestSize);
	
	mNode.clear();
	mNode.writeZero(DigestSize);
}

Identifier::operator BinaryString &(void)
{
	return mUser; 
}

Identifier::operator const BinaryString &(void) const
{
	return mUser;
}

void Identifier::serialize(Serializer &s) const
{
	Assert(mUser.size() == DigestSize);
	Assert(mNode.size() == DigestSize);

	for(int i=0; i<DigestSize; ++i)
		s.output(uint8_t(mUser.at(i)));
	
	for(int i=0; i<DigestSize; ++i)
		s.output(uint8_t(mNode.at(i)));
}

bool Identifier::deserialize(Serializer &s)
{
	uint8_t b;
	if(!s.input(b)) 
	{
		clear();
		return false;
	}

	mUser.clear();
	mUser.push_back(b);
	for(size_t i=1; i<DigestSize; ++i)
	{
		AssertIO(s.input(b));
		mUser.push_back(b);
	}

	mNode.clear();
	for(size_t i=0; i<DigestSize; ++i)
	{
		AssertIO(s.input(b));
		mNode.push_back(b);
	}

	return true;
}

void Identifier::serialize(Stream &s) const
{
	if(mNumber != 0) s.write(mDigest.toString() + "." + String::hexa(mNumber));
	else s.write(mDigest.toString());
}

bool Identifier::deserialize(Stream &s)
{
	clear();
	
	String str;
	if(!s.read(str)) return false;
	if(str.empty()) return true;
	
	String tmp = str.cut('/');
	tmp.hexaMode(true);
	tmp.read(mNode);
	AssertIO(str.read(mUser));
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

