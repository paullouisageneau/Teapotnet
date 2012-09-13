/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
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

namespace arc
{

AddressBook::AddressBook(User *user)
{
	Assert(user != NULL);
	mName = user->name();
	mFileName = user->profilePath() + "contacts";
	
	Interface::Instance->add("/"+mName+"/contacts", this);
	
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
  	Interface::Instance->remove("/"+mName+"/contacts");
}

const String &AddressBook::name(void) const
{
 	return mName; 
}

const Identifier &AddressBook::addContact(String name, const ByteString &secret)
{
	synchronize(this);

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
		throw Exception("Contact already exists");
	}
	
	mContacts.insert(contact->peering(), contact);
	mContactsByUniqueName.insert(contact->uniqueName(), contact);
	
	autosave();
	notify();
	return contact->peering();
}

void AddressBook::removeContact(const Identifier &peering)
{
	synchronize(this);
	
	Contact *contact;
	if(mContacts.get(peering, contact))
	{
		Core::Instance->unregisterPeering(peering);
		mContactsByUniqueName.erase(contact->uniqueName());
 		mContacts.erase(peering);
		delete contact;
		autosave();
	}
}

const AddressBook::Contact *AddressBook::getContact(const Identifier &peering)
{
	return mContacts.get(peering);  
}

void AddressBook::load(Stream &stream)
{
	synchronize(this);
	
	Contact *contact = new Contact(this);
	while(stream.read(*contact))
	{
		mContacts.insert(contact->peering(), contact);
	}	
}

void AddressBook::save(Stream &stream) const
{
	synchronize(this);
  
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		stream.write(*contact);
	}	
}

void AddressBook::autosave(void) const
{
	synchronize(this);
  
	SafeWriteFile file(mFileName);
	save(file);
	file.close();
}

void AddressBook::update(void)
{
	synchronize(this);
	Log("AddressBook::update", "Updating " + String::number(unsigned(mContacts.size())) + " contacts");
	
	for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		Contact *contact = it->second;
		contact->update();
	}
		
	Log("AddressBook::update", "Finished");
	autosave();
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	synchronize(this);
	
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

			page.open("h2");
			page.text("Add new contact");
			page.close("h2");
			page.openForm(prefix+"/","post");
			page.text("Name");
			page.input("text","name");
			page.br();
			page.text("Secret");
			page.input("text","secret");
			page.br();
			page.button("Add contact");
 			page.br();
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

bool AddressBook::publish(const Identifier &remotePeering)
{
	try {
		String url("http://" + Config::Get("tracker") + "/" + remotePeering.toString());
		
		StringMap post;
		post["port"] = Config::Get("port");
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
	  	if(tracker.empty()) url = "http://" + Config::Get("tracker") + "/" + peering.toString();
		else url = "http://" + tracker + "/" + peering.toString();
  
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
	agregate.writeLine(mAddressBook->name());
	agregate.writeLine(mName);
	Sha512::Hash(agregate, mPeering, Sha512::CryptRounds);
	
	// Compute Remote peering
	agregate.clear();
	agregate.writeLine(mSecret);
	agregate.writeLine(mName);
	agregate.writeLine(mAddressBook->name());
	Sha512::Hash(agregate, mRemotePeering, Sha512::CryptRounds);
	
	Interface::Instance->add("/"+mName+"/contacts/"+mUniqueName, this);
}

AddressBook::Contact::Contact(AddressBook *addressBook) :
  	mAddressBook(addressBook)
{
  
}

AddressBook::Contact::~Contact(void)
{
  	Interface::Instance->add("/"+mName+"/contacts/"+mUniqueName, this);
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

void AddressBook::Contact::update(void)
{
	synchronize(this);

	if(!Core::Instance->hasPeer(mPeering))
	{
		Core::Instance->registerPeering(mPeering, mRemotePeering, mSecret, this);
		  
		Log("AddressBook::Contact", "Querying tracker " + mTracker);
		
		if(AddressBook::query(mPeering, mTracker, mAddrs))
		{
			for(int i=0; i<mAddrs.size(); ++i)
			{
				const Address &addr = mAddrs[mAddrs.size()-(i+1)];
				unlock();
				try {
					Socket *sock = new Socket(addr);
					Core::Instance->addPeer(sock, mPeering);
				}
				catch(...)
				{
					 
				}
				lock();
			}
		}
	}
	
	AddressBook::publish(mRemotePeering);
		
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

void AddressBook::Contact::message(const Message &message)
{
	synchronize(this);
	
	Assert(message.receiver() == mPeering);
	mMessages.push_back(message); 
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
  	synchronize(this);
	
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
			page.link(prefix+"/chat/","Chat");
			
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
				
				for(int i=0; i<request.responsesCount(); ++i)
				{
					Request::Response *response = request.response(i);
					StringMap parameters = response->parameters();
					
					if(!response->content()) page.text("No content...");
					else try {
						if(parameters.contains("type") && parameters["type"] == "directory")
						{
							StringMap info;
							while(response->content()->read(info))
							{
								if(info.get("type") == "directory") page.link(base + info.get("name"), info.get("name"));
								else page.link("/" + info.get("hash"), info.get("name"));
								page.br();
							}
						}
						else {
							page.text("Download ");
							page.link("/" + parameters.get("hash"), parameters.get("name"));
							page.br();
						}
					}
					catch(...)
					{

					}
				}
				
				page.footer();
				return;
			}
			else if(directory == "chat")
			{
				if(url != "/") throw 404;
			  
				if(request.method == "POST")
				{
					try {
						Message message(request.post.get("message"));
						mMessages.push_back(message);	// copy stored now so receiver is null
						message.send(mPeering);
					}
					catch(...)
					{
						throw 400;
					}				
					
					Http::Response response(request, 303);
					response.headers["Location"] = prefix + "/chat";
					response.send();
					return;
				}
			  
				Http::Response response(request,200);
				response.send();	
				
				Html page(response.sock);
				page.header("Chat: "+mName);
				page.open("h1");
				page.text("Chat: "+mName);
				page.close("h1");
			  
				for(int i=0; i<mMessages.size(); ++i)
				{
					char buffer[64];
					time_t t = mMessages[i].time();
					std::strftime (buffer, 64, "%x %X", localtime(&t));
	  
					page.open("span",".message");
					page.open("span",".date");
					page.text(buffer);
					page.close("span");
					page.text(" ");
					page.open("span",".user");
					if(mMessages[i].receiver() == Identifier::Null) page.text(mAddressBook->name());
					else page.text(mAddressBook->getContact(mMessages[i].receiver())->name());
					page.close("span");
					page.text(" " + mMessages[i].content());
					page.close("span");
					page.br();
					
					mMessages[i].markRead();
				}
				
				page.openForm(prefix + "/chat", "post");
				page.input("text","message");
				page.button("Envoyer");
				page.br();
				page.closeForm();
				
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

void AddressBook::Contact::serialize(Stream &s) const
{
	synchronize(this);
	
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
	synchronize(this);
	
	mUniqueName.clear();
  	mName.clear();
	mTracker.clear();
	mSecret.clear();
	mPeering.clear();
	mRemotePeering.clear();
	
	StringMap map;
	s.read(map);
	map["uname"] >> mUniqueName;
	map["name"] >> mName;
	map["tracker"] >> mTracker;
	map["secret"] >> mSecret;
	map["peering"] >> mPeering;
	map["remotePeering"] >> mRemotePeering;
	
	s.read(mAddrs);
}

}
