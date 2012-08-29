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
#include "html.h"
#include "store.h"
#include "core.h"

namespace arc
{

Httpd::Httpd(int port) :
	mSock(port)
{

}

Httpd::~Httpd(void)
{
	mSock.close();	// useless
}

void Httpd::run(void)
{
	try {
		while(true)
		{
			Socket *sock = new Socket;
			mSock.accept(*sock);
			Handler *client = new Handler(this, sock);
			client->start(true); // client will destroy itself
		}
	}
	catch(const NetException &e)
	{
		return;
	}
}

Httpd::Handler::Handler(Httpd *httpd, Socket *sock) :
		mHttpd(httpd),
		mSock(sock)
{

}

Httpd::Handler::~Handler(void)
{
	delete mSock;	// deletion closes the socket
}

void Httpd::Handler::run(void)
{
	Http::Request request;
	try {
		request.recv(*mSock);

		String expect;
		if(request.headers.get("Expect",expect)
				&& expect.toLower() == "100-continue")
		{
			mSock->writeLine("HTTP/1.1 100 Continue\r\n\r\n");
		}

		process(request);
	}
	catch(int code)
	{
		Http::Response response(request, code);
		response.headers["Content-Type"] = "text/html; charset=UTF-8";
		response.send();

		if(request.method != "HEAD")
		{
			Html page(response.sock);
			page.header(response.message);
			page.open("h1");
			page.text(String::number(response.code) + " - " + response.message);
			page.close("h1");
			page.footer();
		}
	}
}

void Httpd::Handler::process(Http::Request &request)
{
	try {
		// TODO: registering system

		if(request.url.substr(0,6) == "/peers")
		{
			Core::Instance->http(request);
		}
		else {
			Store::Instance->http(request);
		}

		/*Response response(request);
		response.sock = mSock;

		// TODO

		response.send();
		if(request.method != "HEAD")
		{
			Html page(mSock);
			page.header("Test page");
			page.open("h1");
			page.text("Hello world !");
			page.close("h1");
			page.footer();
		}*/
	}
	catch(Exception &e)
	{
		throw 500;
	}
}

}
