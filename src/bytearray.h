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

#ifndef ARC_BYTEARRAY_H
#define ARC_BYTEARRAY_H

#include "include.h"
#include "bytestream.h"

namespace arc
{

class ByteArray : public ByteStream
{
public:
	ByteArray(size_t size);
	ByteArray(char *array, size_t length);	// data is NOT copied
	ByteArray(const ByteArray &array);		// data is NOT copied
	virtual ~ByteArray(void);

	char *array(void);					// complete array
	const char *array(void) const;
	size_t length(void) const;			// total size

	const char *data(void) const;		// reading position
	size_t size(void) const;			// data left
	
	void clear(void);

protected:
	int readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

private:
	char *mArray;
	size_t mLength;
	size_t mLeft;
	size_t mReadPos;
	size_t mWritePos;
	bool mMustDelete;
};

}

#endif
