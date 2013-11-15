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

#ifndef TPN_LIST_H
#define TPN_LIST_H

#include "tpn/include.h"
#include "tpn/exception.h"

#include <list>

namespace tpn
{

template<typename T>
class List : public std::list<T>
{
public:
	bool remove(const T &value);
	bool contains(const T &value) const;
};

template<typename T>
class SerializableList : public List<T>, public Serializable
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

typedef SerializableList<String> StringList;


template<typename T>
bool List<T>::remove(const T &value)
{
	bool found = false;
	typename List<T>::iterator it = this->begin();
	while(it != this->end())
		  if(*it == value) 
		  {
			  this->erase(it++);
			  found = true;
		  }
		  else ++it;
	return found;
}

template<typename T>
bool List<T>::contains(const T &value) const
{
	for(typename List<T>::iterator it = this->begin(); it != this->end(); ++it)
		  if(*it == value)
			  return true;
	
	return false;
}

template<typename T>
void SerializableList<T>::serialize(Serializer &s) const
{
	s.outputArrayBegin(this->size());

	for(typename List<T>::const_iterator it = this->begin(); it != this->end(); ++it)
	{
		SerializableElement element(*it);
		s.output(element);
	}
	
	s.outputArrayEnd();
}

template<typename T>
bool SerializableList<T>::deserialize(Serializer &s)
{
	this->clear();
	if(!s.inputArrayBegin()) return false;

	while(s.inputArrayCheck())
	{
		SerializableElement element;
		if(!s.input(element)) break;
		this->push_back(element.value);
	}
	
	return true;
}

template<typename T>
bool SerializableList<T>::isInlineSerializable(void) const
{
	return false;  	// recursive, no inlining
}

template<typename T>
SerializableList<T>::SerializableElement::SerializableElement(void)
{
	 
}

template<typename T>
SerializableList<T>::SerializableElement::SerializableElement(const T &value) :
	value(value)
{
	 
}

template<typename T>
void SerializableList<T>::SerializableElement::serialize(Serializer &s) const
{
	s.output(value);
}

template<typename T>
bool SerializableList<T>::SerializableElement::deserialize(Serializer &s)
{
	return s.input(value);
}

}

#endif
