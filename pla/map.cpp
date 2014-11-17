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

#include "pla/map.h"
#include "pla/lineserializer.h"
#include "pla/jsonserializer.h"

namespace pla
{

bool StringMap::input(Pair &pair)
{
	if(this->empty()) return false;
	StringMap::iterator it = this->begin();
	
	String key = it->first;
	LineSerializer keySerializer(&key);
	AssertIO(pair.deserializeKey(keySerializer));
	
	JsonSerializer valueSerializer(&it->second);
	AssertIO(pair.deserializeValue(valueSerializer));
	
	this->erase(it);
	return true;
}

bool StringMap::input(String &str)
{
	if(this->empty()) return false;
	StringMap::iterator it = this->begin();
	str = it->second;
	this->erase(it);
	return true;
}

void StringMap::output(const Pair &pair)
{
	String key;
	LineSerializer keySerializer(&key);
	pair.serializeKey(keySerializer);
	key.trim();
	
	String value;
	JsonSerializer valueSerializer(&value);
	pair.serializeValue(valueSerializer);
	
	this->insert(key, value);
}

void StringMap::output(const String &str)
{
	int key = this->size();
	while(this->contains(String::number(key))) ++key;
	this->insert(String::number(key), str);
}

}
