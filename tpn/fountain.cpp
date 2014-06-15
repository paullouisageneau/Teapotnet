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
#include "tpn/random.h"

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
		mSeed = uint(mSeed*6364136223846793005 + 1442695040888963407);		
		value = uint8_t(mSeed >> 56);
	}
	while(!value);	// zero is not a valid output

	return value;
}

Fountain::Combination::Combination(void)
{
	
}

Fountain::Combination::Combination(int offset, const char *data, size_t size)
{
	addComponent(offset, 1);
	setData(data, size);
}

Fountain::Combination::~Combination(void)
{
	
}

void Fountain::Combination::addComponent(int offset, uint8_t coeff)
{
	if(offset == 0) return;
	
	Map<int, uint8_t>::iterator it = mComponents.find(offset);
	if(it != mComponents.end())
	{
		it->second = gAdd(it->second, coeff);
		if(it->second == 0) mComponents.erase(it);
	}
	else {
		mComponents.insert(offset, coeff);
	}
}

void Fountain::Combination::setData(const char *data, size_t size)
{
	mData.assign(data, size);
}

void Fountain::Combination::setData(const BinaryString &data)
{
	mData = data;
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
	return mData.data();
}

size_t Fountain::Combination::size(void) const
{
	return mData.size();
}

void Fountain::Combination::clear(void)
{
	mComponents.clear();
	mData.clear();
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
	BinaryString other(combination.mData);

	// Assure mData is the longest vector
	if(mData.size() < other.size())
		mData.swap(other);
	
	// Add values from the other (smallest) vector
	for(unsigned i = 0; i < other.size(); ++i)
		mData[i] = gAdd(mData[i], other[i]);

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
	for(unsigned i = 0; i < mData.size(); ++i)
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
	uint8_t p = 0;
	uint8_t i;
	uint8_t carry;
	for(i = 0; i < 8; ++i) 
	{
		if (b & 1) p ^= a;
		carry = (a & 0x80);
		a <<= 1;
		if (carry) a ^= 0x1b; // 0x1b is x^8 modulo x^8 + x^4 + x^3 + x + 1
		b >>= 1;
	}
	
	return p;
}

uint8_t Fountain::Combination::gInv(uint8_t a) 
{
	Assert(a != 0);

	// TODO: table	
	uint8_t b = 1;
	while(b)
	{
		if(gMul(a,b) == 1) return b;
		++b;
	}
	
	throw Exception("Combination::gInv failed for input " + String::number(unsigned(a)));
}
  
Fountain::Source::Source(File *file, int64_t offset, int64_t size) :
	mFile(file),
	mOffset(offset),
	mSize(size)
{
	Assert(mFile);
}

Fountain::Source::~Source(void)
{
	delete mFile;
}

void Fountain::Source::generate(Stream &output)
{
	mFile->seekRead(mOffset);
  
	uint32_t seed = 0;
	Random rnd;
	rnd.readBinary(seed);
	Generator gen(seed);

	Combination c;
	char buffer[ChunkSize];
	size_t size;
	int i = 0;
	
	if(mSize > std::numeric_limits<uint32_t>::max())
		throw Exception("File too big for Foutain::Source");
	
	uint32_t total = 0;
	uint32_t left = (mSize >= 0 ? uint32_t(mSize) : std::numeric_limits<uint32_t>::max());

	while((size = mFile->readBinary(buffer, size_t(std::min(uint32_t(ChunkSize), left)))))
	{
		uint8_t coeff = gen.next();
		c+= Combination(i, buffer, size)*coeff;
		total+= size;
		left-= size;
		++i;
	}
	
	output.writeBinary(uint32_t(total));
	output.writeBinary(uint32_t(seed));
	output.writeBinary(uint16_t(i));
	output.writeBinary(c.data(), c.size());
}
		
Fountain::Sink::Sink(void) :
	mSize(0)
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
	uint16_t count;
	BinaryString data;
	AssertIO(input.readBinary(size));
	AssertIO(input.readBinary(seed));
	AssertIO(input.readBinary(count));
	AssertIO(input.readBinary(data));
	
	mSize = std::max(mSize, size);
	
	Combination c;
	c.setData(data);
	
	Generator gen(seed);
	for(int i=0; i<count; ++i)
	{
	  	uint8_t coeff = gen.next();
		c.addComponent(i, coeff);
	}
	
	mCombinations.push_back(c);
  
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
		Assert(it->coeff(i) == 1);
		
		// Suppress coordinate i in each equation
		int j = 0;			// secondary equation index
		jt = mCombinations.begin();	// secondary equation
		while(jt != mCombinations.end())
		{
			if(it == jt)
			{
				++jt; ++j;
				continue;
			}
			
			uint8_t c = jt->coeff(i);
			if(c)	// if term not supressed
			{
				// it->coeff(i) == 1 here
				(*jt)+= (*it)*c;
				Assert(jt->coeff(i) == 0);
			}
			
			++jt; ++j;
		}
		
		++it; ++i;
	}
	
	int decodedCount = 0;
	it = mCombinations.begin();
	while(it != mCombinations.end())
	{
		if(it->componentsCount() == 0)	// Null vector, useless equation
		{
			mCombinations.erase(it++);
		}
		else {
		  	if(!it->isCoded()) ++decodedCount;
			++it;
		}
	}
	
	mIsComplete = (decodedCount == int(count));
	return mIsComplete;
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
	}
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
	}
	
	hash.finalize(digest);
}

void Fountain::Sink::clear(void)
{
	mCombinations.clear();
	mIsComplete = false;
}

}
