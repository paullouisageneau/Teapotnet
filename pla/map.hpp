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

#ifndef PLA_MAP_H
#define PLA_MAP_H

#include "pla/include.hpp"
#include "pla/exception.hpp"
#include "pla/serializable.hpp"

#include <map>

namespace pla
{

template<typename K, typename V>
class Map : public std::map<K,V>
{
public:
	typename std::map<K,V>::iterator insert(const K &key, const V &value);
	void insert(const std::map<K,V> &other);
	void insertAll(const std::map<K,V> &other) { this->insert(other); }
	bool contains(const K &key) const;
	bool get(const K &key, V &value) const;
	const V &get(const K &key) const;
	V &get(const K &key);
	bool getAndRemove(const K &key, V &value) const;
	const V &getOrDefault(const K &key, const V &defaultValue) const;

	int getKeys(std::set<K> &set) const;
	int getKeys(std::vector<K> &array) const;
	int getKeys(std::list<K> &list) const;
	int getValues(std::vector<V> &array) const;
	int getValues(std::list<V> &array) const;
};

class String;
typedef Map<String, String> StringMap;

template<typename K, typename V>
typename std::map<K,V>::iterator Map<K,V>::insert(const K &key, const V &value)
{
	return std::map<K,V>::insert(std::pair<K,V>(key, value)).first;
}

template<typename K, typename V>
void Map<K,V>::insert(const std::map<K,V> &other)
{
	for(typename std::map<K,V>::const_iterator it = other.begin();
		it != other.end();
		++it)
	{
		insert(it->first, it->second);
	}
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
	if(it == this->end())  throw OutOfBounds("Map key does not exist");
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
bool Map<K,V>::getAndRemove(const K &key, V &value) const
{
	if(!this->get(key, value)) return false;
	this->erase(key);
	return true;
}

template<typename K, typename V>
const V &Map<K,V>::getOrDefault(const K &key, const V &defaultValue) const
{
	typename std::map<K,V>::const_iterator it = this->find(key);
	if(it == this->end()) return defaultValue;
	return it->second;
}

template<typename K, typename V>
int Map<K,V>::getKeys(std::set<K> &set) const
{
	set.clear();
	for(typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		set.insert(it->first);
	}

	return set.size();
}

template<typename K, typename V>
int Map<K,V>::getKeys(std::vector<K> &array) const
{
	array.clear();
	array.reserve(this->size());
	for(typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		array.push_back(it->first);
	}

	return array.size();
}

template<typename K, typename V>
int Map<K,V>::getKeys(std::list<K> &list) const
{
	list.clear();
	for(typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		list.push_back(it->first);
	}

	return list.size();
}

template<typename K, typename V>
int Map<K,V>::getValues(std::vector<V> &array) const
{
	array.clear();
	array.reserve(this->size());
	for(typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		array.push_back(it->second);
	}

	return array.size();
}

template<typename K, typename V>
int Map<K,V>::getValues(std::list<V> &list) const
{
	list.clear();
	for(typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		list.push_back(it->second);
	}

	return list.size();
}

}

#endif
