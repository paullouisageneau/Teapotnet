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

#ifndef PLA_LIST_H
#define PLA_LIST_H

#include "pla/include.hpp"
#include "pla/exception.hpp"

#include <list>

namespace pla
{

template<typename T>
class List : public std::list<T>
{
public:
	bool remove(const T &value);
	bool contains(const T &value) const;
};

class String;
typedef List<String> StringList;

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

}

#endif
