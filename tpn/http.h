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

#ifndef TPN_HTTP_H
#define TPN_HTTP_H

#include "tpn/include.h"
#include "tpn/string.h"
#include "tpn/socket.h"
#include "tpn/serversocket.h"
#include "tpn/thread.h"
#include "tpn/map.h"
#include "tpn/file.h"

namespace tpn
{

class Http
{
public:
	struct Request
	{
		Request(void);
		Request(const String &url, const String &method = "GET");
		~Request(void);
		
		void send(Socket &sock);
		void recv(Socket &sock);
		void clear(void);
		bool extractRange(int64_t &rangeBegin, int64_t &rangeEnd, int64_t contentLength = -1) const;
		
		String method;          // GET, POST, HEAD...
		String version;         // 1.0 or 1.1
		String url;             // URL without host and parameters
		StringMap headers;      // HTTP headers
		StringMap cookies;      // Cookies
		StringMap get;          // URL parameters
		StringMap post;         // POST parameters
		Map<String, TempFile*> files; // Files posted with POST
		
		String fullUrl;		// URL with parameters, used only by recv
		Socket *sock;		// Internal use for Response construction
	};

	struct Response
	{
		Response(void);
		Response(const Request &request, int code = 200);

		void send(void);
		void send(Socket &sock);
		void recv(Socket &sock);
		void clear(void);

		int code;		// Response code
		String version;		// 1.0 or 1.1
		String message;		// Message
		StringMap headers;	// HTTP headers

		Socket *sock;		// Socket where to send/receive data
	};

	class Server : public Thread
	{
	public:
		Server(int port = 80);
		~Server(void);

		virtual void process(Http::Request &request) = 0;

	private:
		void run(void);

		class Handler : public Thread
		{
		public:
			Handler(Server *server, Socket *sock);
			~Handler(void);

		private:
			void run(void);

			Server *mServer;
			Socket *mSock;
		};

		ServerSocket mSock;
	};

	static int Get(const String &url, Stream *output = NULL);
	static int Post(const String &url, const StringMap &post, Stream *output = NULL);
	static void RespondWithFile(const Request &request, const String &fileName);
	static String AppendGet(const String &url, const String &name, const String &value = "1");
	
private:
	Http(void);
};

}

#endif
