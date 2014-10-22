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

#include "tpn/fountain.h"

#include "pla/random.h"
#include "pla/crypto.h"

namespace tpn
{

Fountain::Generator::Generator(uint32_t seed) :
	mSeed(uint(seed))
{

}

Fountain::Generator::~Generator(void)
{

}

uint8_t Fountain::Generator::next(void)
{
	uint8_t value;
	do {
		// Knuth's 64-bit linear congruential generator
		mSeed = uint64_t(mSeed*6364136223846793005 + 1442695040888963407);		
		value = uint8_t(mSeed >> 56);
	}
	while(!value);	// zero is not a valid output

	return value;
}

Fountain::Combination::Combination(void) :
	mData(NULL),
	mSize(0)
{
	
}

Fountain::Combination::Combination(int offset, const char *data, size_t size) :
	mData(NULL),
	mSize(0)
{
	addComponent(offset, 1);
	setData(data, size);
}

Fountain::Combination::~Combination(void)
{
	
}

void Fountain::Combination::addComponent(int offset, uint8_t coeff)
{
	Map<int, uint8_t>::iterator it = mComponents.find(offset);
	if(it != mComponents.end())
	{
		it->second^= coeff;	// it->second = gAdd(it->second, coeff);
		if(it->second == 0) mComponents.erase(it);
	}
	else {
		if(coeff != 0)
			mComponents.insert(offset, coeff);
	}
}

void Fountain::Combination::addComponent(int offset, uint8_t coeff, const char *data, size_t size)
{
	addComponent(offset, coeff);
	
	// Assure mData is the longest vector
	if(mSize < size)
		resize(size, true);	// zerofill
	
	if(coeff == 0) return;
	
	if(coeff == 1)
	{
		// Add values
		//for(unsigned i = 0; i < size; ++i)
		//	mData[i] = gAdd(mData[i], data[i]);
	  
	  	// Faster
		unsigned long *a = reinterpret_cast<unsigned long*>(mData);
		const unsigned long *b = reinterpret_cast<const unsigned long*>(data);
		const int n = size / sizeof(unsigned long);
		for(int i = 0; i < n; ++i)
			a[i]^= b[i];
		for(int i = n*sizeof(unsigned long); i < size; ++i)
			mData[i]^= data[i];
	}
	else {
		// Add values
		//for(unsigned i = 0; i < size; ++i)
		//	mData[i] = gAdd(mData[i], gMul(data[i], coeff));
		
		// Faster
		for(unsigned i = 0; i < size; ++i)
			mData[i]^= gMul(data[i], coeff);
	}
}

void Fountain::Combination::setData(const char *data, size_t size)
{
	resize(size, false);
	std::copy(data, data + size, mData);
}

void Fountain::Combination::setData(const BinaryString &data)
{
	setData(data.data(), data.size());
}

int Fountain::Combination::firstComponent(void) const
{
	if(!mComponents.empty()) return mComponents.begin()->first;
	else return 0;
}

int Fountain::Combination::lastComponent(void) const
{
	if(!mComponents.empty()) return (--mComponents.end())->first;
	else return 0;
}

int Fountain::Combination::componentsCount(void) const
{
	if(!mComponents.empty()) return (lastComponent() - firstComponent()) + 1;
	else return 0;
}

uint8_t Fountain::Combination::coeff(int offset) const
{
	Map<int, uint8_t>::const_iterator it = mComponents.find(offset);
	if(it == mComponents.end()) return 0; 
	
	Assert(it->second != 0);
	return it->second;
}

bool Fountain::Combination::isCoded(void) const
{
	return (mComponents.size() != 1 || mComponents.begin()->second != 1);
}

const char *Fountain::Combination::data(void) const
{
	return mData;
}

size_t Fountain::Combination::size(void) const
{
	return mSize;
}

void Fountain::Combination::clear(void)
{
	mComponents.clear();
	delete[] mData;
	mData = NULL;
	mSize = 0;
}

Fountain::Combination Fountain::Combination::operator+(const Combination &combination) const
{
	Fountain::Combination result(*this);
	result+= combination;
	return result;
}

Fountain::Combination Fountain::Combination::operator*(uint8_t coeff) const
{
	Fountain::Combination result(*this);
	result*= coeff;
	return result;
}

Fountain::Combination Fountain::Combination::operator/(uint8_t coeff) const
{
	Fountain::Combination result(*this);
	result/= coeff;	
	return result;
}
	
Fountain::Combination &Fountain::Combination::operator+=(const Combination &combination)
{
	// Assure mData is long enough
	if(mSize < combination.mSize)
		resize(combination.mSize, true);	// zerofill
	
	// Add values
	//for(int i = 0; i < combination.mSize; ++i)
	//	mData[i] = gAdd(mData[i], combination.mData[i]);

	// Faster
	unsigned long *a = reinterpret_cast<unsigned long*>(mData);
	const unsigned long *b = reinterpret_cast<const unsigned long*>(combination.mData);
	const int n = combination.mSize / sizeof(unsigned long);
	for(int i = 0; i < n; ++i)
		a[i]^= b[i];
	for(int i = n*sizeof(unsigned long); i < combination.mSize; ++i)
		mData[i]^= combination.mData[i];
		
	
	// Add components
	for(	Map<int, uint8_t>::const_iterator jt = combination.mComponents.begin();
		jt != combination.mComponents.end();
		++jt)
	{
		addComponent(jt->first, jt->second);
	}

	return *this;
}

Fountain::Combination &Fountain::Combination::operator*=(uint8_t coeff)
{
	// TODO: coeff == 0
	Assert(coeff != 0);

	// Multiply vector
	for(int i = 0; i < mSize; ++i)
		mData[i] = gMul(mData[i], coeff);

	for(	Map<int, uint8_t>::iterator it = mComponents.begin();
		it != mComponents.end();
		++it)
	{
		it->second = gMul(it->second, coeff);
	}

	return *this;
}

Fountain::Combination &Fountain::Combination::operator/=(uint8_t coeff)
{
	Assert(coeff != 0);

	(*this)*= gInv(coeff);
	return *this;
}


void Fountain::Combination::serialize(Serializer &s) const
{
	Assert(componentsCount() <= std::numeric_limits<uint16_t>::max());
	Assert(firstComponent() >= 0);	// for sanity
	
	s.output(uint(firstComponent()));
	s.output(uint16_t(componentsCount()));
	for(int i=firstComponent(); i<=lastComponent(); ++i)
		s.output(uint8_t(coeff(i)));
}

bool Fountain::Combination::deserialize(Serializer &s)
{
	uint first = 0;
	uint16_t count = 0;
	if(!s.input(first)) return false;
	AssertIO(s.input(count));
	
	for(int i=first; i<first+count; ++i)
	{
		uint8_t coeff = 0;
		AssertIO(s.input(coeff));
		addComponent(i, coeff);
	}
}

uint8_t Fountain::Combination::gAdd(uint8_t a, uint8_t b)
{
	return a ^ b;
}

uint8_t Fountain::Combination::gMul(uint8_t a, uint8_t b) 
{
	static uint8_t *table = NULL;

	if(!table) 
	{
		table = new uint8_t[256*256];
		
		table[0] = 0;
		for(uint8_t i = 1; i != 0; ++i) 
		{
			table[unsigned(i)] = 0;
			table[unsigned(i)*256] = 0;
			
			for(uint8_t j = 1; j != 0; ++j)
			{
				uint8_t a = i;
				uint8_t b = j;
				uint8_t p = 0;
				uint8_t k;
				uint8_t carry;
				for(k = 0; k < 8; ++k)
				{
					if (b & 1) p^= a;
					carry = (a & 0x80);
					a<<= 1;
					if (carry) a^= 0x1b; // 0x1b is x^8 modulo x^8 + x^4 + x^3 + x + 1
					b>>= 1;
				}
				
				table[unsigned(i)*256+unsigned(j)] = p;
			}
		}
	}
	
	return table[unsigned(a)*256+unsigned(b)];
}

uint8_t Fountain::Combination::gInv(uint8_t a) 
{
	static uint8_t *table = NULL;
	
	if(!table)
	{
		table = new uint8_t[256];
		
		table[0] = 0;
		for(uint8_t i = 1; i != 0; ++i)
		{
			for(uint8_t j = i; j != 0; ++j)
			{
				if(gMul(i,j) == 1)	// then gMul(j,i) == 1
				{
					  table[i] = j;
					  table[j] = i;
				}
			}
		}
	}
	
	Assert(a != 0);
	return table[a];
}

void Fountain::Combination::resize(size_t size, bool zerofill)
{
	if(mSize != size)
	{
		char *newData = new char[size];
		std::copy(newData, newData + std::min(mSize, size), newData);
		if(zerofill && size > mSize)
			std::fill(newData + mSize, newData + size, 0);
		
		delete[] mData;
		mData = newData;
		mSize = size;
	}
}

Fountain::Source::Source(File *file, int64_t offset, int64_t size) :
	mFile(file),
	mOffset(offset),
	mSize(size)
{
	Assert(mFile);
	Assert(mSize >= 0);
}

Fountain::Source::~Source(void)
{
	delete mFile;
}

void Fountain::Source::generate(Stream &output, unsigned *tokens)
{
	if(mSize > std::numeric_limits<uint32_t>::max())
		throw Exception("File too big for Foutain::Source");
	
	unsigned chunks = mSize/ChunkSize + (mSize % ChunkSize ? 1 : 0);
	
	unsigned t = 0;
	if(tokens) t = *tokens;
	if(t > chunks) t = chunks;
	
	unsigned count = 16;	// TODO
	unsigned first = chunks - t;
	uint32_t left = uint32_t(mSize);
	
	if(first == chunks) // tokens == 0
		first = Random().uniform(unsigned(0), chunks - count);
	  
	// Seek
	mFile->seekRead(mOffset + first*ChunkSize);
	left-= first*ChunkSize;
	
	// Generate
	Combination c;
	unsigned i = first;
	char buffer[ChunkSize];
	size_t size;
	while(i < first+count && (size = mFile->readBinary(buffer, size_t(std::min(uint32_t(ChunkSize), left)))))
	{
		c.addComponent(i, 1, buffer, size);
		left-= size;
		++i;
	}
	count = i - first;	// Actual count
	
	//LogDebug("Fountain::Source::generate", "Generated combination (first=" + String::number(unsigned(first)) + ", count=" + String::number(unsigned(count)) + ")");
	
	// Write
	output.writeBinary(uint32_t(mSize));
	output.writeBinary(uint32_t(0));	// seed
	output.writeBinary(uint16_t(first));
	output.writeBinary(uint16_t(count));
	output.writeBinary(c.data(), c.size());
	
	if(tokens && *tokens) --*tokens;
}
		
Fountain::Sink::Sink(void) :
	mSize(0),
	mIsComplete(false)
{

}

Fountain::Sink::~Sink(void)
{
  
}

bool Fountain::Sink::solve(Stream &input)
{
	uint32_t size;
	uint32_t seed;
	uint16_t first, count;
	BinaryString data;
	
	if(!input.readBinary(size))
	{
		LogWarn("Fountain::Sink::solve", "Unable to read a combination: input is empty");
		return false;
	}
	
	AssertIO(input.readBinary(seed));
	AssertIO(input.readBinary(first));
	AssertIO(input.readBinary(count));
	input.readBinary(data);
	
	//LogDebug("Fountain::Sink::solve", "Incoming combination (first=" + String::number(unsigned(first)) + ", count=" + String::number(unsigned(count)) + ", size=" + String::number(unsigned(data.size())) + ")");
	
	// TODO: check data.size()
	
	mSize = std::max(mSize, size);
	
	Combination c;
	c.setData(data);
	
	if(!seed)
	{
		for(unsigned i=first; i<first+count; ++i)
			c.addComponent(i, 1);
	}
	else {
		Generator gen(seed);
		for(unsigned i=first; i<first+count; ++i)
		{
			uint8_t coeff = gen.next();
			c.addComponent(i, coeff);
		}
	}
	
	mCombinations.push_back(c);
  
	// Solve only if if we've got enough combinations
	if(mCombinations.size() < mSize/ChunkSize)
		return false;
	
	LogDebug("Fountain::Sink::solve", "Solving with " + String::number(int(mCombinations.size())) + " combinations");
	
	List<Combination>::iterator it;	// current equation
	
	// Gauss-Jordan elimination
	it = mCombinations.begin();	// pivot equation
	int i = 0;			// pivot equation index
	while(it != mCombinations.end())
	{
		List<Combination>::iterator jt = it;
		while(jt != mCombinations.end() && jt->coeff(i) == 0) ++jt;
		if(jt == mCombinations.end()) break;
		if(jt != it) std::iter_swap(jt, it);
		
		// Normalize pivot
		uint8_t c = it->coeff(i);
		if(c != 1) (*it)/= c;
		//Assert(it->coeff(i) == 1);
		
		// Suppress coordinate i in each equation
		int j = 0;			// secondary equation index
		jt = mCombinations.begin();	// secondary equation
		while(jt != mCombinations.end())
		{
			if(jt->componentsCount() && jt != it)
			{
				uint8_t c = jt->coeff(i);
				if(c)	// if term not supressed
				{
					// it->coeff(i) == 1 here
					if(c == 1) (*jt)+= *it;
					else (*jt)+= (*it)*c;
					//Assert(jt->coeff(i) == 0);
				}
			}
			
			++jt; ++j;
		}
		
		++it; ++i;
	}
	
	int decodedCount = 0;
	uint32_t decodedSize = 0;
	
	it = mCombinations.begin();
	while(it != mCombinations.end())
	{
		if(it->componentsCount() == 0)	// Null vector, useless equation
		{
			mCombinations.erase(it++);
		}
		else {
			if(!it->isCoded()) 
			{
				++decodedCount;
				decodedSize+= it->size();
			}
			++it;
		}
	}
	
	LogDebug("Fountain::Sink::solve", "Total " + String::number(int(mCombinations.size())) + " combinations, " + String::number(decodedCount) + " decoded");
	return decodedSize >= mSize;
}

size_t Fountain::Sink::size(void) const
{
	return size_t(mSize);
}

bool Fountain::Sink::isComplete(void) const
{
	return mIsComplete; 
}

void Fountain::Sink::dump(Stream &stream) const
{
	List<Combination>::const_iterator it = mCombinations.begin();
	uint32_t left = mSize;
	while(left && it != mCombinations.end() && !it->isCoded())
	{
		size_t size = size_t(std::min(left, uint32_t(it->size())));
		stream.writeBinary(it->data(), size);
		left-= size;
		++it;
	}
	
	//if(left) throw Exception("Dumping failed: " + String::number(unsigned(left)) + " bytes missing in fountain sink");
}

void Fountain::Sink::hash(BinaryString &digest) const
{
	Sha256 hash;
	hash.init();
	
	List<Combination>::const_iterator it = mCombinations.begin();
	uint32_t left = mSize;
	while(left && it != mCombinations.end() && !it->isCoded())
	{
		size_t size = size_t(std::min(left, uint32_t(it->size())));
		hash.process(it->data(), size);
		left-= size;
		++it;
	}
	
	hash.finalize(digest);
	
	//if(left) throw Exception("Hashing failed: " + String::number(unsigned(left)) + " bytes missing in fountain sink");
}

void Fountain::Sink::clear(void)
{
	mCombinations.clear();
	mIsComplete = false;
}

}
