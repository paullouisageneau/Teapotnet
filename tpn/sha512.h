/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_SHA512_H
#define TPN_SHA512_H

#include "tpn/include.h"
#include "tpn/bytestream.h"
#include "tpn/bytestring.h"
#include "tpn/string.h"

namespace tpn
{

class Sha512
{
public:
	static const int CryptRounds;
  
	static size_t Hash(const char *data, size_t size, ByteStream &out);
	static size_t Hash(const ByteString &message, ByteStream &out);
	static size_t Hash(ByteStream &data, ByteStream &out);
	static size_t Hash(ByteStream &data, size_t size, ByteStream &out);

	static void RecursiveHash(const ByteString &message, const ByteString &salt, ByteStream &out, int rounds = CryptRounds);
	static void AuthenticationCode(const ByteString &key, ByteStream &data, ByteStream &out);
	static void DerivateKey(const ByteString &password, const ByteString &salt, ByteStream &out, int rounds = CryptRounds);

	Sha512(void);
	~Sha512(void);
	
	void init(void);
	void process(const char *data, size_t size);
	size_t process(ByteStream &data);
	size_t process(ByteStream &data, size_t max);
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
