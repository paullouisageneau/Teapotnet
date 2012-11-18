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

#ifndef TPOT_JSONSERIALIZER_H
#define TPOT_JSONSERIALIZER_H

#include "serializer.h"
#include "stream.h"
#include "array.h"

namespace tpot
{

class JsonSerializer : public Serializer
{
public:
	JsonSerializer(Stream *stream);	// stream WON'T be destroyed
	virtual ~JsonSerializer(void);
	
	bool	input(Serializable &s);
	bool	input(Element &element);
	bool	input(Pair &pair);
	bool	input(String &str);
	
	void	output(const Serializable &s);
	void	output(const Element &element);
	void	output(const Pair &pair);
	void	output(const String &str);
	
	inline bool	input(int8_t &i)	{ return read(i); }
	inline bool	input(int16_t &i)	{ return read(i); }
	inline bool	input(int32_t &i)	{ return read(i); }
	inline bool	input(int64_t &i)	{ return read(i); }
	inline bool	input(uint8_t &i)	{ return read(i); }
	inline bool	input(uint16_t &i)	{ return read(i); }
	inline bool	input(uint32_t &i)	{ return read(i); }
	inline bool	input(uint64_t &i)	{ return read(i); }
	inline bool	input(bool &b)		{ return read(b); }
	inline bool	input(float &f)		{ return read(f); }
	inline bool	input(double &f)	{ return read(f); }

	inline void	output(int8_t i)	{ write(i); }
	inline void	output(int16_t i)	{ write(i); }
	inline void	output(int32_t i)	{ write(i); }
	inline void	output(int64_t i)	{ write(i); }
	inline void	output(uint8_t i)	{ write(i); }
	inline void	output(uint16_t i)	{ write(i); }
	inline void	output(uint32_t i)	{ write(i); }
	inline void	output(uint64_t i)	{ write(i); }
	inline void	output(bool b)		{ write(b); }
	inline void	output(float f)		{ write(f); }
	inline void	output(double f)	{ write(f); }
	
	bool	inputArrayBegin(void);
	bool	inputArrayCheck(void);
	bool	inputMapBegin(void);
	bool	inputMapCheck(void);
	
	void	outputArrayBegin(int size = 0);
	void	outputArrayEnd(void);
	void	outputMapBegin(int size = 0);
	void	outputMapEnd(void);
	
private:
  	template<typename T> bool read(T &value);
  	template<typename T> void write(const T &value);
  
  	Stream *mStream;
	int mLevel;
	bool mFirst;
};

template<typename T> 
bool JsonSerializer::read(T &value)
{
	return mStream->read(value);
}

template<typename T> 
void JsonSerializer::write(const T &value)
{
	mStream->space();
	mStream->write(value);
	if(mLevel == 0) *mStream << Stream::NewLine;	
}

}

#endif

