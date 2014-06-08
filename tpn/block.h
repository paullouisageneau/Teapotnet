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
#include "tpn/serializable.h"
#include "tpn/string.h"
#include "tpn/binarystring.h"
#include "tpn/stream.h"
#include "tpn/time.h"
#include "tpn/core.h"
#include "tpn/foutain.h"

namespace tpn
{
	
class Block : protected Core::Caller, public Fountain, public Synchronizable
{
public:
	static const size_t ChunkSize = Foutain::ChunkSize;
	static const size_t MaxChunks = 1024;	// chunks
	
	static bool ProcessFile(File &file, Block &block);
	
	Block(const BinaryString &digest);
	Block(const BinaryString &digest, const String &filename, int64_t offset = 0);	// override storage file
	virtual ~Block(void);

	void load(const BinaryString &digest);
	void save(void);
	bool completed(void);
	BinaryString computeDigest(void);

	class Reader
	{
	public:
		Reader(Block *Block);
		~Reader(void);
		
		// Stream
		size_t readData(char *buffer, size_t size);
		void writeData(const char *buffer, size_t size);
		void seekRead(int64_t position);
		void seekWrite(int64_t position);
		
	private:
		Block *mBlock;
		int64_t mReadPosition;
	};
	
protected:
	BinaryString mDigest;
	File *mFile;
	File *mMapFile;
	int64_t mOffset;
	
	// Block registers to Cache
	// If the block is not complete, it register a Caller to Core (start)
	// Upon reception of a combination, Core pushes it to Cache
	// Cache pushes it to Block as it is registered
	// When complete, the caller is unregistered (stop)
	
	// Fountain
	size_t readChunk(int64_t offset, char *buffer, size_t size);
	void writeChunk(int64_t offset, const char *data, size_t size);
	bool checkChunk(int64_t offset);
	
	bool isWritten(int64_t offset);
	void markWritten(int64_t offset);
	
	friend class Reader;
};

}

#endif

