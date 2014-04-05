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

#include "tpn/crypto.h"
#include "tpn/random.h"
#include "tpn/exception.h"
#include "tpn/binaryserializer.h"

#include <nettle/hmac.h>
#include <nettle/pbkdf2.h>

#include <gmp.h>

namespace tpn
{

void Hash::compute(const char *data, size_t size, char *digest)
{
	init();
	process(data, size);
	finalize(digest);
}
        
void Hash::compute(const char *data, size_t size, BinaryString &digest)
{
	digest.resize(length());
        compute(data, size, digest.ptr());
}

int64_t Hash::compute(Stream &stream, char *digest)
{
	init();

	char buffer[BufferSize];
	size_t size;
	int64_t total = 0;
	while(size = stream.readData(buffer, BufferSize))
	{
		process(buffer, size);
		total+= size;
	}
	
	finalize(digest);
	return total;
}

int64_t Hash::compute(Stream &stream, BinaryString &digest)
{
	digest.resize(length());
        return compute(stream, digest.ptr());
}

int64_t Hash::compute(Stream &stream, int64_t max, char *digest)
{
        init();

        char buffer[BufferSize];
        size_t size;
	int64_t left = max;
        while(left && (size = stream.readData(buffer, size_t(std::min(int64_t(BufferSize), left)))))
        {
                process(buffer, size);
		left-= size;
        }

        finalize(digest);
	return max - left;
}

int64_t Hash::compute(Stream &stream, int64_t max, BinaryString &digest)
{
        digest.resize(length());
        return compute(stream, max, digest.ptr());
}

size_t Sha256::length(void) const
{
	return size_t(SHA256_DIGEST_SIZE);
}

void Sha256::init(void)
{
	sha256_init(&mCtx);
}

void Sha256::process(const char *data, size_t size)
{
	sha256_update(&mCtx, unsigned(size), reinterpret_cast<const uint8_t*>(data));
}

void Sha256::process(const BinaryString &str)
{
	process(str.data(), str.size());
}

void Sha256::finalize(char *digest)
{
	sha256_digest(&mCtx, unsigned(length()), reinterpret_cast<uint8_t*>(digest));
}

void Sha256::finalize(BinaryString &digest)
{
	digest.resize(length());
	finalize(digest.ptr());
}

void Sha256::hmac(const char *message, size_t len, const char *key, size_t key_len, char *digest)
{
	struct hmac_sha256_ctx ctx;
	hmac_sha256_set_key(&ctx, key_len, reinterpret_cast<const uint8_t*>(key));
	hmac_sha256_update(&ctx, len, reinterpret_cast<const uint8_t*>(message));
	hmac_sha256_digest(&ctx, length(), reinterpret_cast<uint8_t*>(digest));
}

void Sha256::hmac(const BinaryString &message, const BinaryString &key, BinaryString &digest)
{
	digest.resize(length());
	hmac(message.data(), message.size(), key.data(), key.size(), digest.ptr());
}

void Sha256::pbkdf2_hmac(const char *secret, size_t len, const char *salt, size_t salt_len, char *key, size_t key_len, unsigned iterations)
{
	Assert(iterations != 0);
	pbkdf2_hmac_sha256(len, reinterpret_cast<const uint8_t*>(secret),
				iterations,
				salt_len, reinterpret_cast<const uint8_t*>(salt), 
				key_len, reinterpret_cast<uint8_t*>(key));
}

void Sha256::pbkdf2_hmac(const BinaryString &secret, const BinaryString &salt, BinaryString &key, size_t key_len, unsigned iterations)
{
	key.resize(key_len);
	pbkdf2_hmac(secret.data(), secret.size(), salt.data(), salt.size(), key.ptr(), key.size(), iterations);
}

size_t Sha512::length(void) const
{
        return size_t(SHA512_DIGEST_SIZE);
}

void Sha512::init(void)
{
        sha512_init(&mCtx);
}

void Sha512::process(const char *data, size_t size)
{
        sha512_update(&mCtx, unsigned(size), reinterpret_cast<const uint8_t*>(data));
}

void Sha512::process(const BinaryString &str)
{
	process(str.data(), str.size());
}

void Sha512::finalize(char *digest)
{
        sha512_digest(&mCtx, unsigned(length()), reinterpret_cast<uint8_t*>(digest));
}

void Sha512::finalize(BinaryString &digest)
{
	digest.resize(length());
	finalize(digest.ptr());
}

void Sha512::hmac(const char *message, size_t len, const char *key, size_t key_len, char *digest)
{
	struct hmac_sha512_ctx ctx;
	hmac_sha512_set_key(&ctx, key_len, reinterpret_cast<const uint8_t*>(key));
	hmac_sha512_update(&ctx, len, reinterpret_cast<const uint8_t*>(message));
	hmac_sha512_digest(&ctx, length(), reinterpret_cast<uint8_t*>(digest));
}

void Sha512::hmac(const BinaryString &message, const BinaryString &key, BinaryString &digest)
{
	digest.resize(length());
	hmac(message.data(), message.size(), key.data(), key.size(), digest.ptr());
}

void Sha512::pbkdf2_hmac(const char *secret, size_t len, const char *salt, size_t salt_len, char *key, size_t key_len, unsigned iterations)
{
	Assert(iterations != 0);
	
	struct hmac_sha512_ctx ctx;
	hmac_sha512_set_key(&ctx, len, reinterpret_cast<const uint8_t*>(secret));
	PBKDF2(&ctx, hmac_sha512_update, hmac_sha512_digest, 
		64, iterations,
		salt_len, reinterpret_cast<const uint8_t*>(salt),
		key_len, reinterpret_cast<uint8_t*>(key));
}

void Sha512::pbkdf2_hmac(const BinaryString &secret, const BinaryString &salt, BinaryString &key, size_t key_len, unsigned iterations)
{
	key.resize(key_len);
	pbkdf2_hmac(secret.data(), secret.size(), salt.data(), salt.size(), key.ptr(), key.size(), iterations);
}

Rsa::PublicKey::PublicKey(void)
{
	rsa_public_key_init(&mKey);
}

Rsa::PublicKey::PublicKey(const Rsa::PublicKey &key)
{
	rsa_public_key_init(&mKey);
	*this = key;
}

Rsa::PublicKey::~PublicKey(void)
{
	rsa_public_key_clear(&mKey);
}

Rsa::PublicKey &Rsa::PublicKey::operator=(const Rsa::PublicKey &key)
{
	mKey.size = key.mKey.size;
	mpz_set(mKey.n, key.mKey.n);
	mpz_set(mKey.e, key.mKey.e);
	return *this;
}

bool Rsa::PublicKey::verify(const BinaryString &digest, const BinaryString &signature) const
{
	if(digest.empty())
		throw Exception("Empty digest used for RSA verification");
	
	mpz_t s;
	mpz_init(s);
	
	int ret = 0;
	try {
		mpz_import_binary(s, signature);
		
		switch(digest.size()*8)
		{
			case 256: ret = rsa_sha256_verify_digest(&mKey, digest.bytes(), s); break;
			case 512: ret = rsa_sha512_verify_digest(&mKey, digest.bytes(), s); break;
			default: throw Exception("Incompatible digest used for RSA signature"); break;
		}
	}
	catch(...)
	{
		mpz_clear(s);
		throw;
	}
	
	mpz_clear(s);
	return (ret != 0);
}

void Rsa::PublicKey::serialize(Serializer &s) const
{
	BinaryString e, n;
	
	mpz_export_binary(mKey.e, e);
	mpz_export_binary(mKey.n, n);
	
	s.output(e);
	s.output(n);
}

bool Rsa::PublicKey::deserialize(Serializer &s)
{
	BinaryString e, n;
	
	if(!s.input(e)) return false;
	AssertIO(s.input(n));
	
	try {
		mpz_import_binary(mKey.e, e);
		mpz_import_binary(mKey.n, n);
		if(!rsa_public_key_prepare(&mKey))
			throw Exception("Invalid parameters");
	}
	catch(const Exception &e)
	{
		throw InvalidData(String("RSA public key: ") + e.what());
	}
	
	return true;
}

void Rsa::PublicKey::serialize(Stream &s) const
{
	BinaryString bs;
	BinarySerializer serializer(&bs);
	
	serialize(serializer);
	String str(bs.base64Encode());
	s.write(str);
}

bool Rsa::PublicKey::deserialize(Stream &s)
{
	String str;
	if(!s.read(str)) return false;
	
	try {
		BinaryString bs(str.base64Decode());
		BinarySerializer serializer(&bs);
		AssertIO(deserialize(serializer));
	}
	catch(const InvalidData &e)
	{
		throw;
	}
	catch(const Exception &e)
	{
		throw InvalidData(String("Invalid RSA public key: ") + e.what());
	}
	
	return true;
}

Rsa::PrivateKey::PrivateKey(void)
{
	rsa_private_key_init(&mKey);
}

Rsa::PrivateKey::PrivateKey(const PrivateKey &key)
{
	rsa_private_key_init(&mKey);
	*this = key;
}

Rsa::PrivateKey::~PrivateKey(void)
{
	rsa_private_key_clear(&mKey);
}

Rsa::PrivateKey &Rsa::PrivateKey::operator=(const Rsa::PrivateKey &key)
{
	mKey.size = key.mKey.size;
	mpz_set(mKey.d, key.mKey.d);
	mpz_set(mKey.p, key.mKey.p);
	mpz_set(mKey.q, key.mKey.q);
	mpz_set(mKey.a, key.mKey.a);
	mpz_set(mKey.b, key.mKey.b);
	mpz_set(mKey.c, key.mKey.c);
}

void Rsa::PrivateKey::sign(const BinaryString &digest, BinaryString &signature) const
{
	if(digest.empty())
		throw Exception("Empty digest used for RSA signature");
	
	mpz_t s;
	mpz_init(s);
	
	try {
		int ret;
		switch(digest.size()*8)
		{
			case 256: ret = rsa_sha256_sign_digest(&mKey, digest.bytes(), s); break;
			case 512: ret = rsa_sha512_sign_digest(&mKey, digest.bytes(), s); break;
			default: throw Exception("Incompatible digest used for RSA signature"); break;
		}
	
		if(!ret) throw Exception("RSA signature failed");
		
		mpz_export_binary(s, signature);
	}
	catch(...)
	{
		mpz_clear(s);
		throw;
	}

	mpz_clear(s);
}

void Rsa::PrivateKey::serialize(Serializer &s) const
{
	BinaryString d, p, q, a, b, c;
	
	mpz_export_binary(mKey.d, d);
	mpz_export_binary(mKey.p, p);
	mpz_export_binary(mKey.q, q);
	mpz_export_binary(mKey.a, a);
	mpz_export_binary(mKey.b, b);
	mpz_export_binary(mKey.c, c);
	
	s.output(d);
	s.output(p);
	s.output(q);
	s.output(a);
	s.output(b);
	s.output(c);
}

bool Rsa::PrivateKey::deserialize(Serializer &s)
{
	BinaryString d, p, q, a, b, c;
	
	if(!s.input(d)) return false;
	AssertIO(s.input(p));
	AssertIO(s.input(q));
	AssertIO(s.input(a));
	AssertIO(s.input(b));
	AssertIO(s.input(c));
	
	try {
		mpz_import_binary(mKey.d, d);
		mpz_import_binary(mKey.p, p);
		mpz_import_binary(mKey.q, q);
		mpz_import_binary(mKey.a, a);
		mpz_import_binary(mKey.b, b);
		mpz_import_binary(mKey.c, c);
		
		if(!rsa_private_key_prepare(&mKey))
			throw Exception("Invalid parameters");
	}
	catch(const Exception &e)
	{
		throw InvalidData(String("RSA private key: ") + e.what());
	}
	
	return true;
}

void Rsa::PrivateKey::serialize(Stream &s) const
{
	BinaryString bs;
	BinarySerializer serializer(&bs);
	
	serialize(serializer);
	String str(bs.base64Encode());
	s.write(str);
}

bool Rsa::PrivateKey::deserialize(Stream &s)
{
	String str;
	if(!s.read(str)) return false;
	
	try {
		BinaryString bs(str.base64Decode());
		BinarySerializer serializer(&bs);
		AssertIO(deserialize(serializer));
	}
	catch(const InvalidData &e)
	{
		throw;
	}
	catch(const Exception &e)
	{
		throw InvalidData(String("RSA private key: ") + e.what());
	}
	
	return true;
}

Rsa::Rsa(unsigned bits) :
	mBits(bits)
{
	Assert(bits >= 1024);
	Assert(bits <= 16384);
}

Rsa::~Rsa(void)
{

}

void Rsa::generate(PublicKey &pub, PrivateKey &priv)
{
	// Use exponent 65537 for compatibility and performance
	const unsigned long exponent = 65537;
	mpz_set_ui(pub.mKey.e, exponent);
	
	if(!rsa_generate_keypair (&pub.mKey, &priv.mKey, NULL, Random::wrapperKey, NULL, NULL, mBits, 0 /*e already set*/))
		throw Exception("RSA keypair generation failed (size=" + String::number(mBits) + ")");
}

void mpz_import_binary(mpz_t n, const BinaryString &bs)
{
	mpz_import(n, bs.size(), 1, 1, 1, 0, bs.ptr());	// big endian
}

void mpz_export_binary(const mpz_t n, BinaryString &bs)
{
	size_t size = (mpz_sizeinbase(n, 2) + 7) / 8;
	bs.resize(size);
	
	size_t len = 0;
	mpz_export(bs.ptr(), &len, 1, 1, 1, 0, n);	// big endian
	
	Assert(len == size);
}

void mpz_import_string(mpz_t n, const String &str)
{
	if(mpz_set_str(n, str.c_str(), 16))
		throw InvalidData("Invalid hexadecimal number: " + str);
}

void mpz_export_string(const mpz_t n, String &str)
{
	size_t size = mpz_sizeinbase(n, 16) + 2;
	str.resize(size);
	mpz_get_str(str.ptr(), 16, n);
	str.resize(std::strlen(str.c_str()));
}

}

