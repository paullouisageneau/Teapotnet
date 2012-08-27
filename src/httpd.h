/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef ARC_HTTPD_H
#define ARC_HTTPD_H

#include "include.h"
#include "exception.h"
#include "map.h"
#include "socket.h"
#include "serversocket.h"
#include "thread.h"

namespace arc
{

class Httpd : public Thread
{
public:
	Httpd(int port = 80);
	~Httpd(void);

	struct Request
	{
		Request(Socket *sock = NULL);

		void parse(Socket *sock);
		void clear(void);

		String method;          // GET, POST, HEAD...
		String protocol;        // HTTP
		String version;         // 1.0 or 1.1
		String file;            // URL without parameters
		String url;             // Complete URL
		StringMap headers;      // HTTP headers
		StringMap cookies;      // Cookies
		StringMap get;          // URL parameters
		StringMap post;         // POST parameters
		Socket *sock;			// Internal use for Response construction
	};

	struct Response
	{
		Response(const Request &request, int code = 200);

		void send(void);
		void clear(void);

		int code;			// Response code
		String version;		// 1.0 or 1.1
		String message;		// Message
		StringMap headers;	// HTTP headers
		Socket *sock;		// Socket where to send data
	};

private:
	class Handler : public Thread
	{
	public:
		Handler(Httpd *httpd, Socket *sock);
		~Handler(void);

	private:
		void run(void);
		void process(Request &request);

		Httpd *mHttpd;
		Socket *mSock;
	};

	void run(void);

	ServerSocket mSock;

	friend class Handler;
};

}

#endif
