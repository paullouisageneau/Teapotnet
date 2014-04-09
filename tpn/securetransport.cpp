/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

namespace tpn
{

// Force 128+ bits cipher, disable SSL3.0 and TLS1.0, disable RC4
const String SecureTransport::DefaultPriorities = "SECURE128:-VERS-SSL3.0:-VERS-TLS1.0:-ARCFOUR-128";
	
SecureTransport::SecureTransport(bool server, Stream *stream) :
	mStream(stream)
{
	Assert(stream);

	try {
		// Init mSession
		// TODO: DTLS
		unsigned int flags = (server ? GNUTLS_SERVER : GNUTLS_CLIENT);
		Assert(gnutls_init(&mSession, flags) == GNUTLS_E_SUCCESS);
		
		// Set priorities
		const char *err_pos = NULL;
		if(gnutls_priority_set_direct(mSession, DefaultPriorities.c_str(), &err_pos))
			throw Exception("Unable to set TLS priorities");
		
		// Set callbacks
		gnutls_transport_set_ptr(mSession, static_cast<gnutls_transport_ptr_t>(this));
		gnutls_transport_set_pull_function(mSession, ReadCallback);
		gnutls_transport_set_push_function(mSession, WriteCallback);
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
	
	for(List<Credentials*>::iterator it = mCreds.begin();
		it != mCreds.end();
		++it)
	{
		delete *it;
	}
	
	delete mStream;
}

void SecureTransport::addCredentials(Credentials *creds)
{
	// Install credentials
	try {
		creds->install(this);
	}
	catch(...)
	{
		delete creds;
		throw;
	}
	
	mCreds.push_back(creds);
}

void SecureTransport::handshake(void)
{
	if(mCreds.empty())
		throw Exception("Missing credentials for TLS handshake");
	
	// Perform the TLS handshake
	int ret;
	do {
                ret = gnutls_handshake(mSession);
        }
        while (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);
	
        if (ret < 0) throw Exception(String("TLS handshake failed: error ") + gnutls_strerror(ret));
}

void SecureTransport::close(void)
{
	gnutls_bye(mSession, GNUTLS_SHUT_RDWR);
	// TODO: DTLS: gnutls_bye(mSession, GNUTLS_SHUT_WR);
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

void SecureTransport::Credentials::install(SecureTransport *st)
{
	install(st->mSession);
}

SecureTransport::CertificateCallback::CertificateCallback(const Rsa::PublicKey &pub, const Rsa::PrivateKey &priv)
{
	// Allocate certificate credentials
	Assert(gnutls_certificate_allocate_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	Assert(gnutls_privkey_init(&mPkey) == GNUTLS_E_SUCCESS);
	Assert(gnutls_x509_crt_init(&mCrt) == GNUTLS_E_SUCCESS);
	Assert(gnutls_x509_privkey_init(&mKey) == GNUTLS_E_SUCCESS);

	// gnutls_certificate_set_verify_flags(mCreds, GNUTLS_VERIFY_DISABLE_CA_SIGN
	// 						| GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT
	// 						| GNUTLS_VERIFY_ALLOW_ANY_X509_V1_CA_CRT);

        gnutls_certificate_set_verify_function(mCreds, VerifyCallback);
	
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

SecureTransport::CertificateCallback::~CertificateCallback(void)
{
	gnutls_certificate_free_credentials(mCreds);
	gnutls_pcert_deinit(&mPcert);
	gnutls_privkey_deinit(mPkey);
	gnutls_x509_crt_deinit(mCrt);
	gnutls_x509_privkey_deinit(mKey);
}

void SecureTransport::CertificateCallback::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, mCreds) == GNUTLS_E_SUCCESS);
	gnutls_session_set_ptr(session, reinterpret_cast<void*>(this));
}

int SecureTransport::CertificateCallback::VerifyCallback(gnutls_session_t session)
{
	CertificateCallback *cc = reinterpret_cast<CertificateCallback*>(gnutls_session_get_ptr(session));
	
	if(!cc) 
	{
		LogWarn("SecureTransport::Certificate::VerifyCallback", "TLS certificate verification callback called with unknown session");
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
			
			Rsa::PublicKey pub(crt);
			bool valid = cc->callback(pub);
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
		LogWarn("SecureTransportServer::CertificateCallback::VerifyCallback", String("TLS certificate verification callback failed: ") + e.what());
		return GNUTLS_E_CERTIFICATE_ERROR;
	}
}

SecureTransportClient::SecureTransportClient(Stream *stream, Credentials *creds) :
	SecureTransport(false, stream)
{
	try {
		if(creds) 
		{
			addCredentials(creds);
			handshake();
		}
	}
	catch(...)
	{
		mStream = NULL;	// so stream won't be delete
		throw;
	}
}

SecureTransportClient::~SecureTransportClient(void)
{
	
}

SecureTransportClient::Anonymous::Anonymous(void)
{
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

SecureTransportServer::SecureTransportServer(Stream *stream, Credentials *creds) :
	SecureTransport(true, stream)
{
	try {
		if(creds) 
		{
			addCredentials(creds);
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

SecureTransportServer::Anonymous::Anonymous(void)
{
	// Allocate anonymous credentials
	Assert(gnutls_anon_allocate_server_credentials(&mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportServer::Anonymous::~Anonymous(void)
{
	gnutls_anon_free_server_credentials(mCreds);
}

void SecureTransportServer::Anonymous::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_ANON, mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportServer::PrivateSharedKeyCallback::PrivateSharedKeyCallback(void)
{
	// Allocate PSK credentials
	Assert(gnutls_psk_allocate_server_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	// Set PSK callback
	gnutls_psk_set_server_credentials_function(mCreds, CredsCallback);
}

SecureTransportServer::PrivateSharedKeyCallback::~PrivateSharedKeyCallback(void)
{
	gnutls_psk_free_server_credentials(mCreds);
}

void SecureTransportServer::PrivateSharedKeyCallback::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_PSK, mCreds) == GNUTLS_E_SUCCESS);
	gnutls_session_set_ptr(session, reinterpret_cast<void*>(this));
}

int SecureTransportServer::PrivateSharedKeyCallback::CredsCallback(gnutls_session_t session, const char* username, gnutls_datum_t* datum)
{
	PrivateSharedKeyCallback *pskcb = reinterpret_cast<PrivateSharedKeyCallback*>(gnutls_session_get_ptr(session));
	
	if(!pskcb) 
	{
		LogWarn("SecureTransportServer::PrivateSharedKeyCallback::CredsCallback", "TLS PSK callback called with unknown session");
		return -1;
	}
	
	BinaryString key;
	try {
		if(!pskcb->callback(String(username), key)) return -1;
	}
	catch(const Exception &e)
	{
		LogWarn("SecureTransportServer::PrivateSharedKeyCallback::CredsCallback", String("TLS PSK callback failed: ") + e.what());
		return -1;
	}
	
	datum->size = key.size();
	datum->data = static_cast<unsigned char *>(gnutls_malloc(datum->size));
	std::memcpy(datum->data, key.data(), datum->size);
	return 0;
}

}
