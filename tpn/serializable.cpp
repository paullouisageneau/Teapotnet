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

#include "tpn/serializable.h"
#include "tpn/string.h"
#include "tpn/exception.h"
#include "tpn/lineserializer.h"

namespace tpn
{

Serializable::Serializable(void)
{

}

Serializable::~Serializable(void)
{

}

void Serializable::serialize(Serializer &s) const
{
	// DUMMY
	throw Unsupported("Object cannot be deserialized");
}

bool Serializable::deserialize(Serializer &s)
{
	// DUMMY
	throw Unsupported("Object cannot be deserialized");
}

void Serializable::serialize(Stream &s) const
{
	LineSerializer serializer(&s);
	serialize(serializer);
}

bool Serializable::deserialize(Stream &s)
{
	LineSerializer serializer(&s);
	return deserialize(serializer);
}

bool Serializable::isInlineSerializable(void) const
{
	return true;	// Most objects allow inline serialization
}

bool Serializable::isNativeSerializable(void) const
{
	return false;	// Most objects are not equivalent to native types
}

String Serializable::toString(void) const
{
	String str;
	serialize(str);
	str.trim();
	return str;
}

void Serializable::fromString(String str)
{
	AssertIO(deserialize(str));  
}

Serializable::operator String(void) const
{
	return toString();
}

std::ostream &operator<<(std::ostream &os, const Serializable &s)
{
	os<<std::string(s.toString());
	return os;
}

}
