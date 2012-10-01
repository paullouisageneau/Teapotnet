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

#include "addressbook.h"
#include "user.h"
#include "core.h"
#include "sha512.h"
#include "config.h"
#include "file.h"
#include "directory.h"
#include "html.h"

namespace tpot
{

AddressBook::AddressBook(User *user) :
	mUser(user)
{
	Assert(mUser != NULL);
	mFileName = mUser->profilePath() + "contacts";
	
	Interface::Instance->add("/"+mUser->name()+"/contacts", this);
	
	try {
	  File file(mFileName, File::Read);
	  load(file);
	  file.close();
	}
	catch(...)
	{
	  
	}
}

AddressBook::~AddressBook(void)
{
  	Interface::Instance->remove("/"+mUser->name()+"/contacts");
}

User *AddressBook::user(void) const
{
 	return mUser; 
}

String AddressBook::userName(void) const
{
 	return mUser->name(); 
}

const Identifier &AddressBook::addContact(String name, const ByteString &secret)
{
	Synchronize(this);

	String tracker = name.cut('@');
	if(tracker.empty()) tracker = Config::Get("tracker");
	
	String uname = name;
	unsigned i = 0;
	while(mContactsByUniqueName.contains(uname))
	{
		uname = name;
		uname << ++i;
	}
	
	Contact *contact = new Contact(this, uname, name, tracker, secret);
	if(mContacts.contains(contact->peering())) 
	{
		delete contact;
		throw Exception("The contact already exists");
	}
	
	mContacts.insert(contact->peering(), contact);
	mContactsByUniqueName.insert(contact->uniqueName(), contact);
	
	save();
	start();
	return contact->peering();
}

void AddressBook::removeContact(const Identifier &peering)
{
	Synchronize(this);
	
	Contact *contact;
	if(mContacts.get(peering, contact))
	{
		Core::Instance->unregisterPeering(peering);
		mContactsByUniqueName.erase(contact->uniqueName());
 		mContacts.erase(peering);
		delete contact;
		save();
		start();
	}
}

const AddressBook::Contact *AddressBook::getContact(const Identifier &peering)
{
	return mContacts.get(peering);  
}

void AddressBook::load(Stream &stream)
{
	Synchronize(this);

	Contact *contact = new Contact(this);
	while(stream.read(*contact))
	{
		mContacts.insert(contact->peering(), contact);
		contact = new Contact(this);
	}
	delete contact;
	start();
}

void AddressBook::save(Stream &stream) const
{
	Synchronize(this);
  
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		stream.write(*contact);
	}	
}

void AddressBook::save(void) const
{
	Synchronize(this);
  
	SafeWriteFile file(mFileName);
	save(file);
	file.close();
}

void AddressBook::update(void)
{
	Synchronize(this);
	Log("AddressBook::update", "Updating " + String::number(unsigned(mContacts.size())) + " contacts");
	
	for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		Contact *contact = it->second;
		contact->update();
	}
		
	Log("AddressBook::update", "Finished");
	save();
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);

	try {
		if(request.url.empty() || request.url == "/")
		{
			if(request.method == "POST")
			{
				try {
					String name, csecret;
					request.post["name"] >> name;
					request.post["secret"] >> csecret;
				  
					ByteString secret;
					Sha512::Hash(csecret, secret, Sha512::CryptRounds);
					
					addContact(name, secret);
				}
				catch(...)
				{
					throw 400;
				}				
				
				Http::Response response(request,303);
				response.headers["Location"] = prefix + "/";
				response.send();
				return;
			}
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header("Contacts");
			page.open("h1");
			page.text("Contacts");
			page.close("h1");

			for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
				it != mContacts.end();
				++it)
			{
		    		Contact *contact = it->second;
				String contactUrl = prefix + '/' + contact->uniqueName() + '/';
				page.link(contactUrl, contact->name() + "@" + contact->tracker());
				page.text(" "+String::hexa(contact->peeringChecksum(),8));
				
				String status("Not connected");
				if(Core::Instance->hasPeer(contact->peering())) status = "Connected";
				page.text(" ("+status+")");
				
				page.br();
			}
	
			page.openForm(prefix+"/","post");
			page.openFieldset("New contact");
			page.label("name","Name"); page.input("text","name"); page.br();
			page.label("secret","Secret"); page.input("text","secret"); page.br();
			page.label("add"); page.button("add","Add contact");
			page.closeFieldset();
			page.closeForm();
			
			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		Log("AddressBook::http",e.what());
		throw 500;	// Httpd handles integer exceptions
	}
	
	throw 404;
}

void AddressBook::run(void)
{
	update();
}

bool AddressBook::publish(const Identifier &remotePeering)
{
	try {
		String url("http://" + Config::Get("tracker") + "/tracker/" + remotePeering.toString());
		
		StringMap post;
		post["port"] = Config::Get("port");
		if(!Config::Get("external_address").empty() && Config::Get("external_address") != "auto")
			post["host"] = Config::Get("external_address");
		if(Http::Post(url, post) != 200) return false;
	}
	catch(const std::exception &e)
	{
		Log("AddressBook::publish", e.what()); 
		return false;
	}
	return true;
}

bool AddressBook::query(const Identifier &peering, const String &tracker, Array<Address> &addrs)
{
	try {
	  	String url;
	  	if(tracker.empty()) url = "http://" + Config::Get("tracker") + "/tracker/" + peering.toString();
		else url = "http://" + tracker + "/tracker/" + peering.toString();
  
		String output;
		if(Http::Get(url, &output) != 200) return false;
	
		String line;
		while(output.readLine(line))
		{
			line.trim();
			if(line.empty()) continue;
			Address a;
			line >> a;
			if(!addrs.contains(a))
				addrs.push_back(a);
		}
	}
	catch(const std::exception &e)
	{
		Log("AddressBook::query", e.what()); 
		return false;
	}
	return true;
}

AddressBook::Contact::Contact(	AddressBook *addressBook, 
				const String &uname,
				const String &name,
			        const String &tracker,
			        const ByteString &secret) :
	mAddressBook(addressBook),
	mUniqueName(uname),
	mName(name),
	mTracker(tracker),
	mSecret(secret)
{	
	Assert(addressBook != NULL);
	Assert(!uname.empty());
	Assert(!name.empty());
	Assert(!tracker.empty());
	Assert(!secret.empty());
	
	// Compute peering
	String agregate;
	agregate.writeLine(mSecret);
	agregate.writeLine(mAddressBook->userName());
	agregate.writeLine(mName);
	Sha512::Hash(agregate, mPeering, Sha512::CryptRounds);
	
	// Compute Remote peering
	agregate.clear();
	agregate.writeLine(mSecret);
	agregate.writeLine(mName);
	agregate.writeLine(mAddressBook->userName());
	Sha512::Hash(agregate, mRemotePeering, Sha512::CryptRounds);
	
	Interface::Instance->add(urlPrefix(), this);
}

AddressBook::Contact::Contact(AddressBook *addressBook) :
  	mAddressBook(addressBook),
	mMessagesCount(0)
{
  
}

AddressBook::Contact::~Contact(void)
{
  	Interface::Instance->remove(urlPrefix());
}

const String &AddressBook::Contact::uniqueName(void) const
{
	return mUniqueName;
}

const String &AddressBook::Contact::name(void) const
{
	return mName;
}

const String &AddressBook::Contact::tracker(void) const
{
	return mTracker;
}

const Identifier &AddressBook::Contact::peering(void) const
{
	return mPeering;
}

const Identifier &AddressBook::Contact::remotePeering(void) const
{
	return mRemotePeering;
}

uint32_t AddressBook::Contact::peeringChecksum(void) const
{
	return mPeering.checksum32() + mRemotePeering.checksum32(); 
}

String AddressBook::Contact::urlPrefix(void) const
{
	if(mUniqueName.empty()) return "";
	return String("/")+mAddressBook->userName()+"/contacts/"+mUniqueName;
}

void AddressBook::Contact::update(void)
{
	Synchronize(this);

	if(!Core::Instance->hasPeer(mPeering))
	{
		Core::Instance->registerPeering(mPeering, mRemotePeering, mSecret, this);
		
		if(Core::Instance->hasPeer(mRemotePeering))	// the user is local
		{
			Address addr("127.0.0.1", Config::Get("port"));
			try {
				Socket *sock = new Socket(addr);
				Core::Instance->addPeer(sock, mPeering);
			}
			catch(...)
			{
				Log("AddressBook::Contact", "WARNING: Unable to connect the local core");	 
			}
		}
		else {
			Log("AddressBook::Contact", "Querying tracker " + mTracker);	
			if(AddressBook::query(mPeering, mTracker, mAddrs))
			{
				for(int i=0; i<mAddrs.size(); ++i)
				{
					const Address &addr = mAddrs[mAddrs.size()-(i+1)];
					unlock();
					try {
						Socket *sock = new Socket(addr, 1000);	// TODO: timeout
						Core::Instance->addPeer(sock, mPeering);
					}
					catch(...)
					{
						
					}
					lock();
				}
			}
			
			Log("AddressBook::Contact", "Publishing to tracker " + mTracker);
			AddressBook::publish(mRemotePeering);
		}
	}
	
	if(!mMessages.empty())
	{
		time_t t = time(NULL);
		while(!mMessages.front().isRead() 
			&& mMessages.front().time() >= t + 7200)	// 2h
		{
				 mMessages.pop_front();
		}
	} 
}

void AddressBook::Contact::message(Message *message)
{
	Synchronize(this);
	
	Assert(message);
	Assert(message->receiver() == mPeering);
	mMessages.push_back(*message);
	++mMessagesCount;
	notifyAll();
}

void AddressBook::Contact::request(Request *request)
{
	Assert(request);
	Store *store = mAddressBook->user()->store();
	request->execute(store);
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
  	Synchronize(this);
	
	String base(prefix+request.url);
	base = base.substr(base.lastIndexOf('/')+1);
	if(!base.empty()) base+= '/';
	
	try {
		if(request.url.empty() || request.url == "/")
		{
			Http::Response response(request,200);
			response.send();
				
			Html page(response.sock);
			page.header("Contact: "+mName);
			page.open("h1");
			page.text("Contact: "+mName);
			page.close("h1");

			page.text("Secret: " + mSecret.toString()); page.br();
			page.text("Peering: " + mPeering.toString()); page.br();
			page.text("Remote peering: " + mRemotePeering.toString()); page.br();
			page.br();
			page.br();
			
			page.link(prefix+"/files/","Files");
			page.br();
			page.link(prefix+"/chat/","Chat");
			page.br();
			
			page.footer();
			return;
		}
		else {
			String url = request.url;
			String directory = url;
			directory.ignore();		// remove first '/'
			url = "/" + directory.cut('/');
			if(directory.empty()) throw 404;
			  
			if(directory == "files")
			{
				String target(url);
				if(target.size() > 1 && target[target.size()-1] == '/') 
					target = target.substr(0, target.size()-1);
				
				Http::Response response(request,200);
				response.send();	
				
				Html page(response.sock);
				page.header("Files: "+mName);
				page.open("h1");
				page.text("Files: "+mName);
				page.close("h1");
				
				Request request(target);
				try {
					request.submit(mPeering);
					request.wait();
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", "Cannot send request, peer not connected");
					page.text("Not connected...");
					page.footer();
					return;
				}
				
				if(request.responsesCount() > 0)
				{
					Request::Response *response = request.response(0);
					StringMap parameters = response->parameters();
					
					if(!response->content()) page.text("No content...");
					else try {
						if(parameters.contains("type") && parameters["type"] == "directory")
						{
							Map<String, StringMap> files;
						  	StringMap info;
							while(response->content()->read(info))
							{
							 	if(!info.contains("type")) break;
								if(info.get("type") == "directory") files.insert("0"+info.get("name"),info);
								else files.insert("1"+info.get("name"),info);
							}
							
							page.open("table");
							for(Map<String, StringMap>::iterator it = files.begin();
								it != files.end();
								++it)
							{
							  	StringMap &info = it->second;
								
								page.open("tr");
								page.open("td"); 
								if(info.get("type") == "directory") page.link(base + info.get("name"), info.get("name"));
								else page.link("/" + info.get("hash"), info.get("name"));
								page.close("td");
								page.open("td"); 
								if(info.get("type") == "directory") page.text("directory");
								else page.text(String::hrSize(info.get("size")));
								page.close("td");
								page.open("tr");
							}

							page.close("table");
						}
						else {
							page.text("Download ");
							page.link("/" + parameters.get("hash"), parameters.get("name"));
							page.br();
						}
					}
					catch(const Exception &e)
					{
						Log("AddressBook::Contact::http", String("Unable to list files: ") + e.what());
						page.text("Error, unable to list files");
					}
				}
				
				page.footer();
				return;
			}
			else if(directory == "chat")
			{
				if(url != "/")
				{
				  	url.ignore();
					unsigned count = 0;
					try { url>>count; }
					catch(...) { throw 404; }
					
					Http::Response response(request,200);
					response.send();
					
					if(count == mMessagesCount)
						wait(60000);
					
					if(count < mMessagesCount && mMessagesCount-count <= mMessages.size())
					{
						Html html(response.sock);
						int i = mMessages.size() - (mMessagesCount-count);
						messageToHtml(html, mMessages[i]);
						mMessages[i].markRead();
					}
					
					return;
				}
			  
				if(request.method == "POST")
				{
					if(request.post.contains("message") && !request.post["message"].empty())
					{
						try {
							Message message(request.post["message"]);
							mMessages.push_back(message);	// copy stored now so receiver is null
							++mMessagesCount;
							notifyAll();
							
							message.send(mPeering);	// send
							
							if(request.post["ajax"].toBool())	//ajax
							{
								Http::Response response(request, 200);
								response.send();
								/*Html html(response.sock);
								messageToHtml(html, mMessages.back());
								mMessages.back().markRead();*/
							}
							else {	// form submit
							 	Http::Response response(request, 303);
								response.headers["Location"] = prefix + "/chat";
								response.send();
							}
						}
						catch(...)
						{
							throw 400;
						}
						
						return;
					}
				}
			  
				Http::Response response(request,200);
				response.send();	
				
				Html page(response.sock);
				page.header("Chat: "+mName);
				page.open("h1");
				page.text("Chat: "+mName);
				page.close("h1");
								
				page.openForm(prefix + "/chat", "post", "chatform");
				page.input("text","message");
				page.button("send","Send");
				page.br();
				page.br();
				page.closeForm();
						
				page.open("div", "chat");
				for(int i=mMessages.size()-1; i>=0; --i)
				{
	  				messageToHtml(page, mMessages[i]);
					mMessages[i].markRead();
				}
				page.close("div");
				
*page.stream()<<"<script type=\"text/javascript\">\n\
	var count = "+String::number(mMessagesCount)+";\n\
	function update()\n\
	{\n\
		var xhr = createXMLHttpRequest();\n\
		xhr.onreadystatechange = function()\n\
		{\n\
  			if(xhr.readyState == 4)\n\
			{\n\
				if(xhr.status === 200 && xhr.responseText)\n\
				{\n\
					var content = document.getElementById('chat').innerHTML;\n\
					document.getElementById('chat').innerHTML = xhr.responseText + content;\n\
					count+= 1;\n\
					setTimeout('update()', 100);\n\
				}\n\
				else setTimeout('update()', 1000);\n\
			}\n\
		}\n\
		xhr.open('GET', '"+prefix+"/chat/'+count, true);\n\
		xhr.send();\n\
	}\n\
	function post()\n\
	{\n\
		var message = document.chatform.message.value;\n\
		if(!message) return false;\n\
		document.chatform.message.value = '';\n\
		var xhr = createXMLHttpRequest();\n\
		xhr.onreadystatechange = function()\n\
		{\n\
  			if(xhr.readyState == 4 && xhr.status != 200)\n\
			{\n\
				alert('The message could not be sent. Is this user online ?');\n\
			}\n\
		}\n\
		xhr.open('POST', '"+prefix+"/chat', true);\n\
		xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');\n\
		xhr.send('message='+encodeURIComponent(message)+'&ajax=1');\n\
	}\n\
	setTimeout('update()', 1000);\n\
	document.chatform.onsubmit = function() {post(); return false;}\n\
</script>\n";
				
				page.footer();
				return;
			}
		}
	}
	catch(const Exception &e)
	{
		Log("AddressBook::Contact::http", e.what());
		throw 500;
	}
	
	throw 404;
}

void AddressBook::Contact::messageToHtml(Html &html, const Message &message) const
{
	char buffer[64];
	time_t t = message.time();
	std::strftime (buffer, 64, "%x %X", localtime(&t));
	html.open("span",".message");
	html.open("span",".date");
	html.text(buffer);
	html.close("span");
	html.text(" ");
	html.open("span",".user");
	if(message.receiver() == Identifier::Null) html.text(mAddressBook->userName());
	else html.text(mAddressBook->getContact(message.receiver())->name());
	html.close("span");
	html.text(": " + message.content());
	html.close("span");
	html.br(); 
}

void AddressBook::Contact::serialize(Stream &s) const
{
	Synchronize(this);
	
	StringMap map;
	map["uname"] << mUniqueName;
	map["name"] << mName;
	map["tracker"] << mTracker;
	map["secret"] << mSecret;
	map["peering"] << mPeering;
	map["remotePeering"] << mRemotePeering;
		
	s.write(map);
	s.write(mAddrs);
}

void AddressBook::Contact::deserialize(Stream &s)
{
	Synchronize(this);
	
	if(!mUniqueName.empty())
		Interface::Instance->remove(urlPrefix());
	
	mUniqueName.clear();
  	mName.clear();
	mTracker.clear();
	mSecret.clear();
	mPeering.clear();
	mRemotePeering.clear();
	
	StringMap map;
	AssertIO(s.read(map))
	AssertIO(!map.empty());
	
	map["uname"] >> mUniqueName;
	map["name"] >> mName;
	map["tracker"] >> mTracker;
	map["secret"] >> mSecret;
	map["peering"] >> mPeering;
	map["remotePeering"] >> mRemotePeering;
	
	s.read(mAddrs);
	
	// TODO: checks
	
	Interface::Instance->add(urlPrefix(), this);
}

}
