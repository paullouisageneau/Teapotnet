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

#ifndef TPN_BYTESTRING_H
#define TPN_BYTESTRING_H

#include "tpn/include.h"
#include "tpn/bytestream.h"
#include "tpn/serializable.h"

#include <deque>

namespace tpn
{

class String;
  
class ByteString : public std::deque<char>, public ByteStream, public Serializable
{
public:	
	ByteString(void);
	ByteString(const char *data, size_t size);
	ByteString(const String &str);
	virtual ~ByteString(void);

	void clear(void);
	void append(char value, int n = 1);
	void append(const ByteString &bs);
	void append(const char *array, size_t size);
	void fill(char value, int n);

	uint16_t checksum16(void) const { uint16_t i = 0; return checksum(i); }
	uint32_t checksum32(void) const { uint32_t i = 0; return checksum(i); }
	uint64_t checksum64(void) const { uint64_t i = 0; return checksum(i); }

	bool constantTimeEquals(const ByteString &bs) const;
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual void serialize(Stream &s) const;
	virtual bool deserialize(Stream &s);
	virtual bool isNativeSerializable(void) const;
	
protected:
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	
	template<typename T> T checksum(T &result) const;
};

template<typename T>
T ByteString::checksum(T &result) const
{
 	ByteString copy(*this);
	
	while(copy.size() % sizeof(T))
	  copy.writeBinary(uint8_t(0));

	result = 0;
	for(int i=0; i<size(); ++i)
	{
	  	T value;
		copy.readBinary(value);
		result = result ^ value;
	}
	
	return result;
}

}

#endif
