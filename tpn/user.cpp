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
	mMessageQueue(new MessageQueue(this)),
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
	delete mMessageQueue;
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

MessageQueue *User::messageQueue(void) const
{
	return mMessageQueue;
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
	
	StringMap tmp(mInfo);	// WTF
	String content;
	YamlSerializer serializer(&content);
	serializer.output(tmp);
	
	Notification notification(content);
	notification.setParameter("type", "info");
	if(identifier == Identifier::Null) notification.send();
	else notification.send(identifier);
}

void User::http(const String &prefix, Http::Request &request)
{
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

			/*page.open("div", "mainheader");
			page.openLink("/"); page.image("/logo.png", APPNAME, "logo"); page.closeLink();
			
			page.open("div", ".search");
			page.openForm(prefix + "/search", "post", "searchForm");
			page.input("text","query");
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			page.br();
			page.close("div");
			
			page.close("div");*/

			page.open("div", "wrapper");

			page.javascript("$('#page').css('max-width','100%');");
			
			/*page.open("div");
			page.open("h1");
			const String tracker = Config::Get("tracker");
			const String instance = Core::Instance->getName().before('.');
			page.text(name() + "@" + tracker);
			if(!instance.empty()) page.text(" (" + instance + ")");
			page.close("h1");
			page.close("div");*/

			page.open("div","leftcolumn.box");

			page.open("div", "logo");
			page.openLink("/"); page.image("/logo.png", APPNAME, "logo"); page.closeLink();
			page.close("div");

			page.open("div","search");
			page.raw("<span class=\"searchinput\">");

			page.openForm(prefix + "/search", "post", "searchForm");
			//page.button("search","Search");
			page.input("text","query");
			page.closeForm();
			page.raw("</span>");

			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			//page.br();
			page.close("div");



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
				page.open("div","contactsTable");
				
				if(self)
				{
					page.open("div", String("contact_")+self->uniqueName());
					
					page.span("", ".name");
					page.link(self->urlPrefix(), self->uniqueName());
					
					page.span("",".tracker");
					page.text(String("@") + self->tracker());
					
					page.span("", ".status");
					
					page.span("", ".files");
					page.link(self->urlPrefix()+"/files/", "Files");
					
					page.close("div");
				}

				unsigned refreshPeriod = 5000;
				page.javascript("displayContacts('"+prefix+"/contacts/?json"+"','"+String::number(refreshPeriod)+"','#contactsTable')");

				page.close("div");
			}
			
			page.close("div");

			page.open("div","files.box");
			
			page.link(prefix+"/files/","Edit",".button");
                        //page.link(prefix+"/files/?action=refresh&redirect="+String(prefix+url).urlEncode(), "Refresh", ".button");

			page.open("h2");
			page.text("Shared folders");
			page.close("h2");
		
			Array<String> directories;
			mStore->getDirectories(directories);
			
			Array<String> globalDirectories;
			Store::GlobalInstance->getDirectories(globalDirectories);
			directories.append(globalDirectories);
			
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
			
			page.close("div");

// End of leftcolumn

			page.open("div", "rightcolumn");

			page.open("div");
			page.open("h1");
			const String tracker = Config::Get("tracker");
			const String instance = Core::Instance->getName().before('.');
			page.text(name() + "@" + tracker);
			if(!instance.empty()) page.text(" (" + instance + ")");
			page.close("h1");
			page.close("div");
			
			String broadcastUrl = "/messages";
			String publicUrl = "?public=1";
			String countUrl = "&count=";
			String defaultCount = "20";
			String displayOthersUrl = "&incoming=1";
			String displaySelfUrl = "&incoming=0";

			String setDisplayUrl = displaySelfUrl;		

			page.open("div", "statuspanel");
			page.openForm("#", "post", "statusform");
			page.textarea("statusinput");
			//page.button("send","Send");
			//page.br();
			page.closeForm();
			page.javascript("$(document).ready(function() { formTextBlur();});");
			page.close("div");


			page.open("div", "newsfeed");

			page.open("div", "optionsnewsfeed");

			page.text("News Feed");

			StringMap optionsCount;
			optionsCount["&count=15"] << "Last 15";
			optionsCount["&count=30"] << "Last 30";
			optionsCount[""] << "All";
			page.raw("<span class=\"customselect\">");
			page.select("listCount", optionsCount, "&count=15");
			page.raw("</span>");

			StringMap optionsIncoming;
			optionsIncoming["0"] << "Mine & others";
			optionsIncoming["1"] << "Others only";
			page.raw("<span class=\"customselect\">");
			page.select("listIncoming", optionsIncoming, "0");
			page.raw("</span>");

			page.close("div");

			page.open("div", "statusmessages");
			page.close("div");

			page.close("div");
		 
			page.javascript("function postStatus()\n\
				{\n\
					var message = document.statusform.statusinput.value;\n\
					if(!message) return false;\n\
					document.statusform.statusinput.value = '';\n\
					var request = $.post('"+prefix+broadcastUrl+"/"+"',\n\
						{ 'message': message , 'public': 1});\n\
					request.fail(function(jqXHR, textStatus) {\n\
						alert('The message could not be sent.');\n\
					});\n\
				}\n\
				function post(object, idParent)\n\
				{\n\
					var message = $(object).val();\n\
					if(!message) return false;\n\
					$(object).val('');\n\
					if(!idParent)\n\
					{\n\
						var request = $.post('"+prefix+broadcastUrl+"/"+"',\n\
							{ 'message': message , 'public': 1});\n\
					}\n\
					else\n\
					{\n\
						var request = $.post('"+prefix+broadcastUrl+"/"+"',\n\
							{ 'message': message , 'public': 1, 'parent': idParent});\n\
					}\n\
					request.fail(function(jqXHR, textStatus) {\n\
						alert('The message could not be sent.');\n\
					});\n\
				}\n\
				document.statusform.onsubmit = function()\n\
				{\n\
					postStatus();\n\
					return false;\n\
				}\n\
				function formTextBlur()\n\
				{\n\
					document.statusform.statusinput.style.color = 'grey';\n\
					document.statusform.statusinput.value = 'What do you have in mind ?';\n\
				}\n\
				document.statusform.statusinput.onblur = function()\n\
				{\n\
					formTextBlur();\n\
				}\n\
				document.statusform.statusinput.onfocus = function()\n\
				{\n\
					document.statusform.statusinput.value = '';\n\
					document.statusform.statusinput.style.color = 'black';\n\
				}\n\
				document.searchForm.query.onfocus = function()\n\
				{\n\
					document.searchForm.query.value = '';\n\
					document.searchForm.query.style.color = 'black';\n\
				}\n\
				document.searchForm.query.onblur = function()\n\
				{\n\
					document.searchForm.query.style.color = 'grey';\n\
					document.searchForm.query.value = 'Search for files...';\n\
				}\n\
				$('textarea.statusinput').keypress(function(e) {\n\
					if (e.keyCode == 13 && !e.shiftKey) {\n\
						e.preventDefault();\n\
						postStatus();\n\
					}\n\
				});\n\
				var listCount = document.getElementsByName(\"listCount\")[0];\n\
				listCount.addEventListener('change', function() {\n\
					updateMessagesReceiver('"+prefix+broadcastUrl+"/?json&public=1"+"&incoming='+listIncoming.value.toString()+'"+"'+listCount.value.toString(),'#statusmessages');\n\
				}, true);\n\
				\n\
				var listIncoming = document.getElementsByName(\"listIncoming\")[0];\n\
				listIncoming.addEventListener('change', function() {\n\
					updateMessagesReceiver('"+prefix+broadcastUrl+"/?json&public=1"+"&incoming='+listIncoming.value.toString()+'"+"'+listCount.value.toString(),'#statusmessages');\n\
				}, true);\n\
				setMessagesReceiver('"+prefix+broadcastUrl+"/"+publicUrl+setDisplayUrl+"&json"+"'+listCount.value.toString(),'#statusmessages');\n\
				// Events for the reply textareas : \n\
				$('#newsfeed').on('keypress','textarea', function (e) {\n\
					if (e.keyCode == 13 && !e.shiftKey) {\n\
						var name = $(this).attr('name');\n\
						var id = name.split(\"replyTo\").pop();\n\
						post(this, id);\n\
						return false; \n\
					}\n\
				});\n\
				$('#newsfeed').on('blur','textarea', function (e) {\n\
					$(this).css('display','none');\n\
				});\n\
				$('#newsfeed').on('focus','textarea', function (e) {\n\
					$(this).css('display','block');\n\
				});\n\
");
			
			page.open("div", "footer");
			page.text(String("Version ") + APPVERSION + " - ");
			page.link(HELPLINK, "Help", "", true);
			page.text(" - ");
			page.link(SOURCELINK, "Source code", "", true);
			page.close("div");

			page.close("div");
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
			
			page.listFilesFromRequest(trequest, prefix, request, addressBook()->user());
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
