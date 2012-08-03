/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef ARC_BYTESTREAM_H
#define ARC_BYTESTREAM_H

#include "include.h"

namespace arc
{

class Serializable;
class ByteString;

class ByteStream
{
public:
	ByteStream(void);
	virtual ~ByteStream(void);

	int	read(char *buffer, int size);
	void	write(const char *data, int size);

	void	readBinary(ByteStream &s);
	bool	readBinary(Serializable &s);
	bool	readBinary(ByteString &s);
	bool	readBinary(sint8_t &i);
	bool	readBinary(sint16_t &i);
	bool	readBinary(sint32_t &i);
	bool	readBinary(uint8_t &i);
	bool	readBinary(uint16_t &i);
	bool	readBinary(uint32_t &i);
	bool	readBinary(float32_t &f);

	template<class T> bool readBinary(T *ptr);

	bool	readInt8(signed int &i);
	bool	readInt16(signed int &i);
	bool	readInt32(signed int &i);
	bool	readInt8(unsigned int &i);
	bool	readInt16(unsigned int &i);
	bool	readInt32(unsigned &i);
	bool	readFloat(float &f);
	bool	readTime(double &t);

	void	writeBinary(ByteStream &s);
	void	writeBinary(const Serializable &s);
	void	writeBinary(const ByteString &s);
	void	writeBinary(sint8_t i = 0);
	void	writeBinary(sint16_t i = 0);
	void	writeBinary(sint32_t i = 0);
	void	writeBinary(uint8_t i = 0);
	void	writeBinary(uint16_t i = 0);
	void	writeBinary(uint32_t i = 0);
	void	writeBinary(float32_t f = 0.f);

	template<class T> void writeBinary(const T *ptr);

	void	writeInt8(signed int i = 0);
	void	writeInt16(signed int i = 0);
	void	writeInt32(signed int i = 0);
	void	writeInt8(unsigned int i = 0);
	void	writeInt16(unsigned int i = 0);
	void	writeInt32(unsigned int i = 0);
	void	writeFloat(float f = 0.f);
	void	writeTime(double time = 0.);

	virtual void clear(void);
	virtual void ignore(int n = 1);
	
protected:
	virtual int readData(char *buffer, int size) = 0;
	virtual void writeData(const char *data, int size) = 0;

private:
	void	fixEndianess16(char *data);
	void	fixEndianess32(char *data);
};

template<class T>
bool ByteStream::readBinary(T *ptr)
{
	return readBinary(*ptr);
}

template<class T>
void ByteStream::writeBinary(const T *ptr)
{
	writeBinary(*ptr);
}

}

#endif
