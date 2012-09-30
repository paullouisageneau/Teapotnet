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

#ifndef TPOT_MAP_H
#define TPOT_MAP_H

#include "include.h"
#include "exception.h"
#include "serializable.h"

#include <map>

namespace tpot
{

template<typename K, typename V>
class Map : public std::map<K,V>
{
public:
	void insert(const K &key, const V &value);
	bool contains(const K &key) const;
	bool get(const K &key, V &value) const;
	const V &get(const K &key) const;
	V &get(const K &key);
};

template<typename K, typename V>
class SerializableMap : public Map<K,V>, public Serializable
{
	void serialize(Stream &s) const;
	void deserialize(Stream &s);
	void serializeBinary(ByteStream &s) const;
	void deserializeBinary(ByteStream &s);
};

typedef SerializableMap<String,String> StringMap;

template<typename K, typename V>
void Map<K,V>::insert(const K &key, const V &value)
{
	(*this)[key] = value;
}

template<typename K, typename V>
bool Map<K,V>::contains(const K &key) const
{
	typename std::map<K,V>::const_iterator it = this->find(key);
	return (it != this->end());
}

template<typename K, typename V>
bool Map<K,V>::get(const K &key, V &value) const
{
	typename std::map<K,V>::const_iterator it = this->find(key);
	if(it == this->end()) return false;
	value = it->second;
	return true;
}

template<typename K, typename V>
const V &Map<K,V>::get(const K &key) const
{
	typename std::map<K,V>::const_iterator it = this->find(key);
	if(it == this->end()) throw OutOfBounds("Map key does not exist");
	return it->second;
}

template<typename K, typename V>
V &Map<K,V>::get(const K &key)
{
	typename std::map<K,V>::iterator it = this->find(key);
	if(it == this->end()) throw OutOfBounds("Map key does not exist");
	return it->second;
}

template<typename K, typename V>
void SerializableMap<K,V>::serialize(Stream &s) const
{
	for(	typename std::map<K,V>::const_iterator it = this->begin();
			it != this->end();
			++it)
	{
		s<<it->first<<"="<<it->second<<Stream::NewLine;
	}

	s<<Stream::NewLine;
}

template<typename K, typename V>
void SerializableMap<K,V>::deserialize(Stream &s)
{
	this->clear();

	String line;
	while(s.readLine(line))
	{
	  	if(line.empty()) return;
	  
		String second = line.cut('=');
		K key;
		V value;
		line.readLine(key);
		second.readLine(value);
		this->insert(key,value);
	}
	
	if(this->empty()) throw IOException();
}

template<typename K, typename V>
void SerializableMap<K,V>::serializeBinary(ByteStream &s) const
{
	s.writeBinary(uint32_t(this->size()));

	for(	typename std::map<K,V>::const_iterator it = this->begin();
				it != this->end();
				++it)
	{
		s.writeBinary(it->first);
		s.writeBinary(it->second);
	}
}

template<typename K, typename V>
void SerializableMap<K,V>::deserializeBinary(ByteStream &s)
{
	this->clear();
	uint32_t size;
	AssertIO(s.readBinary(size));

	for(uint32_t i=0; i<size; ++i)
	{
		K key;
		V value;
		AssertIO(s.readBinary(key));
		AssertIO(s.readBinary(value));
		this->insert(key,value);
	}
}

}

#endif
