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

#include "httpd.h"

namespace arc
{

Httpd::Httpd(int port) :
		mSock(port)
{

}

Httpd::~Httpd(void)
{
	mSock.close();
	// TODO: delete clients
}

void Httpd::run(void)
{
	try {
		while(true)
		{
			Socket sock = mSock.accept();
			Handler *client = new Handler(this, sock);
			client->start();
		}
	}
	catch(const NetException &e)
	{
		return;
	}
}

Httpd::Handler::Handler(Httpd *httpd, const Socket &sock) :
		mHttpd(httpd),
		mSock(sock)
{

}

Httpd::Handler::~Handler(void)
{
	mSock.close();
}

void Httpd::Handler::run(void)
{
	String action;
	if(!mSock.readLine(action)) return;
	String url = action.cut(' ');
	String protocol = url.cut(' ');
	action.trim();
	url.trim();
	protocol.trim();

	if(action != "GET" && action != "POST")
		error(500);

	if(url.empty() || protocol.empty())
		error(500);

	StringMap headers;
	while(true)
	{
		String line;
		assertIO(mSock.readLine(line));
		if(line.empty()) break;

		String value = line.cut(':');
		line.trim();
		value.trim();
		headers.insert(line,value);
	}

	String file(url);
	String getData = file.cut('?');
	StringMap get;
	if(!getData.empty())
	{
		std::list<String> exploded;
		getData.explode(exploded,'&');
		for(	std::list<String>::iterator it = exploded.begin();
				it != exploded.end();
				++it)
		{
			String value = it->cut('=');
			get.insert(*it, value);
		}
	}

	StringMap post;
	if(action == "POST")
	{
		// TODO: read post data
	}

	process(file, url, headers, get, post);
}

void Httpd::Handler::error(int code)
{
	// TODO
}

void Httpd::Handler::process(	const String &file,
								const String &url,
								StringMap &headers,
								StringMap &get,
								StringMap &post)
{
	// TODO
}

}
