/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef PLA_RANDOM_H
#define PLA_RANDOM_H

#include "pla/include.hpp"
#include "pla/stream.hpp"

#include <nettle/bignum.h>	// This will include nettle/version.h if it exists

namespace pla
{

class Random : public Stream
{
public:
	enum QualityLevel { Nonce, Crypto, Key };

	Random(QualityLevel mLevel = Nonce);
	~Random(void);

	void generate(char *buffer, size_t size) const;

	template<typename T> T uniform(T min, T max)
	{
		uint32_t i = 0;
		while(!i) readBinary(i);	// no EOF
		double t = double(i-1)/double(std::numeric_limits<uint32_t>::max());
		return min + T((max-min)*t);
	}

	inline unsigned uniformInt(void)
	{
		unsigned i;
		readBinary(i);
		return i;
	}

	inline double uniformDouble(void)
	{
		return uniform(0., 1.);
	}

	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

	// Wrappers for internal use, used in crypto.cpp
	// The size argument type changed from unsigned to size_t in nettle 3.0 (?!)
#if NETTLE_VERSION_MAJOR >= 3
	typedef size_t wrappersize_t;
#else
	typedef unsigned wrappersize_t;
#endif
	static void wrapperNonce (void *dummy, wrappersize_t size, uint8_t *buffer) { Random(Nonce).generate (reinterpret_cast<char*>(buffer), size_t(size)); }
	static void wrapperCrypto(void *dummy, wrappersize_t size, uint8_t *buffer) { Random(Crypto).generate(reinterpret_cast<char*>(buffer), size_t(size)); }
	static void wrapperKey   (void *dummy, wrappersize_t size, uint8_t *buffer) { Random(Key).generate   (reinterpret_cast<char*>(buffer), size_t(size)); }

private:
	QualityLevel mLevel;
};

}

#endif
