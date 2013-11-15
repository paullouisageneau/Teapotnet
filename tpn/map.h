/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_MAP_H
#define TPN_MAP_H

#include "tpn/include.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"
#include "tpn/array.h"

#include <map>

namespace tpn
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
	bool getAndRemove(const K &key, V &value) const;
	const V &getOrDefault(const K &key, const V &defaultValue) const;
	
	int getKeys(Array<K> &array) const;
	int getValues(Array<V> &array) const;
};

template<typename K, typename V>
class SerializableMap : public Map<K,V>, public Serializable
{
public:
	class SerializablePair : public Serializer::Pair
	{
	public:
		K key;
		V value;
		bool isValid;
	  
		SerializablePair(void);
		SerializablePair(const K &key, const V &value);
		 
		void serializeKey(Serializer &s) const;
		void serializeValue(Serializer &s) const;
		bool deserializeKey(Serializer &s);
		bool deserializeValue(Serializer &s);
	};
	
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isInlineSerializable(void) const;
};

class StringMap : public SerializableMap<String,String>, public Serializer
{
public:
	bool input(Pair &pair);
	bool input(String &str);
	bool input(int8_t &i)		{ return inputAsString(i); }
	bool input(int16_t &i)		{ return inputAsString(i); }
	bool input(int32_t &i)		{ return inputAsString(i); }
	bool input(int64_t &i)		{ return inputAsString(i); }
	bool input(uint8_t &i)		{ return inputAsString(i); }
	bool input(uint16_t &i)		{ return inputAsString(i); }
	bool input(uint32_t &i)		{ return inputAsString(i); }
	bool input(uint64_t &i)		{ return inputAsString(i); }
	bool input(bool &b)		{ return inputAsString(b); }
	bool input(float &f)		{ return inputAsString(f); }
	bool input(double &f)		{ return inputAsString(f); }
	
	void output(const Pair &pair);
	void output(const String &str);
	void output(int8_t i)		{ outputAsString(i); }
	void output(int16_t i)		{ outputAsString(i); }
	void output(int32_t i)		{ outputAsString(i); }
	void output(int64_t i)		{ outputAsString(i); }
	void output(uint8_t i)		{ outputAsString(i); }
	void output(uint16_t i)		{ outputAsString(i); }
	void output(uint32_t i)		{ outputAsString(i); }
	void output(uint64_t i)		{ outputAsString(i); }
	void output(bool b)		{ outputAsString(b); }
	void output(float f)		{ outputAsString(f); }
	void output(double f)		{ outputAsString(f); }
	
private:
	template<typename T> bool inputAsString(T &v);
	template<typename T> void outputAsString(const T &v);
};

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
int Map<K,V>::getKeys(Array<K> &array) const
{
	array.clear();
	array.reserve(this->size());
	for(	typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		      array.push_back(it->first);
	}
	
	return array.size();
}

template<typename K, typename V>
int Map<K,V>::getValues(Array<V> &array) const
{
	array.clear();
	array.reserve(this->size());
	for(	typename std::map<K,V>::const_iterator it = this->begin();
		it != this->end();
		++it)
	{
		      array.push_back(it->second);
	}
	
	return array.size();
}

template<typename K, typename V>
void SerializableMap<K,V>::serialize(Serializer &s) const
{
	s.outputMapBegin(uint32_t(this->size()));

	for(	typename std::map<K,V>::const_iterator it = this->begin();
				it != this->end();
				++it)
	{
		SerializablePair pair(it->first, it->second);
		s.output(pair);
	}
	
	s.outputMapEnd();
}

template<typename K, typename V>
bool SerializableMap<K,V>::deserialize(Serializer &s)
{
	this->clear();
	if(!s.inputMapBegin()) return false;

	while(s.inputMapCheck())
	{
		SerializablePair pair;
		if(!s.input(pair)) break;
		if(!pair.isValid) continue;
		
		this->insert(pair.key, pair.value);
	}
	
	return true;
}

template<typename K, typename V>
bool SerializableMap<K,V>::isInlineSerializable(void) const
{
	return false; 	// recursive, no inlining
}

template<typename K, typename V>
SerializableMap<K,V>::SerializablePair::SerializablePair(void) :
	isValid(false)
{
  
}

template<typename K, typename V>
SerializableMap<K,V>::SerializablePair::SerializablePair(const K &key, const V &value) :
	key(key),
	value(value),
	isValid(true)
{
	  
}

template<typename K, typename V>
void SerializableMap<K,V>::SerializablePair::serializeKey(Serializer &s) const
{
	s.output(key);  
}

template<typename K, typename V>
void SerializableMap<K,V>::SerializablePair::serializeValue(Serializer &s) const
{
	s.output(value);
}

template<typename K, typename V>
bool SerializableMap<K,V>::SerializablePair::deserializeKey(Serializer &s)
{
	isValid = true;
	
	try {
		return s.input(key);
	}
	catch(const InvalidData &e)
	{
		isValid = false;
		return true;
	}
}

template<typename K, typename V>
bool SerializableMap<K,V>::SerializablePair::deserializeValue(Serializer &s)
{
	try {
		return s.input(value);
	}
	catch(const InvalidData &e)
	{
		isValid = false;
		return true;
	}
}

template<typename T> bool StringMap::inputAsString(T &v)
{
	String tmp;
	if(!input(tmp)) return false;
	tmp.read(v);
	return true;
}

template<typename T> void StringMap::outputAsString(const T &v)
{
	String tmp;
	tmp.write(v);
	output(tmp);
}

// Functions Serilizer::inputMapElement and Serializer::outputMapElement defined here

template<class K, class V> 
bool Serializer::inputMapElement(K &key, V &value)
{
	 if(!this->inputMapCheck()) return false;
	 typename SerializableMap<K,V>::SerializablePair tmp;
	 if(!this->input(tmp)) return false;
	 key = tmp.key;
	 value = tmp.value;
	 return true;  
}

template<class K, class V> 
void Serializer::outputMapElement(const K &key, const V &value)
{
  	typename SerializableMap<K,V>::SerializablePair tmp(key, value);
	this->output(tmp);
}

}

#endif
