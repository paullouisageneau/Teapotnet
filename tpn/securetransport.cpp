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

#include "tpn/securetransport.h"
#include "tpn/exception.h"
#include "tpn/random.h"
#include "tpn/thread.h"

#include <gnutls/dtls.h>

namespace tpn
{

// Force 128+ bits cipher, disable SSL3.0 and TLS1.0, disable RC4
const String SecureTransport::DefaultPriorities = "SECURE128:-VERS-SSL3.0:-VERS-TLS1.0:-ARCFOUR-128";
gnutls_dh_params_t SecureTransport::Params;
Mutex SecureTransport::ParamsMutex;

void SecureTransport::Init(void)
{
	Assert(gnutls_global_init() ==  GNUTLS_E_SUCCESS);
	Assert(gnutls_dh_params_init(&Params));
}

void SecureTransport::Cleanup(void)
{
	gnutls_global_deinit();
	gnutls_dh_params_deinit(Params);
}

void SecureTransport::GenerateParams(void)
{
	const int bits = 4096;
	
	MutexLocker lock(&ParamsMutex);
	LogDebug("SecureTransport::GenerateParams", "Generating DH parameters");
	int ret = gnutls_dh_params_generate2(Params, bits);
	if (ret < 0) throw Exception(String("Failed to generate DH parameters: ") + gnutls_strerror(ret));
}

SecureTransport::SecureTransport(Stream *stream, bool server, bool datagram) :
	mStream(stream),
	mVerifier(NULL),
	mIsHandshakeDone(false)
{
	Assert(stream);

	try {
		if(server && Random().uniform(0, 1000) == 0)
			GenerateParams();
		
		// Init session
		unsigned int flags = (server ? GNUTLS_SERVER : GNUTLS_CLIENT);
		if(datagram) flags|= GNUTLS_DATAGRAM;
		Assert(gnutls_init(&mSession, flags) == GNUTLS_E_SUCCESS);
		
		// Set priorities
		const char *err_pos = NULL;
		if(gnutls_priority_set_direct(mSession, DefaultPriorities.c_str(), &err_pos))
			throw Exception("Unable to set TLS priorities");
		
		// Set callbacks
		gnutls_transport_set_ptr(mSession, reinterpret_cast<gnutls_transport_ptr_t>(this));
		gnutls_transport_set_push_function(mSession, WriteCallback);
		gnutls_transport_set_pull_function(mSession, ReadCallback);
		gnutls_transport_set_pull_timeout_function(mSession, TimeoutCallback);
		
		if(datagram) gnutls_dtls_set_mtu(mSession, 1024);	// TODO
	}
	catch(...)
	{
		gnutls_deinit(mSession);
		throw;
	}
}

SecureTransport::~SecureTransport(void)
{
	gnutls_deinit(mSession);	
	delete mStream;
	
	for(List<Credentials*>::iterator it = mCredsToDelete.begin();
		it != mCredsToDelete.end();
		++it)
	{
		delete *it;      
	}
}

void SecureTransport::addCredentials(Credentials *creds, bool mustDelete)
{
	// Install credentials
	creds->install(this);
	
	if(mustDelete) 
		mCredsToDelete.push_back(creds);
}

void SecureTransport::handshake(void)
{
	// Perform the TLS handshake
	int ret;
	do {
                ret = gnutls_handshake(mSession);
        }
        while (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);
	
        if(ret < 0)
		throw Exception(String("TLS handshake failed: ") + gnutls_strerror(ret));
	
	mIsHandshakeDone = true;
}

void SecureTransport::close(void)
{
	gnutls_bye(mSession, GNUTLS_SHUT_RDWR);
}

bool SecureTransport::isHandshakeDone(void)
{
	return mIsHandshakeDone; 
}

bool SecureTransport::isAnonymous(void)
{
	return gnutls_auth_get_type(mSession) == GNUTLS_CRD_ANON;
}

bool hasPrivateSharedKey(void)
{
	return gnutls_auth_get_type(mSession) == GNUTLS_CRD_PSK;
}

bool hasCertificate(void)
{
	return gnutls_auth_get_type(mSession) == GNUTLS_CRD_CERTIFICATE;
}

size_t SecureTransport::readData(char *buffer, size_t size)
{
	ssize_t ret = gnutls_record_recv(mSession, buffer, size);

	if(ret < 0)
	{
		if(gnutls_error_is_fatal(ret)) LogWarn("SecureTransport::readData", gnutls_strerror(ret));
		else throw Exception(gnutls_strerror(ret));
	}
	
	return size_t(ret);
}

void SecureTransport::writeData(const char *data, size_t size)
{
	do {
		ssize_t ret = gnutls_record_send(mSession, data, size);
		
		if(ret < 0) 
		{
			if(gnutls_error_is_fatal(ret)) LogWarn("SecureTransport::writeData", gnutls_strerror(ret));
			else throw Exception(gnutls_strerror(ret));
		}
		
		data+= ret;
		size-= ret;
	}
	while(size);
}

void SecureTransport::setVerifier(Verifier *verifier)
{
	mVerifier = verifier;
}

ssize_t SecureTransport::DirectWriteCallback(gnutls_transport_ptr_t ptr, const void* data, size_t len)
{
	try {
		Stream *s = static_cast<Stream*>(ptr);
		s->writeData(static_cast<const char*>(data), len);
		return ssize_t(len);
	}
	catch(const std::exception &e)
	{
		LogWarn("SecureTransport::DirectWriteCallback", e.what());
		return -1;
	}
}

ssize_t SecureTransport::WriteCallback(gnutls_transport_ptr_t ptr, const void* data, size_t len)
{
	try {
		SecureTransport *st = static_cast<SecureTransport*>(ptr);
		st->mStream->writeData(static_cast<const char*>(data), len);
		return ssize_t(len);
	}
	catch(const std::exception &e)
	{
		LogWarn("SecureTransport::WriteCallback", e.what());
		return -1;
	}
}

ssize_t SecureTransport::ReadCallback(gnutls_transport_ptr_t ptr, void* data, size_t maxlen)
{
	try {
		SecureTransport *st = static_cast<SecureTransport*>(ptr);
		return ssize_t(st->mStream->readData(static_cast<char*>(data), maxlen));
	}
	catch(const std::exception &e)
	{
		LogWarn("SecureTransport::ReadCallback", e.what());
		return -1;
	}
}

int SecureTransport::TimeoutCallback(gnutls_transport_ptr_t ptr, unsigned int ms)
{
	try {
		SecureTransport *st = static_cast<SecureTransport*>(ptr);
		if(st->mStream->waitData(milliseconds(ms))) return 1;
		else return 0;
	}
	catch(const std::exception &e)
	{
		LogWarn("SecureTransport::TimeoutCallback", e.what());
		return -1;
	}
}

int SecureTransport::CertificateCallback(gnutls_session_t session)
{
	SecureTransport *transport = reinterpret_cast<SecureTransport*>(gnutls_session_get_ptr(session));
	if(!transport)
	{
		LogWarn("SecureTransport::CertificateCallback", "TLS certificate verification callback called with unknown session");
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
	
	if(!transport->mVerifier)
	{
		LogWarn("SecureTransport::CertificateCallback", "No certificate verifier specified");
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
	
	try {
		unsigned count = 0;
		const gnutls_datum_t *array = gnutls_certificate_get_peers(session, &count);
		if(!array || !count) throw Exception("No certificates retrieved from peer");
		
		gnutls_x509_crt_t crt;
		Assert(gnutls_x509_crt_init(&crt) == GNUTLS_E_SUCCESS);
		
		bool valid = false;
		try {
			int ret = gnutls_x509_crt_import(crt, array, GNUTLS_X509_FMT_DER);
			if(ret != GNUTLS_E_SUCCESS) throw Exception(String("Unable to retrieve X509 certificate: ") + gnutls_strerror(ret));
			bool valid = transport->mVerifier->verifyCertificate(Rsa::PublicKey(crt));
		}
		catch(...)
		{
			gnutls_x509_crt_deinit(crt);
			throw;
		}
		
		gnutls_x509_crt_deinit(crt);
		
		if(valid) return 0;
		else return GNUTLS_E_CERTIFICATE_ERROR;
	}
	catch(const Exception &e)
	{
		LogWarn("SecureTransport::CertificateCallback", String("TLS certificate verification failed: ") + e.what());
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
}

int SecureTransport::PrivateSharedKeyCallback(gnutls_session_t session, const char* username, gnutls_datum_t* datum)
{
	SecureTransport *transport = reinterpret_cast<SecureTransport*>(gnutls_session_get_ptr(session));
	if(!transport) 
	{
		LogWarn("SecureTransport::PrivateSharedKeyCallback", "TLS PSK callback called with unknown session");
		return -1;
	}
	
	if(!transport->mVerifier) 
	{
		LogWarn("SecureTransport::PrivateSharedKeyCallback", "No PSK verifier specified");
		return -1;
	}
	
	BinaryString key;
	try {
		if(!transport->mVerifier->verifyPrivateSharedKey(String(username), key)) return -1;
	}
	catch(const Exception &e)
	{
		LogWarn("SecureTransport::PrivateSharedKeyCallback", String("TLS PSK verification failed: ") + e.what());
		return -1;
	}
	
	datum->size = key.size();
	datum->data = static_cast<unsigned char *>(gnutls_malloc(datum->size));
	std::memcpy(datum->data, key.data(), datum->size);
	return 0;
}

void SecureTransport::Credentials::install(SecureTransport *st)
{
	install(st->mSession);
}

SecureTransport::Certificate::Certificate(const Rsa::PublicKey &pub, const Rsa::PrivateKey &priv)
{
	// Allocate certificate credentials
	Assert(gnutls_certificate_allocate_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	// Set DH parameters
	ParamsMutex.lock();
	gnutls_certificate_set_dh_params(mCreds, Params);
	ParamsMutex.unlock();
	
	// Init certificate and key
	Assert(gnutls_privkey_init(&mPkey) == GNUTLS_E_SUCCESS);
	Assert(gnutls_x509_crt_init(&mCrt) == GNUTLS_E_SUCCESS);
	Assert(gnutls_x509_privkey_init(&mKey) == GNUTLS_E_SUCCESS);

	// gnutls_certificate_set_verify_flags(mCreds, GNUTLS_VERIFY_DISABLE_CA_SIGN
	// 						| GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT
	// 						| GNUTLS_VERIFY_ALLOW_ANY_X509_V1_CA_CRT);

        gnutls_certificate_set_verify_function(mCreds, SecureTransport::CertificateCallback);
	
	try {
		int ret;
		
		Rsa::CreateCertificate(mCrt, mKey, pub, priv);
		
		ret = gnutls_pcert_import_x509(&mPcert, mCrt, 0);
		if(ret != GNUTLS_E_SUCCESS)
			throw Exception(String("Unable to import X509 certificate: ") + gnutls_strerror(ret));
		
		try {
			ret = gnutls_privkey_import_x509(mPkey, mKey, 0);
			if(ret != GNUTLS_E_SUCCESS)
				throw Exception(String("Unable to import X509 key pair: ") + gnutls_strerror(ret));
			
			ret = gnutls_certificate_set_key(mCreds, NULL, 0, &mPcert, 1, mPkey);
			if(ret != GNUTLS_E_SUCCESS)
				throw Exception(String("Unable to set certificate and key pair in credentials: ") + gnutls_strerror(ret));
		}
		catch(...)
		{
			gnutls_pcert_deinit(&mPcert);
			throw;
		}
	}
	catch(...)
	{
		gnutls_certificate_free_credentials(mCreds);
		gnutls_privkey_deinit(mPkey);
		gnutls_x509_crt_deinit(mCrt);
		gnutls_x509_privkey_deinit(mKey);
		throw;
	}
}

SecureTransport::Certificate::~Certificate(void)
{
	gnutls_certificate_free_credentials(mCreds);
	gnutls_pcert_deinit(&mPcert);
	gnutls_privkey_deinit(mPkey);
	gnutls_x509_crt_deinit(mCrt);
	gnutls_x509_privkey_deinit(mKey);
}

void SecureTransport::Certificate::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, mCreds) == GNUTLS_E_SUCCESS);
	gnutls_session_set_ptr(session, reinterpret_cast<void*>(this));
}

SecureTransportClient::SecureTransportClient(Stream *stream, Credentials *creds, bool datagram) :
	SecureTransport(stream, false, datagram)
{
	try {
		if(creds) 
		{
			addCredentials(creds, true);	
			handshake();
		}
	}
	catch(...)
	{
		mStream = NULL;	// so stream won't be deleted
		throw;
	}
}

SecureTransportClient::~SecureTransportClient(void)
{
	
}

SecureTransportClient::Anonymous::Anonymous(void)
{
	// Enable anonymous auth
	String priorities = DefaultPriorities + ":+ANON-DH:+ANON-ECDH";
	const char *err_pos = NULL;
	if(gnutls_priority_set_direct(session, priorities.c_str(), &err_pos))
		throw Exception("Unable to set TLS priorities for anonymous auth");
	
	// Allocate anonymous credentials
	Assert(gnutls_anon_allocate_client_credentials(&mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportClient::Anonymous::~Anonymous(void)
{
	gnutls_anon_free_client_credentials(mCreds);
}

void SecureTransportClient::Anonymous::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_ANON, mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportClient::PrivateSharedKey::PrivateSharedKey(const String &username, const BinaryString &key)
{
	// Allocate PSK credentials
	Assert(gnutls_psk_allocate_client_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	// Set PSK credentials
	gnutls_datum_t datum;
	datum.size = key.size();
	datum.data = static_cast<unsigned char *>(gnutls_malloc(datum.size));
	std::memcpy(datum.data, key.data(), key.size());
	
	if(gnutls_psk_set_client_credentials(mCreds, username.c_str(), &datum, GNUTLS_PSK_KEY_RAW) != GNUTLS_E_SUCCESS)
	{
		gnutls_free(datum.data);
		throw Exception("Unable to set credentials");
	}
	
	gnutls_free(datum.data);
}

SecureTransportClient::PrivateSharedKey::~PrivateSharedKey(void)
{
	gnutls_psk_free_client_credentials(mCreds);
}

void SecureTransportClient::PrivateSharedKey::install(gnutls_session_t session)
{
	// Enable PSK
	String priorities = DefaultPriorities + ":+PSK:+DHE-PSK";
	const char *err_pos = NULL;
	if(gnutls_priority_set_direct(session, priorities.c_str(), &err_pos))
		throw Exception("Unable to set TLS priorities for PSK");

	Assert(gnutls_credentials_set(session, GNUTLS_CRD_PSK, mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportServer::SecureTransportServer(Stream *stream, Credentials *creds, bool datagram) :
	SecureTransport(stream, true, datagram)
{
	try {
		gnutls_certificate_server_set_request(mSession, GNUTLS_CERT_REQUEST);	// TODO
		gnutls_handshake_set_post_client_hello_function(mSession, PostClientHelloCallback);
		
		if(creds) 
		{
			addCredentials(creds, true);
			handshake();
		}
	}
	catch(...)
	{
		mStream = NULL;	// so stream won't be deleted
		throw;
	}
}

SecureTransportServer::~SecureTransportServer(void)
{
	
}

bool SecureTransportServer::postClientHello(void)
{
	
}

int SecureTransportServer::PostClientHelloCallback(gnutls_session_t session)
{
	SecureTransportServer *transport = reinterpret_cast<SecureTransportServer*>(gnutls_session_get_ptr(session));
	if(!transport) 
	{
		LogWarn("SecureTransportServer::PostClientHelloCallback", "TLS post client hello callback called with unknown session");
		return -1;
	}

	if(!transport->mVerifier) return 0;
	
	try {
		String name;
		
		char buffer[BufferSize];
		size_t size = BufferSize;
		unsigned int type =  GNUTLS_NAME_DNS;
		if(gnutls_server_name_get(mSession, buffer, &size, &type, 0))
			name.assign(buffer, size);
	
		if(!transport->mVerifier->verifyName(name, transport)) return GNUTLS_E_NO_CERTIFICATE_FOUND;
	}
	catch(const Exception &e)
	{
		LogWarn("SecureTransportServer::PostClientHelloCallback", String("TLS client hello callback failed: ") + e.what());
		return -1;
	}
	
	return 0;
}

SecureTransport *SecureTransportServer::Listen(ServerSocket &lsock)
{
	while(true)
	{
		Socket *sock = NULL;
		
		try {
			sock = new Socket;
			lsock.accept(*sock);
		}
		catch(...)
		{
			delete sock;
			throw;
		}
		
		SecureTransportServer *transport = NULL;
		try {
			transport = new SecureTransportServer(sock, NULL, true);
		}
		catch(const std::exception &e)
		{
			LogWarn("SecureTransportServer::Listen(stream)", e.what());
			delete sock;
			return NULL;
		}
		
		return transport;
	}
}

SecureTransport *SecureTransportServer::Listen(DatagramSocket &sock)
{
	gnutls_datum_t cookieKey;
	gnutls_key_generate(&cookieKey, GNUTLS_COOKIE_KEY_SIZE);
	
	while(true)
	{
		char buffer[DatagramSocket::MaxDatagramSize];
		Address sender;
		int len = sock.peek(buffer, DatagramSocket::MaxDatagramSize, sender);
		if(len < 0) throw NetException("Unable to listen on datagram socket");

		gnutls_dtls_prestate_st prestate;
		std::memset(&prestate, 0, sizeof(prestate));
			
		int ret = gnutls_dtls_cookie_verify(&cookieKey,
						const_cast<sockaddr*>(sender.addr()),	// WTF ?
						sender.addrLen(),
						buffer, len,
						&prestate);
		
		if(ret == GNUTLS_E_SUCCESS)	// valid cookie
		{
			Stream *stream = NULL;
			SecureTransportServer *transport = NULL;
			
			try {
				stream = new DatagramStream(&sock, sender);
				transport = new SecureTransportServer(stream, NULL, true);
			}
			catch(...)
			{
				delete stream;
				throw;
			}
			
			gnutls_dtls_prestate_set(transport->mSession, &prestate);
			return transport;
		}
		
		DatagramStream stream(&sock, sender);
		
		gnutls_dtls_cookie_send(&cookieKey,
					const_cast<sockaddr*>(sender.addr()),	// WTF ?
					sender.addrLen(),
					&prestate,
					static_cast<gnutls_transport_ptr_t>(&stream),
					DirectWriteCallback);
		
		// discard peeked data
		NOEXCEPTION(sock.read(buffer, DatagramSocket::MaxDatagramSize, sender));
		Thread::Sleep(milliseconds(1));
	}
}

SecureTransportServer::Anonymous::Anonymous(void)
{
	// Allocate anonymous credentials
	Assert(gnutls_anon_allocate_server_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	// Set DH parameters
	ParamsMutex.lock();
	gnutls_anon_set_server_dh_params(mCreds, Params);
	ParamsMutex.unlock();
}

SecureTransportServer::Anonymous::~Anonymous(void)
{
	gnutls_anon_free_server_credentials(mCreds);
}

void SecureTransportServer::Anonymous::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_ANON, mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportServer::PrivateSharedKey::PrivateSharedKey(void)
{
	// Allocate PSK credentials
	Assert(gnutls_psk_allocate_server_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	// Set DH parameters
	ParamsMutex.lock();
	gnutls_psk_set_server_dh_params(mCreds, Params);
	ParamsMutex.unlock();
	
	// Set PSK callback
	gnutls_psk_set_server_credentials_function(mCreds, SecureTransport::PrivateSharedKeyCallback);
}

SecureTransportServer::PrivateSharedKey::~PrivateSharedKey(void)
{
	gnutls_psk_free_server_credentials(mCreds);
}

void SecureTransportServer::PrivateSharedKey::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_PSK, mCreds) == GNUTLS_E_SUCCESS);
	
}

}
