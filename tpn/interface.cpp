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

#include "tpn/interface.hpp"
#include "tpn/html.hpp"
#include "tpn/user.hpp"
#include "tpn/resource.hpp"
#include "tpn/config.hpp"
#include "tpn/user.hpp"
#include "tpn/addressbook.hpp"

#include "pla/directory.hpp"
#include "pla/jsonserializer.hpp"
#include "pla/mime.hpp"

namespace tpn
{

Interface *Interface::Instance = NULL;

Interface::Interface(int port) :
		Http::Server(port)
{
	add("", this);
	add("/static", this);
	add("/file", this);
	add("/mail", this);
}

Interface::~Interface(void)
{
	// calling remove() is unnecessary
}

void Interface::add(const String &prefix, HttpInterfaceable *interfaceable)
{
	std::unique_lock<std::mutex> lock(mMutex);
	Assert(interfaceable != NULL);
	mPrefixes.insert(prefix, interfaceable);
}

void Interface::remove(const String &prefix, HttpInterfaceable *interfaceable)
{
	std::unique_lock<std::mutex> lock(mMutex);
	HttpInterfaceable *test = NULL;
	if(mPrefixes.get(prefix, test) && (!interfaceable || test == interfaceable))
		mPrefixes.erase(prefix);
}

void Interface::http(const String &prefix, Http::Request &request)
{
	Assert(!request.url.empty());
	
	if(!Network::Instance) throw 400;	// Network is not ready yet !
	
	try {
		if(prefix == "")
		{
			if(request.method == "POST")
			{
				String name, password;
				request.post.get("name", name);
				request.post.get("password", password);
				
				User *user = NULL;
				try {
					if(request.post.contains("create") && !User::Exist(name))
					{
						user = new User(name, password);
						
						String token = user->generateToken("auth");
						Http::Response response(request, 200);
						response.cookies["auth_"+user->name()] = token;
						response.send();
						
						Html page(response.stream);
						page.header("Synchronize device", true);
						page.open("div","login");
						page.open("div","logo");
						page.openLink("/");
						page.image("/static/logo.png", "Teapotnet");
						page.closeLink();
						page.close("div");
						
						page.openForm(user->addressBook()->urlPrefix(), "post");
						page.open("p");
						page.text("If you have Teapotnet installed on another device, generate a synchronization secret code and enter it here.");
						page.br();
						page.text("If you don't, and this is your first Teapotnet installation, just let the field empty.");
						page.br();
						page.close("p");
						page.input("hidden", "token", user->generateToken("contact"));
						page.input("hidden", "action", "acceptsynchronization");
						page.input("hidden", "redirect", user->urlPrefix());
						page.input("text", "code");
						page.button("validate", "Validate");
						page.closeForm();
						
						page.close("div");
						page.footer();
						return;
					}
					
					user = User::Authenticate(name, password);
				}
				catch(const Exception &e)
				{
					Http::Response response(request, 200);
					response.send();
					
					Html page(response.stream);
					page.header("Error", false, "/");
					page.text(e.what());
					page.footer();
					return;
				}
				
				if(!user) throw 401;
				
				String token = user->generateToken("auth");
				Http::Response response(request, 303);
				response.headers["Location"] = user->urlPrefix();
				response.cookies["auth_"+user->name()] = token;
				response.send();
				return;
			}
			
			Http::Response response(request, 200);
			response.send();
			
			Html page(response.stream);
			page.header("Login - Teapotnet", true);
			page.open("div","login");
			page.open("div","logo");
			page.openLink("/");
			page.image("/static/logo.png", "Teapotnet");
			page.closeLink();
			page.close("div");
			
			page.openForm("/", "post");
			page.open("table");
			page.open("tr");
			page.open("td", ".leftcolumn"); page.label("name", "Name"); page.close("td");
			page.open("td", ".middlecolumn"); page.input("text", "name"); page.close("td"); 
			page.open("td", ".rightcolumn"); page.close("td");
			page.close("tr");
			page.open("tr");
			page.open("td",".leftcolumn"); page.label("password", "Password"); page.close("td");
			page.open("td",".middlecolumn"); page.input("password", "password"); page.close("td");
			page.open("td",".rightcolumn"); page.close("td");
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
				page.openLink(user->urlPrefix());
				//page.image(user->profile()->avatarUrl(), "", ".avatar");
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
			page.footer();
			return;
		}
		else if(prefix == "/static")
		{
			String name = request.url;
			if(!name.empty() && name[0] == '/') name.ignore();
			if(!name.empty() && name[name.size()-1] == '/') name.resize(name.size()-1);
			if(name.empty() 
				|| name.contains('/') || name.contains(Directory::Separator)
				|| name == "." || name == "..") 
				throw 404;
			
			String path = Config::Get("static_dir") + Directory::Separator + name;
			if(File::Exist(path)) 
			{
				respondWithFile(request, path);
				return;
			}
		}
		else if(prefix == "/file")
		{
			String tmp = request.url;
			if(!tmp.empty() && tmp[0] == '/') tmp.ignore();
			if(!tmp.empty() && tmp[tmp.size()-1] == '/') tmp.resize(tmp.size()-1);
			if(tmp.empty() || tmp.contains('/')) throw 404;
			
			BinaryString digest;
			try { tmp >> digest; }
			catch(...) { throw 404; }		
		
			// Query resource
			Resource resource;
			resource.fetch(digest);		// this can take some time
			
			// Playlist
			if(request.get.contains("play") || request.get.contains("playlist"))
			{	
				request.get.insert("playlist", "");
				Request req(resource);
				req.http(req.urlPrefix(), request);
				return;
			}
			
			if(resource.isDirectory())
			{
				Request *req = new Request(resource);
				String reqPrefix = req->urlPrefix();
				req->autoDelete();
				
				// JSON
				if(request.get.contains("json"))
				{
					Http::Response response(request, 307);
					response.headers["Location"] = reqPrefix;
					response.send();
					return;
				}
			
				User *user = getAuthenticatedUser(request);
	
				Http::Response response(request, 200);
				response.send();
				
				Html page(response.stream);
				page.header("Browse files");
				page.open("div","topmenu");
				if(user) page.link(user->urlPrefix()+"/search/", "Search", ".button");
				page.link(reqPrefix+"?playlist", "Play all", "playall.button");
				page.close("div");
				
				page.div("", "list.box");
				page.javascript("listDirectory('"+reqPrefix+"','#list',true);");
				page.footer();
				return;
			}
			else { // resource is a file
			
				// JSON
				if(request.get.contains("json"))
				{
					Http::Response response(request, 200);
					response.headers["Content-Type"] = "application/json";
					response.send();

					JsonSerializer json(response.stream);
					json << resource;
					return;
				}
				
				// Get range
				int64_t rangeBegin = 0;
				int64_t rangeEnd = 0;
				bool hasRange = request.extractRange(rangeBegin, rangeEnd, resource.size());
				int64_t rangeSize = rangeEnd - rangeBegin + 1;
				
				if(hasRange && (rangeBegin >= resource.size() || rangeEnd >= resource.size()))
					throw 406;	// Not Acceptable
				
				// Forge HTTP response header
				Http::Response response(request, (hasRange ? 206 : 200));
				
				response.headers["Content-Name"] = resource.name();
				response.headers["Accept-Ranges"] = "bytes";
				
				response.headers["Content-Length"] << rangeSize;
				if(hasRange) response.headers["Content-Range"] << "bytes " << rangeBegin << "-" << rangeEnd << "/" << resource.size();
				
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
				
				if(request.method != "HEAD")
				{
					try {
						// Launch transfer
						Resource::Reader reader(&resource);
						if(hasRange) reader.seekRead(rangeBegin);
						int64_t size = reader.readBinary(*response.stream, rangeSize);	// let's go !
						if(size != rangeSize)
							throw Exception("Range size is " + String::number(rangeSize) + ", but sent size is " + String::number(size));
					}
					catch(const NetException &e)
					{
						return;	// nothing to do
					}
					catch(const Exception &e)
					{
						LogWarn("Interface::process", String("Error during file transfer: ") + e.what());
					}
				}
				
				return;
			}
		}
		else if(prefix == "/mail")
		{
			LogWarn("Interface::process", "Creating board: " + request.url);
			
			Board *board = new Board(request.url.substr(1));
			
			String url = request.url;
			request.url = "/";
			board->http(prefix+url, request);
			return;
		}
	}
	catch(const NetException &e)
	{
		 return;	// nothing to do
	}
	catch(const std::exception &e)
	{
		LogWarn("Interface::http", e.what());
		throw 500;
	}
	
	throw 404;
}

void Interface::process(Http::Request &request)
{
	//LogDebug("Interface", request.method + " " + request.fullUrl);
	
	// URL must begin with /
	if(request.url.empty() || request.url[0] != '/') throw 404;
	
	List<String> list;
	request.url.explode(list,'/');
	list.pop_front();	// first element is empty because url begin with '/'
	if(list.empty()) throw 500;
	
	if(list.front() == "user" && list.size() >= 2)
	{
		User *user = NULL;
		String name;
		
		if(list.size() >= 2)
			name = *(++list.begin());
		
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
			User *tmp = User::Get(name);
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
				
				Html page(response.stream);
				page.header(response.message, true);
				page.open("div", "error");
				page.openLink("/");
				page.image("/static/error.png", "Error");
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
	}
	
	while(!list.empty())
	{
		std::unique_lock<std::mutex> lock(mMutex);
		
		String prefix;
		prefix.implode(list,'/');
		if(!prefix.empty())
			prefix = "/" + prefix;
	
		list.pop_back();
		
		auto it = mPrefixes.find(prefix);
		if(it != mPrefixes.end()) 
		{
			HttpInterfaceable *interfaceable = it->second;
			lock.unlock();
			
			//LogDebug("Interface", "Matched prefix \""+prefix+"\"");
			request.url.ignore(prefix.size());
			if(request.url.empty())
				request.url = "/";
			
			interfaceable->http(prefix, request);
			return;
		}
	}
	
	throw 404;
}

void Interface::generate(Stream &out, int code, const String &message)
{
	Html page(&out);
	page.header(message, true);
	page.open("div", "error");
	page.openLink("/");
	page.image("/static/error.png", "Error");
	page.closeLink();
	page.br();
	page.br();
	page.open("h1",".huge");
	page.text(String::number(code));
	page.br();
	page.text(message);
	page.close("h1");
	page.close("div");
	page.footer();
}

User *HttpInterfaceable::getAuthenticatedUser(Http::Request &request, String name)
{
	if(name.empty())
		request.cookies.get("name", name);
	
	if(!name.empty())
	{
		String token;
		request.cookies.get("auth_"+name, token);
		User *user = User::Get(name);
		if(user && user->checkToken(token, "auth"))
			return user;
	}
	
	Array<User*> users;
	if(getAuthenticatedUsers(request, users))
		return users[0];
	
	return NULL;
}

int HttpInterfaceable::getAuthenticatedUsers(Http::Request &request, Array<User*> &users)
{
	users.clear();
	
	for(	StringMap::iterator it = request.cookies.begin();
		it != request.cookies.end();
		++it)
	{
		String key = it->first;
		String name = key.cut('_');
		
		User *user = User::Get(name);
		if(user && user->checkToken(it->second, "auth"))
			users.push_back(user);
	}
	
	return int(users.size());
}

}
