/*************************************************************************
 *   Copyright (C) 2011-2015 by Paul-Louis Ageneau                       *
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

#include "pla/object.h"
#include "pla/exception.h"
#include "pla/serializer.h"

namespace pla
{

Object::Object(void)
{

}

Object::~Object(void)
{

}

Object &Object::insert(const String &key, Serializable *value)
{
	SerializableMap<String, Serializable*>::insert(key, value);
	return *this;
}

Object &Object::insert(const String &key, Serializable &value)
{
	SerializableMap<String, Serializable*>::insert(key, &value);
	return *this;
}

bool Object::deserialize(Serializer &s)
{
	class Pair : public Serializer::Pair
	{
	public:
		Pair(Object *obj) { this->obj = obj; }
		
		void serializeKey(Serializer &s) const {}	// Dummy
		void serializeValue(Serializer &s) const {}	// Dummy
		 
		bool deserializeKey(Serializer &s)
		{
			 return s.input(lastKey);
		}
		 
		bool deserializeValue(Serializer &s)
		{
			Object::iterator it = obj->find(lastKey);
			if(it != obj->end()) return s.input(*it->second);

			// Special case for id field (used by Database)
			if(lastKey == "id" || lastKey == "rowid")
			{
				int64_t dummy;
				return s.input(dummy);
			}
			else {
				return s.skip();
			}
		}
		 
	private:
		Object *obj;
		String lastKey;
	};
	
	
	if(!s.inputMapBegin()) return false;
	
	Pair pair(this);
	while(s.inputMapCheck())
		if(!s.input(pair))
			break;

	return true;
}
	
ConstObject::ConstObject(void)
{

}

ConstObject::~ConstObject(void)
{

}

ConstObject &ConstObject::insert(const String &key, const Serializable *value)
{
	SerializableMap<String, const Serializable*>::insert(key, value);
	return *this;
}

ConstObject &ConstObject::insert(const String &key, const Serializable &value)
{
	SerializableMap<String, const Serializable*>::insert(key, &value);
	return *this;
}

bool ConstObject::deserialize(Serializer &s)
{
	throw Unsupported("Deserialize on const object");
	return false;
}

}
