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

#ifndef PLA_BINARYSTRING_H
#define PLA_BINARYSTRING_H

#include "pla/include.hpp"
#include "pla/stream.hpp"
#include "pla/serializable.hpp"

namespace pla
{

class BinaryString : public std::string, public Stream, public Serializable
{
public:
	template<typename T> static BinaryString number(T n);

	static const BinaryString Empty;
	static const size_type NotFound = npos;

	BinaryString(void);
	BinaryString(const char *str);
	BinaryString(const char *data, size_t size);
	BinaryString(const std::string &str);
	BinaryString(size_t n, char chr);
	BinaryString(const BinaryString &str, int begin = 0);
	BinaryString(const BinaryString &str, int begin, int end);
	template<class InputIterator> BinaryString(InputIterator first, InputIterator last) : std::string(first, last) {}
	virtual ~BinaryString(void);

	// Access
	// Prefer data() for standard read-only access
	char *ptr(void);
	const char *ptr(void) const;
	byte *bytes(void);
	const byte *bytes(void) const;

	// Encoding
	BinaryString base64Encode(bool safeMode = false) const;
	BinaryString base64Decode(void) const;

	// Checksum
	uint16_t checksum16(void) const { uint16_t i = 0; return checksum(i); }
	uint32_t checksum32(void) const { uint32_t i = 0; return checksum(i); }
	uint64_t checksum64(void) const { uint64_t i = 0; return checksum(i); }

	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual void serialize(Stream &s) const;
	virtual bool deserialize(Stream &s);
	virtual bool isNativeSerializable(void) const;
	virtual bool isInlineSerializable(void) const;

	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	void clear(void);

protected:
	template<typename T> T checksum(T &result) const;
};

BinaryString operator ^ (const BinaryString &a, const BinaryString &b);

template<typename T>
BinaryString BinaryString::number(T n)
{
	BinaryString tmp;
	tmp.writeBinary(n);
	return tmp;
}

template<typename T>
T BinaryString::checksum(T &result) const
{
	BinaryString copy(*this);

	while(copy.size() % sizeof(T))
		copy.writeBinary(uint8_t(0));

	result = 0;
	for(size_t i=0; i<this->size(); ++i)
	{
		T value;
		copy.readBinary(value);
		result = result ^ value;
	}

	return result;
}

}

#endif
