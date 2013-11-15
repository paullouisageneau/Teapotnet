/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/interface.h"
#include "tpn/html.h"
#include "tpn/user.h"
#include "tpn/store.h"
#include "tpn/splicer.h"
#include "tpn/config.h"
#include "tpn/directory.h"
#include "tpn/mime.h"

namespace tpn
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
	mPrefixes.insert(cprefix, interfaceable);
	mMutex.unlock();
}

void Interface::remove(const String &prefix, HttpInterfaceable *interfaceable)
{
	mMutex.lock();
	HttpInterfaceable *test = NULL;
	if(mPrefixes.get(prefix, test) && (!interfaceable || test == interfaceable))
		mPrefixes.erase(prefix);
	mMutex.unlock();
}

void Interface::process(Http::Request &request)
{
	Address remoteAddr = request.sock->getRemoteAddress();
  	
	LogDebug("Interface", "Request for URL \""+request.fullUrl+"\"");
	
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
	
#ifdef ANDROID
	if(!user && remoteAddr.isLocal() && User::Count() == 1)
	{
		Array<String> names;
		User::GetNames(names);
		user = User::Get(names[0]);
	}
#endif
	
	if(request.url == "/")
	{
		if(user)
		{
			Http::Response response(request, 303);
			response.headers["Location"] = "/" + user->name();
			response.send();
			return;
		}
		else if(User::Count() == 0) 
		{
			String name;
			try {
				Directory dir(Config::Get("profiles_dir"));
				while(dir.nextFile())
					if(dir.fileIsDir())
					{
						name = dir.fileName();
						break;
					}
			}
			catch(...)
			{

			}

			if(remoteAddr.isLocal() || remoteAddr.isPrivate())
			{
				if(request.method == "POST")
				{
					if(request.post.contains("name"))
					{
						String name = request.post["name"].trimmed();
						String tracker = request.post["tracker"].trimmed();
						if(tracker == Config::Get("tracker")) tracker.clear();
#ifdef ANDROID
						String password = String::random(32);
#else
						String password = request.post["password"];
#endif
						try {
							if(!name.empty() && !password.empty())
							{
								User *user = new User(name, password, tracker);
								// nothing to do
							}
						}
						catch(const Exception &e)
						{
							Http::Response response(request, 200);
							response.send();
							
							Html page(response.sock);
							page.header("Error", false, "/");
							page.text(e.what());
							page.footer();
							return;
						}
					}
					
					Http::Response response(request,303);
					response.send();
					Html page(response.sock);
					page.header("Please wait...", false, "/");
					page.open("h1");
					page.text("The user has been set up.");
					page.close("h1");
					page.open("p");
#ifdef ANDROID
					page.text("Please wait...");
#else
					page.text("Please enter your username and password when prompted to log in.");
#endif
					page.close("p");
					page.footer();
					return;
				}
				
				Http::Response response(request, 200);
				response.send();
			
				Html page(response.sock);
				page.header();
				page.open("h1");
				page.text(String("Welcome to ") + APPNAME + " !");
				page.close("h1");
				page.open("p");
#ifdef ANDROID
				page.text("Please choose your username.");
#else
				if(name.empty()) page.text("No user has been configured yet, please choose your username and password.");
				else page.text("Please reset your password.");
#endif
				page.close("p");
				
				page.openForm("/", "post");
				page.openFieldset("User");
				page.label("name", "Name"); page.input("text", "name"); page.link("#", "Change tracker", "trackerlink"); page.br();
				page.open("div", "trackerselection");
				page.label("tracker", "Tracker"); page.input("tracker", "tracker", Config::Get("tracker")); page.br();
				page.close("div");
#ifndef ANDROID
				page.label("password", "Password"); page.input("password", "password"); page.br();
#endif
				page.label("add"); page.button("add", "OK");
				page.closeFieldset();
				page.closeForm();
				
				page.javascript("$('#trackerselection').hide();\n\
					$('#trackerlink').click(function() {\n\
						$(this).hide();\n\
						$('#trackerselection').show();\n\
					});");
				
				page.footer();
				return;
			}
			else {
				Http::Response response(request, 200);
				response.send();
			
				Html page(response.sock);
				page.header();
				page.open("h1");
				page.text(String("Welcome to ") + APPNAME + " !");
				page.close("h1");
				if(name.empty()) page.text("No user has been configured yet.");
				else page.text("Local intervention needed.");
				page.footer();
				return;
			}
		}
	}
	
	List<String> list;
	request.url.explode(list,'/');
	list.pop_front();	// first element is empty because url begin with '/'
	if(list.empty()) throw 500;
	
	if(list.size() == 1 && list.front().contains('.') && request.url[request.url.size()-1] != '/') 
	{
		String fileName = Config::Get("static_dir") + Directory::Separator + list.front();
		if(File::Exist(fileName)) 
		{
			Http::RespondWithFile(request, fileName);
			return;
		}
		
		if(!User::Exist(list.front())) throw 404;
	}
	
	if(list.front().empty()
		|| User::Exist(list.front()) 
		|| (!user && !remoteAddr.isLocal() && !remoteAddr.isPrivate()))
	{
		if(!user || list.front() != user->name())
		{
			String realm = APPNAME;
			
			Http::Response response(request, 401);
			response.headers.insert("WWW-Authenticate", "Basic realm=\""+realm+"\"");
			response.send();
			
			Html page(response.sock);
			page.header(response.message, true);
			page.open("div", "error");
			page.openLink("/");
			page.image("/error.png", "Error");
			page.closeLink();
			page.br();
			page.br();
			page.open("h1",".huge");
			page.text("Authentication required");
			page.close("h1");
			page.close("div");
			page.footer();
			return;
		}

		while(!list.empty())
		{
			String prefix;
			prefix.implode(list,'/');
			prefix = "/" + prefix;
			list.pop_back();
			
			mMutex.lock();
			HttpInterfaceable *interfaceable;
			if(mPrefixes.get(prefix,interfaceable)) 
			{
				mMutex.unlock();
				
				request.url.ignore(prefix.size());
				
				LogDebug("Interface", "Matched prefix \""+prefix+"\"");
				
				if(prefix != "/" && request.url.empty())
				{
					Http::Response response(request, 301);	// Moved Permanently
					response.headers["Location"] = prefix+"/";
					response.send();
					return;  
				}
				
				interfaceable->http(prefix, request);
				return;
			}
			mMutex.unlock();
		}
	}
	else {
	  	if(list.size() != 1) throw 404; 
	  
	 	try {
			ByteString digest;
			String tmp = list.front();
			try { tmp >> digest; }
			catch(...) { throw 404; }
		
			if(request.get.contains("play") || request.get.contains("playlist"))
			{			  	
				String host;
				if(!request.headers.get("Host", host))
				host = String("localhost:") + Config::Get("interface_port");
					 
				Http::Response response(request, 200);
				response.headers["Content-Disposition"] = "attachment; filename=\"stream.m3u\"";
				response.headers["Content-Type"] = "audio/x-mpegurl";
				response.send();
				
				response.sock->writeLine("#EXTM3U");
				response.sock->writeLine(String("#EXTINF:-1, ") + APPNAME + " stream");
				response.sock->writeLine("http://" + host + "/" + digest.toString());
				response.sock->close();
				
				try {
					// Request the sources now to gain some time afterwards
					Resource resource(digest);
					resource.fetch();
				}
				catch(...) {}
				return;
			}			
		
			// Query resource
			Resource resource(digest);
			resource.fetch();		// this can take some time
			
			// Get range
			int64_t rangeBegin = 0;
			int64_t rangeEnd = 0;
			bool hasRange = request.extractRange(rangeBegin, rangeEnd, resource.size());
			int64_t rangeSize = rangeEnd - rangeBegin + 1;
			
			// Get resource accessor
			Resource::Accessor *accessor = resource.accessor();
			if(!accessor) throw 404;
			
			// Forge HTTP response header
			Http::Response response(request, 200);
			if(!hasRange) response.headers["Content-SHA512"] << resource.digest();
			response.headers["Content-Length"] << rangeSize;
			response.headers["Content-Name"] = resource.name();
			response.headers["Last-Modified"] = resource.time().toHttpDate();
			response.headers["Accept-Ranges"] = "bytes";
			
			if(request.get.contains("download"))
			{
				response.headers["Content-Disposition"] = "attachment; filename=\"" + resource.name() + "\"";
				response.headers["Content-Type"] = "application/octet-stream";
			}
			else {
				response.headers["Content-Disposition"] = "inline; filename=\"" + resource.name() + "\"";
				response.headers["Content-Type"] = Mime::GetType(resource.name());
			}
			
			response.send();
			if(request.method == "HEAD") return;
			
			try {
				// Launch transfer
				if(hasRange) accessor->seekRead(rangeBegin);
				int64_t size = accessor->readBinary(*response.sock, rangeSize);	// let's go !
				if(size != rangeSize)
					throw Exception("range size is " + String::number(rangeSize) + ", but sent size is " + String::number(size));
			}
			catch(const NetException &e)
			{
				return;	// nothing to do
			}
			catch(const Exception &e)
			{
				LogWarn("Interface::process", String("Error during file transfer: ") + e.what());
			}
				
			return;
		}
		catch(const NetException &e)
		{
			 return;	// nothing to do
		}
		catch(const std::exception &e)
		{
			LogWarn("Interface::process", e.what());
			throw 404;
		}
	}
	
	throw 404;
}

}
