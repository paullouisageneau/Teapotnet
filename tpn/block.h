/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef TPN_BLOCK_H
#define TPN_BLOCK_H

#include "tpn/include.h"
#include "tpn/core.h"
#include "tpn/fountain.h"

#include "pla/serializable.h"
#include "pla/string.h"
#include "pla/binarystring.h"
#include "pla/stream.h"
#include "pla/time.h"

namespace tpn
{
	
class Block : public Stream
{
public:
	static const size_t MaxChunks = 1024;
	static const size_t ChunkSize = Fountain::ChunkSize;
	static const size_t Size = MaxChunks*ChunkSize;
	
	static bool ProcessFile(File &file, Block &block);
	static bool ProcessFile(File &file, BinaryString &digest);
	static bool EncryptFile(Stream &stream, const BinaryString &key, const BinaryString &iv, BinaryString &digest, String *newFileName = NULL);
	static bool EncryptFile(Stream &stream, const BinaryString &key, const BinaryString &iv, Block &block);
	
	Block(const Block &block);
	Block(const BinaryString &digest);
	Block(const BinaryString &digest, const String &filename, int64_t offset = 0, int64_t size = -1);	// override file
	Block(const String &filename, int64_t offset = 0, int64_t size = -1);					// digest computed from file
	virtual ~Block(void);
	
	BinaryString digest(void) const;
	
	void setDecryption(const BinaryString &key, const BinaryString &iv); 
	bool hasDecryption(void) const;
	
	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	bool waitData(double &timeout);
	void seekRead(int64_t position);
	void seekWrite(int64_t position);
	int64_t tellRead(void) const;
	int64_t tellWrite(void) const;
	
	Block &operator = (const Block &block);
	
private:
  	void waitContent(void) const;
	bool waitContent(double &timeout) const;
	bool waitContent(const double &timeout) const;
	void notifyStore(void) const;
  
	BinaryString mDigest;
	
	mutable File *mFile;
	mutable Cipher *mCipher;
	mutable int64_t mOffset;
	mutable int64_t mSize;
};

}

#endif

