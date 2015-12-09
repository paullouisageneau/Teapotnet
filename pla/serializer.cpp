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

#include "pla/serializer.h"
#include "pla/serializable.h"
#include "pla/exception.h"
#include "pla/string.h"
#include "pla/binarystring.h"
#include "pla/map.h"
#include "pla/array.h"
#include "pla/object.h"

namespace pla
{

Serializer::Serializer(void) :
	mOptionalOutputMode(false)
{
  
}

Serializer::~Serializer(void)
{
  
}
 
bool Serializer::input(Serializable &s)
{
	return s.deserialize(*this);
}

bool Serializer::input(const Serializable &s)
{
	// Should not happen
	throw Unsupported("Serializer input called with constant object");
}

bool Serializer::input(Element &element)
{
	return element.deserialize(*this); 
}

bool Serializer::input(Pair &pair)
{
	return pair.deserialize(*this);
}

void Serializer::output(const Serializable &s)
{
        s.serialize(*this);
}

void Serializer::output(const Element &element)
{
        element.serialize(*this); 
}

void Serializer::output(const Pair &pair)
{
        pair.serialize(*this);  
}

bool Serializer::skip(void)
{
	String dummy;
	return input(dummy);
}

bool Serializer::optionalOutputMode(void) const
{
	return mOptionalOutputMode;  
}

void Serializer::setOptionalOutputMode(bool enabled)
{
	mOptionalOutputMode = enabled; 
}

bool Serializer::inputObject(Object &object)
{
	return input(object);
}

void Serializer::outputObject(ConstObject &object)
{
	output(object);
}

void Serializer::Pair::serialize(Serializer &s) const
{
	serializeKey(s);
	serializeValue(s);
}
		 
bool Serializer::Pair::deserialize(Serializer &s)
{
	if(!deserializeKey(s)) return false;
	return deserializeValue(s);  
}

}

