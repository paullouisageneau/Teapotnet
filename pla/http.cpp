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

#include "pla/http.hpp"
#include "pla/exception.hpp"
#include "pla/directory.hpp"
#include "pla/mime.hpp"
#include "pla/proxy.hpp"

namespace pla
{

String Http::UserAgent = "unknown";
duration Http::ConnectTimeout = seconds(10.);
duration Http::RequestTimeout = seconds(10.);

Http::Request::Request(void)
{
	clear();
}

Http::Request::~Request(void)
{
	clear();
}

Http::Request::Request(const String &url, const String &method)
{
	clear();

	int p = url.find("://");
	if(p == String::NotFound) this->url = url;
	else {
		protocol = url.substr(0,p);
		protocol = protocol.toUpper();
		if(protocol != "HTTP" && protocol != "HTTPS")
			throw Exception(String("Unknown protocol in URL: ")+protocol);

		String host(url.substr(p+3));
		this->url = String('/') + host.cut('/');

		if(host.contains('@'))
			throw Unsupported("HTTP authentication");

		headers["Host"] = host;
	}

	if(!method.empty()) this->method = method.toUpper();

	this->headers["User-Agent"] = UserAgent;
	this->headers["Accept-Charset"] = "utf-8";	// force UTF-8
}

void Http::Request::send(Stream *stream)
{
	Assert(stream);
	this->stream = stream;

	if(method != "CONNECT" && version == "1.1" && !headers.contains("Connection"))
		headers["Connection"] = "close";

	//if(!headers.contains("Accept-Encoding"))
	//	headers["Accept-Encoding"] = "identity";

	String completeUrl(url);
	if(!get.empty())
	{
		if(completeUrl.find('?') == String::NotFound)
			completeUrl+= '?';

		for(	StringMap::iterator it = get.begin();
				it != get.end();
				++it)
		{
			completeUrl<<'&'<<it->first.urlEncode()<<'='<<it->second.urlEncode();
		}
	}

	String postData;
	for(	StringMap::iterator it = post.begin();
			it != post.end();
			++it)
	{
		if(!postData.empty()) postData<<'&';
		postData<<it->first.urlEncode()<<'='<<it->second.urlEncode();
	}

	if(!postData.empty())
	{
		headers["Content-Length"] = "";
		headers["Content-Length"] << postData.size();
		headers["Content-Type"] = "application/x-www-form-urlencoded";
	}

	String buf;
	buf<<method<<" "<<completeUrl<<" HTTP/"<<version<<"\r\n";

	for(	StringMap::iterator it = headers.begin();
			it != headers.end();
			++it)
	{
		List<String> lines;
		it->second.remove('\r');
		it->second.explode(lines,'\n');
		for(	List<String>::iterator l = lines.begin();
				l != lines.end();
				++l)
		{
			buf<<it->first<<": "<<*l<<"\r\n";
		}
	}

	if(!cookies.empty())
	{
		buf<<"Cookie: ";
		for(	StringMap::iterator it = cookies.begin();
				it != cookies.end();
				++it)
		{
			if(it != cookies.begin()) buf<<"; ";
			buf<<it->first<<'='<<it->second;
		}
		buf<<"\r\n";
	}

	buf<<"\r\n";
	*stream<<buf;

	if(!postData.empty())
		*stream<<postData;
}

void Http::Request::recv(Stream *stream, bool parsePost)
{
	Assert(stream);
	this->stream = stream;

	clear();

	// Read first line
	String line;
	if(!stream->readLine(line)) throw NetException("Connection closed");
	method.clear();
	url.clear();
	line.readString(method);
	line.readString(url);

	String protocol;
	line.readString(protocol);
	version = protocol.cut('/');

	if(url.empty() || version.empty() || protocol != "HTTP")
		throw 400;

	if(method != "GET" && method != "POST" && method != "HEAD")
		throw 405;

	// Read headers
	while(true)
	{
		String line;
		AssertIO(stream->readLine(line));
		if(line.empty()) break;

		String value = line.cut(':');
		line.trim();
		value.trim();
		headers.insert(line,value);
	}

	// Read cookies
	String cookie;
	if(headers.get("Cookie", cookie))
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

	fullUrl = url;

	// Read URL variables
	String getData = url.cut('?');
	if(!getData.empty())
	{
		List<String> exploded;
		getData.explode(exploded,'&');
		for(	List<String>::iterator it = exploded.begin();
				it != exploded.end();
				++it)
		{
			String value = it->cut('=').urlDecode();
			get.insert(it->urlDecode(), value);
		}
	}

	url = url.urlDecode();

	String expect;
	if((headers.get("Expect",expect) && expect.toLower() == "100-continue")
		|| (method == "POST" && version == "1.1"))
	{
		stream->write("HTTP/1.1 100 Continue\r\n\r\n");
	}

	// Read post variables
	if(method == "POST" && parsePost)
	{
		if(!headers.contains("Content-Length"))
			throw Exception("Missing Content-Length header in POST request");

		size_t contentLength = 0;
		headers["Content-Length"].extract(contentLength);

		String contentType;
		if(headers.get("Content-Type", contentType))
		{
			String parameters = contentType.cut(';');
			contentType.trim();

			if(contentType == "application/x-www-form-urlencoded")
			{
				String data;
				if(stream->read(data, contentLength) != contentLength)
					throw NetException("Connection unexpectedly closed");

				List<String> exploded;
				data.explode(exploded,'&');
				for(	List<String>::iterator it = exploded.begin();
						it != exploded.end();
						++it)
				{
					String value = it->cut('=').urlDecode();
					post.insert(it->urlDecode(), value);
				}
			}
			else if(contentType == "multipart/form-data")
			{
				String boundary;
				while(true)
				{
					String key;
			  		if(!parameters.readUntil(key,';')) break;
					String value = key.cut('=');
					key.trim();
					value.trim();
					value.trimQuotes();
					if(key == "boundary") boundary = String("--") + value;
				}

				Assert(!boundary.empty());

				String line;
				while(line.empty()) AssertIO(stream->readLine(line));
				Assert(line == boundary);

				bool finished = false;
				while(!finished)
				{
					StringMap mimeHeaders;
					while(true)
					{
						String line;
						AssertIO(stream->readLine(line));
						if(line.empty()) break;

						String value = line.cut(':');
						line.trim();
						value.trim();
						mimeHeaders.insert(line,value);
					}

					String contentType;
					if(mimeHeaders.get("Content-Type", contentType))
					{
						String parameters = contentType.cut(';');
						contentType.trim();
					}

					String contentDisposition;
					if(!mimeHeaders.get("Content-Disposition", contentDisposition))
						throw Exception("Missing Content-Disposition header in multipart POST request");

					String parameters = contentDisposition.cut(';');
					contentDisposition.trim();

					String name, fileName;
					while(true)
					{
						String key;
			  			if(!parameters.readUntil(key,';')) break;
						String value = key.cut('=');
						key.trim();
						value.trim();
						value.trimQuotes();
						if(key == "name") name = value;
						else if(key == "filename") fileName = value;
					}

					Stream *output = NULL;
					if(fileName.empty()) output = &post[name];
					else {
						post[name] = fileName;
						TempFile *tempFile = new TempFile();
						files[name] = tempFile;
						output = tempFile;
						LogDebug("Http::Request", String("File upload: ") + fileName);
					}

					String contentLength;
					if(mimeHeaders.get("Content-Length", contentLength))
					{
						int64_t size = 0;
						contentLength >> size;
						stream->read(*output,size);

						String line;
						AssertIO(stream->readLine(line));
						AssertIO(stream->readLine(line));
						if(line == boundary + "--") finished = true;
						else AssertIO(line == boundary);
					}
					else {
					  	/*String line;
						while(stream->readLine(line))
						{
							if(line == boundary) break;
							output->write(line);
							line.clear();
						}*/

						// TODO: This must be replaced with Boyer-Moore algorithm for performance
						String bound = String("\r\n") + boundary;
						char *buffer = new char[bound.size()];
						try {
							int size = 0;
							int c = 0;
							int i = 0;
							while(true)
							{
								if(c == size)
								{
									c = 0;
									size = stream->readData(buffer,bound.size()-i);
									AssertIO(size);
								}

								if(buffer[c] == bound[i])
								{
									++i; ++c;
									if(i == bound.size())
									{
										// If we are here there is no data left in buffer
										String line;
										AssertIO(stream->readLine(line));
										if(line == "--") finished = true;
										break;
									}
								}
								else {
									if(i) output->writeData(bound.data(), i);
									int d = c;
									while(c != size && buffer[c] != bound[0]) ++c;
									if(c != d) output->writeData(buffer + d, c - d);
									i = 0;
								}
							}
						}
						catch(...)
						{
							delete[] buffer;
							throw;
						}
						delete[] buffer;
					}
				}
			}
			else {
				LogWarn("Http::Request", String("Unknown encoding: ") + contentType);
				stream->ignore(contentLength);
			}
		}
		else {
			LogWarn("Http::Request", "Missing Content-Type header in POST request");
			stream->ignore(contentLength);
		}
	}
}

void Http::Request::clear(void)
{
	method = "GET";
	version = "1.0";	// 1.1 requires chunked transfer encoding to be implemented
	url.clear();
	headers.clear();
	cookies.clear();
	get.clear();
	post.clear();
	fullUrl.clear();

	for(Map<String, TempFile*>::iterator it = files.begin(); it != files.end(); ++it)
	 	delete it->second;

	files.clear();
}

bool Http::Request::extractRange(int64_t &rangeBegin, int64_t &rangeEnd, int64_t contentLength) const
{
	if(contentLength < 0) contentLength = std::numeric_limits<int64_t>::max();
	rangeBegin = 0;
	rangeEnd = contentLength-1;

	String range;
	if(headers.get("Range",range))
	{
		range.cut(';');
		String tmp = range.cut('=');
		if(range != "bytes") throw 416;
		else {
			List<String> intervals;
			tmp.explode(intervals, ',');

			rangeBegin = contentLength-1;
			rangeEnd = 0;
			while(!intervals.empty())
			{
				String sbegin = intervals.front();
				String send = sbegin.cut('-');
				intervals.pop_front();

				int64_t begin, end;
				if(!send.empty())   send >> end;
				else end = contentLength-1;
				if(!sbegin.empty()) sbegin >> begin;
				else {
					begin = contentLength-end;
					end = contentLength-1;
				}

				if(begin >= contentLength || end >= contentLength || begin > end)
					throw 416;

				rangeBegin = std::min(begin, rangeBegin);
				rangeEnd   = std::max(end, rangeEnd);
			}

			return true;
		 }
	}

	return false;
}

Http::Response::Response(int code)
{
	clear();

	this->code = code;
	if(code != 204)	// 204 No content
		this->headers["Content-Type"] = "text/html; charset=UTF-8";
}

Http::Response::Response(const Request &request, int code)
{
	clear();

	this->code = code;
	this->version = request.version;
	this->stream = request.stream;
	if(code != 204)	// 204 No content
		this->headers["Content-Type"] = "text/html; charset=UTF-8";
}

void Http::Response::send(void)
{
	if(!stream) throw Exception("No stream specified for HTTP response");
	send(stream);
}

void Http::Response::send(Stream *stream)
{
	Assert(stream);
	this->stream = stream;

	if(version == "1.1" && code >= 200 && !headers.contains("Connexion"))
		headers["Connection"] = "close";

	if(!headers.contains("Date"))
		headers["Date"] = Time::Now().toHttpDate();

	if(!headers.contains("Last-Modified"))
		headers["Last-Modified"] = headers["Date"];

	if(message.empty())
	{
		switch(code)
		{
		case 100: message = "Continue";				break;
		case 200: message = "OK";					break;
		case 204: message = "No content";			break;
		case 206: message = "Partial Content";		break;
		case 301: message = "Moved Permanently"; 	break;
		case 302: message = "Found";				break;
		case 303: message = "See Other";			break;
		case 304: message = "Not Modified"; 		break;
		case 305: message = "Use Proxy"; 			break;
		case 307: message = "Temporary Redirect";	break;
		case 400: message = "Bad Request"; 			break;
		case 401: message = "Unauthorized";			break;
		case 403: message = "Forbidden"; 			break;
		case 404: message = "Not Found";			break;
		case 405: message = "Method Not Allowed";	break;
		case 406: message = "Not Acceptable";		break;
		case 408: message = "Request Timeout";		break;
		case 409: message = "Conflict";				break;
		case 410: message = "Gone";					break;
		case 413: message = "Request Entity Too Large"; 		break;
		case 414: message = "Request-URI Too Long";				break;
		case 416: message = "Requested Range Not Satisfiable";	break;
		case 418: message = "I'm a teapot";						break;
		case 500: message = "Internal Server Error";			break;
		case 501: message = "Not Implemented";					break;
		case 502: message = "Bad Gateway";						break;
		case 503: message = "Service Unavailable";				break;
		case 504: message = "Gateway Timeout";					break;
		case 505: message = "HTTP Version Not Supported";		break;

		default:
			if(code < 300) message = "OK";
			else message = "Error";
			break;
		}
	}

	String buf;
	buf<<"HTTP/"<<version<<" "<<code<<" "<<message<<"\r\n";

	for(StringMap::iterator it = headers.begin(); it != headers.end(); ++it)
	{
		List<String> lines;
		it->second.remove('\r');
		it->second.explode(lines,'\n');
		for(	List<String>::iterator l = lines.begin();
				l != lines.end();
				++l)
		{
			buf<<it->first<<": "<<*l<<"\r\n";
		}
	}

	for(StringMap::iterator it = cookies.begin(); it != cookies.end(); ++it)
		buf<<"Set-Cookie: "<<it->first<<'='<<it->second<<"; Path=/\r\n";

	buf<<"\r\n";
	*stream<<buf;
}

void Http::Response::recv(Stream *stream)
{
	this->stream = stream;
	clear();

	// Read first line
	String line;
	if(!stream->readLine(line)) throw NetException("Connection closed");

	String protocol;
	line.readString(protocol);
	version = protocol.cut('/');

	line.read(code);
	message = line;

	if(version.empty() || protocol != "HTTP")
		throw Exception("Invalid HTTP response");

	// Read headers
	while(true)
	{
		String line;
		AssertIO(stream->readLine(line));
		if(line.empty()) break;

		String value = line.cut(':');
		line.trim();
		value.trim();

		if(line == "Set-Cookie")
		{
			String cookie = value;
			cookie.cut(';');
			value = cookie.cut('=');
			cookies.insert(cookie, value);
		}
		else {
			headers.insert(line, value);
		}
	}
}

void Http::Response::clear(void)
{
	code = 200;
	version = "1.0";
	message.clear();
	cookies.clear();
}

Http::Server::Server(int port, int threads) :
	mSock(port),
	mPool(threads)
{
	mPool.enqueue([this]()
	{
		this->run();
	});
}

Http::Server::~Server(void)
{
	mSock.close();
	mPool.join();
}

void Http::Server::generate(Stream &out, int code, const String &message)
{
	out<<"<!DOCTYPE html>\n";
	out<<"<html>\n";
	out<<"<head><title>"<<message<<"</title></head>\n";
	out<<"<body><h1>"<<code<<" "<<message<<"</h1></body>\n";
	out<<"</html>\n";
}

void Http::Server::handle(Stream *stream, const Address &remote)
{
	Request request;
	try {
		try {
			request.recv(stream);
			request.remoteAddress = remote;
			process(request);
		}
		catch(const Timeout &e)
		{
			throw 408;
		}
		catch(const NetException &e)
		{
			LogDebug("Http::Server::Handler", e.what());
		}
 		catch(const std::exception &e)
		{
			LogWarn("Http::Server::Handler", e.what());
			throw 500;
		}
	}
	catch(int code)
	{
		try {
			Response response(request, code);
			response.headers["Content-Type"] = "text/html; charset=UTF-8";
			response.send();

			if(request.method != "HEAD")
				generate(*response.stream, response.code, response.message);
		}
		catch(...)
		{

		}
	}
}

void Http::Server::respondWithFile(const Request &request, const String &fileName)
{
	int code = 200;
	File file;

	if(!File::Exist(fileName)) code = 404;
	else {
		if(request.method != "GET" && request.method != "HEAD") code = 405;
		else {
			String ifModifiedSince;
			if(request.headers.get("If-Modified-Since", ifModifiedSince))
			{
				try {
					Time time(ifModifiedSince);
					if(time >= File::Time(fileName))
					{
						Response response(request, 304);
						response.send();
						return;
					}
				}
				catch(const Exception &e)
				{
					LogWarn("Http::respondWithFile", e.what());
				}
			}

			try {
				file.open(fileName, File::Read);
			}
			catch(...)
			{
				code = 403;
			}
		}
	}

	int64_t rangeBegin = 0;
	int64_t rangeEnd = 0;
	bool hasRange = request.extractRange(rangeBegin, rangeEnd, file.size());
	if(rangeBegin >= file.size() || rangeEnd >= file.size())
		code = 406;

	if(code != 200)
	{
		Response response(request, code);
		response.headers["Content-Type"] = "text/html; charset=UTF-8";
		response.send();

		if(request.method != "HEAD")
			generate(*response.stream, response.code, response.message);

		return;
	}

	if(hasRange) code = 206;
	Response response(request, code);

	String name = fileName.afterLast(Directory::Separator);
	if(name != request.url.afterLast('/'))
	{
		response.headers["Content-Name"] = name;
		response.headers["Content-Disposition"] = "inline; filename=\"" + name + "\"";
	}

	if(hasRange)
	{
		response.headers["Content-Length"] << (rangeEnd - rangeBegin + 1);
		response.headers["Content-Range"] << rangeBegin << "-" << rangeEnd << "/" << file.size();
	}
	else {
		response.headers["Content-Length"] << file.size();
	}

	response.headers["Accept-Ranges"] = "bytes";
	response.headers["Content-Type"] = Mime::GetType(fileName);
	response.headers["Last-Modified"] = File::Time(fileName).toHttpDate();

	response.send();

	if(request.method != "HEAD")
	{
		if(hasRange)
		{
			file.seekRead(rangeBegin);
			file.read(*response.stream, rangeEnd - rangeBegin + 1);
		}
		else {
			file.read(*response.stream);
		}
		file.close();
	}
}

void Http::Server::run(void)
{
	Socket *sock = NULL;
	try {
		while(true)
		{
			sock = new Socket;
			mSock.accept(*sock);
			sock->setReadTimeout(RequestTimeout);

			mPool.enqueue([this, sock]()
			{
				this->handle(sock, sock->getRemoteAddress());
				delete sock;
			});

			sock = NULL;
		}
	}
	catch(const NetException &e)
	{
		delete sock;
		return;
	}
	catch(...)
	{
		delete sock;
		throw;
	}
}

Http::SecureServer::SecureServer(SecureTransportServer::Credentials *credentials, int port) :
	Server(port),
	mCredentials(credentials)
{

}

Http::SecureServer::~SecureServer(void)
{
	delete mCredentials;
}

void Http::SecureServer::handle(Stream *stream, const Address &remote)
{
	SecureTransportServer *transport = NULL;
	try {
		transport = new SecureTransportServer(stream);
		transport->addCredentials(mCredentials);
		transport->handshake();

		Server::handle(transport, remote);
	}
	catch(const std::exception &e)
	{
		LogDebug("Http::SecureServer::Handler", e.what());
	}

	delete transport;
}

int Http::Action(const String &method, const String &url, const String &data, const StringMap &headers, Stream *output, StringMap *responseHeaders, StringMap *cookies, int maxRedirections, bool noproxy)
{
	Request request(url, method);
	request.headers.insert(headers);

	String host;
	if(!request.headers.get("Host", host))
		throw Exception("Invalid URL");

	if(!data.empty())
		request.headers["Content-Length"] = String::number(data.size());

	if(cookies)
		request.cookies = *cookies;

	Socket *sock = new Socket;
	try {
		sock->setConnectTimeout(ConnectTimeout);
		sock->setReadTimeout(RequestTimeout);

		Address proxyAddr;
		if(!noproxy && Proxy::GetProxyForUrl(url, proxyAddr))
		{
			try {
				sock->connect(proxyAddr, true);	// Connect without proxy

				if(request.protocol == "HTTP")
				{
					request.url = url;              // Full URL for proxy
				}
				else {	// HTTPS
					String connectHost = host;
					if(!connectHost.contains(':')) connectHost+= ":443";
					Http::Request connectRequest(connectHost, "CONNECT");
					connectRequest.version = "1.1";
					connectRequest.headers["Host"] = connectHost;
					connectRequest.send(sock);

					Http::Response connectResponse;
					connectResponse.recv(sock);

					if(connectResponse.code != 200)
					{
						String msg = String::number(connectResponse.code) + " " + connectResponse.message;
						LogWarn("Http::Action", String("HTTP proxy error: ") + msg);
						throw Exception(msg);
					}
				}
			}
			catch(const NetException &e)
			{
				LogWarn("Http::Action", String("HTTP proxy error: ") + e.what());
				throw;
			}
		}
		else {
			List<Address> addrs;
			if(!Address::Resolve(host, addrs, request.protocol.toLower()))
				throw NetException("Unable to resolve: " + host);

			for(List<Address>::iterator it = addrs.begin(); it != addrs.end(); ++it)
			{
				try {
					sock->connect(*it, true);	// Connect without proxy
					break;
				}
				catch(const NetException &e)
				{
					// Connection failed for this address
				}
			}

			if(!sock->isConnected())
				throw NetException("Connection to " + host + " failed");
		}
	}
	catch(...)
	{
		delete sock;
		throw;
	}

	Stream *stream = sock;
	try {
		if(request.protocol == "HTTPS")
			stream = new SecureTransportClient(sock, new SecureTransportClient::Certificate, host);

		request.send(stream);
		if(!data.empty())
			stream->write(data);

		Response response;
		response.recv(stream);

		if(cookies)
			cookies->insertAll(response.cookies);

		if(maxRedirections && response.code/100 == 3 && response.headers.contains("Location"))
		{
			stream->discard();
			delete stream;
			stream = NULL;

			String location(response.headers["Location"]);
			if(!location.empty())
			{
				// Handle relative location even if not RFC-compliant
				if(!location.contains(":/"))
				{
					if(location[0] == '/') location = request.protocol.toLower() + "://" + host + location;
					else {
						int p = url.lastIndexOf('/');
						Assert(p > 0);
						location = url.substr(0, p) + "/" + location;
					}
				}

				return Get(location, output, cookies, maxRedirections-1, noproxy);
			}
		}

		if(responseHeaders)
			*responseHeaders = response.headers;

		if(output) stream->read(*output);
		else stream->discard();
		delete stream;
		stream = NULL;
		return response.code;
	}
	catch(...)
	{
		delete stream;
		throw;
	}
}

int Http::Get(const String &url, Stream *output, StringMap *cookies, int maxRedirections, bool noproxy)
{
	StringMap headers;
	return Action("GET", url, "", headers, output, NULL, cookies, maxRedirections, noproxy);
}

int Http::Post(const String &url, const StringMap &post, Stream *output, StringMap *cookies, int maxRedirections, bool noproxy)
{
	String postData;
	for(StringMap::const_iterator it = post.begin(); it != post.end(); ++it)
	{
		if(!postData.empty()) postData<<'&';
		postData<<it->first.urlEncode()<<'='<<it->second.urlEncode();
	}

	StringMap headers;
	headers["Content-Type"] = "application/x-www-form-urlencoded";

	return Action("POST", url, postData, headers, output, NULL, cookies, maxRedirections, noproxy);
}

int Http::Post(const String &url, const String &data, const String &type, Stream *output, StringMap *cookies, int maxRedirections, bool noproxy)
{
	StringMap headers;
	headers["Content-Type"] = type;

	return Action("POST", url, data, headers, output, NULL, cookies, maxRedirections, noproxy);
}

String Http::AppendParam(const String &url, const String &name, const String &value)
{
	char separator = '?';
	if(url.contains(separator)) separator = '&';
	return url + separator + name + "=" + value;
}

}
