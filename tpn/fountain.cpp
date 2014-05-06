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

namespace tpn
{

Fountain::Fountain(void)
{
	// TODO
}

Fountain::~Fountain(void)
{
	
}

void Fountain::generate(int64_t first, int64_t last, Combination &c)
{
	// TODO
}

void Fountain::generate(int64_t offset, Combination &c)
{
	// TODO
}

void Fountain::solve(const Combination &c)
{
	List<Combination>::iterator it;	// current equation

	// TODO: use readBlock to remove decoded combinations in elimination
	
	uint32_t offset = 0;
	if(!mCombinations.empty())
	{
		// Get index offset
		it = mCombinations.begin();
		offset = it->firstComponent();
		++it;
		while(it != mCombinations.end())
		{
			offset = std::min(offset, it->firstComponent());	
			++it;
		}
	}

	mCombinations.push_back(c);

	// Gauss-Jordan elimination
	it = mCombinations.begin();	// pivot equation
	uint32_t i = offset;		// pivot equation index
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
		uint32_t j = 0;			// secondary equation index
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
	
	// Remove null vectors
	it = mCombinations.begin();
	while(it != mCombinations.end())
	{
		if(it->componentsCount() == 0)	// Null vector, useless equation
		{
			mCombinations.erase(it++);
		}
		else ++it;
	}
	
	it = mCombinations.begin();	// current equation
	while(it != mCombinations.end())
	{
		uint32_t first = it->firstComponent();
		
		// Seen packets are not reported if decoding buffer is full
		if(first >= m_nextSeen)
		{
			m_nextSeen = first + 1;
		}
		
		if(first == m_nextDecoded && it->componentsCount() == 1)
		{
			uint8_t c = it->coeff(first);
			if(c != 1) (*it)/= c;

			// TODO: writeBlock
			
			m_nextDecoded = first + 1;
		}		

		++it;
	}
}

size_t Fountain::hashBlock(int64_t offset, BinaryString &digest)
{
	
}

Fountain::Combination::Combination(void)
{
	
}

Fountain::Combination::~Combination(void)
{
	
}

void Fountain::Combination::addComponent(int64_t i, uint8_t coeff)
{
	if(i == 0) return;
	
	Map<uint64_t, uint8_t>::iterator it = mComponents.find(i);
	if(it != mComponents.end())
	{
		it->second = gAdd(it->second, coefficient);
		if(it->second == 0) mComponents.erase(it);
	}
	else {
		mComponents[i] = coeff;
	}
}

uint64_t Fountain::Combination::firstComponent(void)
{
	if(!mComponents.empty()) return mComponents.begin()->first;
	else return 0;
}

uint64_t Fountain::Combination::lastComponent(void)
{
	if(!mComponents.empty()) return (--mComponents.end())->first;
	else return 0;
}

uint64_t Fountain::Combination::componentsCount(void) const
{
	if(!mComponents.empty()) return (lastComponent() - firstComponent()) + 1;
	else return 0;
}

uint8_t Fountain::Combination::coeff(int64_t i)
{
	Map<uint32_t,uint8_t>::const_iterator it = mComponents.find(i);
	if(it == mComponents.end()) return 0; 
	
	Assert(it->second != 0);
	return it->second;
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
	for(	Map<uint32_t, uint8_t>::const_iterator jt = combination.mComponents.begin();
		jt != combination.mComponents.end();
		++jt)
	{
		AddComponent(jt->first, jt->second);
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

	for(	Map<uint32_t, uint8_t>::iterator it = mComponents.begin();
		it != mComponents.end();
		++it)
	{
		it->second = gMul(it->second, coeff);
	}

	return *this;
}

Fountain::Combination &Fountain::Combination::operator/=(uint8_t coeff)
{
	NS_ASSERT(coeff != 0);

	(*this)*= gInv(coeff);
	return *this;
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
	NS_ASSERT(a != 0);
	
	uint8_t b = 1;
	while(b)
	{
		if(gMul(a,b) == 1) return b;
		++b;
	}
	
	throw Exception("Combination::gInv failed for input " + String::number(unsigned(a)));
}

}
