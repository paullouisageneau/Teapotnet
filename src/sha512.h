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

#ifndef ARC_SHA512_H
#define ARC_SHA512_H

#include "include.h"
#include "bytestream.h"
#include "bytestring.h"
#include "bytearray.h"

namespace arc
{

class Sha512
{
public:
	static void Hash(const char *data, size_t size, ByteStream &out);
	static void Hash(ByteStream &data, ByteStream &out);

	Sha512(void);
	~Sha512(void);
	
	void init(void);
	void process(const char *data, size_t size);
	void process(ByteStream &data);
	void finalize(unsigned char *out);
	void finalize(ByteStream &out);
	
private:
	static const uint64_t K[80];
	
	void compress(const unsigned char *buf);
	
	uint64_t  length, state[8];
	unsigned char buf[128];
	int curlen;
};

}

#endif
