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

#ifndef PLA_OBJECT_H
#define PLA_OBJECT_H

#include "pla/include.h"
#include "pla/map.h"
#include "pla/string.h"
#include "pla/serializable.h"

namespace pla
{

class Object : public SerializableMap<String, Serializable*>
{
public:
	Object(void);
	virtual ~Object(void);
	
	Object &insert(const String &key, Serializable *value);
	Object &insert(const String &key, Serializable &value);
	
	virtual bool deserialize(Serializer &s);
};

class ConstObject : public SerializableMap<String, const Serializable*>
{
public:
	ConstObject(void);
	virtual ~ConstObject(void);
	
	ConstObject &insert(const String &key, const Serializable *value);
	ConstObject &insert(const String &key, const Serializable &value);

	virtual bool deserialize(Serializer &s);
};

}

#endif
 
