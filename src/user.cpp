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

#include "user.h"
#include "config.h"
#include "file.h"
#include "directory.h"
#include "sha512.h"
#include "html.h"
#include "yamlserializer.h"

namespace tpot
{

Map<String, User*>	User::UsersByName;
Map<Identifier, User*>	User::UsersByAuth;
Mutex			User::UsersMutex;
  
unsigned User::Count(void)
{
	 UsersMutex.lock();
	 unsigned count = UsersByName.size();
	 UsersMutex.unlock();
	 return count;
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
	Identifier hash;
	Sha512::Hash(name + ':' + password, hash, Sha512::CryptRounds);
	
	User *user = NULL;
	UsersMutex.lock();
	if(UsersByAuth.get(hash, user)) 
	{
	  	UsersMutex.unlock();
		return user;
	}
	UsersMutex.unlock();
	Log("User::Authenticate", "Authentication failed for \""+name+"\"");
	return NULL;
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
	
	Interface::Instance->add("/"+mName, this);
}

User::~User(void)
{
  	UsersMutex.lock();
	UsersByName.erase(mName);
  	UsersByAuth.erase(mHash);
	UsersMutex.unlock();
	
	Interface::Instance->remove("/"+mName);
	
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
	return (Time::Now()-mLastOnlineTime < 30);	// 30 sec
}

void User::setOnline(void)
{
	mLastOnlineTime = Time::Now();
	mInfo["last"] = String::number(mLastOnlineTime);
}

void User::setInfo(const StringMap &info)
{
	if(info.contains("last") && Time(info.get("last")) >= Time(mInfo.get("last")))
		mInfo["last"] = info.get("last");
	
	if(info.contains("time") && 
		(!mInfo.contains("time") || Time(info.get("time")) >= Time(mInfo.get("time"))))
		mInfo = info;
	else sendInfo();
}

void User::sendInfo(const Identifier &identifier)
{
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
			page.javascript("document.searchForm.query.focus();");
			page.br();
			page.close("div");
			
			page.close("div");
			
			page.open("h1");
			page.text(String("Welcome, ")+mName+" !");
			page.close("h1");
			
			/*
			int msgcount = mAddressBook->unreadMessagesCount();
			if(msgcount) 
			{
				page.span(String("You have ")+String::number(msgcount)+String(" new messages"), ".important");
				page.br();
			}
			*/
			
			page.open("div","contacts.box");
			page.open("h2");
			page.text("Contacts - ");
			page.link(prefix+"/contacts/","Edit");
			page.close("h2");
			
			Array<AddressBook::Contact*> contacts;
			mAddressBook->getContacts(contacts);
			if(contacts.empty()) page.link(prefix+"/contacts/","Add a contact !");
			else {
				page.open("table",".contacts");
				for(int i=0; i<contacts.size(); ++i)
				{	
					AddressBook::Contact *contact = contacts[i];
					
					page.open("tr", String("contact_")+contact->uniqueName());
					page.open("td");
					page.open("span",".contact");
					page.link(contact->urlPrefix(), contact->name() + "@" + contact->tracker());
					page.close("span");
					page.close("td");
					
					page.open("td",".info");
					page.span(contact->status().capitalized(), String(".") + contact->status());
					page.close("td");
					
					page.open("td",".files");
					page.link(contact->urlPrefix()+"/files/", "files");
					page.close("td");
					
					page.open("td",".chat");
					page.openLink(contact->urlPrefix()+"/chat/");
					page.text("chat");
					page.open("span",".messagescount");
					int msgcount = contact->unreadMessagesCount();
					if(msgcount) page.text(String("(")+String::number(msgcount)+String(")"));
					page.close("span");
					page.closeLink();
					page.close("td");
					
					page.close("tr");
				}
				page.close("table");
			}
			page.close("div");
			
			unsigned refreshPeriod = 5000;
			page.javascript("function updateContacts() {\n\
				$.getJSON('"+prefix+"/contacts/?json', function(data) {\n\
  					$.each(data, function(uname, info) {\n\
			  			transition($('#contact_'+uname+' .info'),\n\
							'<span class=\"'+info.info+'\">'+info.info.capitalize()+'</span>\\n');\n\
						var msg = '';\n\
						if(info.messages != 0) msg = ' ('+info.messages+')';\n\
						transition($('#contact_'+uname+' .messagescount'), msg);\n\
  					});\n\
  					setTimeout('updateContacts()',"+String::number(refreshPeriod)+");\n\
				});\n\
			}\n\
			updateContacts();");
			
			page.open("div","files.box");
			page.open("h2");
			page.text("Shared folders - ");
			page.link(prefix+"/files/","Edit");
			if(mAddressBook->getSelf())
			{
				page.text(" - ");
				page.link(prefix+"/myself/","Show all my files");
			}
			page.close("h2");
			
			Array<String> directories;
			mStore->getDirectories(directories);
			if(directories.empty()) page.link(prefix+"/files/","Add a shared personal folder !");
			else {
				page.open("table",".files");
				for(int i=0; i<directories.size(); ++i)
				{	
					const String &directory = directories[i];
					String directoryUrl = prefix + "/files/" + directory + "/";
					
					page.open("tr");
					page.open("td");
					page.open("span",".file");
					page.link(directoryUrl, directory);
					page.close("span");
					page.close("td");
				}
				page.close("table");
			}
			page.close("div");
			
			directories.clear();
			Store::GlobalInstance->getDirectories(directories);
			if(!directories.empty())
			{
				page.open("div","otherfiles.box");
				page.open("h2");
				page.text("Other shared folders");
				page.close("h2");
			
				page.open("table",".files");
				for(int i=0; i<directories.size(); ++i)
				{	
					const String &directory = directories[i];
					String directoryUrl = prefix + "/files/" + directory + "/";
					
					page.open("tr");
					page.open("td");
					page.open("span",".file");
					page.link(directoryUrl, directory);
					page.close("span");
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
			page.header("Search");
				
			page.openForm(prefix + "/search", "post", "searchform");
			page.input("text","query",query);
			page.button("search","Search");
			page.closeForm();
			page.br();
				
			if(query.empty())
			{
				page.footer();
				return;
			}
			
			Store::Query squery;
			squery.setMatch(query);
		
			int count = 0;
			page.open("div",".box");
			page.open("table",".files");
			
			List<Store::Entry> list;
			if(mStore->queryList(squery, list))
			{	
				
				for(List<Store::Entry>::iterator it = list.begin();
					it != list.end();
					++it)
				{
					Store::Entry &entry = *it;
					if(entry.url.empty()) continue;
					
					page.open("tr");
					page.open("td");
					page.text("(" + mName + ")");
					page.close("td");
					page.open("td",".filename"); 
					page.link("/" + mName + "/files" + entry.url, entry.name);
					page.close("td");
					page.open("td",".size"); 
					if(!entry.type) page.text("directory");
					else page.text(String::hrSize(entry.size));
					page.close("td");
					page.close("tr");
					
					++count;
				}
			}
			
			Desynchronize(this);
			Request trequest("search:"+query, false);	// no data
			Synchronize(&trequest);

			const unsigned timeout = Config::Get("request_timeout").toInt();
			trequest.submit();
			trequest.wait(timeout);
			
			if(trequest.isSuccessful())
			try {
				for(int i=0; i<trequest.responsesCount(); ++i)
				{
					Request::Response *tresponse = trequest.response(i);
					if(tresponse->error()) continue;
					
					// Check contact
					const AddressBook::Contact *contact = mAddressBook->getContact(tresponse->peering());
					if(!contact) continue;
					
					// Check info
					StringMap map = tresponse->parameters();
					if(!map.contains("type")) continue;
					if(!map.contains("path")) continue;
					if(!map.contains("hash")) map["hash"] = "";
					if(!map.contains("name")) map["name"] = map["path"].afterLast('/');
					
					page.open("tr");
					page.open("td");
					page.text("("); page.link(contact->urlPrefix()+"/", contact->name()); page.text(")");
					page.close("td");
					page.open("td",".filename");
					if(map.get("type") == "directory") page.link(contact->urlPrefix() + "/files" + map.get("path"), map.get("name"));
					else if(!map.get("hash").empty()) page.link("/" + map.get("hash"), map.get("name"));
					else page.link(contact->urlPrefix() + "/files" + map.get("path") + "?instance=" + tresponse->instance().urlEncode() + "&file=1", map.get("name"));
					page.close("td");
					page.open("td",".size"); 
					if(map.get("type") == "directory") page.text("directory");
					else if(map.contains("size")) page.text(String::hrSize(map.get("size")));
					page.close("td");
					page.close("tr");
					
					++count;
				}

			}
			catch(const Exception &e)
			{
				Log("User::http", String("Unable to list files: ") + e.what());
			}
			
			page.close("table");
			
			if(!count) page.text("No files found");
			
			page.close("div");
			page.footer();
			return;
		}
		
		url.ignore();
		String urlLeft = String("/") + url.cut('/');
		url = String("/") + url;
		
		if(url == "/myself")
		{
			AddressBook::Contact *self = mAddressBook->getSelf();
			if(!self)
			{
				Http::Response response(request, 303);
				response.headers["Location"] = prefix + "/files/";
				response.send();
				return;
			}
			
			request.url = String("/files") + urlLeft;
			String newPrefix = prefix + "/contacts/" + self->uniqueName();
			
			Desynchronize(this);
			self->http(newPrefix, request);
			return;
		}
	}
	catch(const Exception &e)
	{
		Log("User::http",e.what());
		throw 404;	// Httpd handles integer exceptions
	}
			
	throw 404;
}

void User::run(void)
{
	Time oldLastOnlineTime = mLastOnlineTime;
	while(true)
	{
		for(unsigned t=0; t<2*5; ++t)
		{
			wait(30000);
			if(oldLastOnlineTime != mLastOnlineTime)
			{
				oldLastOnlineTime = mLastOnlineTime;
				sendInfo();
			}
		}
		
		if(!mAddressBook->isRunning()) mAddressBook->start();
		if(!mStore->isRunning()) mStore->start();
	}
}

}
