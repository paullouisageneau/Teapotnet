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

#include "tpn/serializer.h"
#include "tpn/serializable.h"
#include "tpn/exception.h"
#include "tpn/map.h"
#include "tpn/array.h"

namespace tpn
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

