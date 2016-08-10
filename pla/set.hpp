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

#ifndef PLA_SET_H
#define PLA_SET_H

#include "pla/include.hpp"
#include "pla/exception.hpp"

#include <set>

namespace pla
{

template<typename T>
class Set : public std::set<T>
{
public:
	bool remove(const T &value);
	bool contains(const T &value) const;
	void insertAll(const Set<T> &set);
};

class String;
typedef Set<String> StringSet;

template<typename T>
bool Set<T>::remove(const T &value)
{
	return (this->erase(value) == 1);
}

template<typename T>
bool Set<T>::contains(const T &value) const
{
	return (this->find(value) != this->end());
}

template<typename T>
void Set<T>::insertAll(const Set<T> &set)
{
	for(typename Set<T>::const_iterator it = set.begin(); it != set.end(); ++it)
		this->insert(*it);
}

}

#endif
