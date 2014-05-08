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
#include "tpn/array.h"
#include "tpn/list.h"
#include "tpn/map.h"
#include "tpn/binarystring.h"

namespace tpn
{
	
class Fountain
{
public:
	static const size_t BlockSize = 1024;
	
	Fountain(void);
	virtual ~Fountain(void);
	
	class Combination : public Serializable
	{
	public:
		Combination(void);
		Combination(int64_t offset, const char *data, size_t size);
		~Combination(void);
		
		void addComponent(int64_t offset, uint8_t coeff);
		int64_t firstComponent(void) const;
		int64_t lastComponent(void) const;
		int64_t componentsCount(void) const;
		uint8_t  coeff(int64_t offset) const;
		bool isCoded(void) const;
		
		const char *data(void) const;
		size_t size(void) const;
		const char *decodedData(void) const;
		size_t decodedSize(void) const;
		
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
		
		Map<int64_t, uint8_t> mComponents;
		BinaryString mData;
	};

	int64_t generate(int64_t first, int64_t last, Combination &c);
	int64_t generate(int64_t offset, Combination &c);	
	void solve(const Combination &c);

protected:
	virtual size_t readBlock(int64_t offset, char *buffer, size_t size) = 0;
	virtual void writeBlock(int64_t offset, const char *data, size_t size) = 0;
	virtual bool checkBlock(int64_t offset) = 0;
	virtual size_t hashBlock(int64_t offset, BinaryString &digest);
	
	void init(void);
	
private:
	List<Combination> mCombinations;
	int64_t mNextDecoded, mNextSeen;
};

}

#endif
