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

#ifndef PLA_LINESERIALIZER_H
#define PLA_LINESERIALIZER_H

#include "pla/serializer.h"
#include "pla/stream.h"
#include "pla/array.h"

namespace pla
{

class LineSerializer : public Serializer
{
public:
	LineSerializer(Stream *stream);	// stream WON'T be destroyed
	virtual ~LineSerializer(void);
	
	bool	input(Serializable &s);
	bool	input(Element &element);
	bool	input(Pair &pair);
	bool	input(String &str);
	
	void	output(const Serializable &s);
	void	output(const Element &element);
	void	output(const Pair &pair);
	void	output(const String &str);
	
	inline bool	input(int8_t &i)	{ return mStream->read(i); }
	inline bool	input(int16_t &i)	{ return mStream->read(i); }
	inline bool	input(int32_t &i)	{ return mStream->read(i); }
	inline bool	input(int64_t &i)	{ return mStream->read(i); }
	inline bool	input(uint8_t &i)	{ return mStream->read(i); }
	inline bool	input(uint16_t &i)	{ return mStream->read(i); }
	inline bool	input(uint32_t &i)	{ return mStream->read(i); }
	inline bool	input(uint64_t &i)	{ return mStream->read(i); }
	inline bool	input(bool &b)		{ return mStream->read(b); }
	inline bool	input(float &f)		{ return mStream->read(f); }
	inline bool	input(double &f)	{ return mStream->read(f); }

	inline void	output(int8_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(int16_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(int32_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(int64_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(uint8_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(uint16_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(uint32_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(uint64_t i)	{ mStream->write(i); mStream->space(); }
	inline void	output(bool b)		{ mStream->write(b); mStream->space(); }
	inline void	output(float f)		{ mStream->write(f); mStream->space(); }
	inline void	output(double f)	{ mStream->write(f); mStream->space(); }
	
	bool	inputArrayBegin(void);
	bool	inputMapBegin(void);
	
private:
  	Stream *mStream;
	char mSeparator;
};

}

#endif

