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

#ifndef ARC_ARRAY_H
#define ARC_ARRAY_H

#include "include.h"
#include "exception.h"
#include "serializable.h"

#include <vector>

namespace arc
{

template<typename T>
class Array : public std::vector<T>, public Serializable
{
public:
	T *data(void);
	const T *data(void) const;

	void append(const T &value, int n = 1);
	void append(const Array<T> array);
	void append(const T *array, int size);
	void fill(const T &value, int n);

	bool remove(const T &value);

	void serialize(Stream &s) const;
	void deserialize(Stream &s);
	void serializeBinary(ByteStream &s) const;
	void deserializeBinary(ByteStream &s);
};

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
void Array<T>::append(const T *array, int size)
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
void Array<T>::serialize(Stream &s) const
{
	for(int i=0; i<this->size(); ++i)
		s<<this->at(i)<<' ';
}

template<typename T>
void Array<T>::deserialize(Stream &s)
{
	this->clear();
	T tmp;
	while(s.read(tmp))
		this->push_back(tmp);
}

template<typename T>
void Array<T>::serializeBinary(ByteStream &s) const
{
	for(int i=0; i<this->size(); ++i)
		s.writeBinary(this->at(i));
}

template<typename T>
void Array<T>::deserializeBinary(ByteStream &s)
{
	this->clear();
	T tmp;
	while(s.readBinary(tmp))
		this->push_back(tmp);
}

}

#endif
