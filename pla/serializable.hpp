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

#ifndef PLA_SERIALIZABLE_H
#define PLA_SERIALIZABLE_H

#include "pla/include.hpp"

namespace pla
{

class Stream;
class String;
class Serializer;

class Serializable
{
public:
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual void serialize(Stream &s) const;
	virtual bool deserialize(Stream &s);
	
	virtual bool isInlineSerializable(void) const;
	virtual bool isNativeSerializable(void) const;
	
	virtual String toString(void) const;
	virtual void fromString(const std::string &str);
	operator String(void) const;
	operator std::string(void) const;
};

std::ostream &operator<<(std::ostream &os, const Serializable &s);

}

#include "pla/serializer.hpp"

#endif
 
