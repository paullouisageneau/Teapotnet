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

Serializer::Serializer(void)
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

bool Serializer::inputObject(ObjectMapping &mapping)
{
	if(!inputMapBegin()) return false;

	while(inputMapCheck())
		if(!input(mapping))
			break;

	return true;
}

void Serializer::outputObject(ConstObjectMapping &mapping)
{
	outputMapBegin(uint32_t(mapping.size()));

	for(ConstObjectMapping::const_iterator it = mapping.begin();
		it != mapping.end();
		++it)
	{
		SerializableMap<String, Serializable*>::SerializablePair pair(it->first, const_cast<Serializable*>(it->second));
		output(pair);
	}
	
	outputMapEnd();
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

Serializer::ObjectMapping::ObjectMapping(void) :
	mLastKey(new String)
{
	
}

Serializer::ObjectMapping::~ObjectMapping(void)
{
	delete mLastKey;
}

void Serializer::ObjectMapping::serializeKey(Serializer &s) const
{
	// This should not happen
	throw Exception("ObjectMapping should not be used for serializing");
}

void Serializer::ObjectMapping::serializeValue(Serializer &s) const
{
	// This should not happen
	throw Exception("ObjectMapping should not be used for serializing");
}

bool Serializer::ObjectMapping::deserializeKey(Serializer &s)
{
	return s.input(*mLastKey);
}

bool Serializer::ObjectMapping::deserializeValue(Serializer &s)
{
	ObjectMapping::iterator it = this->find(*mLastKey);
	if(it != this->end()) return s.input(*it->second);

	// Special case for id field (used by Database)
	if(*mLastKey == "id" || *mLastKey == "rowid")
	{
		int64_t dummy;
		return s.input(dummy);
	}
	else {
		//LogDebug("Serializer::ObjectMapping", String("Warning: Ignoring unknown entry: ") + *mLastKey);
		return s.skip();
	}
}

}

