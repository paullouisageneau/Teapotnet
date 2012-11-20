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

#ifndef TPOT_IDENTIFIER_H
#define TPOT_IDENTIFIER_H

#include "include.h"
#include "bytestring.h"
#include "stream.h"
#include "serializable.h"

namespace tpot
{

class Identifier : public Serializable
{
public:
	static const Identifier Null;

	Identifier(void);
	Identifier(const ByteString &digest, const String &name = "");
	~Identifier(void);

	const ByteString &getDigest(void) const;
	const String &getName(void) const;
	
	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	void serialize(Stream &s) const;
	bool deserialize(Stream &s);

private:
	ByteString 	mDigest;
	String		mName;
};

bool operator < (const Identifier &i1, const Identifier &i2);
bool operator > (const Identifier &i1, const Identifier &i2);
bool operator == (const Identifier &i1, const Identifier &i2);
bool operator != (const Identifier &i1, const Identifier &i2);

}

#endif
