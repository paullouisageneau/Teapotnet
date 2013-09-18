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
		
		user->addressBook()->update();
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

// TODO : added
	mProfile = new Profile(this);
// end
	
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
		DesynchronizeStatement(this, mAddressBook->update());
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

void User::sendInfo(Identifier identifier)
{
	String content;
	YamlSerializer serializer(&content);
	SynchronizeStatement(this, serializer.output(mInfo));
	
	Notification notification(content);
	notification.setParameter("type", "info");
	notification.send(identifier);
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

			page.open("div","leftcolumn");

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
			const String tracker = Config::Get("tracker");
			const String instance = Core::Instance->getName().before('.');
			page.raw("<a href='profile'>");			
			page.text(name() + "@" + tracker);
			if(!instance.empty()) page.text(" (" + instance + ")");
			page.raw("</a>");
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
			
			page.openForm(prefix + "/search", "post", "searchform");
			page.input("text","query", match);
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			page.br();

			unsigned refreshPeriod = 5000;
			page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				if(info.newnotifications) playNotificationSound();\n\
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

void User::run(void)
{
	Time oldLastOnlineTime(0);
	unsigned m = 0;	// minutes
	while(true)
	{
		for(int t=0; t<2; ++t)
		{
			try {
				Thread::Sleep(30.);
				
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
		if((m%60 == 0) && !mStore->isRunning()) mStore->start();
	}
}


User::Profile::Profile(User *user, const String &uname):
	mUser(user),
	mName(uname)
{
	Assert(mUser);
	if(mName.empty()) mName = mUser->name();
	Assert(!mName.empty());
	
	mFileName = infoPath() + mName;
	load();

	Interface::Instance->add(urlPrefix(), this);
}

User::Profile::~Profile()
{
	Interface::Instance->remove(urlPrefix());
}


String User::Profile::infoPath(void) const
{
	String path = mUser->profilePath() + "infos";
	if(!Directory::Exist(path)) Directory::Create(path);
	return path + Directory::Separator;
}

String User::Profile::urlPrefix(void) const
{
	// TODO: redirect /[user]/myself/profile
	if(mName == mUser->name()) return "/"+mUser->name()+"/profile";
	else return "/"+mUser->name()+"/contacts/"+mName+"/profile";
}

void User::Profile::load()
{
	if(!File::Exist(mFileName))
	{
		clear();
		return;
	}
	
	File profileFile(mFileName, File::Read);
	YamlSerializer serializer(&profileFile);
	deserialize(serializer);
}

void User::Profile::save()
{
	SafeWriteFile profileFile(mFileName);
	YamlSerializer serializer(&profileFile);
	serialize(serializer);
	profileFile.close();
}

void User::Profile::clear()
{
	mFirstName.clear();
	mMiddleName.clear();
	mLastName.clear();
	mBirthday.clear();
	mGender.clear();
	mReligion.clear();
	mRelationship.clear();
	mDescription.clear();
	mStatus.clear();
	mCity.clear();
	mAddress.clear();
	mMail.clear();
	mPhone.clear();
	mCollege.clear();
	mUniversity.clear();
	mJob.clear();
	mProfilePhoto.clear();
	mBooks.clear();
	mHobbies.clear();
	mMovies.clear();
	mPolitics.clear();
	mInternship.clear();
	mComputer.clear();
	mResume.clear();
	
	// Do not call save() here
}

void User::Profile::serialize(Serializer &s) const
{
	Serializer::ConstObjectMapping fields;
	
	if(!mStatus.empty())		fields["status"] = &mStatus;
	if(!mProfilePhoto.empty())	fields["profilephoto"] = &mProfilePhoto;

	if(!mFirstName.empty())		fields["firstname"] = &mFirstName;
	if(!mMiddleName.empty())	fields["middlename"] = &mMiddleName;
	if(!mLastName.empty())		fields["lastname"] = &mLastName;
	if(!mBirthday.empty())		fields["birthday"] = &mBirthday;
	if(!mGender.empty())		fields["gender"] = &mGender;
	if(!mRelationship.empty())	fields["relationship"] = &mRelationship;

	if(!mCity.empty())		fields["city"] = &mCity;
	if(!mAddress.empty())		fields["address"] = &mAddress;
	if(!mMail.empty())		fields["mail"] = &mMail;
	if(!mPhone.empty())		fields["phone"] = &mPhone;

	if(!mCollege.empty())		fields["college"] = &mCollege;
	if(!mUniversity.empty())	fields["university"] = &mUniversity;

	if(!mJob.empty())		fields["job"] = &mJob;
	if(!mComputer.empty())		fields["computer"] = &mComputer;
	if(!mResume.empty())		fields["resume"] = &mResume;
	if(!mInternship.empty())	fields["internship"] = &mInternship;

	if(!mReligion.empty())		fields["religion"] = &mReligion;
	if(!mBooks.empty())		fields["books"] = &mBooks;
	if(!mMovies.empty())		fields["movies"] = &mMovies;
	if(!mHobbies.empty())		fields["hobbies"] = &mHobbies;
	if(!mPolitics.empty())		fields["politics"] = &mPolitics;

	if(!mDescription.empty())	fields["description"] = &mDescription;
	
	s.outputObject(fields);
}

bool User::Profile::deserialize(Serializer &s)
{	
	Serializer::ObjectMapping fields;
	
	fields["status"] = &mStatus;
	fields["profilephoto"] = &mProfilePhoto;

	fields["firstname"] = &mFirstName;
	fields["middlename"] = &mMiddleName;
	fields["lastname"] = &mLastName;
	fields["birthday"] = &mBirthday;
	fields["gender"] = &mGender;
	fields["relationship"] = &mRelationship;

	fields["city"] = &mCity;
	fields["address"] = &mAddress;
	fields["mail"] = &mMail;
	fields["phone"] = &mPhone;

	fields["college"] = &mCollege;
	fields["university"] = &mUniversity;

	fields["job"] = &mJob;
	fields["computer"] = &mComputer;
	fields["resume"] = &mResume;
	fields["internship"] = &mInternship;

	fields["religion"] = &mReligion;
	fields["books"] = &mBooks;
	fields["movies"] = &mMovies;
	fields["hobbies"] = &mHobbies;
	fields["politics"] = &mPolitics;

	fields["description"] = &mDescription;
	
	return s.inputObject(fields);
}

void User::Profile::http(const String &prefix, Http::Request &request)
{
	try {
		String url = request.url;
		if(url.empty() || url[0] != '/') throw 404;

		if(request.method == "POST")
		{
			String field, valueField;

			if(request.post.contains("clear"))
			{
				clear();
				save();
			}
			else {
				field = request.post["field"];
				valueField = request.post["valueField"];

				updateField(field, valueField);
			}
			
			Http::Response response(request, 303);
			response.headers["Location"] = prefix + "/";
			response.send();
			return;
		}
		
		if(url == "/")
		{
			Http::Response response(request,200);
			response.send();
		
			Html page(response.sock);
			page.header(APPNAME, true);

			try {
				page.header("My profile : "+mUser->name());

				page.javascript("var valueField;\n\
						var currentId;\n\
						var blocked = false;\n\
						var isEmpty = false;");

				page.open("div","profile.box");

				page.open("div",".inlinetitle");
				page.open("h2");
				page.text("My personal information");
				page.close("h2");

				page.button("clearprofilebutton","Clear profile");
				page.close("div");

				page.open("div",".profileinfoswrapper");

					page.open("div","personalstatus");
						page.raw("<span class=\"statusquotemark\"> “ </span>");
						page.open("span","status.editable");
						if(mStatus!="")
						{
							page.text(mStatus);
						}
						else
						{
							page.open("span","status.empty");
							page.text("(click here to post a status)");
							page.close("span");
						}
						page.close("span");
						page.raw("<span class=\"statusquotemark\"> ” </span>");
					page.close("div");

					page.open("div","profilephoto");
					page.close("div");

				page.close("div");

				page.open("div", ".profileinfoswrapper");

					page.open("div","personalinfos.box");

					page.open("h2");
					page.text("Personal Information");
					page.close("h2");

					displayProfileInfo(page,"First Name","firstname", mFirstName);
					displayProfileInfo(page,"Middle Name","middlename", mMiddleName);
					displayProfileInfo(page,"Last Name","lastname", mLastName);
					displayProfileInfo(page,"Birthday", " : ", "When were you born ?", "birthday", mBirthday);
					displayProfileInfo(page,"Gender","gender", mGender);
					displayProfileInfo(page,"Relationship Status","relationship", mRelationship);
					displayProfileInfo(page,"City","city", mCity);
					displayProfileInfo(page,"Address","address", mAddress);
					displayProfileInfo(page,"Mail","mail", mMail);
					displayProfileInfo(page,"Phone Number","phone", mPhone);
					page.close("div");

					page.open("div","culture.box");

					page.open("h2");
					page.text("Culture");
					page.close("h2");

					displayProfileInfo(page,"Religion","religion", mReligion);
					displayProfileInfo(page,"Books", " : ", "What are your favorite books ?", "books", mBooks);
					displayProfileInfo(page,"Movies", " : ", "What are your favorite movies ?", "movies", mMovies);
					displayProfileInfo(page,"Hobbies", " : ", "What are your favorite hobbies ?","hobbies", mHobbies);
					displayProfileInfo(page,"Politics", " : ", "What are your political opinions ?","politics", mPolitics);
					page.close("div");

				//page.close("div");

				//page.open("div",".profileinfoswrapper");
					page.open("div","jobs.box");

					page.open("h2");
					page.text("Skills and jobs");
					page.close("h2");

					displayProfileInfo(page,"Computer Skills", " : ", "What are your computer skills ?","computer", mComputer);
					displayProfileInfo(page,"Resume", " : ", "Put a link to your resume","resume", mResume);
					displayProfileInfo(page,"Current Job","job", mJob);
					displayProfileInfo(page,"Last Internship", " : ", "Where did you do your last internship ?","internship", mInternship);

					page.close("div");

					page.open("div","education.box");

					page.open("h2");
					page.text("Education");
					page.close("h2");

					displayProfileInfo(page,"College", " : ", "Where did you go to college ?","college", mCollege);
					displayProfileInfo(page,"University", " : ", "What University did you attend ?","university", mUniversity);

					page.close("div");
				//page.close("div");

				//page.open("div",".profileinfoswrapper");
					page.open("div","description.box");

					page.open("h2");
					page.text("Description");
					page.close("h2");

					displayProfileInfo(page,"", "", "Describe yourself", "description", mDescription);

					page.close("div");
				page.close("div");


				page.close("div");

				page.javascript("loadClickHandlers = function()\n\
						{\n\
							$('.editable').click(onEditableClick);\n\
						}\n\
						onEditableClick = function()\n\
						{\n\
						if(!blocked)\n\
						{\n\
							blocked = true;\n\
							currentId = $(this).attr('id');\n\
							var currentText = $(this).html();\n\
							if (!isEmpty) valueField = currentText;\n\
							$(this).after('<div><input type=\"text\" value=\"'+valueField+'\" class=\"inputprofile\"></div>');\n\
							$(this).css('display','none');\n\
							$('input').focus();\n\
							$('input').keypress(function(e) {\n\
						if (e.keyCode == 13 && !e.shiftKey) {\n\
						e.preventDefault();\n\
						var field = currentId;\n\
						valueField = $('input[type=text]').attr('value');\n\
						postVariable(field, valueField);\n\
						}\n\
							});\n\
							$('input').blur(function()\n\
							{\n\
								setTimeout(function(){location.reload();},100);\n\
							}\n\
							);\n\
						}\n\
						}\n\
						postVariable = function(field, valueFieldRaw)\n\
						{\n\
							valueFieldPost = valueFieldRaw;\n\
							//if(valueFieldPost == '') valueFieldPost = 'emptyfield';\n\
							var request = $.post('"+prefix+"/profile"+"',\n\
								{ 'field': field , 'valueField': valueFieldPost });\n\
							request.fail(function(jqXHR, textStatus) {\n\
								alert('The profile update could not be made.');\n\
							});\n\
							isEmpty = false;\n\
							setTimeout(function(){location.reload();},100);\n\
						}\n\
						loadClickHandlers();\n\
						$('.empty').click(function() {isEmpty = true; valueField = ''; //empty Field \n\
							if(!blocked)\n\
							{\n\
								blocked = true;\n\
								currentId = $(this).attr('id');\n\
								var currentText = $(this).html();\n\
								$(this).after('<div><input type=\"text\" class=\"inputprofile\"></div>');\n\
								$(this).css('display','none');\n\
								$('input').focus();\n\
								$('input').keypress(function(e) {\n\
							if (e.keyCode == 13 && !e.shiftKey) {\n\
							e.preventDefault();\n\
							var field = currentId;\n\
							valueField = $('input[type=text]').attr('value');\n\
							postVariable(field, valueField);\n\
							}\n\
								});\n\
								$('input').blur(function()\n\
								{\n\
									setTimeout(function(){location.reload();},100);\n\
								}\n\
								);\n\
							}\n\
						});\n\
						$('.clearprofilebutton').click(function() {\n\
							var request = $.post('"+prefix+"/profile"+"',\n\
								{ 'clear': 1 });\n\
							request.fail(function(jqXHR, textStatus) {\n\
								alert('Profile clear could not be made.');\n\
							});\n\
							setTimeout(function(){location.reload();},100);\n\
						}\n\
						);\n\
						");

			}
			catch(IOException &e)
			{
				LogWarn("User::Profile::http", e.what());
				return;
			}

			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("User::Profile::http", e.what());
		throw 404;	// Httpd handles integer exceptions
	}
			
	throw 404;
}

void User::Profile::displayProfileInfo(Html &page, const String &fieldText, const String &showPunctuation, const String &emptyQuestion, const String &fieldName, String &field)
{
	if(!field.empty())
	{
		page.open("div",fieldName+"div.profileinfo");
		page.text(fieldText+showPunctuation);
		page.open("span",fieldName+".editable");
		page.text(field);
		page.close("span");
		//page.br();
		page.close("div");
	}
	else
	{
		page.open("div",fieldName+".empty");
		page.open("span",".profileinfo");
		page.text(emptyQuestion);
		page.close("span");
		page.close("div");
	}
}

void User::Profile::displayProfileInfo(Html &page, const String &fieldText, const String &fieldName, String &field)
{
	displayProfileInfo(page, fieldText, " : ", "What is your "+fieldText+" ?", fieldName, field);
}

String User::Profile::getStatus()
{
	return mStatus;
}

void User::Profile::updateField(String &key, String &value)
{
	// TODO : be careful with non string (in particular Time)

	if(key == "status") 
		mStatus = value;

	if(key == "firstname")
		mFirstName = value;

	if(key == "middlename")
		mMiddleName = value;

	if(key == "lastname")
		mLastName = value;

	if(key == "profilephoto")
		mProfilePhoto = value;

	if(key == "birthday")
		mBirthday = value; // TODO

	if(key == "gender")
		mGender = value;

	if(key == "relationship")
		mRelationship = value;

	if(key == "city")
		mCity = value;
	if(key == "address")
		mAddress = value;
	if(key == "mail")
		mMail = value;
	if(key == "phone")
		mPhone = value;

	if(key == "college")
		mCollege = value;
	if(key == "university")
		mUniversity = value;

	if(key == "job")
		mJob = value;
	if(key == "computer")
		mComputer = value;
	if(key == "resume")
		mResume = value;
	if(key == "internship")
		mInternship = value;

	if(key == "religion")
		mReligion = value;
	if(key == "books")
		mBooks = value;
	if(key == "movies")
		mMovies = value;
	if(key == "hobbies")
		mHobbies = value;
	if(key == "politics")
		mPolitics = value;

	if(key == "description")
		mDescription = value;
	
	save();
}

User::Profile *User::profile(void) const
{
	//Synchronize(this);
 	return mProfile; 
}

}
