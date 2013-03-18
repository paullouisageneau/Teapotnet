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

#ifndef TPN_BYTESERIALIZER_H
#define TPN_BYTESERIALIZER_H

#include "tpn/serializer.h"
#include "tpn/bytestream.h"
#include "tpn/string.h"

namespace tpn
{

class ByteSerializer : public Serializer
{
public:
	ByteSerializer(ByteStream *stream);	// stream WON'T be destroyed
	virtual ~ByteSerializer(void);
	
	inline bool	input(String &str)	{ return mStream->readBinary(str); }
	inline bool	input(int8_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(int16_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(int32_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(int64_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(uint8_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(uint16_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(uint32_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(uint64_t &i)	{ return mStream->readBinary(i); }
	inline bool	input(float &f)		{ return mStream->readBinary(f); }
	inline bool	input(double &f)	{ return mStream->readBinary(f); }
	
	inline void     output(const String &str) { mStream->writeBinary(str); }
	inline void	output(int8_t i)	{ mStream->writeBinary(i); }
	inline void	output(int16_t i)	{ mStream->writeBinary(i); }
	inline void	output(int32_t i)	{ mStream->writeBinary(i); }
	inline void	output(int64_t i)	{ mStream->writeBinary(i); }
	inline void	output(uint8_t i)	{ mStream->writeBinary(i); }
	inline void	output(uint16_t i)	{ mStream->writeBinary(i); }
	inline void	output(uint32_t i)	{ mStream->writeBinary(i); }
	inline void	output(uint64_t i)	{ mStream->writeBinary(i); }
	inline void	output(float f)		{ mStream->writeBinary(f); }
	inline void	output(double f)	{ mStream->writeBinary(f); }
	
	bool	input(bool &b);
	void	output(bool b);
	
	bool	inputArrayBegin(void);
	bool	inputArrayCheck(void);
	bool	inputMapBegin(void);
	bool	inputMapCheck(void);
	
	void	outputArrayBegin(int size);
	void	outputArrayEnd(void);
	void	outputMapBegin(int size);
	void	outputMapEnd(void);
	
private:
  	ByteStream *mStream;
	Stack<uint32_t> mLeft;
};

}

#endif

