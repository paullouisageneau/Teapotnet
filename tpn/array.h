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

#ifndef TPN_ARRAY_H
#define TPN_ARRAY_H

#include "tpn/include.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"

#include <vector>

namespace tpn
{

template<typename T>
class Array : public std::vector<T>
{
public:
	T *data(void);
	const T *data(void) const;

	void append(const T &value, int n = 1);
	void append(const Array<T> array);
	void append(const T *array, size_t size);
	void fill(const T &value, int n);
	void erase(int i);
	bool remove(const T &value);
	bool contains(const T &value);
};

template<typename T>
class SerializableArray : public Array<T>, public Serializable
{
public:
	class SerializableElement : public Serializer::Element
	{
	public:
		 T value;
		 
		 SerializableElement(void);
		 SerializableElement(const T &value);

		 void serialize(Serializer &s) const;
		 bool deserialize(Serializer &s);
	};
  
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isInlineSerializable(void) const;
};

typedef SerializableArray<String> StringArray;

template<typename T>
T *Array<T>::data(void)
{
	if(this->empty()) return NULL;
	else return &this->at(0);
}

template<typename T>
const T *Array<T>::data(void) const
{
	if(this->empty()) return NULL;
	else return &this->at(0);
}

template<typename T>
void Array<T>::append(const T &value, int n)
{
	this->reserve(this->size()+n);
	for(int i=0; i<n; ++i)
		this->push_back(value);
}

template<typename T>
void Array<T>::append(const Array<T> array)
{
	this->insert(this->end(),array.begin(),array.end());
}

template<typename T>
void Array<T>::append(const T *array, size_t size)
{
	this->insert(this->end(),array,array+size);
}

template<typename T>
void Array<T>::fill(const T &value, int n)
{
	this->assign(n,value);
}

template<typename T>
bool Array<T>::remove(const T &value)
{
	bool found = false;
	for(int i=0; i<this->size();)
		if(this->at(i) == value)
		{
			this->erase(i);
			found = true;
		}
		else ++i;
	return found;
}

template<typename T>
void Array<T>::erase(int i)
{
	std::vector<T>::erase(this->begin()+i);
}

template<typename T>
bool Array<T>::contains(const T &value)
{
	for(int i=0; i<this->size(); ++i)
		if(this->at(i) == value)
			return true;
	return false;
}

template<typename T>
void SerializableArray<T>::serialize(Serializer &s) const
{
	s.outputArrayBegin(this->size());

	for(size_t i=0; i<this->size(); ++i)
	{
		SerializableElement element(this->at(i));
		s.output(element);
	}
	
	s.outputArrayEnd();
}

template<typename T>
bool SerializableArray<T>::deserialize(Serializer &s)
{
	this->clear();
	if(!s.inputArrayBegin()) return false;

	while(s.inputArrayCheck())
	{
		SerializableElement element;
		if(!s.input(element)) break;
		this->append(element.value);
	}
	
	return true;
}

template<typename T>
bool SerializableArray<T>::isInlineSerializable(void) const
{
	return false;  	// recursive, no inlining
}

template<typename T>
SerializableArray<T>::SerializableElement::SerializableElement(void)
{
	 
}

template<typename T>
SerializableArray<T>::SerializableElement::SerializableElement(const T &value) :
	value(value)
{
	 
}

template<typename T>
void SerializableArray<T>::SerializableElement::serialize(Serializer &s) const
{
	s.output(value);
}

template<typename T>
bool SerializableArray<T>::SerializableElement::deserialize(Serializer &s)
{
	return s.input(value);
}

// Functions Serilizer::inputArrayElement and Serializer::outputArrayElement defined here

template<class T>
bool Serializer::inputArrayElement(T &element)
{
	 if(!this->inputArrayCheck()) return false;
	 typename SerializableArray<T>::SerializableElement tmp;
	 if(!this->input(tmp)) return false;
	 element = tmp.value;
	 return true;
}

template<class T> 
void Serializer::outputArrayElement(const T &element)
{
	typename SerializableArray<T>::SerializableElement tmp(element);
	this->output(tmp);
}

}

#endif
