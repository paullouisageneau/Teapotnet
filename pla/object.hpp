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

class Object : public std::map<std::string, sptr<Serializable> >, public Serializable
{
private:
	// Wrapper keeping a non-const reference
	template<typename T>
	class Wrapper : public Serializable
	{
	public:
		Wrapper(T &v) : value(&v) {}
		void serialize(Serializer &s) const { s << *value; }
		bool deserialize(Serializer &s) { return s >> *value; }

	private:
		T *value;
	};

	// Wrapper keeping a const reference
	template<typename T>
	class ConstWrapper : public Serializable
	{
	public:
		ConstWrapper(const T &v) : value(&v) {}
		void serialize(Serializer &s) const { s << *value; }
		bool deserialize(Serializer &s) { throw RuntimeException("deserialize on const object");}

	private:
		const T *value;
	};

	// Wrapper copying the value
	template<typename T>
	class CopyWrapper : public Serializable
	{
	public:
		CopyWrapper(T &&v) : value(v) {}	// copy
		void serialize(Serializer &s) const { s << value; }
		bool deserialize(Serializer &s) { throw RuntimeException("deserialize on temp object"); }

	private:
		T value;
	};

public:
	template<typename T> Object &insert(const std::string &key, T &value)
	{
		emplace(key, std::make_shared<Wrapper<T> >(value));
		return *this;
	}

	template<typename T> Object &insert(const std::string &key, const T &value, bool cond = true)
	{
		if(cond) emplace(key, std::make_shared<ConstWrapper<T> >(value));
		return *this;
	}

	template<typename T> Object &insert(const std::string &key, T &&value, bool cond = true)
	{
		if(cond) emplace(key, std::make_shared<CopyWrapper<T> >(std::forward<T>(value)));
		return *this;
	}

	void serialize(Serializer &s) const
	{
		s << *static_cast<const std::map<std::string, sptr<Serializable> >*>(this);
	}

	bool deserialize(Serializer &s)
	{
		return s >> *static_cast<std::map<std::string, sptr<Serializable> >*>(this);
	}

	bool isInlineSerializable(void) const
	{
		return false;
	}

	bool isNativeSerializable(void) const
	{
		return false;
	}
};

}

#endif
