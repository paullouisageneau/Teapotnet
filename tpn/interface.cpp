/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
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
	
#ifdef ANDROID
	bool localAutoLogin = true;
#else
	bool localAutoLogin = false;
#endif
	
	if(request.url == "/")
	{
		if(request.method == "POST")
		{
			String name, password, tracker;
			request.post.get("name", name);
			request.post.get("password", password);
			request.post.get("tracker", tracker);
			
			if(name.contains('@'))
				tracker = name.cut('@');
			
			User *user = NULL;
			try {
				if(localAutoLogin && remoteAddr.isLocal())
				{
					if(request.post.contains("create") && !User::Exist(name))
					{
						if(password.empty()) password = String::random(32);
						user = new User(name, password, tracker);
					}
					else {
						user = User::Get(name);
						if(user && !tracker.empty())
							user->setTracker(tracker);
					}
				}
				else {
					if(request.post.contains("create") && !User::Exist(name))
					{
						user = new User(name, password, tracker);
					}
					else {
						user = User::Authenticate(name, password);
						if(user && !tracker.empty())
							user->setTracker(tracker);
					}
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
			
			if(!user) throw 401;	// TODO
			
			String token = user->generateToken("auth");
				
			Http::Response response(request, 303);
			response.headers["Location"] = "/" + user->name();
			response.cookies["auth_"+user->name()] = token;
			response.send();
			return;
		}
		
#ifdef ANDROID
		if(remoteAddr.isLocal() && !request.get.contains("changeuser"))
		{
			Array<String> names;
			User::GetNames(names);
			if(names.size() == 1)
			{
				Http::Response response(request, 303);
				response.headers["Location"] = "/" + names[0];
				response.send();
				return;
			}
		}
#endif
		
		Http::Response response(request, 200);
		response.send();
		
		Html page(response.sock);
		page.header("Login - Teapotnet", true);
		page.open("div","login");
		page.open("div","logo");
		page.openLink("/");
		page.image("/logo.png", "Teapotnet");
		page.closeLink();
		page.close("div");
		
		page.openForm("/", "post");
		page.open("table");
		page.open("tr");
		page.open("td", ".leftcolumn"); page.label("name", "Name"); page.close("td");
		page.open("td", ".middlecolumn"); page.input("text", "name"); page.close("td"); 
		page.open("td", ".rightcolumn"); page.link("#", "Tracker", "trackerlink"); page.close("td");
		page.close("tr");
		if(!localAutoLogin || !remoteAddr.isLocal())
		{
			page.open("tr");
			page.open("td",".leftcolumn"); page.label("password", "Password"); page.close("td");
			page.open("td",".middlecolumn"); page.input("password", "password"); page.close("td");
			page.open("td",".rightcolumn"); page.close("td");
			page.close("tr");
		}
		page.open("tr", "trackerselection");
		page.open("td", ".leftcolumn"); page.label("tracker", "Tracker"); page.close("td");
		page.open("td", ".middlecolumn"); page.input("text", "tracker", ""); page.close("td");
		page.open("td", ".rightcolumn"); page.close("td");
		page.close("tr");
		page.open("tr");
		page.open("td",".leftcolumn"); page.close("td");
		page.open("td",".middlecolumn"); if(User::Count() > 0) page.button("login", "Login"); page.button("create", "Create"); page.close("td");
		page.open("td",".rightcolumn"); page.close("td");
		page.close("tr");
		page.close("table");
		page.closeForm();
		
		for(StringMap::iterator it = request.cookies.begin();
			it != request.cookies.end(); 
			++it)
		{
			String cookieName = it->first;
			String name = cookieName.cut('_');
			if(cookieName != "auth" || name.empty()) 
				continue;
			
			User *user = User::Get(name);
			if(!user || !user->checkToken(it->second, "auth"))
				continue;
			
			page.open("div",".user");
			page.openLink("/" + name);
			page.image(user->profile()->avatarUrl(), "", ".avatar");
			page.open("span", ".username");
			page.text(name);
			page.close("span");
			page.closeLink();
			page.text(" - ");
			page.link("#", "Logout", ".logoutlink");
			page.close("div");
			
			page.javascript("$('.user a.logoutlink').click(function() {\n\
				unsetCookie('auth_'+$(this).parent().find('.username').text());\n\
				window.location.reload();\n\
				return false;\n\
			});");
		}
		
		page.close("div");
		
		page.javascript("$('#trackerselection').hide();\n\
			$('#trackerlink').click(function() {\n\
				$(this).hide();\n\
				$('#trackerselection').show();\n\
				$('#trackerselection .tracker').val('"+Config::Get("tracker")+"');\n\
			});");
		
		page.footer();
		return;
	}
	
	List<String> list;
	request.url.explode(list,'/');
	list.pop_front();	// first element is empty because url begin with '/'
	if(list.empty()) throw 500;
	if(list.front().empty()) throw 404;
	
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
	
	if(list.front().size() < 64 || User::Exist(list.front()))
	{
		String name = list.front();
		User *user = NULL;
	
		String auth;
		if(request.headers.get("Authorization", auth))
		{
			String tmp = auth.cut(' ');
			auth.trim();
			tmp.trim();
			if(auth != "Basic") throw 400;

			String authName = tmp.base64Decode();
			String authPassword = authName.cut(':');
			
			if(authName == name)
				user = User::Authenticate(authName, authPassword);
		}
		else {
			String token;
			request.cookies.get("auth_"+name, token);
			User *tmp = User::Get(list.front());
			if(tmp && tmp->checkToken(token, "auth"))
				user = tmp;
		}
		
		if(!user)
		{
			if(request.get.contains("json"))
			{
				Http::Response response(request, 401);
				response.send();
				return;
			}
		
			String userAgent;
			request.headers.get("User-Agent", userAgent);
		
			// If it is a browser
			if(userAgent.substr(0,7) == "Mozilla")
			{
				Http::Response response(request, 303);
				response.headers["Location"] = "/";
				response.send();
				return;
			}
			else {
				Http::Response response(request, 401);
				response.headers.insert("WWW-Authenticate", "Basic realm=\""+String(APPNAME)+"\"");
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
	  
		// TODO: Security: if remote address is not local, check if one user at least is authenticated
		
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
			
			String ext = resource.name().afterLast('.');
			if(request.get.contains("download") || ext == "htm" || ext == "html" || ext == "xhtml")
			{
				response.headers["Content-Disposition"] = "attachment; filename=\"" + resource.name() + "\"";
				response.headers["Content-Type"] = "application/force-download";
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
