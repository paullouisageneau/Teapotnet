/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/map.h"
#include "tpn/lineserializer.h"
#include "tpn/yamlserializer.h"

namespace tpn
{

bool StringMap::input(Pair &pair)
{
	if(this->empty()) return false;
	StringMap::iterator it = this->begin();
	
	String key = it->first;
	LineSerializer keySerializer(&key);
	AssertIO(pair.deserializeKey(keySerializer));
	
	YamlSerializer valueSerializer(&it->second, 1);
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
	YamlSerializer valueSerializer(&value, 1);
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
