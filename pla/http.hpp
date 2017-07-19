/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#ifndef PLA_HTTP_H
#define PLA_HTTP_H

#include "pla/include.hpp"
#include "pla/string.hpp"
#include "pla/stream.hpp"
#include "pla/serversocket.hpp"
#include "pla/threadpool.hpp"
#include "pla/securetransport.hpp"
#include "pla/file.hpp"
#include "pla/map.hpp"

namespace pla
{

class Http
{
public:
	static String UserAgent;
	static duration ConnectTimeout;
	static duration RequestTimeout;

	struct Request
	{
		Request(void);
		Request(const String &url, const String &method = "GET");
		virtual ~Request(void);

		void send(Stream *stream);
		void recv(Stream *stream, bool parsePost = true);
		void clear(void);
		bool extractRange(int64_t &rangeBegin, int64_t &rangeEnd, int64_t contentLength = -1) const;

		String protocol;		// HTTP or HTTPS
		String method;			// GET, POST, HEAD...
		String version;			// 1.0 or 1.1
		String url;				// URL without host and parameters
		StringMap headers;		// HTTP headers
		StringMap cookies;		// Cookies
		StringMap get;			// URL parameters
		StringMap post;			// POST parameters
		Map<String, TempFile*> files;	// Files posted with POST
		Address remoteAddress;			// Remote address, set by Server

		String fullUrl;		// URL with parameters
		Stream *stream;		// Internal use for Response construction
	};

	struct Response
	{
		Response(int code = 200);
		Response(const Request &request, int code = 200);

		void send(void);
		void send(Stream *stream);
		void recv(Stream *stream);
		void clear(void);

		int code;			// Response code
		String version;		// 1.0 or 1.1
		String message;		// Message
		StringMap headers;	// HTTP headers
		StringMap cookies;	// Cookies

		Stream *stream;		// Stream where to send/receive data
	};

	class Server
	{
	public:
		Server(int port = 80, int threads = 8);
		virtual ~Server(void);

		virtual void process(Http::Request &request) = 0;
		virtual void generate(Stream &out, int code, const String &message);

	protected:
		virtual void handle(Stream *stream, const Address &remote);
		virtual void respondWithFile(const Request &request, const String &fileName);

		ServerSocket mSock;
		ThreadPool mPool;

	private:
		void run(void);
	};

	class SecureServer : public Server
	{
	public:
		SecureServer(SecureTransportServer::Credentials *credentials, int port = 443);	// credentials will be deleted
		virtual ~SecureServer(void);

	protected:
		virtual void handle(Stream *stream, const Address &remote);

	private:
		SecureTransportServer::Credentials *mCredentials;
	};

	static int Action(const String &method, const String &url, const String &data, const StringMap &headers, Stream *output = NULL, StringMap *responseHeaders = NULL, StringMap *cookies = NULL, int maxRedirections = 5, bool noproxy = false);
	static int Get(const String &url, Stream *output = NULL, StringMap *cookies = NULL, int maxRedirections = 5, bool noproxy = false);
	static int Post(const String &url, const StringMap &post, Stream *output = NULL, StringMap *cookies = NULL, int maxRedirections = 5, bool noproxy = false);
	static int Post(const String &url, const String &data, const String &type, Stream *output = NULL, StringMap *cookies = NULL, int maxRedirections = 5, bool noproxy = false);
	static String AppendParam(const String &url, const String &name, const String &value = "1");

private:
	Http(void);
};

}

#endif
