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

#include "interface.h"
#include "html.h"
#include "user.h"
#include "store.h"
#include "splicer.h"
#include "config.h"
#include "directory.h"
#include "mime.h"

namespace tpot
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
			if(request.sock->getRemoteAddress().isLocal())
			{
				if(request.method == "POST")
				{
					if(request.post.contains("name"))
					{
						String name = request.post["name"];
						String password = request.post["password"];
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
					page.text("Please enter your username and password when prompted to log in.");
					page.close("p");
					page.footer();
					return;
				}
			  
				Http::Response response(request, 200);
				response.send();
			
				Html page(response.sock);
				page.header();
				page.open("h1");
				page.text("Welcome to TeapotNet !");
				page.close("h1");
				page.open("p");
				page.text("No user has been configured yet, please enter your new username and password.");
				page.close("p");
				
				page.openForm("/","post");
				page.openFieldset("New user");
				page.label("name","Name"); page.input("text","name"); page.br();
				page.label("password","Password"); page.input("password","password"); page.br();
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
				page.text("Welcome to TeapotNet !");
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
	
	Address remoteAddr = request.sock->getRemoteAddress();
	
	if(list.front().empty()
		|| User::Exist(list.front()) 
		|| (!remoteAddr.isLocal() && !remoteAddr.isPrivate()))
	{
		if(!user || list.front() != user->name())
		{
			String realm = "TeapotNet";
			
			Http::Response response(request, 401);
			response.headers.insert("WWW-Authenticate", "Basic realm=\""+realm+"\"");
			response.send();
				
			Html page(response.sock);
			page.header("Authentication required");
			
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
				
				//Log("Interface", "Matched prefix \""+prefix+"\"");
				
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
		
			Store::Entry entry;
			if(Store::GetResource(digest, entry) && !entry.path.empty())
			{
				File file(entry.path);
				
				Http::Response response(request, 200);
				response.headers["Content-Disposition"] = "inline; filename=\"" + entry.name + "\"";
				response.headers["Content-Type"] = Mime::GetType(entry.name);
				response.headers["Content-Length"] << entry.size;
				response.headers["Last-Modified"] = entry.time.toHttpDate();
				response.headers["Content-SHA512"] = entry.digest.toString();
				
				response.send();
				response.sock->write(file);
				return;
			}
			else {
				size_t blockSize = 128*1024;		// TODO
				String filename  = File::TempName();	// TODO
				
				try {
					Splicer splicer(digest, filename, blockSize);
					File file(filename, File::Read);
					
					Http::Response response(request, 200);
					response.headers["Content-Disposition"] = "inline; filename=\"" + splicer.name() + "\"";
					response.headers["Content-Type"] = Mime::GetType(splicer.name());
					response.headers["Content-Length"] << splicer.size();
					response.headers["Content-SHA512"] = digest.toString();
					// TODO: Missing headers
					response.send();
					
					uint64_t total = 0;
					size_t current = 0;
					while(!splicer.finished())
					{
						splicer.process();
					  
						size_t finished = splicer.finishedBlocks();
						while(current < finished)
						{
							size_t size;
							Assert(size = file.read(*response.sock, blockSize));
							total+= size;
							++current;
						}
						
						msleep(100);
					}
					
					splicer.close();
					
					total+= file.read(*response.sock);
					if(total != splicer.size())
						Log("Interface::http", String("Warning: Splicer downloaded ") + String::number(total) + " bytes whereas size was " + String::number(splicer.size())); 
					return;
				}
				catch(...)
				{
					if(File::Exist(filename)) File::Remove(filename);
					throw;
				}
			}
		}
		catch(const NetException &e)
		{
			 return;	// nothing to do
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
