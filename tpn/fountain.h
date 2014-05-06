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
#include "tpn/binarystring.h"

namespace tpn
{

class Fountain : protected Synchronizable
{
public:
	Fountain(void);
	virtual ~Fountain(void);
	
	struct Combination
	{
		Combination(void) { this->offset = 0; }
		Combination(int64_t first) { this->first = first; this->coeffs.append(1); }
		~Combination(void);
		
		int64_t first;
		Array<uint8_t> coeffs;
		BinaryString data;
	};

	void generate(int64_t first, int64_t last, Combination &c);
	void generate(int64_t offset, Combination &c);	
	void solve(const Combination &c);

protected:
	virtual size_t readBlock(int64_t offset, char *buffer, size_t size) = 0;
	virtual void writeBlock(int64_t offset, const char *data, size_t size) = 0;
	virtual size_t hashBlock(int64_t offset, BinaryString &digest);
};

}

#endif
