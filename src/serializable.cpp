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

#include "serializable.h"
#include "string.h"
#include "bytestring.h"
#include "exception.h"

namespace tpot
{

Serializable::Serializable(void)
{

}

Serializable::~Serializable(void)
{

}

void Serializable::serialize(Stream &s) const
{
	// DUMMY
	throw Unsupported("Object cannot be deserialized");
}

void Serializable::deserialize(Stream &s)
{
	// DUMMY
	throw Unsupported("Object cannot be deserialized");
}

void Serializable::serializeBinary(ByteStream &s) const
{
	// DUMMY
	throw Unsupported("Object cannot be serialized to binary");
}

void Serializable::deserializeBinary(ByteStream &s)
{
	// DUMMY
	throw Unsupported("Object cannot be deserialized from binary");
}

String Serializable::toString(void) const
{
	String str;
	serialize(str);
	return str;
}

ByteString Serializable::toBinary(void) const
{
	ByteString str;
	serializeBinary(str);
	return str;
}

Serializable::operator String(void) const
{
	return toString();
}

Serializable::operator ByteString(void) const
{
	return toBinary();
}

void Serializable::html(Stream &s) const
{
	serialize(s);
}

std::istream &operator>>(std::istream &is, Serializable &s)
{
	std::string stdstr;
	is>>stdstr;
	String str(stdstr);
	s.deserialize(str);
	return is;
}

std::ostream &operator<<(std::ostream &os, const Serializable &s)
{
	os<<std::string(s.toString());
	return os;
}

}
