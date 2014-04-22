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

#ifndef TPN_SECURETRANSPORT_H
#define TPN_SECURETRANSPORT_H

#include "tpn/include.h"
#include "tpn/stream.h"
#include "tpn/string.h"
#include "tpn/list.h"
#include "tpn/crypto.h"
#include "tpn/serversocket.h"
#include "tpn/datagramsocket.h"

#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>

namespace tpn
{
	
class SecureTransport : public Stream
{
public:
	static void Init(void);
	static void Cleanup(void);
	static void GenerateParams(void);
	
	class Credentials
	{
	public:
		Credentials(void) {}
		virtual ~Credentials(void) {}
		void install(SecureTransport *st);
		
	protected:
		virtual void install(gnutls_session_t session) = 0;
	};
	
	class CertificateCallback : public Credentials
        {
        public:
                CertificateCallback(const Rsa::PublicKey &pub, const Rsa::PrivateKey &priv);
                ~CertificateCallback(void);
		
		virtual bool callback(const Rsa::PublicKey &pub) = 0;
		
	protected:
		static int VerifyCallback(gnutls_session_t session);
		
		void install(gnutls_session_t session);
                gnutls_certificate_credentials_t mCreds;
		gnutls_pcert_st mPcert;
		gnutls_privkey_t mPkey;
		gnutls_x509_crt_t mCrt;
		gnutls_x509_privkey_t mKey;
        };
	
	void addCredentials(Credentials *creds); // creds will NOT be deleted
	void handshake(void);
	void close(void);
	
	bool isAnonymous(void);
	bool hasPrivateSharedKey(void);
	bool hasCertificate(void);
	
	size_t readData(char *buffer, size_t size); 
	void writeData(const char *data, size_t size);
	// TODO: waitData
	
	struct Verifier
        {
		virtual bool verifyCertificate(const Rsa::PublicKey &pub) { return false; }
		virtual bool verifyPrivateSharedKey(const String &username, BinaryString &key) { return false; }
		virtual bool verifyName(const String &name, SecureTransport *transport) { return true; }	// default is true
        };
	
	void setVerifier(Verifier *verifier);
	
protected:
	static ssize_t	DirectWriteCallback(gnutls_transport_ptr_t ptr, const void* data, size_t len);
	static ssize_t	WriteCallback(gnutls_transport_ptr_t ptr, const void* data, size_t len);
	static ssize_t	ReadCallback(gnutls_transport_ptr_t ptr, void* data, size_t maxlen);
	static int	TimeoutCallback(gnutls_transport_ptr_t ptr, unsigned int ms);
	
	static int CertificateCallback(gnutls_session_t session);
	static int PrivateSharedKeyCallback(gnutls_session_t session, const char* username, gnutls_datum_t* datum); 
	
	static const String DefaultPriorities;
	static gnutls_dh_params_t Params;
	static Mutex ParamsMutex;
	
	SecureTransport(Stream *stream, bool server, bool datagram);	// stream will be deleted on success
	virtual ~SecureTransport(void);
	
	gnutls_session_t mSession;
	Stream *mStream;
	
	Verifier *mVerifier;
};

class SecureTransportClient : public SecureTransport
{
public:
	class Anonymous : public Credentials
	{
	public:
		Anonymous(void);
		~Anonymous(void);
		
	protected:
		void install(gnutls_session_t session);
		gnutls_anon_client_credentials_t mCreds;
	};

	class PrivateSharedKey : public Credentials
        {
        public:
                PrivateSharedKey(const String &name, const BinaryString &key);
                ~PrivateSharedKey(void);
		
	protected:
		void install(gnutls_session_t session);
                gnutls_psk_client_credentials_t mCreds;
        };

	SecureTransportClient(Stream *stream, Credentials *creds = NULL, bool datagram = false);	// creds will NOT be deleted
	~SecureTransportClient(void);
};

class SecureTransportServer : public SecureTransport
{
public:
	class Anonymous : public Credentials
	{
	public:
		Anonymous(void);
		~Anonymous(void);
		
	protected:
		void install(gnutls_session_t session);
		gnutls_anon_server_credentials_t mCreds;
	};

	class PrivateSharedKeyCallback : public Credentials
        {
        public:
                PrivateSharedKeyCallback(void);
                ~PrivateSharedKeyCallback(void);

		// Callback to fetch key given username
		virtual bool callback(const String &username, BinaryString &key) = 0;
		
	protected:
		static int CredsCallback(gnutls_session_t session, const char* username, gnutls_datum_t* datum); 
		
		void install(gnutls_session_t session);
                gnutls_psk_server_credentials_t mCreds;
        };	

	// These functions are preferred, especially for datagrams (protection agains DoS)
	static SecureTransport *Listen(ServerSocket &sock);
	static SecureTransport *Listen(DatagramSocket &sock);
	
	SecureTransportServer(Stream *stream, Credentials *creds = NULL, bool datagram = false);	// creds will NOT be deleted
	~SecureTransportServer(void);
	
protected:
	static int PostClientHelloCallback(gnutls_session_t session);
};

}

#endif
