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

private:
	class Handler : public Thread
	{
	public:
		Handler(Httpd *httpd, const Socket &sock);
		~Handler(void);

	private:
		struct Request
		{
			void parse(Stream &stream);
			void clear(void);

			String method;		// GET, POST, HEAD...
			String protocol;	// HTTP
			String version;		// 1.0 or 1.1
			String file;		// URL without parameters
			String url;			// Complete URL
			StringMap headers;	// HTTP headers
			StringMap cookies;	// Cookies
			StringMap get;		// URL parameters
			StringMap post;		// POST parameters
		};

		void run(void);
		void respond(int code, StringMap &headers, Request &request, String *message = NULL);
		void process(Request &request);

		Httpd *mHttpd;
		Socket mSock;
	};

	void run(void);

	ServerSocket mSock;

	friend class Handler;
};

}

#endif
