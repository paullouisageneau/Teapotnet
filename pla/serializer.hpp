/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#ifndef PLA_SERIALIZER_H
#define PLA_SERIALIZER_H

#include "pla/include.hpp"
#include "pla/serializable.hpp"
#include "pla/exception.hpp"

namespace pla
{

class String;
class BinaryString;

class Serializer
{
public:
	template<typename T> Serializer &operator<< (const T& value);
	template<typename T> Serializer &operator>> (T& value);
	bool operator!(void) const { return mReadFailed; }

	virtual bool skip(void);

	bool optionalOutputMode(void) const { return mOptionalOutput; }
	void setOptionalOutputMode(bool enabled = true) { mOptionalOutput = enabled; }

protected:
	template<class T> bool read(T *&ptr);
	template<class T> void write(const T *ptr);

	template<class T> bool read(sptr<T> &ptr);
	template<class T> void write(sptr<T> ptr);
	template<class T> void write(sptr<const T> ptr);

	template<typename T> bool read(std::vector<T> &container);
	template<typename T> void write(const std::vector<T> &container);

	template<typename T> bool read(std::list<T> &container);
	template<typename T> void write(const std::list<T> &container);

	template<typename T> bool read(std::set<T> &container);
	template<typename T> void write(const std::set<T> &container);

	template<typename K, typename V> bool read(std::map<K, V> &container);
	template<typename K, typename V> void write(const std::map<K, V> &container);

	template<typename K, typename V> bool read(std::pair<K, V> &pair);
	template<typename K, typename V> void write(const std::pair<K, V> &pair);

	template<typename Iterator> bool read(Iterator begin, Iterator end);
	template<typename Iterator> void write(Iterator begin, Iterator end);

	virtual bool	read(Serializable &s);
	virtual void	write(const Serializable &s);
	virtual bool	read(String &s);
	virtual void	write(const String &s);
	virtual bool	read(BinaryString &s);
	virtual void	write(const BinaryString &s);

	virtual bool	read(std::string &str) = 0;
	virtual bool	read(uint8_t &i);
	virtual bool	read(uint16_t &i);
	virtual bool	read(uint32_t &i);
	virtual bool	read(uint64_t &i);
	virtual bool	read(int8_t &i);
	virtual bool	read(int16_t &i);
	virtual bool	read(int32_t &i);
	virtual bool	read(int64_t &i)	= 0;
	virtual bool	read(float &f);
	virtual bool	read(double &f)		= 0;
	virtual bool	read(bool &b);

	virtual void	write(const std::string &str) = 0;
	virtual void	write(uint8_t i);
	virtual void	write(uint16_t i);
	virtual void	write(uint32_t i);
	virtual void	write(uint64_t i);
	virtual void	write(int8_t i);
	virtual void	write(int16_t i);
	virtual void	write(int32_t i);
	virtual void	write(int64_t i)	= 0;
	virtual void	write(float f);
	virtual void	write(double f)		= 0;
	virtual void	write(bool b);

	virtual bool	readArrayBegin(void)	{ return true; }
	virtual bool	readArrayNext(void)		{ return true; }
	virtual bool	readMapBegin(void)		{ return true; }
	virtual bool	readMapNext(void)		{ return true; }

	virtual void	writeArrayBegin(size_t size)	{}
	virtual void	writeArrayNext(size_t i)		{}
	virtual void	writeArrayEnd(void)			{}
	virtual void	writeMapBegin(size_t size)	{}
	virtual void	writeMapNext(size_t i)		{}
	virtual void	writeMapEnd(void)		{}
	virtual void	writeEnd(void)			{}

private:
	template<typename T> bool readElement(T &element, size_t i);
	template<typename T> void writeElement(const T &element, size_t i, size_t size);
	template<typename K, typename V> bool readElement(std::pair<K, V> &pair, size_t i);
	template<typename K, typename V> void writeElement(const std::pair<K, V> &pair, size_t i, size_t size);

	bool mReadFailed = false;
	bool mOptionalOutput = false;
};

template<typename T>
Serializer &Serializer::operator>> (T& value)
{
	mReadFailed = !read(value);
	return *this;
}

template<typename T>
Serializer &Serializer::operator<< (const T& value)
{
	write(value);
	return *this;
}

template<class T>
bool Serializer::read(T *&ptr)
{
	if(!ptr) ptr = new T();
	return read(*ptr);
}

template<class T>
void Serializer::write(const T *ptr)
{
	write(*ptr);
}

template<class T>
bool Serializer::read(sptr<T> &ptr)
{
	if(!ptr) ptr = std::make_shared<T>();
	return read(*ptr);
}

template<class T>
void Serializer::write(sptr<T> ptr)
{
	write(*ptr);
}

template<class T>
void Serializer::write(sptr<const T> ptr)
{
	write(*ptr);
}

template<typename T>
bool Serializer::read(std::vector<T> &container)
{
	container.clear();
	if(!readArrayBegin()) return false;
	while(readArrayNext())
	{
		T v;
		if(!read(v)) break;
		container.emplace(container.end(), v);
	}
	return true;
}

template<typename T>
void Serializer::write(const std::vector<T> &container)
{
	writeArrayBegin(container.size());
	size_t i = 0;
	for(const T &v: container)
	{
		writeArrayNext(i++);
		write(v);
	}
	writeArrayEnd();
}

template<typename T>
bool Serializer::read(std::list<T> &container)
{
	container.clear();
	if(!readArrayBegin()) return false;
	while(readArrayNext())
	{
		T v;
		if(!read(v)) break;
		container.emplace(container.end(), v);
	}
	return true;
}

template<typename T>
void Serializer::write(const std::list<T> &container)
{
	writeArrayBegin(container.size());
	size_t i = 0;
	for(const T &v: container)
	{
		writeArrayNext(i++);
		write(v);
	}
	writeArrayEnd();
}

template<typename T>
bool Serializer::read(std::set<T> &container)
{
	container.clear();
	if(!readArrayBegin()) return false;
	while(readArrayNext())
	{
		T v;
		if(!read(v)) break;
		container.emplace(v);
	}
	return true;
}

template<typename T>
void Serializer::write(const std::set<T> &container)
{
	writeArrayBegin(container.size());
	size_t i = 0;
	for(const T &v: container)
	{
		writeArrayNext(i++);
		write(v);
	}
	writeArrayEnd();
}

template<typename K, typename V>
bool Serializer::read(std::map<K, V> &container)
{
	if(!readMapBegin())
	{
		container.clear();
		return false;
	}

	std::set<K> keys;
	while(readMapNext())
	{
		std::pair<K, V> p;
		if(!read(p.first)) break;
		keys.insert(p.first);

		auto it = container.find(p.first); // check if key already exists
		if(it != container.end())
		{
			AssertIO(read(it->second));
		}
		else {
			AssertIO(read(p.second));
			container.emplace(p);
		}
	}

	auto it = container.begin();
	while(it != container.end())
	{
		if(keys.find(it->first) != keys.end()) ++it;
		else it = container.erase(it);
	}

	return true;
}

template<typename K, typename V>
void Serializer::write(const std::map<K, V> &container)
{
	writeMapBegin(container.size());
	size_t i = 0;
	for(const std::pair<K, V> &p: container)
	{
		writeMapNext(i++);
		write(p);
	}
	writeMapEnd();
}

template<typename K, typename V>
bool Serializer::read(std::pair<K, V> &pair)
{
	if(!readMapNext()) return false;
	if(!read(pair.first)) return false;
	AssertIO(read(pair.second));
	return true;
}

template<typename K, typename V>
void Serializer::write(const std::pair<K, V> &pair)
{
	write(pair.first);
	write(pair.second);
}

template<typename Iterator>
bool Serializer::read(Iterator begin, Iterator end)
{
	size_t i = 0;
	for(Iterator it = begin; it != end; ++it)
		AssertIO(readElement(*it, i++));
	return true;
}

template<typename Iterator>
void Serializer::write(Iterator begin, Iterator end)
{
	size_t size = 0;
	for(Iterator it = begin; it != end; ++it)
		++size;
	size_t i = 0;
	for(Iterator it = begin; it != end; ++it)
		writeElement(*it, i++, size);
}

template<typename T> bool Serializer::readElement(T &element, size_t i)
{
	if(i == 0 && !readArrayBegin()) return false;
	if(!readArrayNext()) return false;
	if(!read(element)) return false;
	return true;
}

template<typename T> void Serializer::writeElement(const T &element, size_t i, size_t size)
{
	if(i == 0) writeArrayBegin(size);
	writeArrayNext(i);
	write(element);
	if(i+1 == size) writeArrayEnd();
}

template<typename K, typename V> bool Serializer::readElement(std::pair<K, V> &pair, size_t i)
{
	if(i == 0 && !readMapBegin()) return false;
	if(!readMapNext()) return false;
	if(!read(pair.first)) return false;
	AssertIO(read(pair.second));
	return true;
}

template<typename K, typename V> void Serializer::writeElement(const std::pair<K, V> &pair, size_t i, size_t size)
{
	if(i == 0) writeMapBegin(size);
	writeMapNext(i);
	write(pair.first);
	write(pair.second);
	if(i+1 == size) writeMapEnd();
}

}

#endif
