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

#ifndef PLA_BINARYSERIALIZER_H
#define PLA_BINARYSERIALIZER_H

#include "pla/serializer.hpp"
#include "pla/stream.hpp"

namespace pla
{

class BinarySerializer : public Serializer
{
public:
	BinarySerializer(Stream *stream);	// stream WON'T be destroyed
	~BinarySerializer(void);
	
private:
	bool		read(std::string &str);
	bool		read(bool &b);
	inline bool	read(int8_t &i)		{ return mStream->readBinary(i); }
	inline bool	read(int16_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(int32_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(int64_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(uint8_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(uint16_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(uint32_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(uint64_t &i)	{ return mStream->readBinary(i); }
	inline bool	read(float &f)		{ return mStream->readBinary(f); }
	inline bool	read(double &f)		{ return mStream->readBinary(f); }
	
	void		write(const std::string &str);
	void		write(bool b);
	inline void	write(int8_t i)		{ mStream->writeBinary(i); }
	inline void	write(int16_t i)	{ mStream->writeBinary(i); }
	inline void	write(int32_t i)	{ mStream->writeBinary(i); }
	inline void	write(int64_t i)	{ mStream->writeBinary(i); }
	inline void	write(uint8_t i)	{ mStream->writeBinary(i); }
	inline void	write(uint16_t i)	{ mStream->writeBinary(i); }
	inline void	write(uint32_t i)	{ mStream->writeBinary(i); }
	inline void	write(uint64_t i)	{ mStream->writeBinary(i); }
	inline void	write(float f)		{ mStream->writeBinary(f); }
	inline void	write(double f)		{ mStream->writeBinary(f); }
	
	bool	readArrayBegin(void);
	bool	readArrayNext(void);
	bool	readMapBegin(void);
	bool	readMapNext(void);
	
	void	writeArrayBegin(size_t size);
	void	writeMapBegin(size_t size);
	
  	Stream *mStream;
	Stack<uint32_t> mLeft;
};

}

#endif

