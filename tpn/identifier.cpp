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

#include "tpn/identifier.h"
#include "tpn/exception.h"

namespace tpn
{

const Identifier Identifier::Null;

Identifier::Identifier(void)
{

}

Identifier::Identifier(const ByteString &digest, const String &name) :
	mDigest(digest),
	mName(name)
{

}

Identifier::~Identifier(void)
{

}

const ByteString &Identifier::getDigest(void) const
{
	return mDigest;
}

const String &Identifier::getName(void) const
{
	return mName;
}
	
void Identifier::setDigest(const ByteString &digest)
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

Identifier::operator ByteString &(void)
{
	return mDigest;  
}

Identifier::operator const ByteString &(void) const
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
	String str;
	if(!s.read(str)) return false;
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

