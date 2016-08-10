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

#ifndef PLA_ARRAY_H
#define PLA_ARRAY_H

#include "pla/include.hpp"
#include "pla/exception.hpp"

#include <vector>

namespace pla
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
	void prepend(const T &value, int n = 1);
	void fill(const T &value, int n);
	void erase(int i, int n = 1);
	bool remove(const T &value);
	bool contains(const T &value) const;
	void reverse(void);
};

class String;
typedef Array<String> StringArray;

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
void Array<T>::prepend(const T &value, int n)
{
	this->insert(this->begin(), n, value);
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
void Array<T>::erase(int i, int n)
{
	std::vector<T>::erase(this->begin()+i, this->begin()+i+n);
}

template<typename T>
bool Array<T>::contains(const T &value) const
{
	for(int i=0; i<this->size(); ++i)
		if(this->at(i) == value)
			return true;
	return false;
}

template<typename T>
void Array<T>::reverse(void)
{
	int n = this->size()/2;
	for(int i=0; i<n; ++i)
		std::swap(this->at(i), this->at(this->size()-1-i));
}

}

#endif
