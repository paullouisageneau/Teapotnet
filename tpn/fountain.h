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
	class Generator
	{
	public:
		Generator(uint32_t seed);
		~Generator(void);
		uint8_t next(void);
	
	private:
		uint64_t mSeed;
	};
	
	class Combination : public Serializable
	{
	public:
		Combination(void);
		Combination(int offset, const char *data, size_t size);
		~Combination(void);
		
		void addComponent(int offset, uint8_t coeff);
		void addComponent(int offset, uint8_t coeff, const char *data, size_t size);
		void setData(const char *data, size_t size);
		void setData(const BinaryString &data);
		
		int firstComponent(void) const;
		int lastComponent(void) const;
		int componentsCount(void) const;
		uint8_t coeff(int offset) const;
		bool isCoded(void) const;
		bool isNull(void) const;
		
		const char *data(void) const;
		size_t size(void) const;
		void clear(void);
		
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
		// GF(256) operations
		static uint8_t gAdd(uint8_t a, uint8_t b);
		static uint8_t gMul(uint8_t a, uint8_t b); 
		static uint8_t gInv(uint8_t a);
		
		void resize(size_t size, bool zerofill = false);
		
		Map<int, uint8_t> mComponents;
		char *mData;
		size_t mSize;
	};
	
public:
	static const size_t ChunkSize = 1024;	// bytes
  
	class Source
	{
	public:
		Source(File *file, int64_t offset, int64_t size);	// file will be deleted
		~Source(void);
		
		void generate(Stream &output, unsigned *token);	// Generate combination from seed
		
	private:
		File *mFile;
		int64_t mOffset, mSize;
	};
	
	class Sink
	{
	public:
		Sink(void);
		~Sink(void);
		
		bool solve(Stream &input);	// Add combination described by seed and try to solve
						// returns true if solved
		
		size_t size(void) const;
		bool isComplete(void) const;
		void dump(Stream &stream) const;
		void hash(BinaryString &digest) const;
		void clear(void);
		
	private:
		List<Combination> mCombinations;
		uint32_t mSize;
		bool mIsComplete;
	};
	
private:
	Fountain(void);
};

}

#endif
