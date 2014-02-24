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

#include "tpn/cipher.h"
#include "tpn/exception.h"
#include "tpn/bytearray.h"

namespace tpn
{

Cipher::Cipher(	Stream *bs,				// WILL NOT be deleted
	CryptoPP::SymmetricCipher *readCipher,		// WILL be deleted
	CryptoPP::SymmetricCipher *writeCipher) :		// WILL be deleted
	
	mStream(bs),
	mDumpStream(NULL),
	mReadCipher(NULL),
	mReadFilter(NULL),
	mWriteCipher(NULL),
	mWriteFilter(NULL)
{
	Assert(mStream);
	setReadCipher(readCipher);
	setWriteCipher(writeCipher);
}

Cipher::~Cipher(void)
{
	// mStream is not deleted
	
	delete mReadFilter;
	delete mReadCipher;
	
	delete mWriteFilter;
	delete mWriteCipher;
}

void Cipher::setReadCipher(CryptoPP::SymmetricCipher *cipher)
{
	delete mReadFilter;
	delete mReadCipher;
	
	if(cipher)
	{
		mReadCipher = cipher;
		mReadFilter = new CryptoPP::StreamTransformationFilter(
					*mReadCipher, 
					new CryptoPP::StringSink(mReadBuffer),
					CryptoPP::StreamTransformationFilter::PKCS_PADDING);
	}
	else {
		mReadCipher = NULL;
		mReadFilter = NULL;
	}
}

void Cipher::setWriteCipher(CryptoPP::SymmetricCipher *cipher)
{
	delete mWriteFilter;
	delete mWriteCipher; 
	
	if(cipher)
	{
		mWriteCipher = cipher;
		mWriteFilter = new CryptoPP::StreamTransformationFilter(
					*mWriteCipher,
					new CryptoPP::StringSink(mWriteBuffer),
					CryptoPP::StreamTransformationFilter::PKCS_PADDING);
	}
	else {
		mWriteCipher = NULL;
		mWriteFilter = NULL;
	}
}

void Cipher::dumpStream(Stream *bs)
{
	mDumpStream = bs; 
}

size_t Cipher::readData(char *buffer, size_t size)
{
	Assert(buffer);
	if(!mReadCipher) throw Exception("Block cipher not initialized for reading");
	
	size_t readSize;
	while((readSize = mReadBuffer.readData(buffer, size)) == 0)
	{
		readSize = mStream->readData(buffer, size);
		if(!readSize) return 0;
		
		mReadFilter->Put(reinterpret_cast<const byte*>(buffer), readSize);
		mReadFilter->Flush(false);
	}
	
	return readSize;
}

void Cipher::writeData(const char *data, size_t size)
{
	Assert(data);
	if(!mWriteCipher) throw Exception("Block cipher not initialized for writing");
	
	mWriteFilter->Put(reinterpret_cast<const byte*>(data), size);
	mWriteFilter->Flush(false);
	
	mStream->writeData(mWriteBuffer.data(), mWriteBuffer.size());
}

}
