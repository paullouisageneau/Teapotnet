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

const size_t Fountain::ChunkSize;
	
uint8_t *Fountain::MulTable = NULL;
uint8_t *Fountain::InvTable = NULL;
  
void Fountain::Init(void)
{
	if(!MulTable) 
	{
		MulTable = new uint8_t[256*256];
		
		MulTable[0] = 0;
		for(uint8_t i = 1; i != 0; ++i) 
		{
			MulTable[unsigned(i)] = 0;
			MulTable[unsigned(i)*256] = 0;
			
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
				
				MulTable[unsigned(i)*256+unsigned(j)] = p;
			}
		}
	}
	
	if(!InvTable)
	{
		InvTable = new uint8_t[256];
		
		InvTable[0] = 0;
		for(uint8_t i = 1; i != 0; ++i)
		{
			for(uint8_t j = i; j != 0; ++j)
			{
				if(Fountain::gMul(i,j) == 1)	// then Fountain::gMul(j,i) == 1
				{
					InvTable[i] = j;
					InvTable[j] = i;
				}
			}
		}
	}
}

void Fountain::Cleanup(void)
{
	delete[] MulTable;
	delete[] InvTable;
	MulTable = NULL;
	InvTable = NULL;
}

uint8_t Fountain::gAdd(uint8_t a, uint8_t b)
{
	return a ^ b;
}

uint8_t Fountain::gMul(uint8_t a, uint8_t b) 
{
	return MulTable[unsigned(a)*256+unsigned(b)];
}

uint8_t Fountain::gInv(uint8_t a) 
{
	return InvTable[a];
}

Fountain::Generator::Generator(uint64_t seed) :
	mSeed(seed)
{

}

Fountain::Generator::~Generator(void)
{

}

uint8_t Fountain::Generator::next(void)
{
	if(!mSeed) return 1;
	
	uint8_t value;
	do {
		// Knuth's 64-bit linear congruential generator
		mSeed = uint64_t(mSeed*6364136223846793005 + 1442695040888963407);
		value = uint8_t(mSeed >> 56);
	}
	while(!mSeed || !value);	// zero is not a valid output

	return value;
}

Fountain::Combination::Combination(void) :
	mData(NULL),
	mSize(0),
	mNonce(0)
{
	
}

Fountain::Combination::Combination(const Combination &combination) :
	mData(NULL),
	mSize(0),
	mNonce(0)
{
	*this = combination;
}

Fountain::Combination::Combination(unsigned offset, const char *data, size_t size, bool last) :
	mData(NULL),
	mSize(0),
	mNonce(0)
{
	addComponent(offset, 1, data, size, last);
}

Fountain::Combination::~Combination(void)
{
	delete[] mData;
}

void Fountain::Combination::addComponent(unsigned offset, uint8_t coeff)
{
	Map<unsigned, uint8_t>::iterator it = mComponents.find(offset);
	if(it != mComponents.end())
	{
		it->second^= coeff;	// it->second = Fountain::gAdd(it->second, coeff);
		if(it->second == 0)
			mComponents.erase(it);
	}
	else {
		if(coeff != 0)
			mComponents.insert(offset, coeff);
	}
}

void Fountain::Combination::addComponent(unsigned offset, uint8_t coeff, const char *data, size_t size, bool last)
{
	addComponent(offset, coeff);
	
	if(!mSize && coeff == 1)
	{
		setData(data, size, last);
		return;
	}
	
	// Assure mData is the longest vector
	if(mSize < size+1) // +1 for padding
		resize(size+1, true);	// zerofill
	
	if(coeff == 1)
	{
		// Add values
		//for(unsigned i = 0; i < size; ++i)
		//	mData[i] = Fountain::gAdd(mData[i], data[i]);
	  
	  	// Faster
		memxor(mData, data, size);
		mData[size]^= (last ? 0x81 : 0x80); // 1-byte padding
	}
	else {
		if(coeff == 0) return;
		
		// Add values
		//for(unsigned i = 0; i < size; ++i)
		//	mData[i] = Fountain::gAdd(mData[i], Fountain::gMul(data[i], coeff));
		
		// Faster
		for(unsigned i = 0; i < size; ++i)
			mData[i]^= Fountain::gMul(data[i], coeff);
		
		mData[size]^= Fountain::gMul((last ? 0x81 : 0x80), coeff); // 1-byte padding
	}
}

void Fountain::Combination::setData(const char *data, size_t size, bool last)
{
	resize(size+1, false);	// +1 for padding
	std::copy(data, data + size, mData);
	mData[size] = (last ? 0x81 : 0x80);	// 1-byte ISO/IEC 7816-4 padding with fin flag
}

void Fountain::Combination::setData(const BinaryString &data, bool last)
{
	setData(data.data(), data.size(), last);
}

void Fountain::Combination::setCodedData(const char *data, size_t size)
{
	resize(size, false);
	std::copy(data, data + size, mData);
}

void Fountain::Combination::setCodedData(const BinaryString &data)
{
	setCodedData(data.data(), data.size());
}

uint64_t Fountain::Combination::seed(unsigned first, unsigned count)
{
	Assert(first <= std::numeric_limits<uint32_t>::max());
	Assert(count <= std::numeric_limits<uint16_t>::max());
	
	if(count <= 1) 
	{
		mNonce = 0;
		return 0;
	}
	
	while(!mNonce) Random().readBinary(mNonce);
	return (uint64_t(mNonce) << 48) | (uint64_t(count) << 32) | uint64_t(first);
}

unsigned Fountain::Combination::firstComponent(void) const
{
	if(!mComponents.empty()) return mComponents.begin()->first;
	else return 0;
}

unsigned Fountain::Combination::lastComponent(void) const
{
	if(!mComponents.empty()) return (--mComponents.end())->first;
	else return 0;
}

unsigned Fountain::Combination::componentsCount(void) const
{
	if(!mComponents.empty()) return (lastComponent() - firstComponent()) + 1;
	else return 0;
}

uint8_t Fountain::Combination::coeff(unsigned offset) const
{
	Map<unsigned, uint8_t>::const_iterator it = mComponents.find(offset);
	if(it == mComponents.end()) return 0; 
	return it->second;
}

bool Fountain::Combination::isCoded(void) const
{
	return (mComponents.size() != 1 || mComponents.begin()->second != 1);
}

bool Fountain::Combination::isNull(void) const
{
	return (mComponents.size() == 0);
}

bool Fountain::Combination::isLast(void) const
{
	if(!mSize || isCoded())
		return false;
	
	size_t size = mSize - 1;
	while(size && !mData[size])
		--size;
	
	if(mData[size] == char(0x80)) return false;
	else if(mData[size] == char(0x81)) return true;
	else throw Exception("Data corruption in fountain: invalid padding");
}

const char *Fountain::Combination::data(void) const
{
	return mData;
}

size_t Fountain::Combination::size(void) const
{
	if(!mSize || isCoded())
		return mSize;
	
	size_t size = mSize - 1;
	while(size && !mData[size])
		--size;
	
	if(mData[size] != char(0x80) && mData[size] != char(0x81))
		throw Exception("Data corruption in fountain: invalid padding");
	
	return size;
}

size_t Fountain::Combination::codedSize(void) const
{
	return mSize;
}


void Fountain::Combination::clear(void)
{
	mComponents.clear();
	delete[] mData;
	mData = NULL;
	mSize = 0;
	mNonce = 0;
}

Fountain::Combination &Fountain::Combination::operator=(const Combination &combination)
{
	mComponents = combination.mComponents;
	resize(combination.mSize);
	std::copy(combination.mData, combination.mData + combination.mSize, mData);
	return *this;
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
	//for(size_t i = 0; i < combination.mSize; ++i)
	//	mData[i] = Fountain::gAdd(mData[i], combination.mData[i]);

	// Faster
	memxor(mData, combination.mData, combination.mSize);
	
	// Add components
	for(	Map<unsigned, uint8_t>::const_iterator jt = combination.mComponents.begin();
		jt != combination.mComponents.end();
		++jt)
	{
		addComponent(jt->first, jt->second);
	}
	
	return *this;
}

Fountain::Combination &Fountain::Combination::operator*=(uint8_t coeff)
{
	if(coeff != 1)
	{
		if(coeff != 0)
		{
			// Multiply vector
			for(size_t i = 0; i < mSize; ++i)
				mData[i] = Fountain::gMul(mData[i], coeff);

			for(	Map<unsigned, uint8_t>::iterator it = mComponents.begin();
				it != mComponents.end();
				++it)
			{
				it->second = Fountain::gMul(it->second, coeff);
			}
		}
		else {
			std::fill(mData, mData + mSize, 0);
			mComponents.clear();
		}
	}
	
	return *this;
}

Fountain::Combination &Fountain::Combination::operator/=(uint8_t coeff)
{
	Assert(coeff != 0);

	(*this)*= Fountain::gInv(coeff);
	return *this;
}

void Fountain::Combination::serialize(Serializer &s) const
{
	Assert(firstComponent() <= std::numeric_limits<uint32_t>::max());
	Assert(componentsCount() <= std::numeric_limits<uint16_t>::max());
	
	// 64-bit combination descriptor
	s.write(uint32_t(firstComponent()));	// 32-bit first
	s.write(uint16_t(componentsCount()));	// 16-bit count
	s.write(mNonce);			// 16-bit nonce
}

bool Fountain::Combination::deserialize(Serializer &s)
{
	clear();
	
	uint32_t first = 0;
	uint16_t count = 0;
	
	// 64-bit combination descriptor
	if(!s.read(first)) return false;	// 32-bit first
	AssertIO(s.read(count));		// 16-bit count
	AssertIO(s.read(mNonce));		// 16-bit nonce
	
	Generator gen(seed(first, count));
	for(unsigned i=0; i<count; ++i)
	{
		uint8_t c = gen.next();
		addComponent(first + i, c);
	}

	return true;
}

void Fountain::Combination::resize(size_t size, bool zerofill)
{
	if(mSize != size)
	{
		char *newData = new char[size];
		std::copy(mData, mData + std::min(mSize, size), newData);
		if(zerofill && size > mSize)
			std::fill(newData + mSize, newData + size, 0);
		
		delete[] mData;
		mData = newData;
		mSize = size;
	}
}

Fountain::DataSource::DataSource(void) :
	mFirstComponent(0),
	mCurrentComponent(0)
{
	
}

Fountain::DataSource::~DataSource(void)
{
	
}

unsigned Fountain::DataSource::write(const char *data, size_t size)
{
	// size should be a multiple of ChuckSize
	
	unsigned count = 0;
	while(size)
	{
		size_t len = std::min(size, ChunkSize);
		mComponents.push_back(BinaryString(data, len));
		++mCurrentComponent;
		++count;
		data+= len;
		size-= len;
	}
	
	return count;
}

unsigned Fountain::DataSource::rank(void) const
{
	return unsigned(mComponents.size());
}

bool Fountain::DataSource::generate(Combination &result)
{
	result.clear();
	if(mComponents.empty())
		return true;
	
	unsigned first = Random().uniform(unsigned(0), unsigned(mComponents.size()) + GenerateSize);
	unsigned count = std::min(unsigned(mComponents.size()), GenerateSize);
	if(first < GenerateSize) first = 0;
	else first-= GenerateSize;
	if(first > mComponents.size() - count)
		first = mComponents.size() - count;
	
	first+= mFirstComponent;	// real offset
	
	Generator gen(result.seed(first, count));
	
	// TODO
	unsigned i = first;
	for(List<BinaryString>::iterator it = mComponents.begin();
		it != mComponents.end() && result.componentsCount() < count;
		++it)
	{
		uint8_t coeff = gen.next();
		result.addComponent(i, coeff, it->data(), it->size());
		++i;
	}
	
	//Assert(result.componentsCount() == mComponents.size());
	//LogDebug("Fountain::Dataource::generate", "Generated combination (first=" + String::number(result.firstComponent()) + ", count=" + String::number((result.componentsCount())) + ")");
	return true;
}

unsigned Fountain::DataSource::drop(unsigned nextSeen)
{
	unsigned dropped = 0;
	while(!mComponents.empty() && nextSeen > mFirstComponent)
	{
		++mFirstComponent;
		++dropped;
		mComponents.pop_front();
	}
	
	mCurrentComponent = std::max(mCurrentComponent, mFirstComponent);
	return dropped;
}

Fountain::FileSource::FileSource(File *file, int64_t offset, int64_t size) :
	mFile(file),
	mOffset(offset),
	mSize(size)
{
	Assert(mFile);
	Assert(mSize >= 0);
}

Fountain::FileSource::~FileSource(void)
{
	delete mFile;
}

unsigned Fountain::FileSource::rank(void) const
{
	return mSize/ChunkSize + (mSize % ChunkSize ? 1 : 0);
}

bool Fountain::FileSource::generate(Combination &result)
{
	const unsigned chunks = rank();
	
	result.clear();
	if(!chunks)
		return true;
	
	unsigned first = Random().uniform(unsigned(0), chunks + GenerateSize);
	unsigned count = std::min(chunks, GenerateSize);
	if(first < GenerateSize) first = 0;
	else first-= GenerateSize;
	if(first > chunks - count)
		first = chunks - count;
	
	// Seek
	mFile->seekRead(mOffset + first*ChunkSize);
	uint32_t left = uint32_t(mSize) - first*ChunkSize;
	
	// Generate
	Generator gen(result.seed(first, count));
	unsigned i = first;
	char buffer[ChunkSize];
	size_t size;
	for(unsigned i=0; i<count; ++i)
	{
		size = mFile->readBinary(buffer, size_t(std::min(uint32_t(ChunkSize), left)));
		Assert(size > 0);
		
		uint8_t coeff = gen.next();
		result.addComponent(first+i, coeff, buffer, size, (first+i == chunks-1));
		left-= size;
	}
	
	//LogDebug("Fountain::FileSource::generate", "Generated combination (first=" + String::number(result.firstComponent()) + ", count=" + String::number(result.componentsCount()) + ")");
	return true;
}
		
Fountain::Sink::Sink(void) :
	mNextDiscovered(0),
	mNextSeen(0),
	mNextDecoded(0),
	mNextRead(0),
	mDropped(0),
	mFinished(false),
	mAlreadyRead(0)
{

}

Fountain::Sink::~Sink(void)
{
  
}

int64_t Fountain::Sink::solve(Combination &incoming)
{
	//LogDebug("Fountain::Sink::solve", "Incoming combination (first=" + String::number(incoming.firstComponent()) + ", count=" + String::number(incoming.componentsCount()) + ")");
	
	if(mFinished) return true;
	
	if(incoming.isNull()) return false;
	
	mNextDiscovered = std::max(mNextDiscovered, incoming.lastComponent() + 1);
	
	// ==== Gauss-Jordan elimination ====
	
	Map<unsigned, Combination>::iterator it, jt;
	Map<unsigned, Combination>::reverse_iterator rit;
	
	// Eliminate coordinates, so the system is triangular
	for(unsigned i = incoming.firstComponent(); i <= incoming.lastComponent(); ++i)
	{
		uint8_t c = incoming.coeff(i);
		if(c != 0)
		{
			jt = mCombinations.find(i);
			if(jt == mCombinations.end()) break;
			incoming+= jt->second*c;
		}
	}
	
	if(incoming.isNull())
	{
		//LogDebug("Fountain::Sink::solve", "Incoming combination is redundant");
		return false;
	}
	
	// Insert incoming combination
	incoming/= incoming.coeff(incoming.firstComponent());
	mCombinations.insert(incoming.firstComponent(), incoming);
	
	// Attempt to substitute to solve
	rit = mCombinations.rbegin();
	while(rit != mCombinations.rend())
	{
		unsigned first = std::max(rit->second.firstComponent(), rit->first);
		for(unsigned i = rit->second.lastComponent(); i > first; --i)
		{
			jt = mCombinations.find(i);
			if(jt != mCombinations.end())
			{
				if(jt->second.isCoded()) break;
				rit->second+= jt->second*rit->second.coeff(i);
			}
		}

		if(rit->second.lastComponent() != rit->first)
			break;
		
		++rit;
	}
	
	// Remove null components and count decoded
	int64_t total = 0;
	it = mCombinations.begin();
	while(it != mCombinations.end())
	{
		if(it->second.isNull())	// Null vector, useless equation
		{
			mCombinations.erase(it++);
		}
		else {
			Assert(it->second.firstComponent() == it->first);
			mNextSeen = std::max(mNextSeen, it->first + 1);
			if(mNextDecoded == it->first && !it->second.isCoded()) 
			{
				total+= it->second.size();
				++mNextDecoded;
				
				if(it->second.isLast())
					mFinished = true;
			}
			++it;
		}
	}
	
	// ==================================
	
	//LogDebug("Fountain::Sink::solve", "Total " + String::number(int(mCombinations.size())) + " combinations, next seen " + String::number(mNextSeen) + ", next decoded " + String::number(mNextDecoded));
	return total;
}

unsigned Fountain::Sink::drop(unsigned firstIncoming)
{
	// Remove old combinations
	unsigned count = 0;
	Map<unsigned, Combination>::iterator it = mCombinations.begin();
	while(it != mCombinations.end() && it->first+1 < mNextRead && it->first < firstIncoming)
	{
		mCombinations.erase(it++);
		++count;
	}
	
	mDropped+= count;
	return count;
}

void Fountain::Sink::clear(void)
{
	mCombinations.clear();
	mNextDiscovered = 0;
	mNextSeen = 0;
	mNextDecoded = 0;
	mNextRead = 0;
	mDropped = 0;
	mFinished = false;
	mAlreadyRead = 0;
}

unsigned Fountain::Sink::rank(void) const
{
	return mCombinations.size();
}

unsigned Fountain::Sink::missing(void) const
{
	return mNextDiscovered - (mCombinations.size() + mDropped);
}

unsigned Fountain::Sink::nextSeen(void) const
{
	return mNextSeen;
}

unsigned Fountain::Sink::nextDecoded(void) const
{
	return mNextDecoded;
}

bool Fountain::Sink::isDecoded(void) const
{
	return mFinished;
}

size_t Fountain::Sink::read(char *buffer, size_t size)
{
	int64_t total = 0;
	Map<unsigned,Combination>::const_iterator it = mCombinations.lower_bound(mNextRead);
	if(it != mCombinations.end() && it->first == mNextRead && !it->second.isCoded())
	{
		size_t s = it->second.size();	// unpad
		size = std::min(size, s - mAlreadyRead);
		std::memcpy(buffer, it->second.data() + mAlreadyRead, size);
		mAlreadyRead+= size;
		
		if(mAlreadyRead == s)
		{
			mAlreadyRead = 0;
			++mNextRead;
		}
		
		return size;
	}
	
	return 0;
}

int64_t Fountain::Sink::dump(Stream &stream) const
{
	int64_t total = 0;
	Map<unsigned,Combination>::const_iterator it = mCombinations.begin();
	while(it != mCombinations.end() && !it->second.isCoded())
	{
		size_t s = it->second.size();	// unpad
		stream.writeData(it->second.data(), s);
		total+= s;
		++it;
	}
	
	return total;
}

int64_t Fountain::Sink::hash(BinaryString &digest) const
{
	Sha256 hash;
	hash.init();
	
	int64_t total = 0;
	Map<unsigned,Combination>::const_iterator it = mCombinations.begin();
	while(it != mCombinations.end() && !it->second.isCoded())
	{
		size_t s = it->second.size();	// unpad
		hash.process(it->second.data(), s);
		total+= s;
		++it;
	}
	
	hash.finalize(digest);
	return total;
}

}
