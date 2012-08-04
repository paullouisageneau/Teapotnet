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

#ifndef ARC_SERIALIZABLE_H
#define ARC_SERIALIZABLE_H

#include "include.h"

namespace arc
{

class Stream;
class ByteStream;
class String;
class ByteString;

class Serializable
{
public:
	Serializable(void);
	virtual ~Serializable(void);

	virtual void serialize(Stream &s) const;
	virtual void deserialize(Stream &s);
	virtual void serializeBinary(ByteStream &s) const;
	virtual void deserializeBinary(ByteStream &s);

	virtual String toString(void) const;
	virtual ByteString toBinary(void) const;

	operator String(void) const;
	operator ByteString(void) const;

	virtual void html(Stream &s) const;
};

// WARNING: These operators are not symetric if toString is reimplemented
std::istream &operator>>(std::istream &is, Serializable &s);
std::ostream &operator<<(std::ostream &os, const Serializable &s);

}

#include "string.h"
#include "bytestring.h"

#endif
 
