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

#ifndef PLA_JSONSERIALIZER_H
#define PLA_JSONSERIALIZER_H

#include "pla/serializer.h"
#include "pla/stream.h"
#include "pla/array.h"

namespace pla
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
	
	inline bool	input(int8_t &i)	{ return readValue(i); }
	inline bool	input(int16_t &i)	{ return readValue(i); }
	inline bool	input(int32_t &i)	{ return readValue(i); }
	inline bool	input(int64_t &i)	{ return readValue(i); }
	inline bool	input(uint8_t &i)	{ return readValue(i); }
	inline bool	input(uint16_t &i)	{ return readValue(i); }
	inline bool	input(uint32_t &i)	{ return readValue(i); }
	inline bool	input(uint64_t &i)	{ return readValue(i); }
	inline bool	input(bool &b)		{ return readValue(b); }
	inline bool	input(float &f)		{ return readValue(f); }
	inline bool	input(double &f)	{ return readValue(f); }

	inline void	output(int8_t i)	{ writeValue(i); }
	inline void	output(int16_t i)	{ writeValue(i); }
	inline void	output(int32_t i)	{ writeValue(i); }
	inline void	output(int64_t i)	{ writeValue(i); }
	inline void	output(uint8_t i)	{ writeValue(i); }
	inline void	output(uint16_t i)	{ writeValue(i); }
	inline void	output(uint32_t i)	{ writeValue(i); }
	inline void	output(uint64_t i)	{ writeValue(i); }
	inline void	output(bool b)		{ writeValue(b); }
	inline void	output(float f)		{ writeValue(f); }
	inline void	output(double f)	{ writeValue(f); }
	
	bool	inputArrayBegin(void);
	bool	inputArrayCheck(void);
	bool	inputMapBegin(void);
	bool	inputMapCheck(void);
	
	void	outputArrayBegin(int size = 0);
	void	outputArrayEnd(void);
	void	outputMapBegin(int size = 0);
	void	outputMapEnd(void);
	
private:
  	template<typename T> bool readValue(T &value);
  	template<typename T> void writeValue(const T &value);
  
  	Stream *mStream;
	int mLevel;
	bool mFirst;
};

template<typename T> 
bool JsonSerializer::readValue(T &value)
{
	String tmp;
	if(!input(tmp)) return false;
	AssertIO(tmp.read(value));
	return true;
}

template<typename T> 
void JsonSerializer::writeValue(const T &value)
{
	mStream->space();
	mStream->write(value);
	if(mLevel == 0) *mStream << Stream::NewLine;	
}

}

#endif

