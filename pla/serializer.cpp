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

bool Serializer::inputObject(Object &object)
{
	if(!inputMapBegin()) return false;

	while(inputMapCheck())
		if(!input(object))
			break;

	return true;
}

void Serializer::outputObject(ConstObject &object)
{
	outputMapBegin(uint32_t(object.size()));

	for(ConstObject::const_iterator it = object.begin();
		it != object.end();
		++it)
	{
		SerializableMap<String, Serializable*>::SerializablePair pair(it->first, const_cast<Serializable*>(it->second));
		output(pair);
	}
	
	outputMapEnd();
}

bool Serializer::optionalOutputMode(void) const
{
	return mOptionalOutputMode;  
}

void Serializer::setOptionalOutputMode(bool enabled)
{
	mOptionalOutputMode = enabled; 
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

Serializer::Object::Object(void) :
	mLastKey(new String)
{
	
}

Serializer::Object::~Object(void)
{
	delete mLastKey;
}

Serializer::ConstObject::ConstObject(void)
{
	
}

Serializer::ConstObject::~ConstObject(void)
{
	
}

Serializer::ConstObject &Serializer::ConstObject::insert(const String &key, const Serializable *value)
{
	(*this)[key] = value;
	return *this;
}

void Serializer::Object::serializeKey(Serializer &s) const
{
	// This should not happen
	throw Exception("Object should not be used for serializing");
}

void Serializer::Object::serializeValue(Serializer &s) const
{
	// This should not happen
	throw Exception("Object should not be used for serializing");
}

bool Serializer::Object::deserializeKey(Serializer &s)
{
	return s.input(*mLastKey);
}

bool Serializer::Object::deserializeValue(Serializer &s)
{
	Object::iterator it = this->find(*mLastKey);
	if(it != this->end()) return s.input(*it->second);

	// Special case for id field (used by Database)
	if(*mLastKey == "id" || *mLastKey == "rowid")
	{
		int64_t dummy;
		return s.input(dummy);
	}
	else {
		//LogDebug("Serializer::Object", String("Warning: Ignoring unknown entry: ") + *mLastKey);
		return s.skip();
	}
}

}

