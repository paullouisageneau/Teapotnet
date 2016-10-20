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

#include "pla/crypto.hpp"
#include "pla/random.hpp"
#include "pla/exception.hpp"
#include "pla/binaryserializer.hpp"
#include "pla/time.hpp"

#include <nettle/hmac.h>
#include <nettle/pbkdf2.h>

#include <gmp.h>

namespace pla
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
	while((size = stream.readData(buffer, BufferSize)) > 0)
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

BinaryString Hash::compute(const BinaryString &str)
{
	BinaryString result;
	compute(str.data(), str.size(), result);
	return result;
}

size_t Sha1::length(void) const
{
	return size_t(SHA1_DIGEST_SIZE);
}

void Sha1::init(void)
{
	sha1_init(&mCtx);
}

void Sha1::process(const char *data, size_t size)
{
	sha1_update(&mCtx, unsigned(size), reinterpret_cast<const uint8_t*>(data));
}

void Sha1::process(const BinaryString &str)
{
	process(str.data(), str.size());
}

void Sha1::finalize(char *digest)
{
	sha1_digest(&mCtx, unsigned(length()), reinterpret_cast<uint8_t*>(digest));
}

void Sha1::finalize(BinaryString &digest)
{
	digest.resize(length());
	finalize(digest.ptr());
}

void Sha1::hmac(const char *message, size_t len, const char *key, size_t key_len, char *digest)
{
	struct hmac_sha1_ctx ctx;
	hmac_sha1_set_key(&ctx, key_len, reinterpret_cast<const uint8_t*>(key));
	hmac_sha1_update(&ctx, len, reinterpret_cast<const uint8_t*>(message));
	hmac_sha1_digest(&ctx, length(), reinterpret_cast<uint8_t*>(digest));
}

void Sha1::hmac(const BinaryString &message, const BinaryString &key, BinaryString &digest)
{
	digest.resize(length());
	hmac(message.data(), message.size(), key.data(), key.size(), digest.ptr());
}

void Sha1::pbkdf2_hmac(const char *secret, size_t len, const char *salt, size_t salt_len, char *key, size_t key_len, unsigned iterations)
{
	Assert(iterations != 0);
	pbkdf2_hmac_sha1(len, reinterpret_cast<const uint8_t*>(secret),
				iterations,
				salt_len, reinterpret_cast<const uint8_t*>(salt), 
				key_len, reinterpret_cast<uint8_t*>(key));
}

void Sha1::pbkdf2_hmac(const BinaryString &secret, const BinaryString &salt, BinaryString &key, size_t key_len, unsigned iterations)
{
	key.resize(key_len);
	pbkdf2_hmac(secret.data(), secret.size(), salt.data(), salt.size(), key.ptr(), key.size(), iterations);
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

Cipher::Cipher(Stream *stream, bool mustDelete) :
	mStream(stream),
	mMustDelete(mustDelete),
	mReadBlock(NULL),
	mWriteBlock(NULL),
	mReadBlockSize(0),
	mWriteBlockSize(0),
	mReadPosition(0),
	mWritePosition(0)
{
	Assert(mStream);
}

Cipher::~Cipher(void)
{
	delete mReadBlock;
	delete mWriteBlock;
	
	if(mMustDelete)
		delete mStream;
}

size_t Cipher::readData(char *buffer, size_t size)
{
	if(!mReadBlock)
		mReadBlock = new char[blockSize()];
	
	if(!mReadBlockSize)
	{
		mReadBlockSize = mStream->readBinary(mReadBlock, blockSize());
		decryptBlock(mReadBlock, mReadBlockSize);
	}
	
	size = std::min(size, mReadBlockSize);
	std::memcpy(buffer, mReadBlock, size);
	mReadBlockSize-= size;
	std::memmove(mReadBlock, mReadBlock+size, mReadBlockSize);
	return size;
}

void Cipher::writeData(const char *data, size_t size)
{
	if(!mWriteBlock)
		mWriteBlock = new char[blockSize()];
	
	while(size)
	{
		 size_t len = std::min(blockSize() - mWriteBlockSize, size);
		 std::memcpy(mWriteBlock + mWriteBlockSize, data, len);
		 mWriteBlockSize+= len;
		 data+= len;
		 size-= len;
		 
		 if(mWriteBlockSize == blockSize())
		 {
		 	encryptBlock(mWriteBlock, mWriteBlockSize);
		 	mStream->writeBinary(mWriteBlock, mWriteBlockSize);
			mWriteBlockSize = 0;
		 }
	}
}

void Cipher::seekRead(int64_t position)
{
	throw Unsupported("Seeking in block cipher");
}

void Cipher::seekWrite(int64_t position)
{
	throw Unsupported("Seeking in block cipher"); 
}

int64_t Cipher::tellRead(void) const
{
	return mReadPosition;
}

int64_t Cipher::tellWrite(void) const
{
	return mWritePosition;
}

void Cipher::close(void)
{
	// Finish encryption
	if(mWriteBlockSize)
	{
		encryptBlock(mWriteBlock, mWriteBlockSize);
		mStream->writeBinary(mWriteBlock, mWriteBlockSize);
		mWriteBlockSize = 0;
	}
	
	mStream->close();
}

Aes::Aes(Stream *stream, bool mustDelete) :
	Cipher(stream, mustDelete)
{

}

Aes::~Aes(void)
{
	close();	// must be called here and not in Cipher
}

void Aes::setEncryptionKey(const BinaryString &key)
{
	aes_set_encrypt_key(&mCtx.ctx, key.size(), key.bytes());
}

void Aes::setDecryptionKey(const BinaryString &key)
{
	// Counter mode also uses encrypt function for decryption
	aes_set_encrypt_key(&mCtx.ctx, key.size(), key.bytes()); 
}

void Aes::setInitializationVector(const BinaryString &iv)
{
	Assert(iv.size() >= AES_BLOCK_SIZE);
	CTR_SET_COUNTER(&mCtx, iv.bytes());
}

size_t Aes::blockSize(void) const
{
	return AES_BLOCK_SIZE;  
}

void Aes::encryptBlock(char *block, size_t size)
{
	uint8_t *ptr = reinterpret_cast<uint8_t*>(block);
	CTR_CRYPT(&mCtx, aes_encrypt, size, ptr, ptr);
}

void Aes::decryptBlock(char *block, size_t size)
{
	// Counter mode also uses encrypt function for decryption
	uint8_t *ptr = reinterpret_cast<uint8_t*>(block);
	CTR_CRYPT(&mCtx, aes_encrypt, size, ptr, ptr);
}

Rsa::PublicKey::PublicKey(void)
{
	rsa_public_key_init(&mKey);
	mKey.size = 0;
}

Rsa::PublicKey::PublicKey(const Rsa::PublicKey &key)
{
	rsa_public_key_init(&mKey);
	mKey.size = 0;
	*this = key;
}

Rsa::PublicKey::PublicKey(gnutls_x509_crt_t crt)
{
	if(gnutls_x509_crt_get_pk_algorithm(crt, NULL) != GNUTLS_PK_RSA)
		throw Exception("Certificate public key algorithm is not RSA");
	
	rsa_public_key_init(&mKey);
	mKey.size = 0;
	
	try {
		gnutls_datum_t n, e;
		
		int ret = gnutls_x509_crt_get_pk_rsa_raw(crt, &n, &e);
		if(ret != GNUTLS_E_SUCCESS)
			throw Exception(String("Key exportation failed: ") + gnutls_strerror(ret));
		
		mpz_import(mKey.n, n.size, 1, 1, 1, 0, n.data);	// big endian
		mpz_import(mKey.e, e.size, 1, 1, 1, 0, e.data);	// big endian
		gnutls_free(n.data);
		gnutls_free(e.data);
		
		if(!rsa_public_key_prepare(&mKey))
			throw Exception("Invalid parameters");
	}
	catch(const std::exception &e)
	{
		rsa_public_key_clear(&mKey);
		throw Exception(String("Unable to get RSA public key from x509 certificate: ") + e.what());
	}
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
	mDigest = key.mDigest;
	return *this;
}

bool Rsa::PublicKey::operator==(const PublicKey &key) const
{
	return mpz_cmp(mKey.n, key.mKey.n) == 0 && mpz_cmp(mKey.e, key.mKey.e) == 0;
}

bool Rsa::PublicKey::isNull(void) const
{
	return (mKey.size == 0);
}

void Rsa::PublicKey::clear(void)
{
	rsa_public_key_clear(&mKey);
	rsa_public_key_init(&mKey);
	mKey.size = 0;
	
	mDigest.clear();
}

const BinaryString &Rsa::PublicKey::digest(void) const
{
	if(mDigest.empty())
	{
		BinaryString tmp;
		BinarySerializer serializer(&tmp);
		serialize(serializer);
		Sha256().compute(tmp, mDigest);
	}
	
	return mDigest;
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
	
	s << e;
	s << n;
}

bool Rsa::PublicKey::deserialize(Serializer &s)
{
	BinaryString e, n;
	
	if(!(s >> e)) return false;
	AssertIO(s >> n);
	
	try {
		mKey.size = 0;
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
	s << String(bs.base64Encode());
}

bool Rsa::PublicKey::deserialize(Stream &s)
{
	clear();  
	
	String str;
	if(!(s >> str)) return false;

	try {
		BinaryString bs(str.base64Decode());
		BinarySerializer serializer(&bs);
		AssertIO(deserialize(serializer));
	}
	catch(const Exception &e)
	{
		throw InvalidData(String("Invalid RSA public key: ") + e.what());
	}
	
	return true;
}

bool Rsa::PublicKey::isInlineSerializable(void) const
{
	return true;
}

Rsa::PrivateKey::PrivateKey(void)
{
	rsa_private_key_init(&mKey);
	mKey.size = 0;
}

Rsa::PrivateKey::PrivateKey(const PrivateKey &key)
{
	rsa_private_key_init(&mKey);
	mKey.size = 0;
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
	return *this;
}

bool Rsa::PrivateKey::isNull(void) const
{
	return (mKey.size == 0);
}


void Rsa::PrivateKey::clear(void)
{
	rsa_private_key_clear(&mKey);
	rsa_private_key_init(&mKey);
	mKey.size = 0;
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
	
	s << d;
	s << p;
	s << q;
	s << a;
	s << b;
	s << c;
}

bool Rsa::PrivateKey::deserialize(Serializer &s)
{
	clear();
	
	BinaryString d, p, q, a, b, c;
	
	if(!(s >> d)) return false;
	AssertIO(s >> p);
	AssertIO(s >> q);
	AssertIO(s >> a);
	AssertIO(s >> b);
	AssertIO(s >> c);
	
	try {
		mKey.size = 0;
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
	s << String(bs.base64Encode());
}

bool Rsa::PrivateKey::deserialize(Stream &s)
{
	String str;
	if(!(s >> str)) return false;
	
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

bool Rsa::PrivateKey::isInlineSerializable(void) const
{
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

void Rsa::CreateCertificate(gnutls_x509_crt_t crt, gnutls_x509_privkey_t key, const PublicKey &pub, const PrivateKey &priv, const String &name)
{
	if(pub.isNull() || priv.isNull())
		throw Exception("Creating certificate from null key pair");
	
	BinaryString bs_n; mpz_export_binary(pub.mKey.n,  bs_n);
	BinaryString bs_e; mpz_export_binary(pub.mKey.e,  bs_e);
	BinaryString bs_d; mpz_export_binary(priv.mKey.d, bs_d);
	BinaryString bs_p; mpz_export_binary(priv.mKey.p, bs_p);
	BinaryString bs_q; mpz_export_binary(priv.mKey.q, bs_q);
	BinaryString bs_c; mpz_export_binary(priv.mKey.c, bs_c);
	
	gnutls_datum_t n; n.data = bs_n.bytes(); n.size = bs_n.size();
	gnutls_datum_t e; e.data = bs_e.bytes(); e.size = bs_e.size();
	gnutls_datum_t d; d.data = bs_d.bytes(); d.size = bs_d.size();
	gnutls_datum_t p; p.data = bs_p.bytes(); p.size = bs_p.size();
	gnutls_datum_t q; q.data = bs_q.bytes(); q.size = bs_q.size();
	gnutls_datum_t c; c.data = bs_c.bytes(); c.size = bs_c.size();
	
	int ret = gnutls_x509_privkey_import_rsa_raw(key, &n, &e, &d, &p, &q, &c);
	if(ret != GNUTLS_E_SUCCESS)
		throw Exception(String("Unable to convert RSA key pair to X509: ") + gnutls_strerror(ret));
	
	Time activationTime(Time::Now());
	Time expirationTime(Time::Now()); expirationTime.addDays(365);
	
	gnutls_x509_crt_set_activation_time(crt, activationTime.toUnixTime());
	gnutls_x509_crt_set_expiration_time(crt, expirationTime.toUnixTime());
	gnutls_x509_crt_set_version(crt, 1);
	gnutls_x509_crt_set_key(crt, key);
	gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COMMON_NAME, 0, name.data(), name.size());

	const size_t serialSize = 16;
	char serial[serialSize];
	Random(Random::Nonce).readData(serial, serialSize);
	gnutls_x509_crt_set_serial(crt, serial, serialSize);
}

void Rsa::SignCertificate(gnutls_x509_crt_t crt, gnutls_x509_crt_t issuer, gnutls_x509_privkey_t issuerKey)
{
	int ret = gnutls_x509_crt_sign2(crt, issuer, issuerKey, GNUTLS_DIG_SHA256, 0);
	if(ret != GNUTLS_E_SUCCESS)
		throw Exception(String("Unable to sign X509 certificate: ") + gnutls_strerror(ret));
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
	
	if(len == 0) bs.clear();
	Assert(len == bs.size());
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

