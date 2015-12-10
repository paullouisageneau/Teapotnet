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

#ifndef TPN_FOUNTAIN_H
#define TPN_FOUNTAIN_H

#include "tpn/include.h"

#include "pla/binarystring.h"
#include "pla/file.h"
#include "pla/array.h"
#include "pla/list.h"
#include "pla/map.h"

namespace tpn
{

// PRNG-based linear data fountain implementation
class Fountain
{
private:
	// GF(256) operations
	static uint8_t gAdd(uint8_t a, uint8_t b);
	static uint8_t gMul(uint8_t a, uint8_t b); 
	static uint8_t gInv(uint8_t a);
	
	// GF(256) operations tables
	static uint8_t *MulTable;
	static uint8_t *InvTable;
	
	class Generator
	{
	public:
		Generator(uint64_t seed);
		~Generator(void);
		uint8_t next(void);
	
	private:
		uint64_t mSeed;
	};
	
public:
	static void Init(void);
	static void Cleanup(void);
	static const size_t ChunkSize = 1024;	// bytes
	
	class Combination : public Serializable
	{
	public:
		Combination(void);
		Combination(const Combination &combination);
		Combination(unsigned offset, const char *data, size_t size, bool last = false);
		~Combination(void);
		
		void addComponent(unsigned offset, uint8_t coeff);
		void addComponent(unsigned offset, uint8_t coeff, const char *data, size_t size, bool last = false);
		void setData(const char *data, size_t size, bool last = false);
		void setData(const BinaryString &data, bool last = false);
		void setCodedData(const char *data, size_t size);
		void setCodedData(const BinaryString &data);
		
		uint64_t seed(unsigned first, unsigned count);
		
		unsigned firstComponent(void) const;
		unsigned lastComponent(void) const;
		unsigned componentsCount(void) const;
		uint8_t coeff(unsigned offset) const;
		bool isCoded(void) const;
		bool isNull(void) const;
		bool isLast(void) const;
		
		const char *data(void) const;
		size_t size(void) const;
		size_t codedSize(void) const;
		void clear(void);
		
		Combination &operator=(const Combination &combination);
		Combination operator+(const Combination &combination) const;
		Combination operator*(uint8_t coeff) const;
		Combination operator/(uint8_t coeff) const;	
		Combination &operator+=(const Combination &combination);
		Combination &operator*=(uint8_t coeff);
		Combination &operator/=(uint8_t coeff);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		
	private:
		void resize(size_t size, bool zerofill = false);
		
		Map<unsigned, uint8_t> mComponents;
		char *mData;
		size_t mSize;
		uint16_t mNonce;
	};
	
	class Source
	{
	public:
		virtual bool generate(Combination &output, unsigned *counter = NULL) = 0;	// Generate combination
        };
	
	class DataSource : public Source
	{
	public:
		DataSource(void);
		~DataSource(void);
		
		unsigned write(const char *data, size_t size);
		
		unsigned count(void) const;
		bool generate(Combination &result, unsigned *counter = NULL);
		unsigned drop(unsigned nextSeen);

	private:
		List<BinaryString> mComponents;
		unsigned mFirstComponent;
		unsigned mCurrentComponent;
	};
	
	class FileSource : public Source
	{
	public:
		FileSource(File *file, int64_t offset, int64_t size);	// file will be deleted
		~FileSource(void);
		
		bool generate(Combination &result, unsigned *counter = NULL);
		
	private:
		File *mFile;
		int64_t mOffset, mSize;
	};
        
	class Sink
	{
	public:
		Sink(void);
		~Sink(void);
		
		int64_t solve(Combination &incoming);			// Add combination and try to solve, return decoded bytes
		void clear(void);
		
		unsigned nextSeen(void) const;
		unsigned nextDecoded(void) const;
		bool isDecoded(void) const;
		
		size_t read(char *buffer, size_t size);		// Non-const, read some new data
		
		int64_t dump(Stream &stream) const;		// Read all decoded data in buffer
		int64_t hash(BinaryString &digest) const;	// Hash all decoded data in buffer
		
	private:
		Map<unsigned, Combination> mCombinations;	// combinations sorted by pivot component
		
		unsigned mNextSeen, mNextDecoded, mNextRead;	// decoding status counters
		unsigned mEnd;
		size_t mAlreadyRead;
	};
	
private:
	Fountain(void);
};

}

#endif
