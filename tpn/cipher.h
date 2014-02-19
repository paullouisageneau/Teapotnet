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

#ifndef TPN_CIPHER_H
#define TPN_CIPHER_H

#include "tpn/include.h"
#include "tpn/stream.h"
#include "tpn/bytestream.h"
#include "tpn/string.h"

#include <cryptopp/cryptlib.h>
#include <cryptopp/filters.h>

namespace tpn
{

class Cipher : public Stream, public ByteStream
{
public:
  	Cipher(	ByteStream *bs,					// WILL NOT be deleted
		CryptoPP::SymmetricCipher *readCipher = NULL,	// WILL be deleted
		CryptoPP::SymmetricCipher *writeCipher = NULL);	// WILL be deleted
	~Cipher(void);
	
	void setReadCipher(CryptoPP::SymmetricCipher *cipher);
	void setWriteCipher(CryptoPP::SymmetricCipher *cipher);
	void dumpStream(ByteStream *bs);
	
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

private:
	ByteStream *mByteStream;
	ByteStream *mDumpStream;
	
	CryptoPP::SymmetricCipher *mReadCipher;			// can be NULL
	CryptoPP::StreamTransformationFilter *mReadFilter;
	String mReadBuffer;
	
	CryptoPP::SymmetricCipher *mWriteCipher;		// can be NULL
	CryptoPP::StreamTransformationFilter *mWriteFilter;
	String mWriteBuffer;
};

}

#endif
