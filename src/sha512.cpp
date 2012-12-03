/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

/* 
 * This file is based on code under public domain from LibTomCrypt by
 * Tom St Denis, tomstdenis (at) gmail (dot) com, http://libtom.org/
 */

#include "sha512.h"
#include "exception.h"
#include "bytearray.h"

namespace tpot
{

const int Sha512::CryptRounds = 5000;
  
void Sha512::Hash(const char *data, size_t size, ByteStream &out, int rounds)
{
	// TODO: ConstByteArray ?
	ByteArray array(const_cast<char *>(data), size);
	Hash(array, out, rounds);
}

void Sha512::Hash(const String &str, ByteStream &out, int rounds)
{
	Hash(str.data(), str.size(), out, rounds);
}

size_t Sha512::Hash(ByteStream &data, ByteStream &out, int rounds)
{
	Assert(rounds > 0);
  
	unsigned char buffer[64];
	Sha512 sha;
	size_t size = sha.process(data);
	sha.finalize(buffer);
	
	while(--rounds)
	{
	  sha.init();
	  sha.process(reinterpret_cast<char*>(buffer),64);
	  sha.finalize(buffer);
	}
	
	out.writeData(reinterpret_cast<char*>(buffer),64);
	return size;
}

size_t Sha512::Hash(ByteStream &data, size_t size, ByteStream &out)
{
	Sha512 sha;
	size = sha.process(data, size);
	sha.finalize(out);
	return size;
}

/* Various macros */
#define ROR64c(x, y)															\
    ( ((((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((uint64_t)(y)&CONST64(63))) |		\
      ((x)<<((uint64_t)(64-((y)&CONST64(63)))))) & CONST64(0xFFFFFFFFFFFFFFFF))

#define STORE64H(x, y)																	\
   { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);	\
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);	\
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);	\
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); }

#define LOAD64H(x, y)															\
   { x = (((uint64_t)((y)[0] & 255))<<56)|(((uint64_t)((y)[1] & 255))<<48) |	\
         (((uint64_t)((y)[2] & 255))<<40)|(((uint64_t)((y)[3] & 255))<<32) |	\
         (((uint64_t)((y)[4] & 255))<<24)|(((uint64_t)((y)[5] & 255))<<16) |	\
         (((uint64_t)((y)[6] & 255))<<8)|(((uint64_t)((y)[7] & 255))); }

/* Various logical functions */
#define CH(x,y,z)       (z ^ (x & (y ^ z)))
#define MAJ(x,y,z)      (((x | y) & z) | (x & y)) 
#define S(x, n)         ROR64c(x, n)
#define R(x, n)         (((x)&CONST64(0xFFFFFFFFFFFFFFFF))>>((uint64_t)n))
#define SIGMA0(x)       (S(x, 28) ^ S(x, 34) ^ S(x, 39))
#define SIGMA1(x)       (S(x, 14) ^ S(x, 18) ^ S(x, 41))
#define GAMMA0(x)       (S(x, 1) ^ S(x, 8) ^ R(x, 7))
#define GAMMA1(x)       (S(x, 19) ^ S(x, 61) ^ R(x, 6))

/* the K array */
const uint64_t Sha512::K[80] = {
	CONST64(0x428a2f98d728ae22), CONST64(0x7137449123ef65cd), 
	CONST64(0xb5c0fbcfec4d3b2f), CONST64(0xe9b5dba58189dbbc),
	CONST64(0x3956c25bf348b538), CONST64(0x59f111f1b605d019), 
	CONST64(0x923f82a4af194f9b), CONST64(0xab1c5ed5da6d8118),
	CONST64(0xd807aa98a3030242), CONST64(0x12835b0145706fbe), 
	CONST64(0x243185be4ee4b28c), CONST64(0x550c7dc3d5ffb4e2),
	CONST64(0x72be5d74f27b896f), CONST64(0x80deb1fe3b1696b1), 
	CONST64(0x9bdc06a725c71235), CONST64(0xc19bf174cf692694),
	CONST64(0xe49b69c19ef14ad2), CONST64(0xefbe4786384f25e3), 
	CONST64(0x0fc19dc68b8cd5b5), CONST64(0x240ca1cc77ac9c65),
	CONST64(0x2de92c6f592b0275), CONST64(0x4a7484aa6ea6e483), 
	CONST64(0x5cb0a9dcbd41fbd4), CONST64(0x76f988da831153b5),
	CONST64(0x983e5152ee66dfab), CONST64(0xa831c66d2db43210), 
	CONST64(0xb00327c898fb213f), CONST64(0xbf597fc7beef0ee4),
	CONST64(0xc6e00bf33da88fc2), CONST64(0xd5a79147930aa725), 
	CONST64(0x06ca6351e003826f), CONST64(0x142929670a0e6e70),
	CONST64(0x27b70a8546d22ffc), CONST64(0x2e1b21385c26c926), 
	CONST64(0x4d2c6dfc5ac42aed), CONST64(0x53380d139d95b3df),
	CONST64(0x650a73548baf63de), CONST64(0x766a0abb3c77b2a8), 
	CONST64(0x81c2c92e47edaee6), CONST64(0x92722c851482353b),
	CONST64(0xa2bfe8a14cf10364), CONST64(0xa81a664bbc423001),
	CONST64(0xc24b8b70d0f89791), CONST64(0xc76c51a30654be30),
	CONST64(0xd192e819d6ef5218), CONST64(0xd69906245565a910), 
	CONST64(0xf40e35855771202a), CONST64(0x106aa07032bbd1b8),
	CONST64(0x19a4c116b8d2d0c8), CONST64(0x1e376c085141ab53), 
	CONST64(0x2748774cdf8eeb99), CONST64(0x34b0bcb5e19b48a8),
	CONST64(0x391c0cb3c5c95a63), CONST64(0x4ed8aa4ae3418acb), 
	CONST64(0x5b9cca4f7763e373), CONST64(0x682e6ff3d6b2b8a3),
	CONST64(0x748f82ee5defb2fc), CONST64(0x78a5636f43172f60), 
	CONST64(0x84c87814a1f0ab72), CONST64(0x8cc702081a6439ec),
	CONST64(0x90befffa23631e28), CONST64(0xa4506cebde82bde9), 
	CONST64(0xbef9a3f7b2c67915), CONST64(0xc67178f2e372532b),
	CONST64(0xca273eceea26619c), CONST64(0xd186b8c721c0c207), 
	CONST64(0xeada7dd6cde0eb1e), CONST64(0xf57d4f7fee6ed178),
	CONST64(0x06f067aa72176fba), CONST64(0x0a637dc5a2c898a6), 
	CONST64(0x113f9804bef90dae), CONST64(0x1b710b35131c471b),
	CONST64(0x28db77f523047d84), CONST64(0x32caab7b40c72493), 
	CONST64(0x3c9ebe0a15c9bebc), CONST64(0x431d67c49c100d4c),
	CONST64(0x4cc5d4becb3e42b6), CONST64(0x597f299cfc657e2a), 
	CONST64(0x5fcb6fab3ad6faec), CONST64(0x6c44198c4a475817)
};

Sha512::Sha512(void)
{
	init();
}

Sha512::~Sha512(void)
{
	
}

// Initialize the hash state
void Sha512::init(void)
{
	curlen = 0;
	length = 0;
	state[0] = CONST64(0x6a09e667f3bcc908);
	state[1] = CONST64(0xbb67ae8584caa73b);
	state[2] = CONST64(0x3c6ef372fe94f82b);
	state[3] = CONST64(0xa54ff53a5f1d36f1);
	state[4] = CONST64(0x510e527fade682d1);
	state[5] = CONST64(0x9b05688c2b3e6c1f);
	state[6] = CONST64(0x1f83d9abfb41bd6b);
	state[7] = CONST64(0x5be0cd19137e2179);
}

// Process a block of memory though the hash
void Sha512::process(const char *data, size_t size)
{
	Assert(data != NULL);
	Assert(curlen <= sizeof(buf));
	
	while (size)
	{
		if (curlen == 0 && size >= 128)
		{
			compress(reinterpret_cast<const unsigned char*>(data));
			
			length += 128 * 8;
			data+= 128;
			size-= 128;
		}
		else {
			size_t n = std::min(size, size_t(128 - curlen));
			std::memcpy(buf + curlen, data, n);
			curlen += n;
			data+= n;
			size-= n;
			if (curlen == 128)
			{
				compress(buf);
				length += 8*128;
				curlen = 0;
			}
		}
	}
}

// Process a stream through the hash
size_t Sha512::process(ByteStream &data)
{
	char buffer[BufferSize];
	size_t total = 0;
	size_t size;
	while((size = data.readData(buffer, BufferSize)))
	{
		total+= size;
		process(buffer, size);
	}
	return total;
}

// Process a stream through the hash
size_t Sha512::process(ByteStream &data, size_t max)
{
	char buffer[BufferSize];
	size_t left = max;
	size_t size;
	while(left && (size = data.readData(buffer, std::min(BufferSize,left))))
	{
		left-= size;
		process(buffer, size);
	}
	return max-left;
}

// Terminate the hash and get the digest
void Sha512::finalize(unsigned char *out)
{
	Assert(out != NULL);
	Assert(curlen < sizeof(buf));
	
	int i;
	
	/* increase the length of the message */
	length += curlen * CONST64(8);
	
	/* append the '1' bit */
	buf[curlen++] = 0x80;
	
	/* if the length is currently above 112 bytes we append zeros
	 * then compress.  Then we can fall back to padding zeros and length
	 * encoding like normal.
	 */
	if (curlen > 112)
	{
		while (curlen < 128)
			buf[curlen++] = 0;

		compress(buf);
		curlen = 0;
	}
	
	/* pad upto 120 bytes of zeroes 
	 * note: that from 112 to 120 is the 64 MSB of the length.  We assume that you won't hash
	 * > 2^64 bits of data... :-)
	 */
	while (curlen < 120)
		buf[curlen++] = 0;
	
	// store length */
	STORE64H(length, buf+120);
	compress(buf);
	
	// copy output
	for (i = 0; i < 8; i++)
		STORE64H(state[i], out+(8*i));
}

// Terminate the hash to get the digest
void Sha512::finalize(ByteStream &out)
{
	unsigned char temp[64];
	finalize(temp);
	out.writeData(reinterpret_cast<char*>(temp),64);
}

// Compress 1024-bits
void Sha512::compress(const unsigned char *buf)
{
	uint64_t S[8], W[80], t0, t1;
	int i;
	
	/* copy state into S */
	for (i = 0; i < 8; i++)
		S[i] = state[i];
	
	/* copy the state into 1024-bits into W[0..15] */
	for (i = 0; i < 16; i++)
		LOAD64H(W[i], buf + (8*i));
	
	/* fill W[16..79] */
	for (i = 16; i < 80; i++)
		W[i] = GAMMA1(W[i - 2]) + W[i - 7] + GAMMA0(W[i - 15]) + W[i - 16];      
	
	/* Compress */
	#define RND(a,b,c,d,e,f,g,h,i) \
		t0 = h + SIGMA1(e) + CH(e, f, g) + K[i] + W[i]; \
		t1 = SIGMA0(a) + MAJ(a, b, c); \
		d += t0; \
		h  = t0 + t1;
	
	for (i = 0; i < 80; i += 8) 
	{
		RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i+0);
		RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],i+1);
		RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],i+2);
		RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],i+3);
		RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],i+4);
		RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],i+5);
		RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],i+6);
		RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],i+7);
	}   
	
	/* feedback */
	for (i = 0; i < 8; i++)
		state[i] = state[i] + S[i];
}

}
