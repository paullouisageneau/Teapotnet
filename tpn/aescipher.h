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

#ifndef TPN_AES_H
#define TPN_AES_H

#include "tpn/include.h"
#include "tpn/stream.h"
#include "tpn/bytestream.h"
#include "tpn/bytestring.h"

namespace tpn
{

#define AES_BLOCK_SIZE 16
#define AES_MAX_KEY_SIZE 32
#define AES_MAXNR 14

class AesCipher : public Stream, public ByteString
{
public:
  	AesCipher(ByteStream *bs);	// bs will not be deleted
	~AesCipher(void);
	
	size_t setEncryptionKey(const ByteString &key);
	size_t setEncryptionKey(const char *key, size_t size);
	size_t setDecryptionKey(const ByteString &key);
	size_t setDecryptionKey(const char *key, size_t size);
	
	void setEncryptionInit(const ByteString &iv);
	void setEncryptionInit(const char *iv, size_t size);
	void setDecryptionInit(const ByteString &iv);
	void setDecryptionInit(const char *iv, size_t size);
	
	void dumpStream(ByteStream *bs);
	
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

private:
  	struct Key
	{
	    uint32_t rd_key[4 *(AES_MAXNR + 1)];
	    int rounds;
	};
  
  	size_t setEncryptionKey(const char *key, size_t size, Key &out);
	size_t setDecryptionKey(const char *key, size_t size, Key &out);
  
  	void encrypt(char *in, char *out);
	void decrypt(char *in, char *out);
  
	size_t readBlock(char *out);
	
	
	ByteStream *mByteStream;
	ByteStream *mDumpStream;
	
	Key mEncryptionKey;
	Key mDecryptionKey;
	char mEncryptionInit[AES_BLOCK_SIZE];
	char mDecryptionInit[AES_BLOCK_SIZE];
	
	char	mTempBlockIn[AES_BLOCK_SIZE];
	size_t	mTempBlockInSize;
	char	mTempBlockOut[AES_BLOCK_SIZE];
	size_t	mTempBlockOutSize;
	
	static const uint32_t Te0[];
	static const uint32_t Te1[];
	static const uint32_t Te2[];
	static const uint32_t Te3[];
	static const uint32_t Te4[];
	static const uint32_t Td0[];
	static const uint32_t Td1[];
	static const uint32_t Td2[];
	static const uint32_t Td3[];
	static const uint32_t Td4[];
	static const uint32_t rcon[];
};

}

#endif
