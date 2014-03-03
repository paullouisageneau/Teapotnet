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

void SecureTransport::Init(void)
{
	gnutls_global_init();
}

void SecureTransport::Cleanup(void)
{
	gnutls_global_deinit();
}

SecureTransport::SecureTransport(bool server, Stream *stream, Credentials *creds) :
	mStream(stream),
	mCreds(creds)
{
	Assert(stream);
	Assert(creds);

	try {
		// Init mSession
		// TODO: DTLS
		unsigned int flags = (server ? GNUTLS_SERVER : GNUTLS_CLIENT);
		Assert(gnutls_init(&mSession, flags) == GNUTLS_E_SUCCESS);
		
		// Force 128+ bits cipher, disable SSL3.0 and TLS1.0, disable RC4
		const char *priorities = "SECURE128:-VERS-SSL3.0:-VERS-TLS1.0:-ARCFOUR-128";
		const char *err_pos = NULL;
		if(gnutls_priority_set_direct(mSession, priorities, &err_pos))
			throw Exception("Unable to set TLS priorities");
		
		// Set callbacks
		gnutls_transport_set_ptr(mSession, static_cast<gnutls_transport_ptr_t>(this));
		gnutls_transport_set_pull_function(mSession, ReadCallback);
		gnutls_transport_set_pull_function(mSession, WriteCallback);
		
		if(creds) handshake(creds);
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
	
	delete mCreds;
	delete mStream;
}

void SecureTransport::handshake(Credentials *creds)
{
	if(creds)
	{
		delete mCreds;
		mCreds = NULL;
		
		// Install credentials
		try {
			creds->install(this);
		}
		catch(...)
		{
			delete creds;
			throw;
		}
		
		mCreds = creds;
	}
	else {
		if(!mCreds) throw Exception("Missing credentials for TLS handshake");
	}
	
	// Perform the TLS handshake
	int ret;
	do {
                ret = gnutls_handshake(mSession);
        }
        while (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);
	
        if (ret < 0) throw Exception("TLS handshake failed");
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

ssize_t SecureTransport::WriteCallback(gnutls_transport_ptr_t ptr, void* data, size_t len)
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

SecureTransportClient::SecureTransportClient(Stream *stream, Credentials *creds) :
	SecureTransport(false, stream, NULL)
{
	if(creds) handshake(creds);
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
	std::memcpy(datum.data, key.data(), datum.size);
	
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
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_PSK, mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportClient::Certificate::Certificate(const String &cafile)
{
	// Allocate certificate credentials
	Assert(gnutls_certificate_allocate_credentials(&mCreds) == GNUTLS_E_SUCCESS);
	
	// Set certificate
	if(gnutls_certificate_set_x509_trust_file(mCreds, cafile.c_str(), GNUTLS_X509_FMT_PEM) != GNUTLS_E_SUCCESS)
		throw Exception("Invalid CA file: " + cafile);
}

SecureTransportClient::Certificate::~Certificate(void)
{
	gnutls_certificate_free_credentials(mCreds);
}

void SecureTransportClient::Certificate::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, mCreds) == GNUTLS_E_SUCCESS);
}

SecureTransportServer::SecureTransportServer(Stream *stream, Credentials *creds) :
	SecureTransport(true, stream, NULL)
{
	if(creds) handshake(creds);
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

Map<gnutls_session_t, SecureTransportServer::PrivateSharedKeyCallback*> SecureTransportServer::PrivateSharedKeyCallback::CredsMap;
Map<SecureTransportServer::PrivateSharedKeyCallback*, gnutls_session_t> SecureTransportServer::PrivateSharedKeyCallback::CredsMapReverse;
Mutex SecureTransportServer::PrivateSharedKeyCallback::CredsMapMutex;

int SecureTransportServer::PrivateSharedKeyCallback::CredsCallback(gnutls_session_t session, const char* username, gnutls_datum_t* datum)
{
	PrivateSharedKeyCallback *pskcb = NULL;
	CredsMapMutex.lock();
	CredsMap.get(session, pskcb);
	CredsMapMutex.unlock();
	
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
	
	// Remove mapping
	CredsMapMutex.lock();
	gnutls_session_t mSession;
	if(CredsMapReverse.get(this, mSession))
	{
		CredsMap.erase(mSession);
		CredsMapReverse.erase(this);
	}
	CredsMapMutex.unlock();
}

void SecureTransportServer::PrivateSharedKeyCallback::install(gnutls_session_t session)
{
	Assert(gnutls_credentials_set(session, GNUTLS_CRD_PSK, mCreds) == GNUTLS_E_SUCCESS);
	
	// Set mapping
	CredsMapMutex.lock();
	CredsMap.insert(session, this);
	CredsMapReverse.insert(this, session);
	CredsMapMutex.unlock();
}

}
