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
#include "yamlserializer.h"
#include "jsonserializer.h"
#include "portmapping.h"
#include "mime.h"

namespace tpot
{

AddressBook::AddressBook(User *user) :
	mUser(user)
{
	Assert(mUser != NULL);
	mFileName = mUser->profilePath() + "contacts";
	
	Interface::Instance->add("/"+mUser->name()+"/contacts", this);
	
	if(File::Exist(mFileName))
	{
		try {
			File file(mFileName, File::Read);
			load(file);
			file.close();
		}
		catch(const Exception &e)
		{
			Log("AddressBook", String("Loading failed: ") + e.what());
		}
	}
}

AddressBook::~AddressBook(void)
{
  	Interface::Instance->remove("/"+mUser->name()+"/contacts");
	
	clear();
}

User *AddressBook::user(void) const
{
 	return mUser; 
}

String AddressBook::userName(void) const
{
 	return mUser->name(); 
}

int AddressBook::unreadMessagesCount(void) const
{
	Synchronize(this);
  
	int count = 0;
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		count+= contact->unreadMessagesCount();
	}
	return count;
}

Identifier AddressBook::addContact(String name, const ByteString &secret)
{
	Synchronize(this);

	String tracker = name.cut('@');
	if(tracker.empty()) tracker = Config::Get("tracker");
	
	String uname = name;
	unsigned i = 1;
	while(mContactsByUniqueName.contains(uname) || uname == userName())	// userName reserved for self
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
	Interface::Instance->add(contact->urlPrefix(), contact);
	
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

AddressBook::Contact *AddressBook::getContact(const Identifier &peering)
{
	Synchronize(this);
  
	Contact *contact;
	if(mContacts.get(peering, contact)) return contact;
	else return NULL;
}

const AddressBook::Contact *AddressBook::getContact(const Identifier &peering) const
{
	Synchronize(this);
  
	Contact *contact;
	if(mContacts.contains(peering)) return mContacts.get(peering);
	else return NULL;
}

void AddressBook::getContacts(Array<AddressBook::Contact *> &array)
{
	Synchronize(this); 
  
	mContactsByUniqueName.getValues(array);
	Contact *self = getSelf();
	if(self) array.remove(self);
}

Identifier AddressBook::setSelf(const ByteString &secret)
{
	Synchronize(this);
  
	const String tracker = Config::Get("tracker");
	
	Contact *self = getSelf();
	if(self) removeContact(self->peering());
	
	self = new Contact(this, userName(), userName(), tracker, secret);
	if(mContacts.contains(self->peering())) 
	{
		delete self;
		throw Exception("a contact with the same peering already exists");
	}
	
	mContacts.insert(self->peering(), self);
	mContactsByUniqueName.insert(userName(), self);
	Interface::Instance->add(self->urlPrefix(), self);
	
	save();
	start();
	return self->peering();
}

AddressBook::Contact *AddressBook::getSelf(void)
{
	Synchronize(this);
	
	Contact *contact;
	if(mContactsByUniqueName.get(userName(), contact)) return contact;
	else return NULL;
}

const AddressBook::Contact *AddressBook::getSelf(void) const
{
	Synchronize(this);
	
	Contact *contact;
	if(mContactsByUniqueName.get(userName(), contact)) return contact;
	else return NULL;
}

void AddressBook::clear(void)
{
	Synchronize(this);
  
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		delete contact;
	} 
	
	mContacts.clear();
	mContactsByUniqueName.clear();
}

void AddressBook::load(Stream &stream)
{
	Synchronize(this);

	bool changed = false;

	Contact *contact = new Contact(this);
	
	//YamlSerializer serializer(&stream);
	//while(serializer.input(*contact))
	while(true)
	{
		// Not really clean but protect from parse errors propagation
		YamlSerializer serializer(&stream);
		if(!serializer.input(*contact)) break;

		Contact *oldContact = NULL;
		if(mContactsByUniqueName.get(contact->uniqueName(), oldContact))
		{
			if(oldContact->time() >= contact->time()) continue;
			contact->addAddresses(oldContact->addresses());
			delete oldContact;
		}

		mContacts.insert(contact->peering(), contact);
		mContactsByUniqueName.insert(contact->uniqueName(), contact);
		Interface::Instance->add(contact->urlPrefix(), contact);
		changed = true;
		
		contact = new Contact(this);
	}
	delete contact;
	
	if(changed) start();
}

void AddressBook::save(Stream &stream) const
{
	Synchronize(this);
  
	YamlSerializer serializer(&stream);
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		serializer.output(*contact);
	}	
}

void AddressBook::save(void) const
{
	Synchronize(this);
  
	String data;
	save(data);
	
	SafeWriteFile file(mFileName);
	file.write(data);
	file.close();
	
	const Contact *self = getSelf();
	if(self && self->isConnected())
	{
		try {
			Message message(data);
			message.setParameter("type", "contacts");
			message.send(self->peering());
		}
		catch(Exception &e)
		{
			Log("AddressBook::Save", String("Contacts synchronization failed: ") + e.what()); 
		}
	}
}

void AddressBook::update(void)
{
	Synchronize(this);
	//Log("AddressBook::update", "Updating " + String::number(unsigned(mContacts.size())) + " contacts");
	
	Array<Identifier> keys;
	mContacts.getKeys(keys);
	
	for(int i=0; i<keys.size(); ++i)
	{
		Contact *contact = NULL;
		if(mContacts.get(keys[i], contact))
		{
			Desynchronize(this);
			Assert(contact);
			contact->update();
		}
	}
		
	//Log("AddressBook::update", "Finished");
	save();
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);

	try {
		user()->setOnline();
		
		if(request.url.empty() || request.url == "/")
		{
			if(request.method == "POST")
			{
				try {
			  		String command = request.post["command"];
			  		if(command == "delete")
					{
				  		Identifier peering;
						request.post["argument"] >> peering;
						
						removeContact(peering);
					}
					else {
						String name, csecret;
						name = request.post["name"];
						csecret = request.post["secret"];
					  
						if(name.empty() || csecret.empty()) throw 400;
						
						ByteString secret;
						Sha512::Hash(csecret, secret, Sha512::CryptRounds);
						
						if(request.post.contains("self")) setSelf(secret);
						else addContact(name, secret);
					}
				}
				catch(const Exception &e)
				{
					Log("AddressBook::http", String("Error: ") + e.what());
					throw 400;
				}				
				
				Http::Response response(request, 303);
				response.headers["Location"] = prefix + "/";
				response.send();
				return;
			}
			
			if(request.get.contains("json"))
			{
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();

				JsonSerializer json(response.sock);
				json.outputMapBegin();
				for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
					it != mContacts.end();
					++it)
				{
					Contact *contact = it->second;
	
					StringMap map;
					map["name"] << contact->name();
					map["tracker"] << contact->tracker();
					map["status"] << contact->status();
					map["messages"] << contact->unreadMessagesCount();
					
					json.outputMapElement(contact->uniqueName(), map);
				}
				json.outputMapEnd();
				return;
			}
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header("Contacts");
			
			Array<Contact*> contacts;
			getContacts(contacts);
			if(!contacts.empty())
			{
				page.open("div",".box");
				
				page.openForm(prefix+"/", "post", "executeForm");
				page.input("hidden", "command");
				page.input("hidden", "argument");
				page.closeForm();
				
				page.javascript("function deleteContact(name, identifier) {\n\
					if(confirm('Do you really want to delete '+name+' ?')) {\n\
						document.executeForm.command.value = 'delete';\n\
						document.executeForm.argument.value = identifier;\n\
						document.executeForm.submit();\n\
					}\n\
				}");
				
				page.open("table",".contacts");
				for(int i=0; i<contacts.size(); ++i)
				{
					Contact *contact = contacts[i];
					String contactUrl = prefix + '/' + contact->uniqueName() + '/';
					
					page.open("tr");
					page.open("td",".contact");
					page.link(contact->urlPrefix(), contact->name());
					page.close("td");
					page.open("td",".tracker");
					page.text(String("@") + contact->tracker());
					page.close("td");
					page.open("td",".checksum");
					page.text(String(" check: ")+String::hexa(contact->peeringChecksum(),8));
					page.close("td");
					page.open("td",".delete");
					page.openLink("javascript:deleteContact('"+contact->name()+"','"+contact->peering().toString()+"')");
					page.image("/delete.png", "Delete");
					page.closeLink();
					page.close("td");
					page.close("tr");
				}
				page.close("table");
				page.close("div");
			}
			
			page.openForm(prefix+"/","post");
			page.openFieldset("New contact");
			page.label("name","Name"); page.input("text","name"); page.br();
			page.label("secret","Secret"); page.input("text","secret"); page.br();
			page.label("add"); page.button("add","Add contact");
			page.closeFieldset();
			page.closeForm();
			
			page.openForm(prefix+"/","post");
			page.openFieldset("Personal secret");
			page.input("hidden","name",userName());
			page.input("hidden","self","true");
			if(getSelf()) page.text("Your personal secret is already set, but you can change it here.");
			else page.text("Set the same username and the same personal secret on multiple devices to enable automatic synchronization.");
			page.br();
			page.br();
			page.label("secret","Secret"); page.input("text","secret"); page.br();
			page.label("add"); page.button("add","Set secret");
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
		String url("http://" + Config::Get("tracker") + "/tracker?id=" + remotePeering.toString());
		
		List<Address> list;
		Config::GetExternalAddresses(list);
		
		String addresses;
		for(	List<Address>::iterator it = list.begin();
			it != list.end();
			++it)
		{
			if(!addresses.empty()) addresses+= ',';
			addresses+= it->toString();
		}
		
		StringMap post;
		post["instance"] = Core::Instance->getName();
		post["port"] = Config::Get("port");
		post["addresses"] = addresses;
		
		if(!Core::Instance->isPublicConnectable())
		{
			list.clear();
			Core::Instance->getKnownPublicAdresses(list);
			
			String altAddresses;
			for(	List<Address>::iterator it = list.begin();
				it != list.end();
				++it)
			{
				if(!altAddresses.empty()) altAddresses+= ',';
				altAddresses+= it->toString();
			}
			
			post["alternate"] = altAddresses;
		}
		
		if(Http::Post(url, post) != 200) return false;
	}
	catch(const std::exception &e)
	{
		Log("AddressBook::publish", e.what()); 
		return false;
	}
	return true;
}

bool AddressBook::query(const Identifier &peering, const String &tracker, AddressMap &output, bool alternate)
{
	output.clear();
  
	try {
	  	String url;
	  	if(tracker.empty()) url = "http://" + Config::Get("tracker") + "/tracker?id=" + peering.toString();
		else url = "http://" + tracker + "/tracker?id=" + peering.toString();
  		if(alternate) url+= "&alternate=1";
		  
		String tmp;
		if(Http::Get(url, &tmp) != 200) return false;
		tmp.trim();
		if(tmp.empty()) return false;
	
		YamlSerializer serializer(&tmp);
		typedef SerializableMap<String, SerializableArray<Address> > content_t;
		content_t content;
		
		while(true)
		try {
			serializer.input(content);
			break;
		}
		catch(const InvalidData &e)
		{
			 
		}
		
		for(content_t::const_iterator it = content.begin();
			it != content.end();
			++it)
		{
			const String &instance = it->first;
			const Array<Address> &addrs = it->second;
			for(int i=0; i<addrs.size(); ++i)
				output[instance].insert(addrs[i], Time::Now());
		}
		
		return !output.empty();
	}
	catch(const std::exception &e)
	{
		Log("AddressBook::query", e.what()); 
		return false;
	}
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
	mSecret(secret),
	mTime(Time::Now()),
	mFound(false)
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
}

AddressBook::Contact::Contact(AddressBook *addressBook) :
  	mAddressBook(addressBook),
	mMessagesCount(0)
{
  
}

AddressBook::Contact::~Contact(void)
{
	Interface::Instance->remove(urlPrefix(), this);
}

String AddressBook::Contact::uniqueName(void) const
{
	Synchronize(this);
	return mUniqueName;
}

String AddressBook::Contact::name(void) const
{
	Synchronize(this);
	return mName;
}

String AddressBook::Contact::tracker(void) const
{
	Synchronize(this);
	return mTracker;
}

Identifier AddressBook::Contact::peering(void) const
{
	Synchronize(this);
	return mPeering;
}

Identifier AddressBook::Contact::remotePeering(void) const
{
	Synchronize(this);
	return mRemotePeering;
}

Time AddressBook::Contact::time(void) const
{
	Synchronize(this);
	return mTime; 
}

uint32_t AddressBook::Contact::peeringChecksum(void) const
{
	Synchronize(this);
	return mPeering.getDigest().checksum32() + mRemotePeering.getDigest().checksum32(); 
}

String AddressBook::Contact::urlPrefix(void) const
{
	Synchronize(this);
	if(mUniqueName.empty()) return "";
	return String("/")+mAddressBook->userName()+"/contacts/"+mUniqueName;
}

int AddressBook::Contact::unreadMessagesCount(void) const
{
	Synchronize(this);

	int count = 0;
	for(int i=mMessages.size()-1; i>=0; --i)
	{
		if(mMessages[i].isRead()) break;
		++count;
	}
	return count;
}

bool AddressBook::Contact::isFound(void) const
{
	return mFound;
}

bool AddressBook::Contact::isConnected(void) const
{
	Synchronize(this);
	return Core::Instance->hasPeer(mPeering); 
}

bool AddressBook::Contact::isConnected(const String &instance) const
{
	Synchronize(this);
	return Core::Instance->hasPeer(Identifier(mPeering, instance)); 
}

bool AddressBook::Contact::isOnline(void) const
{
	Synchronize(this);
	
	if(!isConnected()) return false;
	if(!mInfo.contains("last")) return false;
	return (Time::Now()-Time(mInfo.get("last")) < 60);	// 60 sec
}

String AddressBook::Contact::status(void) const
{
	Synchronize(this);
	
	if(isOnline()) return "online";
	else if(isConnected()) return "connected";
	else if(isFound()) return "found";
	else return "disconnected";
}

AddressBook::AddressMap AddressBook::Contact::addresses(void) const
{
	Synchronize(this);
	return mAddrs;
}

bool AddressBook::Contact::addAddresses(const AddressMap &map)
{
	Synchronize(this);
  
	for(AddressMap::const_iterator it = map.begin();
		it != map.end();
		++it)
	{
		const String &instance = it->first;
		const AddressBlock &block = it->second;
		mAddrs.insert(instance, block);
	}
	
	return true;
}

bool AddressBook::Contact::connectAddress(const Address &addr, const String &instance, bool save)
{
 	Synchronize(this);
	
	if(addr.isNull()) return false;
	if(instance == Core::Instance->getName()) return false;
	
	//Log("AddressBook::Contact", "Connecting " + instance + " on " + addr.toString() + "...");
	
	Identifier identifier(mPeering, instance);
	try {
		Desynchronize(this);
		Socket *sock = new Socket(addr, 1000);	// TODO: timeout
		if(Core::Instance->addPeer(sock, identifier))
		{
			if(save) SynchronizeStatement(this, mAddrs[instance][addr] = Time::Now());	
			return true;
		}
		
		// A node is running at this address but the user does not exist
		SynchronizeStatement(this, if(mAddrs.contains(instance)) mAddrs[instance].erase(addr));
	}
	catch(...)
	{

	}

	return false; 
}

bool AddressBook::Contact::connectAddresses(const AddressMap &map, bool save)
{
	Synchronize(this);
  
	bool success = false;
	for(AddressMap::const_iterator it = map.begin();
		it != map.end();
		++it)
	{
		const String &instance = it->first;
		const AddressBlock &block = it->second;
	  
		if(instance == Core::Instance->getName()) continue;

	  	// TODO: look for a better address than the already connected one
		if(isConnected(instance)) 
		{
			// TODO: update time for currenly connected address
			success = true;
			continue;
		}
	  
		//Log("AddressBook::Contact", "Connecting instance " + instance + " for " + mName + " (" + String::number(block.size()) + " address(es))...");

		for(AddressBlock::const_reverse_iterator jt = block.rbegin();
			jt != block.rend();
			++jt)
		{
			if(connectAddress(jt->first, instance, save))
			{
				success = true;
				break;
			}
		}
			
	}
	
	return success;
}

void AddressBook::Contact::update(void)
{
	Synchronize(this);
        
	//Log("AddressBook::Contact", "Looking for " + mUniqueName);
	Core::Instance->registerPeering(mPeering, mRemotePeering, mSecret, this);
		
	if(mPeering != mRemotePeering && Core::Instance->hasRegisteredPeering(mRemotePeering))	// the user is local
	{
		Identifier identifier(mPeering, Core::Instance->getName());
		if(!Core::Instance->hasPeer(identifier))
		{
			Log("AddressBook::Contact", mUniqueName + " found locally");
			  
			Address addr("127.0.0.1", Config::Get("port"));
			try {
				Desynchronize(this);
				Socket *sock = new Socket(addr);
				Core::Instance->addPeer(sock, identifier);
			}
			catch(...)
			{
				Log("AddressBook::Contact", "Warning: Unable to connect the local core");	 
			}
		}
	}
	
	//Log("AddressBook::Contact", "Publishing to tracker " + mTracker + " for " + mUniqueName);
	DesynchronizeStatement(this, AddressBook::publish(mRemotePeering));
		  
	//Log("AddressBook::Contact", "Querying tracker " + mTracker + " for " + mUniqueName);	
	
	AddressMap newAddrs;
	DesynchronizeStatement(this, AddressBook::query(mPeering, mTracker, newAddrs, false));
	if(!newAddrs.empty())
	{
		//Log("AddressBook::Contact", "Contact " + mName + " found (" + String::number(newAddrs.size()) + " instance(s))");
		mFound = true;	
		connectAddresses(newAddrs, true);
	}
	else {
		mFound = false;
		connectAddresses(mAddrs, true);
	}
	
	//if(!Core::Instance->isPublicConnectable())	// Can actually be connectable with IPv6 only
	//{
		AddressMap altAddrs;
		DesynchronizeStatement(this, AddressBook::query(mPeering, mTracker, altAddrs, true));
		if(!altAddrs.empty())
		{
			//Log("AddressBook::Contact", "Alternative addresses for " + mName + " found (" + String::number(altAddrs.size()) + " instance(s))");
			
			if(!mFound) Log("AddressBook::Contact", "Warning: Contact " + mName + " registered alternative addresses but no direct addresses !"); 
			
			mFound = true;
			connectAddresses(altAddrs, false);
		}
	//}
	
	AddressMap::iterator it = mAddrs.begin();
	while(it != mAddrs.end())
	{
		const String &instance = it->first;
		AddressBlock &block = it->second;
		AddressBlock::iterator jt = block.begin();
		while(jt != block.end())
		{
			if(Time::Now() - jt->second >= 3600*24*8)	// 8 days
				block.erase(jt++);
			else jt++;
		}
		
		if(block.empty())
			mAddrs.erase(it++);
		else it++;
	}
	
	if(!mMessages.empty())
        {
                while(!mMessages.front().isRead()
                        && Time::Now() - mMessages.front().time() >= 7200)        // 2h
                {
                                 mMessages.pop_front();
                }
        }
}

void AddressBook::Contact::welcome(const Identifier &peering)
{
	Synchronize(this);
	Assert(peering == mPeering);	

	// Send info
	mAddressBook->user()->sendInfo(peering);

	// Send contacts if self
        if(mUniqueName == mAddressBook->userName())
        {
                String data;
        	mAddressBook->save(data);
                Message message(data);
                message.setParameter("type", "contacts");
                message.send(peering);
	}
}

void AddressBook::Contact::message(Message *message)
{
	Synchronize(this);
	
	Assert(message);
	Assert(message->receiver() == mPeering);
	
	String type;
	message->parameters().get("type", type);
	
	if(type.empty() || type == "text")
	{
		mMessages.push_back(*message);
		++mMessagesCount;
		notifyAll();
	}
	else if(type == "info")
	{
		String data = message->content();
		YamlSerializer serializer(&data);
		StringMap info;
		serializer.input(info);
		
		// TODO: variables for time and last
		
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
			info["last"] = last;
			mInfo = info;
			
			if(mUniqueName == mAddressBook->userName())
			{
				Desynchronize(this);
				mAddressBook->user()->setInfo(info);
			}
		}
		else {
			mInfo["last"] = last;
		}
	}
	else if(type == "contacts")
	{
		if(mUniqueName != mAddressBook->userName())
		{
			Log("AddressBook::Contact::message", "Warning: received contacts update from other than self, dropping");
			return;
		}
		
		String data = message->content();
		mAddressBook->load(data);
	}
}

void AddressBook::Contact::request(Request *request)
{
	Assert(request);
	request->execute(mAddressBook->user());
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	
	try {
		mAddressBook->user()->setOnline();
		
		String base(prefix+request.url);
		base = base.substr(base.lastIndexOf('/')+1);
		if(!base.empty()) base+= '/';
	
		if(request.url.empty() || request.url == "/")
		{
			Http::Response response(request,200);
			response.send();

			Html page(response.sock);
			page.header("Contact: "+mName);
				
			page.open("div",".menu");
				
			page.span("Status:", ".title");
			page.open("span", "status.status");
			page.span(status().capitalized(), String(".")+status());
			page.close("span");
			page.br();
			page.br();
			
			page.link(prefix+"/files/","Files");
			page.br();
			page.link(prefix+"/search/","Search");
			page.br();
			page.openLink(prefix+"/chat/");
			page.text("Chat");
			page.open("span", "messagescount.messagescount");
			int msgcount = unreadMessagesCount();
			if(msgcount) page.text(String(" (")+String::number(msgcount)+String(")"));
			page.close("span");
			page.closeLink();
			page.br();
			page.close("div");
			
			unsigned refreshPeriod = 5000;
			page.javascript("function updateContact() {\n\
				$.ajax({\n\
					url: '/"+mAddressBook->userName()+"/contacts/?json',\n\
					dataType: 'json',\n\
					timeout: 2000,\n\
					success: function(data) {\n\
						var info = data."+uniqueName()+";\n\
						transition($('#status'),\n\
							'<span class=\"'+info.status+'\">'+info.status.capitalize()+'</span>\\n');\n\
						var msg = '';\n\
						if(info.messages != 0) msg = ' ('+info.messages+')';\n\
						transition($('#messagescount'), msg);\n\
					}\n\
				});\n\
				setTimeout('updateContact()',"+String::number(refreshPeriod)+");\n\
			}\n\
			setTimeout('updateContact()',100);");
			
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
				
				bool isFile = request.get.contains("file");
				
				if(isFile)
				{
					while(!target.empty() && target[target.size()-1] == '/')
						target.resize(target.size()-1);
				}
				else {
					if(target.empty() || target[target.size()-1] != '/') 
						target+= '/';
				}
				
				Request trequest(target, isFile);
				if(isFile) trequest.setContentSink(new TempFile);
				
				String instance;
				if(request.get.get("instance", instance));
				
				// Self
				if(mUniqueName == mAddressBook->userName()
					&& (instance.empty() || instance == Core::Instance->getName()))
				{
					trequest.execute(mAddressBook->user());
				}
				
				Synchronize(&trequest);
				try {
					const unsigned timeout = Config::Get("request_timeout").toInt();
				  	if(!instance.empty()) trequest.submit(Identifier(mPeering, instance));
					else trequest.submit(mPeering);
					trequest.wait(timeout);
				}
				catch(const Exception &e)
				{
					// If not self
					if(mUniqueName != mAddressBook->userName())
					{
						Log("AddressBook::Contact::http", "Cannot send request, peer not connected");
						
						Http::Response response(request,200);
						response.send();

						Html page(response.sock);
						page.header(mName+": Files");
						page.open("div",".box");
						page.text("Not connected...");
						page.close("div");
						page.footer();
						return;
					}
				}
				
				if(trequest.responsesCount() == 0) throw Exception("No response");
				if(!trequest.isSuccessful()) throw 404;
					
				try {
					if(request.get.contains("file"))
					{
						for(int i=0; i<trequest.responsesCount(); ++i)
						{
					  		Request::Response *tresponse = trequest.response(i);
							if(tresponse->error()) continue;
							StringMap params = tresponse->parameters();
					 		if(!params.contains("name")) continue;
							ByteStream *content = tresponse->content();
							if(!content) continue;
							
							Desynchronize(&trequest);
							
							Time time = Time::Now();
							if(params.contains("time")) params.get("time").extract(time);
							
							Http::Response response(request,200);
							response.headers["Content-Disposition"] = "inline; filename=\"" + params.get("name") + "\"";
							response.headers["Content-Type"] = Mime::GetType(params.get("name"));
							if(params.contains("size")) response.headers["Content-Length"] = params.get("size");
							response.headers["Last-Modified"] = time.toHttpDate();
							
							response.send();
							
							try {
								Desynchronize(this);
								response.sock->writeBinary(*content);
							}
							catch(const NetException &e)
							{
				  
							}
							
							return;
						}
						
						throw 404;
					}
					
					Http::Response response(request,200);
					response.send();
					
					Html page(response.sock);
					if(target.empty() || target == "/") page.header(mName+": Browse files");
					else page.header(mName+": "+target.substr(1));
					page.link(prefix+"/search/","Search files");
					
					Set<String> instances;
					Map<String, StringMap> files;
					for(int i=0; i<trequest.responsesCount(); ++i)
					{
					  	Request::Response *tresponse = trequest.response(i);
						if(tresponse->error()) continue;
					
						StringMap params = tresponse->parameters();
						instances.insert(tresponse->instance());
						params.insert("instance", tresponse->instance());

						// Check info
						if(!params.contains("type")) continue;
						if(!params.contains("name")) continue;
						if(!params.contains("hash")) params.insert("hash", "");
						
						// Sort
						// Directories with the same name appears only once
						// Files with identical content appears only once
						if(params.get("type") == "directory") files.insert("0"+params.get("name"),params);
						else files.insert("1"+params.get("name")+params.get("hash"), params);
					}

					page.open("div",".box");
					if(files.empty()) page.text("No shared files");
					else {
					  	String desc;
						if(instances.size() == 1) desc << files.size() << " files";
						else desc << files.size() << " files on " << instances.size() << " instances";
						page.text(desc);
						page.text(" - ");
						if(url[url.size()-1] == '/') page.link("..", "Parent");
						else page.link(".", "Parent");
						page.br();
						
						page.open("table",".files");
						for(Map<String, StringMap>::iterator it = files.begin();
							it != files.end();
							++it)
						{
							StringMap &map = it->second;

							page.open("tr");
							page.open("td",".icon");
							if(map.get("type") == "directory") page.image("/dir.png");
							else page.image("/file.png");
							page.close("td");
							page.open("td",".filename"); 
							if(map.get("type") == "directory") page.link(base + map.get("name"), map.get("name"));
							else if(!map.get("hash").empty()) page.link("/" + map.get("hash"), map.get("name"));
							else page.link(base + map.get("name") + "?instance=" + map.get("instance").urlEncode() + "&file=1", map.get("name"));
							page.close("td");
							page.open("td",".size"); 
							if(map.get("type") == "directory") page.text("directory");
							else if(map.contains("size")) page.text(String::hrSize(map.get("size")));
							page.close("td");
							page.close("tr");
						}
						page.close("table");
					}
					page.close("div");
					page.footer();
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", String("Error during access to remote file or directory: ") + e.what());
				}
				
				return;
			}
			else if(directory == "search")
			{
				if(url != "/") throw 404;
				
				String query;
				if(request.post.contains("query"))
				{
					query = request.post.get("query");
					query.trim();
				}
				
				Http::Response response(request,200);
				response.send();
				
				Html page(response.sock);
				page.header(mName+": Search");
				page.openForm(prefix + "/search", "post", "searchForm");
				page.input("text","query",query);
				page.button("search","Search");
				page.closeForm();
				page.javascript("document.searchForm.query.focus();");
				page.br();
				
				if(query.empty())
				{
					page.footer();
					return;
				}
				
				Request trequest("search:"+query, false);	// no data
				Synchronize(&trequest);
				try {
					trequest.submit(mPeering);
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", "Cannot send request, peer not connected");
					page.open("div",".box");
					page.text("Not connected...");
					page.close("div");
					page.footer();
					return;
				}
				
				{
					const unsigned timeout = Config::Get("request_timeout").toInt();
					Desynchronize(this);
					trequest.wait(timeout);
				}
				
				page.open("div",".box");
				if(!trequest.isSuccessful()) page.text("No files found");
				else try {
					
					page.open("table",".files");
					for(int i=0; i<trequest.responsesCount(); ++i)
					{
						Request::Response *tresponse = trequest.response(i);
						if(tresponse->error()) continue;
					
						// Check info
						StringMap map = tresponse->parameters();
						if(!map.contains("type")) continue;
						if(!map.contains("path")) continue;
						if(!map.contains("hash")) map["hash"] = "";
						if(!map.contains("name")) map["name"] = map["path"].afterLast('/');
						
						page.open("tr");
						page.open("td",".icon");
						if(map.get("type") == "directory") page.image("/dir.png");
						else page.image("/file.png");
						page.close("td");
						page.open("td",".filename"); 
						if(map.get("type") == "directory") page.link(urlPrefix() + "/files" + map.get("path"), map.get("name"));
						else if(!map.get("hash").empty()) page.link("/" + map.get("hash"), map.get("name"));
						else page.link(urlPrefix() + "/files" + map.get("path") + "?instance=" + tresponse->instance().urlEncode() + "&file=1", map.get("name"));
						page.close("td");
						page.open("td",".size"); 
						if(map.get("type") == "directory") page.text("directory");
						else if(map.contains("size")) page.text(String::hrSize(map.get("size")));
						page.close("td");
						page.close("tr");
					}
					page.close("table");
					
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", String("Unable to list files: ") + e.what());
					page.close("table");
					page.text("Error, unable to list files");
				}
				page.close("div");
				
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
						wait(120000);
					
					if(count < mMessagesCount && mMessagesCount-count <= mMessages.size())
					{
						Html html(response.sock);
						int i = mMessages.size() - (mMessagesCount-count);
						messageToHtml(html, mMessages[i], false);
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
						  	message.send(mPeering);	// send to other
							
							// TODO
							//Contact *self = mAddressBook->getSelf();
							//if(self && self->isConnected()) message.send(self->peering());
							
							mMessages.push_back(Message(request.post["message"]));	// thus receiver is null
							++mMessagesCount;
							notifyAll();
							
							if(request.post.contains("ajax") && request.post["ajax"].toBool())	//ajax
							{
								Http::Response response(request, 200);
								response.send();
								/*Html html(response.sock);
								messageToHtml(html, mMessages.back(), false);
								mMessages.back().markRead();*/
							}
							else {	// form submit
							 	Http::Response response(request, 303);
								response.headers["Location"] = prefix + "/chat";
								response.send();
							}
						}
						catch(const Exception &e)
						{
							Log("AddressBook::Contact::http", String("Cannot post message: ") + e.what());
							throw 409;
						}
						
						return;
					}
				}
			  
				bool isPopup = request.get.contains("popup");
			  
				Http::Response response(request,200);
				response.send();	
				
				Html page(response.sock);
				page.header("Chat with "+mName, isPopup);
				if(isPopup)
				{
					page.open("b");
					page.text("Chat with "+mName+" - ");
					page.open("span", "status.status");
					page.span(status().capitalized(), String(".")+status());
					page.close("span");
					page.close("b");
				}
				else {
					page.open("div", "chat.box");
					page.open("span", "status.status");
					page.span(status().capitalized(), String(".")+status());
					page.close("span");
				}
				
				page.openForm(prefix + "/chat", "post", "chatForm");
				page.input("text","message");
				page.button("send","Send");
				page.space();
				
				if(!isPopup)
				{
					String popupUrl = prefix + "/chat?popup=1";
					page.raw("<a href=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','/');\">Popup</a>");
				}
				
				page.br();
				page.br();
				page.closeForm();
				page.javascript("document.chatForm.message.focus();");
				
				
				unsigned refreshPeriod = 5000;
				page.javascript("function updateContact() {\n\
					$.ajax({\n\
						url: '/"+mAddressBook->userName()+"/contacts/?json',\n\
						dataType: 'json',\n\
						timeout: 2000,\n\
						success: function(data) {\n\
							var info = data."+uniqueName()+";\n\
							transition($('#status'),\n\
								'<span class=\"'+info.status+'\">'+info.status.capitalize()+'</span>\\n');\n\
						}\n\
					});\n\
					setTimeout('updateContact()',"+String::number(refreshPeriod)+");\n\
				}\n\
				setTimeout('updateContact()',100);");
				
				page.open("div", "chatmessages");
				for(int i=mMessages.size()-1; i>=0; --i)
				{
	  				messageToHtml(page, mMessages[i], mMessages[i].isRead());
					mMessages[i].markRead();
				}
				page.close("div");
				if(!isPopup) page.close("div");
				
				page.javascript("var count = "+String::number(mMessagesCount)+";\n\
					var title = document.title;\n\
					var hasFocus = true;\n\
					var nbNewMessages = 0;\n\
					$(window).blur(function() {\n\
						hasFocus = false;\n\
						$('span.message').attr('class', 'oldmessage');\n\
					});\n\
					$(window).focus(function() {\n\
						hasFocus = true;\n\
						nbNewMessages = 0;\n\
						document.title = title;\n\
					});\n\
					function update()\n\
					{\n\
						var request = $.ajax({\n\
							url: '"+prefix+"/chat/'+count,\n\
							dataType: 'html',\n\
							timeout: 300000\n\
						});\n\
						request.done(function(html) {\n\
							if($.trim(html) != '')\n\
							{\n\
								$(\"#chatmessages\").prepend(html);\n\
								var text = $('#chatmessages span.text:first');\n\
								if(text) text.html(text.html().linkify());\n\
								if(!hasFocus)\n\
								{\n\
									nbNewMessages+= 1;\n\
									document.title = title+' ('+nbNewMessages+')';\n\
								}\n\
								count+= 1;\n\
							}\n\
							setTimeout('update()', 100);\n\
						});\n\
						request.fail(function(jqXHR, textStatus) {\n\
							setTimeout('update()', 10000);\n\
						});\n\
					}\n\
					function post()\n\
					{\n\
						var message = document.chatForm.message.value;\n\
						if(!message) return false;\n\
						document.chatForm.message.value = '';\n\
						var request = $.post('"+prefix+"/chat',\n\
							{ 'message': message, 'ajax': 1 });\n\
						request.fail(function(jqXHR, textStatus) {\n\
							alert('The message could not be sent. Is this user online ?');\n\
						});\n\
					}\n\
					setTimeout('update()', 1000);\n\
					document.chatForm.onsubmit = function() {post(); return false;}");
				
				page.footer();
				return;
			}
		}
	}
	catch(const NetException &e)
	{
		throw;
	}
	catch(const Exception &e)
	{
		Log("AddressBook::Contact::http", e.what());
		throw 500;
	}
	
	throw 404;
}

void AddressBook::Contact::messageToHtml(Html &html, const Message &message, bool old) const
{
	if(old) html.open("span",".oldmessage");
	else html.open("span",".message");
	html.open("span",".date");
	html.text(message.time().toDisplayDate());
	html.close("span");
	html.text(" ");
	if(message.receiver() == Identifier::Null) html.open("span",".out");
	else html.open("span",".in");
	html.open("span",".user");
	if(message.receiver() == Identifier::Null) html.text(mAddressBook->userName());
	else html.text(mAddressBook->getContact(message.receiver())->name());
	html.close("span");
	html.text(": ");
	html.open("span",".text");
	html.text(message.content());
	html.close("span");
	html.close("span");
	html.close("span");
	html.br(); 
}

void AddressBook::Contact::serialize(Serializer &s) const
{
	Synchronize(this);
	
	StringMap map;
	map["uname"] << mUniqueName;
	map["name"] << mName;
	map["tracker"] << mTracker;
	map["secret"] << mSecret;
	map["peering"] << mPeering;
	map["remote"] << mRemotePeering;
	map["time"] << mTime;
	
	s.outputMapBegin(2);
	s.outputMapElement(String("info"),map);
	s.outputMapElement(String("addrs"),mAddrs);
	s.outputMapEnd();
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	Synchronize(this);
	
	mUniqueName.clear();
  	mName.clear();
	mTracker.clear();
	mSecret.clear();
	mPeering.clear();
	mRemotePeering.clear();
	
	StringMap map;
	
	String key;
	AssertIO(s.inputMapBegin());
	AssertIO(s.inputMapElement(key, map) && key == "info");
	
	// TEMPORARY : try/catch block should be removed
	try {
		AssertIO(s.inputMapElement(key, mAddrs) && key == "addrs");
	}
	catch(...) {
		// HACK
		Log("AddressBook::Contact::deserialize", "Warning: bad or outdated addresses block, ignoring...");
		String hack;
		do s.input(hack);
		while(!hack.empty());
	}
	//

	map["uname"] >> mUniqueName;
	map["name"] >> mName;
	map["tracker"] >> mTracker;
	map["secret"] >> mSecret;
	map["peering"] >> mPeering;
	map["remote"] >> mRemotePeering;

	if(map.contains("time")) map["time"] >> mTime;
	else mTime = Time::Now();
	
	// TODO: checks
	
	mMessages.clear();
	mMessagesCount = 0;
	mFound = false;
	return true;
}

bool AddressBook::Contact::isInlineSerializable(void) const
{
	return false; 
}

}
