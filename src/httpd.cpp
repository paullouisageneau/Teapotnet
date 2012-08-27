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
			Socket *sock = new Socket;
			mSock.accept(*sock);
			Handler *client = new Handler(this, sock);
			client->start();
		}
	}
	catch(const NetException &e)
	{
		return;
	}
}

void Httpd::Request::parse(Stream &stream)
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
		AssertIO(stream.readLine(line));
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

void Httpd::Request::clear(void)
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

Httpd::Response::Response(const Request &request, int code)
{
	this->code = code;
	this->version = request.version;
}

Httpd::Response::~Response(void)
{

}

void Httpd::Response::send(void)
{
	if(version == "1.1" && code >= 200)
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

	if(message.empty())
	{
		switch(code)
		{
		case 100: message = "Continue";			break;
		case 200: message = "OK";			break;
		case 204: message = "No content";		break;
		case 206: message = "Partial Content";		break;
		case 301: message = "Moved Permanently"; 	break;
		case 302: message = "Found";			break;
		case 303: message = "See Other";		break;
		case 304: message = "Not Modified"; 		break;
		case 305: message = "Use Proxy"; 		break;
		case 307: message = "Temporary Redirect";	break;
		case 400: message = "Bad Request"; 		break;
		case 401: message = "Unauthorized";		break;
		case 403: message = "Forbidden"; 		break;
		case 404: message = "Not Found";		break;
		case 405: message = "Method Not Allowed";	break;
		case 406: message = "Not Acceptable";		break;
		case 408: message = "Request Timeout";		break;
		case 410: message = "Gone";				break;
		case 413: message = "Request Entity Too Large"; 	break;
		case 414: message = "Request-URI Too Long";		break;
		case 416: message = "Requested Range Not Satisfiable";	break;
		case 500: message = "Internal Server Error";		break;
		case 501: message = "Not Implemented";			break;
		case 502: message = "Bad Gateway";			break;
		case 503: message = "Service Unavailable";		break;
		case 504: message = "Gateway Timeout";			break;
		case 505: message = "HTTP Version Not Supported";	break;

		default:
			if(code < 300) message = "OK";
			else message = "Error";
			break;
		}
	}

	*sock<<"HTTP/";
	if(version == "1.1") *sock<<"1.1";
	else *sock<<"1.0";
	*sock<<" "<<code<<" "<<message<<"\r\n";

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
			*sock<<it->first<<": "<<*l<<"\r\n";
		}
	}

	*sock<<"\r\n";
}

void Httpd::Response::clear(void)
{
	code = 0;
	message.clear();
	version.clear();
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
	Request request;

	try {
		request.parse(*mSock);

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
		Response response(request);
		response.sock = mSock;
		response.code = code;
		response.headers["Content-Type"] = "text/html; charset=UTF-8";
		response.send();

		if(request.method != "HEAD")
		{
			Html page(response.sock);
			page.header(response.message);
			page.open("h1");
			page.text(response.message);
			page.close("h1");
			page.footer();
		}
	}
}

void Httpd::Handler::process(Request &request)
{
	Response response(request);
	response.sock = mSock;
	response.headers["Content-Type"] = "text/html; charset=UTF-8";

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
	}
}

}
