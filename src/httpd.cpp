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
	if(mSock.isConnected())
		mSock.close();
}

void Httpd::Handler::Request::parse(Stream &stream)
{
	clear();

	// Read first line
	String line;
	if(!stream.readLine(line)) return;
	line.readString(method);
	line.readString(url);
	line.readString(protocol);
	version = protocol.cut('/');

	if(url.empty() || protocol != "HTTP")
		throw 400;

	if(method != "GET" && method != "HEAD" && method != "POST")
		throw 405;

	// Read headers
	while(true)
	{
		String line;
		assertIO(stream.readLine(line));
		if(line.empty()) break;

		String value = line.cut(':');
		line.trim();
		value.trim();
		headers.insert(line,value);
	}

	// Read cookies
	String cookie;
	if(headers.get("Cookie",cookie))
	{
		while(!cookie.empty())
		{
			String next = cookie.cut(';');
			String value = cookie.cut('=');
			cookie.trim();
			value.trim();
			cookies.insert(cookie,value);
			cookie = next;
		}
	}

	// Read URL variables
	file = url;
	String getData = file.cut('?');
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

	// Read post variables
	if(method == "POST")
	{
		// TODO: read post data
	}
}

void Httpd::Handler::Request::clear(void)
{
	method.clear();
	protocol.clear();
	version.clear();
	file.clear();
	url.clear();
	headers.clear();
	cookies.clear();
	get.clear();
	post.clear();
}

void Httpd::Handler::run(void)
{
	Request request;

	try {
		request.parse(mSock);

		String expect;
		if(request.headers.get("Expect",expect)
				&& expect.toLower() == "100-continue")
		{
			mSock.writeLine("HTTP/1.1 100 Continue\r\n\r\n");
		}

		process(request);
	}
	catch(int code)
	{
		StringMap headers;
		headers["Content-Type"] = "text/html; charset=UTF-8";

		String message;
		respond(code, headers, request, &message);

		if(request.method != "HEAD")
		{
			Html page(mSock);
			page.header(message);
			page.open("h1");
			page.text(message);
			page.close("h1");
			page.footer();
		}
	}

	mSock.close();
}

void Httpd::Handler::respond(int code, StringMap &headers, Request &request, String *message)
{
	if(request.version == "1.1" && code >= 200)
		headers["Connection"] = "Close";

	if(!headers.contains("Date"))
	{
		// TODO
		time_t rawtime;
		time(&rawtime);
		struct tm *timeinfo = localtime(&rawtime);
		char buffer[256];
		strftime(buffer, 256, "%a, %d %b %Y %H:%M:%S %Z", timeinfo);
		headers["Date"] = buffer;
	}

	String msg;
	switch(code)
	{
	case 100: msg = "Continue";				break;
	case 200: msg = "OK";					break;
	case 204: msg = "No content";			break;
	case 206: msg = "Partial Content";		break;
	case 301: msg = "Moved Permanently"; 	break;
	case 302: msg = "Found";			 	break;
	case 303: msg = "See Other";			break;
	case 304: msg = "Not Modified"; 		break;
	case 305: msg = "Use Proxy"; 			break;
	case 307: msg = "Temporary Redirect"; 	break;
	case 400: msg = "Bad Request"; 			break;
	case 401: msg = "Unauthorized";			break;
	case 403: msg = "Forbidden"; 			break;
	case 404: msg = "Not Found";			break;
	case 405: msg = "Method Not Allowed";	break;
	case 406: msg = "Not Acceptable";		break;
	case 408: msg = "Request Timeout";		break;
	case 410: msg = "Gone";					break;
	case 413: msg = "Request Entity Too Large"; 		break;
	case 414: msg = "Request-URI Too Long";				break;
	case 416: msg = "Requested Range Not Satisfiable";	break;
	case 500: msg = "Internal Server Error";			break;
	case 501: msg = "Not Implemented";					break;
	case 502: msg = "Bad Gateway";						break;
	case 503: msg = "Service Unavailable";				break;
	case 504: msg = "Gateway Timeout";					break;
	case 505: msg = "HTTP Version Not Supported";		break;

	default:
		if(code < 300) msg = "OK";
		else msg = "Error";
		break;
	}

	mSock<<"HTTP/";
	if(request.version == "1.1") mSock<<"1.1";
	else mSock<<"1.0";
	mSock<<" "<<code<<" "<<msg<<"\r\n";

	for(	StringMap::iterator it = headers.begin();
			it != headers.end();
			++it)
	{
		std::list<String> lines;
		it->second.remove('\r');
		it->second.explode(lines,'\n');
		for(	std::list<String>::iterator l = lines.begin();
				l != lines.end();
				++l)
		{
			mSock<<it->first<<": "<<*l<<"\r\n";
		}
	}

	mSock<<"\r\n";

	if(message)
	{
		message->clear();
		*message<<code<<" "<<msg;
	}
}

void Httpd::Handler::process(Request &request)
{
	StringMap headers;
	headers["Content-Type"] = "text/html; charset=UTF-8";

	respond(200, headers, request);

	if(request.method != "HEAD")
	{
		Html page(mSock);
		page.header("Test page");
		page.open("h1");
		page.text("Hello world !");
		page.close("h1");
		page.footer();
	}
}

}
