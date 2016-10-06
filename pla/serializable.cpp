/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "pla/serializable.hpp"
#include "pla/string.hpp"
#include "pla/exception.hpp"
#include "pla/lineserializer.hpp"

namespace pla
{

void Serializable::serialize(Serializer &s) const
{
	throw Unsupported("Object cannot be serialized");
}

bool Serializable::deserialize(Serializer &s)
{
	return s.skip();
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
	return false;	// Most objects don't allow inline serialization
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

void Serializable::fromString(const std::string &str)
{
	String tmp(str);
	deserialize(tmp);  
}

Serializable::operator String(void) const
{
	return toString();
}

Serializable::operator std::string(void) const
{
        return toString();
}

std::ostream &operator<<(std::ostream &os, const Serializable &s)
{
	os<<std::string(s.toString());
	return os;
}

}
