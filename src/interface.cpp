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

#include "interface.h"
#include "html.h"
#include "user.h"
#include "store.h"
#include "splicer.h"

namespace arc
{

Interface *Interface::Instance = NULL;

Interface::Interface(int port) :
		Http::Server(port)
{

}

Interface::~Interface(void)
{

}

void Interface::add(const String &prefix, HttpInterfaceable *interfaceable)
{
	Assert(interfaceable != NULL);
	
	String cprefix(prefix);
	if(cprefix.empty() || cprefix[0] != '/')
		cprefix = "/" + cprefix;
	
	mMutex.lock();
	if(mPrefixes.contains(cprefix))
	{
		mMutex.unlock();
		throw Exception("URL prefix \""+cprefix+"\" is already registered");

	}
	mPrefixes.insert(cprefix, interfaceable);
	mMutex.unlock();
}

void Interface::remove(const String &prefix)
{
	mMutex.lock();
	mPrefixes.erase(prefix);
	mMutex.unlock();
}

void Interface::process(Http::Request &request)
{
	//Log("Interface", "Request for URL \""+request.url+"\"");
	
	// URL must begin with /
	if(request.url.empty() || request.url[0] != '/') throw 404;

	User *user = NULL;
	String auth;
	if(request.headers.get("Authorization", auth))
	{
	 	String tmp = auth.cut(' ');
		auth.trim();
		tmp.trim();
		if(auth != "Basic") throw 400;
		
		String name = tmp.base64Decode();
		String password = name.cut(':');
		user = User::Authenticate(name, password);
	}
	
	if(user && request.url == "/")
	{
		Http::Response response(request, 303);
		response.headers["Location"] = "/" + user->name();
		response.send();
		return;
	}
	
	List<String> list;
	request.url.explode(list,'/');
	list.pop_front();	// first element is empty because url begin with '/'
	if(list.empty()) throw 500;
	
	if(!user || list.front() != user->name())
	{
	 	String realm;
		if(list.front().empty()) realm = "Arcanet";	// empty iff url == '/'
		else realm = list.front() + " on Arcanet";
		
		Http::Response response(request, 401);
		response.headers.insert("WWW-Authenticate", "Basic realm=\""+realm+"\"");
		response.send();
			
		Html page(response.sock);
		page.header("Arcanet");
		page.open("h1");
		page.text("Authentication required");
		page.close("h1");
		
		page.footer();
		return;
	}

	mMutex.lock();
	while(!list.empty())
	{
		String prefix;
		prefix.implode(list,'/');
		prefix = "/" + prefix;
	 	list.pop_back();
		
		HttpInterfaceable *interfaceable;
		if(mPrefixes.get(prefix,interfaceable)) 
		{
			request.url.ignore(prefix.size());
			
			//Log("Interface", "Matched prefix \""+prefix+"\"");
			
			if(prefix != "/" && request.url.empty())
			{
				Http::Response response(request, 301);	// Moved Permanently
				response.headers["Location"] = prefix+"/";
				response.send();
				mMutex.unlock();
				return;  
			}
			
			interfaceable->http(prefix, request);
			mMutex.unlock();
			return;
		}
	}
	mMutex.unlock();
	
	String url(request.url);
	if(url[0] == '/') url.ignore();
	if(!url.contains('/'))
	{
	 	try {
			Identifier hash;
			url >> hash;  
		
			Store::Entry entry;
			if(Store::Instance->get(hash, entry, true))
			{
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/octet-stream";
				response.headers["Content-Disposition"] = "attachment";
				if(entry.info.contains("name")) response.headers["Content-Disposition"]+= "; filename=\"" + entry.info.get("name") + "\"";
				if(entry.info.contains("hash")) response.headers["Content-SHA512"] = entry.info.get("hash");
				// TODO: Date + Last-Modified
				response.send();
				
				if(entry.content) response.sock->write(*entry.content);
				delete entry.content;
				return;
			}
			else {
				size_t blockSize = Store::ChunkSize;
				String filename("/tmp/"+hash.toString());
				Splicer splicer(hash, filename, blockSize);
				File file(filename, File::Read);
				
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/octet-stream";
				response.headers["Content-Disposition"] = "attachment; filename=\"" + splicer.name() + "\"";
				response.headers["Content-SHA512"] = hash.toString();
				// TODO: Missing headers
				response.send();
				
				size_t current = 0;
				while(!splicer.finished())
				{
					size_t finished = splicer.finishedBlocks();
					while(current < finished)
					{
						if(!file.read(*response.sock, blockSize)) return;
						++current;
					}
					
					msleep(500);
				}
				
				file.read(*response.sock);
				return;
			}
		}
		catch(const std::exception &e)
		{
			Log("Interface::process", String("Error: ") + e.what());
			throw 404;
		}
	}
	
	throw 404;
}

}
