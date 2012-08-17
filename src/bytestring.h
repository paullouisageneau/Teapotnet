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

#ifndef ARC_BYTESTRING_H
#define ARC_BYTESTRING_H

#include "include.h"
#include "bytestream.h"
#include "serializable.h"

#include <deque>

namespace arc
{

class ByteString : public std::deque<char>, public ByteStream, public Serializable
{
public:	
	ByteString(void);
	ByteString(const char *data, size_t size);
	virtual ~ByteString(void);

	void clear(void);
	void append(char value, int n = 1);
	void append(const ByteString &bs);
	void append(const char *array, size_t size);
	void fill(char value, int n);

	// Serializable
	virtual void serialize(Stream &s) const;
	virtual void deserialize(Stream &s);
	virtual void serializeBinary(ByteStream &s) const;
	virtual void deserializeBinary(ByteStream &s);

protected:
	int readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
};

}

#endif
