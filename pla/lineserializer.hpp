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

#include "pla/serializer.hpp"
#include "pla/stream.hpp"
#include "pla/string.hpp"

namespace pla
{

// Dummy serializer that only writes values as text
// WARNING: For debugging purposes only, never use for complex structures
class LineSerializer : public Serializer
{
public:
	LineSerializer(Stream *stream);	// stream WON'T be destroyed
	~LineSerializer(void);
	
private:
	bool read(Serializable &s);
	bool read(std::string &str);
	inline bool	read(int8_t &i)		{ return mStream->read(i); }
	inline bool	read(int16_t &i)	{ return mStream->read(i); }
	inline bool	read(int32_t &i)	{ return mStream->read(i); }
	inline bool	read(int64_t &i)	{ return mStream->read(i); }
	inline bool	read(uint8_t &i)	{ return mStream->read(i); }
	inline bool	read(uint16_t &i)	{ return mStream->read(i); }
	inline bool	read(uint32_t &i)	{ return mStream->read(i); }
	inline bool	read(uint64_t &i)	{ return mStream->read(i); }
	inline bool	read(bool &b)		{ return mStream->read(b); }
	inline bool	read(float &f)		{ return mStream->read(f); }
	inline bool	read(double &f)		{ return mStream->read(f); }

	void write(const Serializable &s);
	void write(const std::string &str);
	inline void	write(int8_t i)		{ mStream->write(i); mStream->space(); }
	inline void	write(int16_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(int32_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(int64_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(uint8_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(uint16_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(uint32_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(uint64_t i)	{ mStream->write(i); mStream->space(); }
	inline void	write(bool b)		{ mStream->write(b); mStream->space(); }
	inline void	write(float f)		{ mStream->write(f); mStream->space(); }
	inline void	write(double f)		{ mStream->write(f); mStream->space(); }
	
	bool	readArrayBegin(void);
	bool	readArrayNext(void);
	bool	readMapBegin(void);
	bool	readMapNext(void);

	void writeArrayBegin(size_t size);
	void writeArrayNext(size_t i);
	void writeArrayEnd(void);
	void writeMapBegin(size_t size);
	void writeMapNext(size_t i);
	void writeMapEnd(void);

	Stream *mStream;
	bool mKey = false;
};

}

#endif

