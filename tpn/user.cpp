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
#include "tpn/jsonserializer.h"
#include "tpn/byteserializer.h"
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
	Sha512::RecursiveHash(password, name, hash, Sha512::CryptRounds);
	
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
		
		user->addressBook()->update();
	}
}

User::User(const String &name, const String &password, const String &tracker) :
	mName(name),
	mOnline(false),
	mSetOfflineTask(this)
{
	if(mName.empty()) 
		throw Exception("Empty user name");
	
	if(!mName.isAlphanumeric()) 
		throw Exception("User name must be alphanumeric");
	
	// TODO: backward compatibility, should be removed
	if(File::Exist(profilePath()+"password"))
		File::Remove(profilePath()+"password");
	//

	// Auth digest
	if(password.empty())
	{
		File file(profilePath()+"auth", File::Read);
		file.read(mAuth);
		file.close();
	}
	else {
		Sha512::RecursiveHash(password, mName, mAuth, Sha512::CryptRounds);
		
		File file(profilePath()+"auth", File::Truncate);
		file.write(mAuth);
		file.close();
	}
	
	// Secret
	if(File::Exist(profilePath()+"secret"))
	{
		File file(profilePath()+"secret", File::Read);
		file.read(mSecret);
		file.close();
	}
	else {
		ByteString secret;
		secret.writeRandom(64);
		setSecret(secret, Time::Now());
	}
	
	// Token secret
	mTokenSecret.writeRandom(16);
	
	mStore = NULL;
	mAddressBook = NULL;
	mMessageQueue = NULL;
	mProfile = NULL;

	try {
		mStore = new Store(this); // must be created first
        	mAddressBook = new AddressBook(this);
       	 	mMessageQueue = new MessageQueue(this);
        	mProfile = new Profile(this, mName, tracker);
	}
	catch(...)
	{
		delete mStore;
		delete mAddressBook;
		delete mMessageQueue;
		delete mProfile;
		throw;
	}

	try {
		mProfile->load();
	}
	catch(const Exception &e)
	{
		LogWarn("User", String("Unable to load profile: ") + e.what());
	}
	
	UsersMutex.lock();
	UsersByName.insert(mName, this);
	UsersByAuth.insert(mAuth, this);
	UsersMutex.unlock();

	Interface::Instance->add(urlPrefix(), this);
}

User::~User(void)
{
  	UsersMutex.lock();
	UsersByName.erase(mName);
  	UsersByAuth.erase(mAuth);
	UsersMutex.unlock();
	
	Interface::Instance->remove(urlPrefix());
	Scheduler::Global->remove(&mSetOfflineTask);
	
	delete mAddressBook;
	delete mMessageQueue;
	delete mStore;
}

String User::name(void) const
{
	Synchronize(this);
	return mName; 
}

String User::tracker(void) const
{
	Synchronize(this);
        return mProfile->tracker();
}

String User::profilePath(void) const
{
	Synchronize(this);
	if(!Directory::Exist(Config::Get("profiles_dir"))) Directory::Create(Config::Get("profiles_dir"));
	String path = Config::Get("profiles_dir") + Directory::Separator + mName;
	if(!Directory::Exist(path)) Directory::Create(path);
	return path + Directory::Separator;
}

String User::urlPrefix(void) const
{
	Synchronize(this);
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

Profile *User::profile(void) const
{
        return mProfile;
}

bool User::isOnline(void) const
{
	Synchronize(this);
	return mOnline;
}

void User::setOnline(void)
{
	Synchronize(this);
	if(!mOnline) 
	{
		mOnline = true;
		sendStatus();
		
		DesynchronizeStatement(this, mAddressBook->update());
	}
	
	Scheduler::Global->schedule(&mSetOfflineTask, 60.);
}

void User::setOffline(void)
{
	Synchronize(this);
	
	if(mOnline) 
	{
		mOnline = false;
		sendStatus();
	}
}

void User::sendStatus(const Identifier &identifier)
{
	Synchronize(this);
	
	String status = (mOnline ? "online" : "offline");
	
	Notification notification(status);
	notification.setParameter("type", "status");
	
	if(identifier != Identifier::Null)
	{
		DesynchronizeStatement(this, notification.send(identifier));
	}
	else {
		DesynchronizeStatement(this, addressBook()->send(notification));
	}
}

void User::sendSecret(const Identifier &identifier)
{
	Synchronize(this);
	
	if(identifier == Identifier::Null)
		throw Exception("Prevented sendSecret() to broadcast");
	
	Notification notification(mSecret.toString());
	notification.setParameter("type", "secret");
	notification.setParameter("time", File::Time(profilePath()+"secret").toString());
	
	DesynchronizeStatement(this, notification.send(identifier));
}

void User::setSecret(const ByteString &secret, const Time &time)
{
	Synchronize(this);
	
	if(!File::Exist(profilePath()+"secret") || time >  File::Time(profilePath()+"secret"))
	{
		mSecret = secret;
		
		File file(profilePath()+"secret", File::Truncate);
		file.write(mSecret);
		file.close();
		
		if(mSecret != secret)
		{
			AddressBook::Contact *self = addressBook()->getSelf();
			if(self) sendSecret(self->peering());
		}
	}
}

ByteString User::getSecretKey(const String &action) const
{
	ByteString key;
	if(!mSecretKeysCache.get(action, key))
	{
		Sha512::DerivateKey(mSecret, action, key, Sha512::CryptRounds);
		mSecretKeysCache.insert(action, key);
	}

	return key;
}

String User::generateToken(const String &action) const
{
	ByteString salt;
	salt.writeRandom(8);

	ByteString plain;
	ByteSerializer splain(&plain);
	splain.output(name());
	splain.output(action);
	splain.output(salt);
	SynchronizeStatement(this, splain.output(mTokenSecret));

	ByteString digest;
	Sha512::Hash(plain, digest);
	
	ByteString key;
	digest.readBinary(key, 8);

	ByteString token;
	token.writeBinary(salt);	// 8 bytes
	token.writeBinary(key);		// 8 bytes
	
	Assert(token.size() == 16);
	return token;
}

bool User::checkToken(const String &token, const String &action) const
{
	if(!token.empty())
	{
		ByteString bs;
		try {
			token.extract(bs);
		}
		catch(const Exception &e)
		{
			LogWarn("User::checkToken", String("Error parsing token: ") + e.what());
			return false;
		}

		if(bs.size() == 16)
		{
			ByteString salt, remoteKey;
			AssertIO(bs.readBinary(salt, 8));
			AssertIO(bs.readBinary(remoteKey, 8));
			
			ByteString plain;
			ByteSerializer splain(&plain);
			splain.output(name());
			splain.output(action);
			splain.output(salt);
			SynchronizeStatement(this, splain.output(mTokenSecret));
			
			ByteString digest;
			Sha512::Hash(plain, digest);
			
			ByteString key;
			digest.readBinary(key, 8);

			if(key == remoteKey) 
				return true;
		}
	}
	
	LogWarn("User::checkToken", String("Invalid token") + (!action.empty() ? " for action \"" + action + "\"" : ""));
	return false;
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

			// TODO: This is awful
			page.javascript("$('#page').css('max-width','100%');");
			
			page.open("div", "wrapper");
			
			page.open("div","leftcolumn");

			page.open("div", "logo");
			page.openLink("/"); page.image("/logo.png", APPNAME); page.closeLink();
			page.close("div");

			page.open("div","search");
			page.openForm(prefix + "/search", "post", "searchForm");
			//page.button("search","Search");
			page.input("text","query", "Search for files...");
			page.closeForm();
			//page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");	// really annoying with touchscreens
			page.javascript("$(document).ready(function() { document.searchForm.query.style.color = 'grey'; });");
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
					page.open("div", ".contactstr");
					page.open("div", "contact_"+self->uniqueName());
					
					page.open("span", ".name");
					page.link(self->urlPrefix(), self->uniqueName());
					
					page.open("span",".tracker");
					page.text("@" + self->tracker());
					page.close("span");
					
					page.close("span");
					
					page.open("span", ".status");
					page.close("span");
					
					/*page.open("span", ".files");
					page.link(self->urlPrefix()+"/files/", "Files");
					page.close("span");*/
					
					page.close("div");
					
					page.open("div", "contactinfo_" + self->uniqueName() + ".contactinfo");
					page.close("div");
					page.close("div");
				}

				unsigned refreshPeriod = 5000;
				page.javascript("displayContacts('"+prefix+"/contacts/?json"+"','"+String::number(refreshPeriod)+"','#contactsTable')");

				page.close("div");
			}
			
			page.close("div");

			page.open("div","files.box");
			
			Array<String> directories;
			mStore->getDirectories(directories);
			
			Array<String> globalDirectories;
			Store::GlobalInstance->getDirectories(globalDirectories);
			directories.append(globalDirectories);
			
			page.link(prefix+"/files/","Edit",".button");
			if(!directories.empty()) page.link(prefix+"/files/?action=refresh&redirect="+String(prefix+url).urlEncode(), "Refresh", "refreshfiles.button");
			
			page.open("h2");
			page.text("Shared folders");
			page.close("h2");
			
			if(directories.empty()) page.link(prefix+"/files/","Add a shared folder");
			else {
				page.open("div",".files");
				for(int i=0; i<directories.size(); ++i)
				{	
					const String &directory = directories[i];
					String directoryUrl = prefix + "/files/" + directory + "/";

					page.open("div", ".filestr");
					
					page.span("", ".icon");
					page.image("/dir.png");
					
					page.span("", ".filename");
					page.link(directoryUrl, directory);
					
					page.close("div");
				}
				page.close("div");
			}
			page.close("div");
			
			page.close("div");

// End of leftcolumn

			page.open("div", "rightcolumn");

			page.open("div");
			page.open("h1");
			const String instance = Core::Instance->getName().before('.');
			page.openLink(profile()->urlPrefix());			
			page.text(name() + "@" + tracker());
			if(!instance.empty()) page.text(" (" + instance + ")");
			page.closeLink();
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
			page.raw("<a class=\"button\" href=\"#\" onclick=\"createFileSelector('/"+name()+"/myself/files/?json', '#fileSelector', 'input.attachment', 'input.attachmentname','"+generateToken("directory")+"');\"><img src=\"/paperclip.png\" alt=\"File\"></a>");
			page.openForm("#", "post", "statusform");
			page.input("hidden", "attachment");
			page.input("hidden", "attachmentname");
			page.textarea("statusinput");
			//page.button("send","Send");
			//page.br();
			page.closeForm();
			page.div("","attachedfile");
			page.javascript("$(document).ready(function() { formTextBlur();});");
			page.close("div");

			page.div("", "fileSelector");

			page.open("div", "newsfeed.box");

			page.open("div", "optionsnewsfeed");

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

			page.open("h2");
			page.text("Public news feed");
			page.close("h2");
			
			page.open("div", "statusmessages");
			page.close("div");

			page.close("div");
		 
			String token = generateToken("message");
			
			page.javascript("function postStatus() {\n\
					var message = $(document.statusform.statusinput).val();\n\
					var attachment = $(document.statusform.attachment).val();\n\
					if(!message) return false;\n\
					var fields = {};\n\
					fields['message'] = message;\n\
					fields['public'] = 1;\n\
					fields['token'] = '"+token+"';\n\
					if(attachment) fields['attachment'] = attachment;\n\
					var request = $.post('"+prefix+broadcastUrl+"/"+"', fields);\n\
					request.fail(function(jqXHR, textStatus) {\n\
						alert('The message could not be sent.');\n\
					});\n\
					$(document.statusform.statusinput).val('');\n\
					$(document.statusform.attachment).val('');\n\
					$(document.statusform.attachmentname).val('');\n\
					$('#attachedfile').hide();\n\
				}\n\
				$(document.statusform.attachment).change(function() {\n\
					$('#attachedfile').html('');\n\
					$('#attachedfile').hide();\n\
					var filename = $(document.statusform.attachmentname).val();\n\
					if(filename != '') {\n\
						$('#attachedfile').append('<img class=\"icon\" src=\"/file.png\">');\n\
						$('#attachedfile').append('<span class=\"filename\">'+filename+'</span>');\n\
						$('#attachedfile').show();\n\
					}\n\
					$(document.statusform.statusinput).focus();\n\
					$(document.statusform.statusinput).val(filename);\n\
					$(document.statusform.statusinput).select();\n\
				});\n\
				function post(object, parentStamp) {\n\
					var message = $(object).val();\n\
					if(!message) return false;\n\
					$(object).val('');\n\
					if(!parentStamp) {\n\
						var request = $.post('"+prefix+broadcastUrl+"/"+"',\n\
							{ 'message': message , 'public': 1, 'token': '"+token+"'});\n\
					}\n\
					else {\n\
						var request = $.post('"+prefix+broadcastUrl+"/"+"',\n\
							{ 'message': message , 'public': 1, 'parent': parentStamp, 'token': '"+token+"'});\n\
					}\n\
					request.fail(function(jqXHR, textStatus) {\n\
						alert('The message could not be sent.');\n\
					});\n\
				}\n\
				document.statusform.onsubmit = function() {\n\
					postStatus();\n\
					return false;\n\
				}\n\
				function formTextBlur() {\n\
					document.statusform.statusinput.style.color = 'grey';\n\
					document.statusform.statusinput.value = 'Click here to post a public message for all your contacts';\n\
				}\n\
				document.statusform.statusinput.onblur = function() {\n\
					formTextBlur();\n\
				}\n\
				document.statusform.statusinput.onfocus = function() {\n\
					document.statusform.statusinput.value = '';\n\
					document.statusform.statusinput.style.color = 'black';\n\
				}\n\
				document.searchForm.query.onfocus = function() {\n\
					document.searchForm.query.value = '';\n\
					document.searchForm.query.style.color = 'black';\n\
				}\n\
				document.searchForm.query.onblur = function() {\n\
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
						var parentStamp = name.substr(name.lastIndexOf('_')+1);\n\
						post(this, parentStamp);\n\
						return false; \n\
					}\n\
				});\n\
				$('#newsfeed').on('blur','.reply', function (e) {\n\
					$(this).hide();\n\
				});\n\
				$('#attachedfile').hide();\n\
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
			String match;
			if(!request.post.get("query", match))
				request.get.get("query", match);
			match.trim();
			
			if(request.get.contains("json") || request.get.contains("playlist"))
			{
				if(match.empty()) throw 400;
				
				Resource::Query query(store());
				query.setMatch(match);
				
				SerializableSet<Resource> resources;
				if(!query.submit(resources))
					throw 404;

				if(request.get.contains("json"))
				{
					Http::Response response(request, 200);
					response.headers["Content-Type"] = "application/json";
					response.send();
					JsonSerializer json(response.sock);
					json.output(resources);
				}
				else {
					Http::Response response(request, 200);
					response.headers["Content-Disposition"] = "attachment; filename=\"playlist.m3u\"";
					response.headers["Content-Type"] = "audio/x-mpegurl";
					response.send();
					
					String host;
					request.headers.get("Host", host);
					Resource::CreatePlaylist(resources, response.sock, host);
				}
				return;
			}
			
			Http::Response response(request, 200);
			response.send();
				
			Html page(response.sock);
			
			if(match.empty()) page.header("Search");
			else page.header(String("Searching ") + match);
			
			page.open("div","topmenu");
			page.openForm(prefix + "/search", "post", "searchform");
			page.input("text","query", match);
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			if(!match.empty()) page.link(prefix+request.url+"?query="+match.urlEncode()+"&playlist","Play all",".button");
			page.close("div");
			
			unsigned refreshPeriod = 5000;
			page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				if(info.newmessages) playMessageSound();\n\
			});");
			
			if(!match.empty())
			{
				page.div("", "#list.box");
				page.javascript("listDirectory('"+prefix+request.url+"?query="+match.urlEncode()+"&json','#list','"+name()+"');");
				page.footer();
			}
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

}
