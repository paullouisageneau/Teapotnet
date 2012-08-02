/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef ARC_MAP_H
#define ARC_MAP_H

#include "include.h"
#include "exception.h"
#include "serializable.h"

#include <map>

namespace arc
{

template<typename K, typename V>
class Map : public std::map<K,V>, public Serializable
{
public:
	void insert(const K &key, const V &value);
	bool contains(const K &key);
	bool get(const K &key, V &value) const;
	const V &get(const K &key) const;
	V &get(const K &key);

	void serialize(Stream &s) const;
	void deserialize(Stream &s);
	void serializeBinary(ByteStream &s) const;
	void deserializeBinary(ByteStream &s);
};

template<typename K, typename V>
void Map<K,V>::insert(const K &key, const V &value)
{
	(*this)[key] = value;
}

template<typename K, typename V>
bool Map<K,V>::contains(const K &key)
{
	typename std::map<K,V>::iterator it = this->find(key);
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
void Map<K,V>::serialize(Stream &s) const
{
	for(	typename std::map<K,V>::const_iterator it = this->begin();
			it != this->end();
			++it)
	{
		s<<it->first<<','<<it->second<<' ';
	}
}

template<typename K, typename V>
void Map<K,V>::deserialize(Stream &s)
{
	this->clear();
	K key;
	V value;
	while(s.read(key))
	{
		assertIO(s.read(value));
		this->insert(key,value);
	}
}

template<typename K, typename V>
void Map<K,V>::serializeBinary(ByteStream &s) const
{
	for(	typename std::map<K,V>::const_iterator it = this->begin();
			it != this->end();
			++it)
	{
		s.writeBinary(it->first);
		s.writeBinary(it->second);
	}
}

template<typename K, typename V>
void Map<K,V>::deserializeBinary(ByteStream &s)
{
	this->clear();
	K key;
	V value;
	while(s.readBinary(key))
	{
		assertIO(s.readBinary(value))
		this->insert(key,value);
	}
}

}

#endif
