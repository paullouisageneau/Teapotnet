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

#ifndef TPN_RANDOM_H
#define TPN_RANDOM_H

#include "tpn/include.h"
#include "tpn/stream.h"

namespace tpn
{

class Random : Stream
{
public:
	enum QualityLevel { Nonce, Crypto, Key };
	
	Random(QualityLevel mLevel = Nonce);
	~Random(void);
	
	void generate(char *buffer, size_t size) const;
	
	template<typename T> T uniform(T min, T max)
	{
		uint32_t i;
		read(i);	// no EOF
		double t = double(i)/double(std::numeric_limits<uint32_t>::max());
		return min + T((max-min)*t);
	}
	
	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	
	// Wrappers for internal use, used in crypto.cpp
	static void wrapperNonce (void *dummy, unsigned int size, uint8_t *buffer) { Random(Nonce).generate (reinterpret_cast<char*>(buffer), size_t(size)); }
	static void wrapperCrypto(void *dummy, unsigned int size, uint8_t *buffer) { Random(Crypto).generate(reinterpret_cast<char*>(buffer), size_t(size)); }
	static void wrapperKey   (void *dummy, unsigned int size, uint8_t *buffer) { Random(Key).generate   (reinterpret_cast<char*>(buffer), size_t(size)); }
	
private:
	QualityLevel mLevel;
};

}

#endif

