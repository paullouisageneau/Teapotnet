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
					NULL,
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
					NULL,
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
	if(!size) return 0;
	
	while(!mReadFilter->AnyRetrievable())
	{
		char buf[BufferSize];
		size_t readSize = mStream->readData(buf, BufferSize);
		if(!readSize) return 0;
		
		mReadFilter->Put(reinterpret_cast<byte*>(buf), readSize);
		mReadFilter->Flush(false);
	}
	
	// DEBUG
	byte debug[1024];
	size_t debugSize = mReadFilter->Peek(debug, 1024);
	VAR(debugSize);
	VAR(BinaryString(reinterpret_cast<char*>(debug), debugSize));
	//
	
	size = mReadFilter->Get(reinterpret_cast<byte*>(buffer), size);
	//VAR(size);
	return size;
}

void Cipher::writeData(const char *data, size_t size)
{
	Assert(data);
	if(!mWriteCipher) throw Exception("Block cipher not initialized for writing");
	if(!size) return;

	mWriteFilter->Put(reinterpret_cast<const byte*>(data), size);
	mWriteFilter->MessageEnd();
	mWriteFilter->Flush(true);
	
	mWriteFilter->GetNextMessage();
	Assert(mWriteFilter->MaxRetrievable() >= size);

	char buf[BufferSize];
	while(size = mWriteFilter->Get(reinterpret_cast<byte*>(buf), BufferSize))
	{
		mStream->writeData(buf, size);
	}
}

}
