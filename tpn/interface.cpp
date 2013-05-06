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
			if(remoteAddr.isLocal() || remoteAddr.isPrivate())
			{
				if(request.method == "POST")
				{
					if(request.post.contains("name"))
					{
						String name = request.post["name"].trimmed();
#ifdef ANDROID
						String password = String::random(32);
#else
						String password = request.post["password"];
#endif
						try {
							if(!name.empty() && !password.empty())
							{
								User *user = new User(name, password);
								user->start();
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
					page.header("Account created", false, "/");
					page.open("h1");
					page.text("Your account has been created.");
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
				page.text("Please enter your new username.");
#else
				page.text("No user has been configured yet, please enter your new username and password.");
#endif
				page.close("p");
				
				page.openForm("/","post");
				page.openFieldset("New user");
#ifdef ANDROID
				page.label("name",""); page.input("text","name"); page.br();
#else
				page.label("name","Name"); page.input("text","name"); page.br();
				page.label("password","Password"); page.input("password","password"); page.br();
#endif
				page.label("add"); page.button("add","OK");
				page.closeFieldset();
				page.closeForm();
				
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
				page.text("No user has been configured yet.");
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
		
			if(request.get.contains("play"))
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
					Store::Entry entry;
					if(!Store::GetResource(digest, entry))
					{
						// Request the sources
						Splicer splicer(digest);
					}
				}
				catch(...) {}
				return;
			}			
		
			Store::Entry entry;
			if(Store::GetResource(digest, entry) && !entry.path.empty())
			{
				File file(entry.path);
				
				Http::Response response(request, 200);
				response.headers["Content-Length"] << entry.size;
				response.headers["Last-Modified"] = entry.time.toHttpDate();
				response.headers["Content-SHA512"] = entry.digest.toString();
				
				if(request.get.contains("download"))
				{
					response.headers["Content-Disposition"] = "attachment; filename=\"" + entry.name + "\"";
					response.headers["Content-Type"] = "application/octet-stream";
				}
				else {
					response.headers["Content-Disposition"] = "inline; filename=\"" + entry.name + "\"";
					response.headers["Content-Type"] = Mime::GetType(entry.name);
				}
				
				response.send();
				response.sock->write(file);
				return;
			}
			else {
				int64_t rangeBegin = 0;
				int64_t rangeEnd = 0;
				bool hasRange = request.extractRange(rangeBegin, rangeEnd);
				
				try {
					LogDebug("Interface::process", "Starting download");
				  
				  	// TODO: range error
					Splicer splicer(digest, rangeBegin, rangeEnd);
					splicer.start();
					
					int64_t contentLength = splicer.size();
					int code = 200;
					
					Http::Response response(request, code);
					response.headers["Content-Length"] << contentLength;
					response.headers["Accept-Ranges"] = "bytes";
					
					if(hasRange) response.headers["Content-Range"] << splicer.begin() << '-' << splicer.end() << '/' << splicer.size();
					else response.headers["Content-SHA512"] = digest.toString();
					
				   	if(request.get.contains("download")) 
					{
						response.headers["Content-Disposition"] = "attachment; filename=\"" + splicer.name() + "\"";
						response.headers["Content-Type"] = "application/octet-stream";
					}
					else {
						response.headers["Content-Disposition"] = "inline; filename=\"" + splicer.name() + "\"";
						response.headers["Content-Type"] = Mime::GetType(splicer.name());
					}
				   
					response.send();
					
					response.sock->setTimeout(0);
					
					int64_t total = 0;
					while(!splicer.outputFinished())
					{
						int64_t size = splicer.process(response.sock);
						total+= size;
						if(!size) msleep(100);
					}
					
					splicer.stop();
					
					LogDebug("Interface::process", "Download finished");
					
					if(total == contentLength)
					{
						// TODO: copy in incoming if splicer.finished() == true
					}
					else LogWarn("Interface::http", String("Splicer downloaded ") + String::number(total) + " bytes whereas length was " + String::number(contentLength));
				}
				catch(const NetException &e)
				{
					LogDebug("Interface::process", e.what());
				}
				catch(const Exception &e)
				{
					LogWarn("Interface::process", String("Error during file transfer: ") + e.what());
				}
				
				return;
			}
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
