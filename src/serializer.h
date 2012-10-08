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

#ifndef TPOT_SERIALIZER_H
#define TPOT_SERIALIZER_H

#include "include.h"
#include "serializable.h"

namespace tpot
{

class String;

class Serializer
{
public:
	Serializer(void);
	virtual ~Serializer(void);

	class Element
	{
	public:
		 virtual void serialize(Serializer &s) const = 0;
		 virtual bool deserialize(Serializer &s) = 0;
	};
	
	class Pair
	{
	public:
		 virtual void serializeKey(Serializer &s) const = 0;
		 virtual void serializeValue(Serializer &s) const = 0;
		 virtual bool deserializeKey(Serializer &s) = 0;
		 virtual bool deserializeValue(Serializer &s) = 0;
		 
		 void serialize(Serializer &s) const;
		 bool deserialize(Serializer &s);
	};
	
	virtual bool    input(Serializable &s);
	virtual bool	input(Element &element);
	virtual bool	input(Pair &pair);
	
	virtual bool    input(String &str) = 0;
	virtual bool	input(int8_t &i) = 0;
	virtual bool	input(int16_t &i) = 0;
	virtual bool	input(int32_t &i) = 0;
	virtual bool	input(int64_t &i) = 0;
	virtual bool	input(uint8_t &i) = 0;
	virtual bool	input(uint16_t &i) = 0;
	virtual bool	input(uint32_t &i) = 0;
	virtual bool	input(uint64_t &i) = 0;
	virtual bool	input(bool &b) = 0;
	virtual bool	input(float &f) = 0;
	virtual bool	input(double &f) = 0;

	virtual void    output(const Serializable &s);
	virtual void	output(const Element &element);
	virtual void	output(const Pair &pair);
	
	virtual void    output(const String &str) = 0;
	virtual void	output(int8_t i) = 0;
	virtual void	output(int16_t i) = 0;
	virtual void	output(int32_t i) = 0;
	virtual void	output(int64_t i) = 0;
	virtual void	output(uint8_t i) = 0;
	virtual void	output(uint16_t i) = 0;
	virtual void	output(uint32_t i) = 0;
	virtual void	output(uint64_t i) = 0;	
	virtual void	output(bool b) = 0;
	virtual void	output(float f) = 0;
	virtual void	output(double f) = 0;

	virtual bool	inputArrayBegin(void)		{ return true; }
	virtual bool	inputArrayCheck(void)		{ return true; }
	virtual bool	inputMapBegin(void)		{ return true; }
	virtual bool	inputMapCheck(void)		{ return true; }
	virtual void	outputArrayBegin(int size)	{}
	virtual void	outputArrayEnd(void)		{}
	virtual void	outputMapBegin(int size)	{}
	virtual void	outputMapEnd(void)		{}
	
	// Shortcuts replacing element output or check + element input
	template<class T>		bool inputArrayElement(T &element);
	template<class T> 		bool outputArrayElement(const T &element);
	template<class K, class V>	bool inputMapElement(K &key, V &value);
	template<class K, class V>	bool outputMapElement(const K &key, const V &value);
	
        template<class T> bool input(T *ptr);
	template<class T> void output(const T *ptr);
};


template<class T>
bool Serializer::input(T *ptr)
{
	return this->input(*ptr);
}

template<class T>
void Serializer::output(const T *ptr)
{
	this->output(*ptr);
}

// Functions inputArrayElement and outputArrayElement defined in array.h
// Functions inputMapElement and outputMapElement defined in map.h

}

#endif

