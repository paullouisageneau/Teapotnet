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
#include "tpn/exception.h"

#include <nettle/hmac.h>
#include <nettle/pbkdf2.h>

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

Ecdsa::PrivateKey::PrivateKey(void)
{
	//ecc_scalar_init(&mScalar, secp256r1);
}

Ecdsa::PrivateKey::~PrivateKey(void)
{
	ecc_scalar_clear(&mScalar);
}

void Ecdsa::PrivateKey::serialize(Serializer &s) const
{
	// TODO
}

bool Ecdsa::PrivateKey::deserialize(Serializer &s)
{
	// TODO
	return false;
}

void Ecdsa::PrivateKey::serialize(Stream &s) const
{
	// TODO
}

bool Ecdsa::PrivateKey::deserialize(Stream &s)
{
	// TODO
	return false;
}
		
Ecdsa::PublicKey::PublicKey(void)
{
	//ecc_point_init(&mPoint, secp256r1);
}

Ecdsa::PublicKey::~PublicKey(void)
{
	ecc_point_clear(&mPoint);
}

void Ecdsa::PublicKey::serialize(Serializer &s) const
{
	// TODO
}

bool Ecdsa::PublicKey::deserialize(Serializer &s)
{
	// TODO
	return false;
}

void Ecdsa::PublicKey::serialize(Stream &s) const
{
	// TODO
}

bool Ecdsa::PublicKey::deserialize(Stream &s)
{
	// TODO
	return false;
}

Ecdsa::Ecdsa(void)
{

}

Ecdsa::~Ecdsa(void)
{

}

void Ecdsa::generate(void)
{
	//ecdsa_generate_keypair(&mPublicKey.mPoint, &mPrivateKey.mScalar, random_ctx, random_func);
}

void Ecdsa::sign(const BinaryString &digest, BinaryString &signature) const
{
	//ecdsa_sign(&mPrivateKey.mScalar, random_ctx, random_func, digest.size(), digest.bytes(), struct dsa_signature *signature);
}

bool Ecdsa::verify(const BinaryString &digest, const BinaryString &signature) const
{
	//return (ecdsa_verify(&mPublicKey.mPoint, digest.size(), digest.bytes(), const struct dsa_signature *signature) == 1);
}

const Ecdsa::PublicKey &Ecdsa::publicKey(void) const
{
	return mPublicKey;
}

const Ecdsa::PrivateKey &Ecdsa::privateKey(void) const
{
	return mPrivateKey;
}

void Ecdsa::serialize(Serializer &s) const
{
	// TODO
}

bool Ecdsa::deserialize(Serializer &s)
{
	// TODO
	return false;
}

void Ecdsa::serialize(Stream &s) const
{
	// TODO
}

bool Ecdsa::deserialize(Stream &s)
{
	// TODO
	return false;
}

}

