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

#include "tpn/user.h"
#include "tpn/config.h"
#include "tpn/file.h"
#include "tpn/directory.h"
#include "tpn/sha512.h"
#include "tpn/html.h"
#include "tpn/yamlserializer.h"
#include "tpn/mime.h"

namespace tpn
{

Map<String, User*>	User::UsersByName;
Map<ByteString, User*>	User::UsersByAuth;
Mutex			User::UsersMutex;
  
unsigned User::Count(void)
{
	 UsersMutex.lock();
	 unsigned count = UsersByName.size();
	 UsersMutex.unlock();
	 return count;
}

void User::GetNames(Array<String> &array)
{
	 UsersMutex.lock();
	 UsersByName.getKeys(array);
	 UsersMutex.unlock();
}

bool User::Exist(const String &name)
{
	return (User::Get(name) != NULL);
}

User *User::Get(const String &name)
{
	User *user = NULL;
	UsersMutex.lock();
	if(UsersByName.get(name, user)) 
	{
	  	UsersMutex.unlock();
		return user;
	}
	UsersMutex.unlock();
	return NULL; 
}

User *User::Authenticate(const String &name, const String &password)
{
	ByteString hash;
	Sha512::Hash(name + ':' + password, hash, Sha512::CryptRounds);
	
	User *user = NULL;
	UsersMutex.lock();
	if(UsersByAuth.get(hash, user))
	{
	  	UsersMutex.unlock();
		return user;
	}
	UsersMutex.unlock();
	LogWarn("User::Authenticate", "Authentication failed for \""+name+"\"");
	return NULL;
}

void User::UpdateAll(void)
{
	Array<String> names;
	UsersMutex.lock();
	UsersByName.getKeys(names);
	UsersMutex.unlock();
	
	for(int i=0; i<names.size(); ++i)
	{
		User *user = NULL;
		UsersMutex.lock();
		UsersByName.get(names[i], user);
		UsersMutex.unlock();
		
		if(user && !user->addressBook()->isRunning()) user->addressBook()->start();
	}
}

User::User(const String &name, const String &password) :
	mName(name),
	mAddressBook(new AddressBook(this)),
	mStore(new Store(this)),
	mLastOnlineTime(0)
{
	if(password.empty())
	{
		File file(profilePath()+"password", File::Read);
		file.read(mHash);
		file.close();
	}
	else {
		Sha512::Hash(name + ':' + password, mHash, Sha512::CryptRounds);
		
		File file(profilePath()+"password", File::Write);
		file.write(mHash);
		file.close();
	}
	
	UsersMutex.lock();
	UsersByName.insert(mName, this);
	UsersByAuth.insert(mHash, this);
	UsersMutex.unlock();
	
	Interface::Instance->add(urlPrefix(), this);
}

User::~User(void)
{
  	UsersMutex.lock();
	UsersByName.erase(mName);
  	UsersByAuth.erase(mHash);
	UsersMutex.unlock();
	
	Interface::Instance->remove(urlPrefix());
	
	delete mAddressBook;
	delete mStore;
}

const String &User::name(void) const
{
	return mName; 
}

String User::profilePath(void) const
{
	if(!Directory::Exist(Config::Get("profiles_dir"))) Directory::Create(Config::Get("profiles_dir"));
	String path = Config::Get("profiles_dir") + Directory::Separator + mName;
	if(!Directory::Exist(path)) Directory::Create(path);
	return path + Directory::Separator;
}

String User::urlPrefix(void) const
{
	return String("/") + mName;
}

AddressBook *User::addressBook(void) const
{
	return mAddressBook;
}

Store *User::store(void) const
{
	return mStore;
}

bool User::isOnline(void) const
{
	Synchronize(this);
	return (Time::Now()-mLastOnlineTime < 30);	// 30 sec
}

void User::setOnline(void)
{
	Synchronize(this);
	
	bool wasOnline = isOnline();
	mLastOnlineTime = Time::Now();
	mInfo["last"] = mLastOnlineTime.toString();
	
	if(!wasOnline) 
	{
		if(!mAddressBook->isRunning()) mAddressBook->start();
		sendInfo();
	}
}

void User::setInfo(const StringMap &info)
{
	Synchronize(this);
	
	Time l1(info.getOrDefault("last", Time(0)));
	Time l2(mInfo.getOrDefault("last", Time(0)));
	l1 = std::min(l1, Time::Now());
	l2 = std::min(l2, Time::Now());
	String last = std::max(l1,l2).toString();
		
	Time t1(info.getOrDefault("time", Time(0)));
	Time t2(mInfo.getOrDefault("time", Time(0)));
	t1 = std::min(t1, Time::Now());
	t2 = std::min(t2, Time::Now());
	if(t1 > t2)
	{
		mInfo = info;
		mInfo["last"] = last;
		return;
	}
	else {
		mInfo["last"] = last;
		if(mInfo.contains("time"))
			sendInfo();
	}
}

void User::sendInfo(const Identifier &identifier)
{
	Synchronize(this);
	
	String content;
	YamlSerializer serializer(&content);
	serializer.output(mInfo);
	
	Message message(content);
	message.setParameter("type", "info");
	if(identifier == Identifier::Null) message.send();
	else message.send(identifier);
}

void User::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	
	try {
		setOnline();
		
		String url = request.url;
		if(url.empty() || url[0] != '/') throw 404;
		
		if(url == "/")
		{
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header(APPNAME, true);

			page.open("div", "mainheader");
			page.openLink("/"); page.image("/logo.png", APPNAME, "logo"); page.closeLink();
			
			page.open("div", ".search");
			page.openForm(prefix + "/search", "post", "searchForm");
			page.input("text","query");
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			page.br();
			page.close("div");
			
			page.close("div");
			
			page.open("h1");
			const String tracker = Config::Get("tracker");
			const String instance = Core::Instance->getName().before('.');
			if(!instance.empty()) page.text(mName + "@" + tracker + " / " + instance);
			else page.text(mName);
			page.close("h1");
			
			page.open("div","contacts.box");
			
			page.link(prefix+"/contacts/","Edit",".button");
		
			page.open("h2");
			page.text("Contacts");
			page.close("h2");
	
			AddressBook::Contact *self = mAddressBook->getSelf();
			Array<AddressBook::Contact*> contacts;
			mAddressBook->getContacts(contacts);
			
			if(contacts.empty() && !self) page.link(prefix+"/contacts/","Add a contact");
			else {
				page.open("table",".contacts");
				
				if(self)
				{
					page.open("tr", String("contact_")+self->uniqueName());
					
					page.open("td",".name");
					page.link(self->urlPrefix(), self->name());
					page.close("td");
						
					page.open("td",".tracker");
					page.text(String("@") + self->tracker());
					page.close("td");
						
					page.open("td",".status");
					page.close("td");
					
					page.open("td",".files");
					page.link(self->urlPrefix()+"/files/", "Files");
					page.close("td");
					
					page.open("td",".chat");
					// Dummy
					page.close("td");
					
					page.close("tr");
				}
			    
				for(int i=0; i<contacts.size(); ++i)
				{	
					AddressBook::Contact *contact = contacts[i];
					
					page.open("tr", String("contact_")+contact->uniqueName());
					
					page.open("td",".name");
					page.link(contact->urlPrefix(), contact->name());
					page.close("td");
					
					page.open("td",".tracker");
					page.text(String("@") + contact->tracker());
					page.close("td");
					
					page.open("td",".status");
					page.close("td");
					
					page.open("td",".files");
					page.link(contact->urlPrefix()+"/files/", "Files");
					page.close("td");
					
					page.open("td",".chat");
					page.openLink(contact->urlPrefix()+"/chat/");
					page.text("Chat");
					page.span("", ".messagescount");
					page.closeLink();
					page.close("td");
					
					page.close("tr");
				}
				page.close("table");
			}
			page.close("div");
			
			unsigned refreshPeriod = 5000;
			page.javascript("var title = document.title;\n\
					setInfoCallback(\"/"+mAddressBook->userName()+"/contacts/?json\", "+String::number(refreshPeriod)+", function(data) {\n\
					var totalmessages = 0;\n\
					play = false;\n\
					$.each(data, function(uname, info) {\n\
						$('#contact_'+uname).attr('class', info.status);\n\
						transition($('#contact_'+uname+' .status'), info.status.capitalize());\n\
						var count = parseInt(info.messages);\n\
						var tmp = '';\n\
						if(count != 0) tmp = ' ('+count+')';\n\
						transition($('#contact_'+uname+' .messagescount'), tmp);\n\
						totalmessages+= count;\n\
						if(info.newmessages == 'true') play = true;\n\
					});\n\
					if(totalmessages != 0) document.title = title+' ('+totalmessages+')';\n\
					else document.title = title;\n\
					if(play) playMessageSound();\n\
			});");
			
			page.open("div","files.box");
			
			page.link(prefix+"/files/","Edit",".button");
                        page.link(prefix+"/files/?action=refresh&redirect="+String(prefix+url).urlEncode(), "Refresh", ".button");

			page.open("h2");
			page.text("Shared folders");
			page.close("h2");
		
			Array<String> directories;
			mStore->getDirectories(directories);
			if(directories.empty()) page.link(prefix+"/files/","Add a shared folder");
			else {
				page.open("table",".files");
				for(int i=0; i<directories.size(); ++i)
				{	
					const String &directory = directories[i];
					String directoryUrl = prefix + "/files/" + directory + "/";
					
					page.open("tr");
					page.open("td",".icon");
					page.image("/dir.png");
					page.close("td");
					page.open("td",".filename");
					page.link(directoryUrl, directory);
					page.close("td");
					page.close("tr");
				}
				page.close("table");
			}
			page.close("div");
			
			directories.clear();
			Store::GlobalInstance->getDirectories(directories);
			if(!directories.empty())
			{
				page.open("div","otherfiles.box");
				
				page.link(prefix+"/files/?action=refreshglobal&redirect="+String(prefix+url).urlEncode(), "Refresh", ".button");
		
				page.open("h2");
				page.text("Other shared folders - ");
				page.close("h2");
			
				page.open("table",".files");
				for(int i=0; i<directories.size(); ++i)
				{
					const String &directory = directories[i];
					String directoryUrl = prefix + "/files/" + directory + "/";
					
					page.open("tr");
					page.open("td",".icon");
					page.image("/dir.png");
					page.close("td");
					page.open("td",".filename");
					page.link(directoryUrl, directory);
					page.close("td");
				}
				page.close("table");
				
				page.close("div");
			}
			
			page.open("div", "footer");
			page.text(String("Version ") + APPVERSION + " - ");
			page.link(SOURCELINK, "Source code", "", true);
			page.close("div");
			
			page.footer();
			return;
		}
		
		if(url == "/search" || url == "/search/")
		{
			String query;
			if(request.post.contains("query"))
			{
				query = request.post.get("query");
				query.trim();
			}
				
			Http::Response response(request,200);
			response.send();
				
			Html page(response.sock);
			if(query.empty()) page.header("Search");
			else page.header(String("Searching ") + query);
			
			page.openForm(prefix + "/search", "post", "searchform");
			page.input("text","query", query);
			page.button("search","Search");
			page.closeForm();
			page.br();
				
			if(query.empty())
			{
				page.footer();
				return;
			}
			
			const unsigned timeout = Config::Get("request_timeout").toInt();
	
			Desynchronize(this);
			Request trequest("search:"+query, false);	// no data
			trequest.execute(this);
			trequest.submit();
			trequest.wait(timeout);
			
			page.listFilesFromRequest(trequest, prefix, request, mAddressBook->user());
			page.footer();
			return;
		}
		
		url.ignore();
		String urlLeft = String("/") + url.cut('/');
		url = String("/") + url;
		
		if(url == "/myself")
		{
			Http::Response response(request, 303);
			response.headers["Location"] = prefix + "/files/";
			response.send();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("User::http", e.what());
		throw 404;	// Httpd handles integer exceptions
	}
			
	throw 404;
}

void User::run(void)
{
	Time oldLastOnlineTime(0);
	unsigned m = 0;	// minutes
	while(true)
	{
		for(int t=0; t<2; ++t)
		{
			try {
				msleep(30000);
				
				if(oldLastOnlineTime != mLastOnlineTime)
				{
					oldLastOnlineTime = mLastOnlineTime;
					sendInfo();
				}
			}
			catch(const Exception &e)
			{
				LogWarn("User::run", e.what());
			}
		}
		
		++m;
		if((m%5 == 0) && !mAddressBook->isRunning()) mAddressBook->start();
		if((m%60 == 0) && !mStore->isRunning()) mStore->start();
	}
}

}
