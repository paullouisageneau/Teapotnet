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

namespace tpn
{

const Identifier Identifier::Null;

Identifier::Identifier(void)
{

}

Identifier::Identifier(const BinaryString &digest, const String &name) :
	mDigest(digest),
	mName(name)
{

}

Identifier::~Identifier(void)
{

}

BinaryString Identifier::getDigest(void) const
{
	return mDigest;
}

String Identifier::getName(void) const
{
	return mName;
}
	
void Identifier::setDigest(const BinaryString &digest)
{
	mDigest = digest;
}

void Identifier::setName(const String &name)
{
	mName = name;
}

bool Identifier::empty(void)
{
 	return mDigest.empty(); 
}

void Identifier::clear(void)
{
	mDigest.clear();
	mName.clear();
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
	mDigest.serialize(s);
	mName.serialize(s);
}

bool Identifier::deserialize(Serializer &s)
{
	if(!mDigest.deserialize(s)) return false;
	AssertIO(mName.deserialize(s));
	return true;
}

void Identifier::serialize(Stream &s) const
{
	String str = mDigest.toString();
	if(!mName.empty()) str+= ':' + mName;
	s.write(str);
}

bool Identifier::deserialize(Stream &s)
{
	clear();
	String str;
	if(!s.read(str)) return false;
	if(str.empty()) return true;
	mName = str.cut(':');
	AssertIO(str.read(mDigest));
	return true;
}

bool operator < (const Identifier &i1, const Identifier &i2)
{
	return (i1.getDigest() < i2.getDigest() 
	  || (i1.getDigest() == i2.getDigest()
	  	&& !i1.getName().empty() && !i2.getName().empty() && i1.getName() < i2.getName()));
}

bool operator > (const Identifier &i1, const Identifier &i2)
{
	return (i1.getDigest() > i2.getDigest()
	  || (i1.getDigest() == i2.getDigest()
	  	&& !i1.getName().empty() && !i2.getName().empty() && i1.getName() > i2.getName()));
}

bool operator == (const Identifier &i1, const Identifier &i2)
{
	return (i1.getDigest() == i2.getDigest()
		&& (i1.getName().empty() || i2.getName().empty() || i1.getName() == i2.getName()));
}

bool operator != (const Identifier &i1, const Identifier &i2)
{
	return !(i1 == i2);
}

}

