/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_IDENTIFIER_H
#define TPN_IDENTIFIER_H

#include "tpn/include.h"
#include "tpn/string.h"
#include "tpn/binarystring.h"
#include "tpn/serializable.h"

namespace tpn
{

class Identifier : public Serializable
{
public:
	// Hardcoded identifier size, do not change
	static const size_t DigestSize = 32;		// digest: 32 B
	static const size_t Size = DigestSize + 8;	// total:  40 B
	static const Identifier Null;
	
	Identifier(void);
	Identifier(const BinaryString &digest, uint64_t number = 0);
	~Identifier(void);
	
	const BinaryString &digest(void) const;
	uint64_t number(void) const;
	
	void setDigest(const BinaryString &digest);
	void setNumber(uint64_t number);
	
	bool empty(void) const;
	void clear(void);
	
	operator BinaryString &(void);
	operator const BinaryString &(void) const;
	
	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	void serialize(Stream &s) const;
	bool deserialize(Stream &s);

private:
	BinaryString mDigest;
	uint64_t mNumber;
};

// The principle is that a null number is equal to ANY other number
bool operator < (const Identifier &i1, const Identifier &i2);
bool operator > (const Identifier &i1, const Identifier &i2);
bool operator == (const Identifier &i1, const Identifier &i2);
bool operator != (const Identifier &i1, const Identifier &i2);

}

#endif
