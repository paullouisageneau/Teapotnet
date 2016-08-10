/*************************************************************************
 *   Copyright (C) 2011-2015 by Paul-Louis Ageneau                       *
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

#ifndef PLA_OBJECT_H
#define PLA_OBJECT_H

#include "pla/include.hpp"
#include "pla/serializable.hpp"

namespace pla
{

class Object : public std::map<std::string, Serializable>
{
public:
	template<typename T>
	class Wrapper : Serializable
	{
	public:
		Wrapper(T &&v) : value(v) {}
		void serialize(Serializer &s)	{ s << value; }
		bool deserialize(Serializer &s)	{ s >> value; }
		
	private:
		T &&value;
	};
	
	template<typename T> Object &insert(const std::string &key, T &&value)
	{
		emplace(key, Wrapper<T>(value));
	}
};

}

#endif
 
