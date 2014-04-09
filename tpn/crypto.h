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

#ifndef TPN_CRYPTO_H
#define TPN_CRYPTO_H

#include "tpn/include.h"
#include "tpn/binarystring.h"
#include "tpn/string.h"
#include "tpn/stream.h"

#include <nettle/sha2.h>
#include <nettle/rsa.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

namespace tpn
{

// Hash function interface
class Hash
{
public:
	Hash(void) {}
	virtual ~Hash(void) {}
	
	// Implementation
	virtual size_t length(void) const = 0;
	virtual void init(void) = 0;
	virtual void process(const char *data, size_t size) = 0;
	virtual void process(const BinaryString &str) = 0;
	virtual void finalize(char *digest) = 0;	// digest size must be length()
	virtual void finalize(BinaryString &digest) = 0;

	// Direct hash function call
	void    compute(const char *data, size_t size, char *digest);
	void    compute(const char *data, size_t size, BinaryString &digest);
	int64_t compute(Stream &stream, char *digest);
	int64_t compute(Stream &stream, BinaryString &digest);
	int64_t compute(Stream &stream, int64_t max, char *digest);
        int64_t compute(Stream &stream, int64_t max, BinaryString &digest);

	// HMAC
	virtual void hmac(const char *message, size_t len, const char *key, size_t key_len, char *digest) = 0;
	virtual void hmac(const BinaryString &message, const BinaryString &key, BinaryString &digest) = 0;
};

// SHA256 hash function implementation
class Sha256 : public Hash
{
public:
        size_t length(void) const;
        void init(void);
        void process(const char *data, size_t size);
	void process(const BinaryString &str);
        void finalize(char *digest);
	void finalize(BinaryString &digest);
	
	void hmac(const char *message, size_t len, const char *key, size_t key_len, char *digest);
	void hmac(const BinaryString &message, const BinaryString &key, BinaryString &digest);
	
	// PBKDF2-HMAC-SHA256
	void pbkdf2_hmac(const char *secret, size_t len, const char *salt, size_t salt_len, char *key, size_t key_len, unsigned iterations);
	void pbkdf2_hmac(const BinaryString &secret, const BinaryString &salt, BinaryString &key, size_t key_len, unsigned iterations);

private:
	struct sha256_ctx mCtx;
};

// SHA512 hash function implementation
class Sha512 : public Hash
{
public:
	size_t length(void) const;
	void init(void);
        void process(const char *data, size_t size);
	void process(const BinaryString &str);
        void finalize(char *digest);
	void finalize(BinaryString &digest);

	void hmac(const char *message, size_t len, const char *key, size_t key_len, char *digest);
	void hmac(const BinaryString &message, const BinaryString &key, BinaryString &digest);

	// PBKDF2-HMAC-SHA256
	void pbkdf2_hmac(const char *secret, size_t len, const char *salt, size_t salt_len, char *key, size_t key_len, unsigned iterations);
	void pbkdf2_hmac(const BinaryString &secret, const BinaryString &salt, BinaryString &key, size_t key_len, unsigned iterations);
	
private:
	struct sha512_ctx mCtx;
};

class Rsa
{
public:
	class PublicKey : public Serializable
        {
        public:
		PublicKey(void);
		PublicKey(const PublicKey &key);
		PublicKey(gnutls_x509_crt_t crt);
		~PublicKey(void);
		PublicKey &operator=(const PublicKey &key);
		
		const BinaryString &digest(void) const;
		bool verify(const BinaryString &digest, const BinaryString &signature) const;
		
                 // Serializable
                void serialize(Serializer &s) const;
                bool deserialize(Serializer &s);
                void serialize(Stream &s) const;
                bool deserialize(Stream &s);

        private:
		struct rsa_public_key mKey;
		mutable BinaryString mDigest;
		friend class Rsa;
        };
	
	class PrivateKey : public Serializable
	{
	public:
		PrivateKey(void);
		PrivateKey(const PrivateKey &key);
		~PrivateKey(void);
		PrivateKey &operator=(const PrivateKey &key);
		
		void sign(const BinaryString &digest, BinaryString &signature) const;
		
		// Serializable
        	void serialize(Serializer &s) const;
        	bool deserialize(Serializer &s);
        	void serialize(Stream &s) const;
        	bool deserialize(Stream &s);

	private:
		struct rsa_private_key mKey;
		friend class Rsa;
	};
	
	Rsa(unsigned bits = 4096);
	~Rsa(void);
	
	void generate(PublicKey &pub, PrivateKey &priv);

	static void CreateCertificate(gnutls_x509_crt_t crt, gnutls_x509_privkey_t key, const PublicKey &pub, const PrivateKey &priv);
	
private:
	unsigned mBits;
};

// Add-on functions for custom mpz import/export
void mpz_import_binary(mpz_t n, const BinaryString &bs);
void mpz_export_binary(const mpz_t n, BinaryString &bs);
void mpz_import_string(mpz_t n, const String &str);
void mpz_export_string(const mpz_t n, String &str);

}

#endif
