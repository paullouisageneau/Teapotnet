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

#include "tpn/addressbook.h"
#include "tpn/user.h"
#include "tpn/core.h"
#include "tpn/sha512.h"
#include "tpn/config.h"
#include "tpn/file.h"
#include "tpn/directory.h"
#include "tpn/html.h"
#include "tpn/yamlserializer.h"
#include "tpn/jsonserializer.h"
#include "tpn/portmapping.h"
#include "tpn/httptunnel.h"
#include "tpn/mime.h"

namespace tpn
{

const double AddressBook::UpdateInterval = 300.;
const double AddressBook::UpdateStep = 0.20;
const int AddressBook::MaxChecksumDistance = 1000;

AddressBook::AddressBook(User *user) :
	mUser(user),
	mUserName(user->name())
{
	Assert(UpdateInterval > 0);
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
			LogError("AddressBook", String("Loading failed: ") + e.what());
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
	Synchronize(this);
 	return mUserName; 
}

Identifier AddressBook::addContact(String name, const ByteString &secret)
{
	Synchronize(this);

	String tracker = name.cut('@');
	if(tracker.empty()) tracker = Config::Get("tracker");
	
	String uname = name;
	unsigned i = 1;
	while((mContactsByUniqueName.contains(uname) && !mContactsByUniqueName.get(uname)->isDeleted())
		|| uname == userName())	// userName reserved for self
	{
		uname = name;
		uname << ++i;
	}
	
	Contact *oldContact;
	if(mContactsByUniqueName.get(uname, oldContact))
	{
		unregisterContact(oldContact);
		delete oldContact;
	}
	
	Contact *contact = new Contact(this, uname, name, tracker, secret);
	if(mContacts.get(contact->peering(), oldContact))
	{
		oldContact->copy(contact);
		delete contact;
		contact = oldContact;
	}

	registerContact(contact);
	
	save();
	return contact->peering();
}

void AddressBook::removeContact(const Identifier &peering)
{
	Synchronize(this);
	
	Contact *contact;
	if(mContacts.get(peering, contact))
	{
		contact->setDeleted();
		Core::Instance->unregisterPeering(contact->peering());
		Interface::Instance->remove(contact->urlPrefix(), contact);
		mScheduler.remove(contact);
		save();
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
  
	Contact *contact = NULL;
	if(mContacts.get(peering, contact)) return contact;
	else return NULL;
}

AddressBook::Contact *AddressBook::getContactByUniqueName(const String &uname)
{
	Synchronize(this);
  
	Contact *contact = NULL;
	if(mContactsByUniqueName.get(uname, contact)) return contact;
	else return NULL;
}

const AddressBook::Contact *AddressBook::getContactByUniqueName(const String &uname) const
{
	Synchronize(this);
  
	Contact *contact = NULL;
	if(mContactsByUniqueName.get(uname, contact)) return contact;
	else return NULL;
}

void AddressBook::getContacts(Array<AddressBook::Contact *> &array)
{
	Synchronize(this); 
  
	mContactsByUniqueName.getValues(array);
	
	Contact *self = getSelf();
	if(self) array.remove(self);
	
	int i=0; 
	while(i<array.size())
	{
		if(array[i]->isDeleted()) array.erase(i);
		else ++i;
	}
}

Identifier AddressBook::setSelf(const ByteString &secret)
{
	Synchronize(this);
  
	const String tracker = Config::Get("tracker");	
	Contact *self = new Contact(this, userName(), userName(), tracker, secret);

	Contact *tmp;
        if(mContacts.get(self->peering(), tmp))
        	if(tmp->uniqueName() != userName())
			throw Exception("a contact with the same peering already exists");
	
	Contact *oldSelf = getSelf();
        if(oldSelf)
	{
		unregisterContact(oldSelf);
		oldSelf->copy(self);
		delete self;
		self = oldSelf;
	}

	registerContact(self);
	save();
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
  
	mScheduler.clear();	// we make sure no task is running
	
	for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		Contact *contact = it->second;
		delete contact;
	}
	
	mContacts.clear();
	mContactsByUniqueName.clear();
}

void AddressBook::load(Stream &stream)
{
	Synchronize(this);

	Contact *contact = new Contact(this);
	
	int i = 0;
	//YamlSerializer serializer(&stream);
	//while(serializer.input(*contact))
	while(true)
	{
		// Not really clean but it protects from parse errors propagation
		YamlSerializer serializer(&stream);
		if(!serializer.input(*contact)) break;

		Contact *oldContact = NULL;
		if(mContactsByUniqueName.get(contact->uniqueName(), oldContact))
		{
			if(oldContact->time() >= contact->time())
			{
				oldContact->addAddresses(contact->addresses());
				continue;
			}
		
			unregisterContact(oldContact);
			oldContact->copy(contact);
			std::swap(oldContact, contact);
			delete oldContact;
		}
		
		registerContact(contact, i);
		contact = new Contact(this);
		++i;
	}
	
	delete contact;
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
			Desynchronize(this);
			Notification notification(data);
			notification.setParameter("type", "contacts");
			notification.send(self->peering());
		}
		catch(Exception &e)
		{
			LogWarn("AddressBook::Save", String("Contacts synchronization failed: ") + e.what()); 
		}
	}
}

void AddressBook::update(void)
{
	Synchronize(this);
	
	Array<Identifier> keys;
	mContacts.getKeys(keys);
	std::random_shuffle(keys.begin(), keys.end());

	for(int i=0; i<keys.size(); ++i)
        {
                Contact *contact = getContact(keys[i]);
		if(contact) mScheduler.schedule(contact, UpdateStep*i + UpdateStep*uniform(0., UpdateStep));
	}
}

bool AddressBook::send(const Notification &notification)
{
	Array<Identifier> keys;
	SynchronizeStatement(this, mContacts.getKeys(keys));

	bool success = false;
	for(int i=0; i<keys.size(); ++i)
        {
                Contact *contact = getContact(keys[i]);
		if(contact) success|= contact->send(notification);
	}
	
	return success;
}

bool AddressBook::send(const Message &message)
{
	// TODO: could be more efficient
	// should convert the message and use send(notification)
	
	Array<Identifier> keys;
	SynchronizeStatement(this, mContacts.getKeys(keys));

	bool success = false;
	for(int i=0; i<keys.size(); ++i)
        {
                Contact *contact = getContact(keys[i]);
		if(contact) success|= contact->send(message);
	}
	
	return success;
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	user()->setOnline();
	
	try {
		if(request.url.empty() || request.url == "/")
		{
			if(request.method == "POST")
			{
				try {
					if(!user()->checkToken(request.post["token"], "contact")) 
						throw 403;
					
			  		String command = request.post["command"];
			  		if(command == "delete")
					{
						Synchronize(this);
						String uname = request.post["argument"];
						removeContact(mContactsByUniqueName.get(uname)->peering());
					}
					else {
						String name, csecret;
						name = request.post["name"];
						csecret = request.post["secret"];
					  
						if(name.empty() || csecret.empty()) throw 400;
						
						ByteString secret;
						Sha512::RecursiveHash(csecret, String("TeapotNet"), secret, Sha512::CryptRounds);
						
						if(request.post.contains("self")) setSelf(secret);
						else addContact(name, secret);
					}
				}
				catch(const Exception &e)
				{
					LogWarn("AddressBook::http", e.what());
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

				Array<Contact*> contacts;
				getContacts(contacts);
				Contact *self = getSelf();
				if(self) contacts.append(self);

				JsonSerializer json(response.sock);
				json.outputMapBegin();
				for(int i=0; i<contacts.size(); ++i)
        			{
                			Contact *contact = contacts[i];
                			if(contact->isDeleted()) continue;
                        			
					String name = contact->name();
					String tracker = contact->tracker();
					String status = contact->status();
					String strPrefix = contact->urlPrefix();
					
					MessageQueue *messageQueue = user()->messageQueue();
					ConstSerializableWrapper<int> messagesWrapper(messageQueue->select(contact->peering()).unreadCount());
					ConstSerializableWrapper<bool> newMessagesWrapper(messageQueue->hasNew());
					
					SerializableMap<String, Serializable*> map;
					map["name"] = &name;
					map["tracker"] = &tracker;
					map["status"] = &status;
					map["prefix"] = &strPrefix;
					map["messages"] = &messagesWrapper;
					map["newmessages"] = &newMessagesWrapper;
					json.outputMapElement(contact->uniqueName(), map);
				}
				json.outputMapEnd();
				return;
			}
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header("Contacts");
			
			String token = user()->generateToken("contact");
			
			Array<Contact*> contacts;
			getContacts(contacts);
			if(!contacts.empty())
			{
				page.open("div",".box");
				page.open("table",".contacts");
				
				for(int i=0; i<contacts.size(); ++i)
				{
					Contact *contact = contacts[i];
					if(contact->isDeleted()) continue;
					String contactUrl = prefix + '/' + contact->uniqueName() + '/';
					
					page.open("tr");
					page.open("td",".name");
					page.link(contact->urlPrefix(), contact->name());
					page.close("td");
					page.open("td",".tracker");
					page.text(String("@") + contact->tracker());
					page.close("td");
					page.open("td",".uname");
					page.text(contact->uniqueName());
					page.close("td");
					page.open("td",".checksum");
					page.text(String(" check: ")+String::hexa(contact->peeringChecksum(),8));
					page.close("td");
					page.open("td",".delete");
					page.image("/delete.png", "Delete");
					page.closeLink();
					page.close("td");
					page.close("tr");
				}
				
				page.close("table");
				page.close("div");
				
				page.openForm(prefix+"/", "post", "executeForm");
				page.input("hidden", "command");
				page.input("hidden", "argument");
				page.input("hidden", "token", token);
				page.closeForm();
				
				page.javascript("function deleteContact(uname) {\n\
					if(confirm('Do you really want to delete '+uname+' ?')) {\n\
						document.executeForm.command.value = 'delete';\n\
						document.executeForm.argument.value = uname;\n\
						document.executeForm.submit();\n\
					}\n\
				}");
				
				page.javascript("$('td.delete').css('cursor', 'pointer').click(function(event) {\n\
					event.stopPropagation();\n\
					var uname = $(this).closest('tr').find('td.uname').text();\n\
					deleteContact(uname);\n\
				});");
			}
			
			page.openForm(prefix+"/","post");
			page.openFieldset("New contact");
			page.input("hidden", "token", token);
			page.label("name","Name"); page.input("text","name"); page.br();
			page.label("secret","Secret"); page.input("text","secret","",true); page.br();
			page.label("add"); page.button("add","Add contact");
			page.closeFieldset();
			page.closeForm();
			
			page.openForm(prefix+"/","post");
			page.openFieldset("Personal secret");
			page.input("hidden", "token", token);
			page.input("hidden","name",userName());
			page.input("hidden","self","true");
			if(getSelf()) page.text("Your personal secret is already set, but you can change it here.");
			else page.text("Set the same username and the same personal secret on multiple devices to enable automatic synchronization.");
			page.br();
			page.br();
			page.label("secret","Secret"); page.input("text","secret","",true); page.br();
			page.label("add"); page.button("add","Set secret");
			page.closeFieldset();
			page.closeForm();
			
			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("AddressBook::http",e.what());
		throw 500;	// Httpd handles integer exceptions
	}
	
	throw 404;
}

void AddressBook::registerContact(Contact *contact, int ordinal)
{
	Synchronize(this);
	
	mContacts.insert(contact->peering(), contact);
	mContactsByUniqueName.insert(contact->uniqueName(), contact);
	
	if(!contact->isDeleted())
	{
		if(contact->isSelf()) Interface::Instance->remove("/"+userName()+"/myself/files");	// overriding Store
		Interface::Instance->add(contact->urlPrefix(), contact);
		mScheduler.schedule(contact, UpdateStep*ordinal + uniform(0., UpdateStep));
		mScheduler.repeat(contact, UpdateInterval);
	}
}

void AddressBook::unregisterContact(Contact *contact)
{
	Synchronize(this);
	
	mContacts.erase(contact->peering());
	mContactsByUniqueName.erase(contact->uniqueName());
	
	Core::Instance->unregisterPeering(contact->peering());
	Interface::Instance->remove(contact->urlPrefix(), contact);
	mScheduler.remove(contact);
}

bool AddressBook::publish(const Identifier &remotePeering)
{
	String tracker = Config::Get("tracker");
	
	try {
		String url("http://" + tracker + "/tracker?id=" + remotePeering.toString());
		
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
		post["addresses"] = addresses;
	
		String externalPort = Config::Get("external_port");
		if(!externalPort.empty() && externalPort != "auto") post["port"] = externalPort;
                else post["port"] = Config::Get("port");
	
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
		
		if(Http::Post(url, post) == 200)
		{
			SynchronizeStatement(this, mBogusTrackers.erase(tracker));
			return true;
		}
	}
	catch(const Timeout &e)
	{
		LogDebug("AddressBook::publish", e.what()); 
	}
	catch(const NetException &e)
	{
		LogDebug("AddressBook::publish", e.what()); 
	}
	catch(const std::exception &e)
	{
		LogWarn("AddressBook::publish", e.what()); 
	}
	
	SynchronizeStatement(this, mBogusTrackers.insert(tracker));
	return false;
}

bool AddressBook::query(const Identifier &peering, const String &tracker, AddressMap &output, bool alternate)
{
	output.clear();
	
	String host(tracker);
	if(host.empty()) host = Config::Get("tracker");
	
	try {
		String url = "http://" + host + "/tracker?id=" + peering.toString();
  		if(alternate) url+= "&alternate=1";
		  
		String tmp;
		if(Http::Get(url, &tmp) == 200) 
		{
			SynchronizeStatement(this, mBogusTrackers.erase(tracker));
			
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
	}
	catch(const Timeout &e)
	{
		LogDebug("AddressBook::query", e.what()); 
	}
	catch(const NetException &e)
	{
		LogDebug("AddressBook::query", e.what()); 
	}
	catch(const std::exception &e)
	{
		LogWarn("AddressBook::query", e.what()); 
	}
	
	SynchronizeStatement(this, mBogusTrackers.insert(tracker));
	return false;
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
	mDeleted(false),
	mFound(false),
	mOnline(false),
	mFirstUpdateTime(0),
	mProfile(NULL)
{	
	Assert(addressBook != NULL);
	Assert(!uname.empty());
	Assert(!name.empty());
	Assert(!tracker.empty());
	Assert(!secret.empty());
	
	// Compute peering
	ByteString salt;
	salt.writeBinary(mAddressBook->userName());
	salt.writeBinary(mName);
	Sha512::DerivateKey(mSecret, salt, mPeering, Sha512::CryptRounds);
	
	// Compute Remote peering
	salt.clear();
	salt.writeBinary(mName);
	salt.writeBinary(mAddressBook->userName());
	Sha512::DerivateKey(mSecret, salt, mRemotePeering, Sha512::CryptRounds);
	
	createProfile();
}

AddressBook::Contact::Contact(AddressBook *addressBook) :
	mAddressBook(addressBook),
	mDeleted(false),
	mFound(false),
	mOnline(false),
	mFirstUpdateTime(0),
	mProfile(NULL)
{

}

AddressBook::Contact::~Contact(void)
{
	Interface::Instance->remove(urlPrefix(), this);
	delete mProfile;
}

String AddressBook::Contact::uniqueName(void) const
{
	Synchronize(mAddressBook);
	return mUniqueName;
}

String AddressBook::Contact::name(void) const
{
	Synchronize(mAddressBook);
	return mName;
}

String AddressBook::Contact::tracker(void) const
{
	Synchronize(mAddressBook);
	if(mProfile) return mProfile->tracker();
	else return mTracker;
}

Identifier AddressBook::Contact::peering(void) const
{
	Synchronize(mAddressBook);
	return mPeering;
}

Identifier AddressBook::Contact::remotePeering(void) const
{
	Synchronize(mAddressBook);
	return mRemotePeering;
}

Time AddressBook::Contact::time(void) const
{
	Synchronize(mAddressBook);
	return mTime; 
}

uint32_t AddressBook::Contact::peeringChecksum(void) const
{
	Synchronize(mAddressBook);
	return mPeering.getDigest().checksum32() + mRemotePeering.getDigest().checksum32(); 
}

String AddressBook::Contact::urlPrefix(void) const
{
	Synchronize(mAddressBook);
	if(mUniqueName.empty()) return "";
	if(isSelf()) return String("/")+mAddressBook->userName()+"/myself";
	return String("/")+mAddressBook->userName()+"/contacts/"+mUniqueName;
}

ByteString AddressBook::Contact::secret(void) const
{
        Synchronize(mAddressBook);
        return mSecret;
}

Profile *AddressBook::Contact::profile(void) const
{
	Synchronize(mAddressBook);

	if(isSelf()) return mAddressBook->user()->profile();
	else {
		Assert(mProfile);
		return mProfile;
	}
}

bool AddressBook::Contact::isSelf(void) const
{
	return (mUniqueName == mAddressBook->userName());
}

bool AddressBook::Contact::isFound(void) const
{
	return mFound;
}

bool AddressBook::Contact::isConnected(void) const
{
	Synchronize(mAddressBook);
	return Core::Instance->hasPeer(mPeering); 
}

bool AddressBook::Contact::isConnected(const String &instance) const
{
	Synchronize(mAddressBook);
	if(isSelf() && instance == Core::Instance->getName()) return true;
	return Core::Instance->hasPeer(Identifier(mPeering, instance)); 
}

bool AddressBook::Contact::isOnline(void) const
{
	Synchronize(mAddressBook);
	if(isSelf() && mAddressBook->user()->isOnline()) return true;
	if(!isConnected()) return false;
	return mOnline;
}

String AddressBook::Contact::status(void) const
{
	Synchronize(mAddressBook);
	if(isSelf()) return "myself";
	if(isOnline()) return "online";
	else if(isConnected()) return "connected";
	else if(isFound()) return "found";
	else return "disconnected";
}

AddressBook::AddressMap AddressBook::Contact::addresses(void) const
{
	Synchronize(mAddressBook);
	return mAddrs;
}

bool AddressBook::Contact::isDeleted(void) const
{
	Synchronize(mAddressBook);
	return mDeleted;
}

void AddressBook::Contact::setDeleted(void)
{
	Synchronize(mAddressBook);
	mDeleted = true;
	mTime = Time::Now();
}

void AddressBook::Contact::getInstancesNames(Array<String> &array)
{
	SynchronizeStatement(mAddressBook, mAddrs.getKeys(array));
	
	Array<String> others;
	Core::Instance->getInstancesNames(peering(), others);
	
	for(int i=0; i<others.size(); ++i)
		if(!array.contains(others[i]))
			array.append(others[i]);
}

bool AddressBook::Contact::addAddresses(const AddressMap &map)
{
	bool changed = false;
	for(AddressMap::const_iterator it = map.begin();
		it != map.end();
		++it)
	{
		Synchronize(mAddressBook);
		
		const String &instance = it->first;
		const AddressBlock &block = it->second;
		AddressBlock &target = mAddrs[instance];
		
		for(AddressBlock::const_iterator jt = block.begin();
			jt != block.end();
			++jt)
		{
			const Address &addr = jt->first;
			const Time &time = jt->second;
			
			if(!target.contains(addr) || target[addr] < time)
			{
				target[addr] = time;
				changed = true;
			}
		}
	}
	
	if(changed) mAddressBook->save();
	return true;
}

bool AddressBook::Contact::connectAddress(const Address &addr, const String &instance, bool save)
{
	// Warning: NOT synchronized !
	
	if(addr.isNull()) return false;
	if(instance == Core::Instance->getName()) return false;
	
	LogDebug("AddressBook::Contact", "Connecting " + instance + " on " + addr.toString() + "...");
	
	Identifier peering(this->peering(), instance);
	ByteStream *bs = NULL;

	if(!Config::Get("force_http_tunnel").toBool() || !addr.isPublic())
	{
		Socket *sock = NULL;
		try {
			sock = new Socket(addr, 4.);
		}
		catch(const Exception &e)
		{
			sock = NULL;
			LogDebug("AddressBook::Contact::connectAddress", String("Direct connection failed: ") + e.what());
		}
		
		if(sock)
		{
			sock->setTimeout(milliseconds(Config::Get("tpot_read_timeout").toInt()));
			bs = sock;
		}
	}

	if(!bs && addr.isPublic())
	{
		try {
			bs = new HttpTunnel::Client(addr, 4.);
		}
		catch(const Exception &e)
		{
			bs = NULL;
			LogDebug("AddressBook::Contact::connectAddress", String("HTTP tunnel failed: ") + e.what());
		}
	}

	if(!bs) return false;

	if(Core::Instance->addPeer(bs, addr, peering))
	{
		if(save)
		{
			SynchronizeStatement(mAddressBook, mAddrs[instance][addr] = Time::Now());
			mAddressBook->save();
		}
		return true;
	}
	else {
		// A node is running at this address but the user does not exist
		if(save && mAddrs.contains(instance)) 
		{
			SynchronizeStatement(mAddressBook, mAddrs[instance].erase(addr));
			mAddressBook->save();
		}
		return false;
	}
}

bool AddressBook::Contact::connectAddresses(const AddressMap &map, bool save, bool shuffle)
{
	// Warning: NOT synchronized !
	
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

		Array<Address> addrs;
		block.getKeys(addrs);
		if(shuffle) std::random_shuffle(addrs.begin(), addrs.end());
		
		LogDebug("AddressBook::Contact", "Connecting instance " + instance + " for " + name() + " (" + String::number(addrs.size()) + " address(es))...");
		
		for(Array<Address>::const_reverse_iterator jt = addrs.rbegin();
			jt != addrs.rend();
			++jt)
		{
			if(connectAddress(*jt, instance, save))
			{
				success = true;
				break;
			}
		}
			
	}
	
	return success;
}

void AddressBook::Contact::update(bool alternate)
{
	// Warning: NOT synchronized !
	
	if(isDeleted()) return;
	Core::Instance->registerPeering(peering(), remotePeering(), secret(), this);
	
	LogDebug("AddressBook::Contact", "Looking for " + uniqueName());
	
	if(peering() != remotePeering() && Core::Instance->hasRegisteredPeering(remotePeering()))	// the user is local
	{
		Identifier identifier(peering(), Core::Instance->getName());
		if(!Core::Instance->hasPeer(identifier))
		{
			LogDebug("AddressBook::Contact", uniqueName() + " found locally");
			  
			Address addr("127.0.0.1", Config::Get("port"));
			try {
				Socket *sock = new Socket(addr);
				Core::Instance->addPeer(sock, identifier);
			}
			catch(...)
			{
				LogDebug("AddressBook::Contact", "Warning: Unable to connect the local core");	 
			}
		}
	}

	if(!alternate) 
	{
		if(mAddressBook->publish(remotePeering()))
			LogDebug("AddressBook::Contact", "Published to tracker " + tracker() + " for " + uniqueName());
	}
		  
	AddressMap newAddrs;
	if(mAddressBook->query(peering(), tracker(), newAddrs, alternate))
		LogDebug("AddressBook::Contact", "Queried tracker " + tracker() + " for " + uniqueName());	
	
	if(!newAddrs.empty())
	{
		if(!alternate) LogDebug("AddressBook::Contact", "Contact " + name() + " found (" + String::number(newAddrs.size()) + " instance(s))");
		else LogDebug("AddressBook::Contact", "Alternative addresses for " + name() + " found (" + String::number(newAddrs.size()) + " instance(s))");
		
		mFound = true;
		connectAddresses(newAddrs, !alternate, alternate);
	}
	else if(!alternate) 
	{
		mFound = false;
		connectAddresses(addresses(), true, false);
	}
	
	{
		Synchronize(mAddressBook);

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
	}
}

void AddressBook::Contact::createProfile(void)
{
	if(isSelf()) return;
	
	if(!mProfile)
	{
		mProfile = new Profile(mAddressBook->user(), mUniqueName, mTracker);
		mProfile->load();
	}
}

void AddressBook::Contact::run(void)
{
	Synchronize(mAddressBook);
	
	if(mFirstUpdateTime == Time(0))
		mFirstUpdateTime = Time::Now();
	
	DesynchronizeStatement(mAddressBook, update(false));
	
	if((Time::Now() - mFirstUpdateTime)*1000 >= UpdateInterval)
		DesynchronizeStatement(mAddressBook, update(true));
}

void AddressBook::Contact::connected(const Identifier &peering, bool incoming)
{
	// Send status and profile
	mAddressBook->user()->sendStatus(peering);
	mAddressBook->user()->profile()->send(peering);

	// Send contacts if self
        if(isSelf())
        {
                String data;
        	mAddressBook->save(data);
                Notification notification(data);
                notification.setParameter("type", "contacts");
                notification.send(peering);
	}
	
	if(incoming) 
	{
		MessageQueue::Selection selection = selectMessages();
		int total = selection.count();
		if(total > MaxChecksumDistance)
		{
			Message base;
			if(selection.getOffset(total - MaxChecksumDistance, base))
				Assert(selection.setBaseStamp(base.stamp()));
		}
		
		sendMessagesChecksum(selection, 0, selection.count(), true);
	}
}

void AddressBook::Contact::disconnected(const Identifier &peering)
{
	mAddressBook->mScheduler.schedule(this, 10.);
	SynchronizeStatement(mAddressBook, mOnline = false);
}

void AddressBook::Contact::notification(Notification *notification)
{
	Assert(notification);
	if(isDeleted()) return;

	StringMap parameters = notification->parameters();
	
	String type;
	parameters.get("type", type);
	LogDebug("AddressBook::Contact", "Incoming notification from "+uniqueName()+" (type='" + type + "')");
	
	if(type.empty() || type == "message")
	{
		Message message;
		Assert(message.recv(*notification));
		
		if(!isSelf())
		{
			message.toggleIncoming();
			message.setPeering(peering());
			message.setDefaultHeader("from", name());
		}
		
		mAddressBook->user()->messageQueue()->add(message);
	}
	else if(type == "read" || type == "ack")
	{
		String data = notification->content();
                YamlSerializer serializer(&data);
		StringArray stamps;
		serializer.input(stamps);

		// Mark messages as read
		MessageQueue::Selection selection = selectMessages();
		for(int i=0; i<stamps.size(); ++i)
			selection.markRead(stamps[i]);
	}
	else if(type == "unread")
	{
		String data = notification->content();
                YamlSerializer serializer(&data);
		StringSet recvStamps;
		serializer.input(recvStamps);

		MessageQueue::Selection selection = selectMessages();
		Array<Message> unread;
		selection.getUnread(unread);
		
		// Mark not present stamps as read
		for(int i=0; i<unread.size(); ++i)
		{
			String stamp = unread[i].stamp();
			if(recvStamps.contains(stamp)) recvStamps.erase(stamp);
			else selection.markRead(stamp);
		}
		
		// Ack left stamps
		if(!recvStamps.empty())
		{
			StringArray ackedStamps;
			ackedStamps.assign(recvStamps.begin(), recvStamps.end());
			
			String tmp;
			YamlSerializer outSerializer(&tmp);
			outSerializer.output(ackedStamps);

			Notification notification(tmp);
			notification.setParameter("type", "read");
			notification.send(peering());
		}
	}
	else if(type == "checksum")
	{
		int offset = 0;
		int count = 0;
		int total = 0;
		bool recursion = false;
		String base;
		
		try {
			parameters["offset"] >> offset;
			parameters["count"] >> count;
			parameters["total"] >> total;
			parameters["recursion"] >> recursion;
			if(parameters.contains("base"))
				parameters["base"] >> base;
			
			Assert(offset >= 0);
			Assert(count >= 0);
			Assert(offset + count <= total);
		}
		catch(const Exception &e)
		{
			throw InvalidData("checksum notification parameters: " + String(e.what()));
		}
		
		ByteString checksum;
		
		try {
			notification->content().extract(checksum);
		}
		catch(...)
		{
			throw InvalidData("checksum notification content: " + notification->content());
		}
	
		LogDebug("AddressBook::Contact", "Synchronization: Received checksum: " + String::number(offset) + ", " + String::number(count) + " (recursion " + (recursion ? "enabled" : "disabled") + ")");
	
		MessageQueue::Selection selection = selectMessages();
		if(!base.empty())
		{
			if(!selection.setBaseStamp(base))
				LogDebug("AddressBook::Contact", "Synchronization: Base message '"+base+"' does not exist");
		}
		
		int localTotal = selection.count();
		bool isLastIteration = false;
		if(offset + count <= localTotal)
		{
			ByteString result;
			selection.checksum(offset, count, result);
		
			if(result != checksum)
			{
				LogDebug("AddressBook::Contact", "Synchronization: No match: " + String::number(offset) + ", " + String::number(count));
			
				if(count == 1)	// TODO
				{
					sendMessages(selection, offset, count);
					if(recursion)
					{
						sendMessagesChecksum(selection, offset, count, false);
						isLastIteration = true;
					}
				}
				else if(recursion)
				{
					sendMessagesChecksum(selection, offset, count/2, true);
					sendMessagesChecksum(selection, offset + count/2, count - count/2, true);
				}
			
				if(!recursion) 
					isLastIteration = true;
			}
			else {
				LogDebug("AddressBook::Contact", "Synchronization: Match: " + String::number(offset) + ", " + String::number(count));
				isLastIteration = true;
			}
		}
		else {
			if(offset == 0)
			{
				LogDebug("AddressBook::Contact", "Synchronization: Remote has more messages");
				sendMessagesChecksum(selection, 0, localTotal, true);
			}
		}

		if(isLastIteration && offset == 0)
		{
			// If messages are missing remotely
			if(total < localTotal)
				sendMessages(selection, total, localTotal - total);
			
			sendUnread();
		}
	}
	else if(type == "status")
	{
		String status = notification->content().toLower().trimmed();
		SynchronizeStatement(mAddressBook, mOnline = (status == "online"));
	}
	else if(type == "profile" || type == "profilediff")
	{
		String data = notification->content();
		YamlSerializer serializer(&data);
		
		Profile *p = profile();
		Synchronize(p);
		if(type != "profilediff") p->clear();
		p->deserialize(serializer);
		p->save();
	}
	else if(type == "contacts")
	{
		if(!isSelf())
		{
			LogWarn("AddressBook::Contact::notification", "Received contacts update from other than self, dropping");
			return;
		}
		
		// DO NOT synchronize here, as the contact could disappear during load
		String data = notification->content();
		mAddressBook->load(data);
	}
	else if(type == "text")
	{
		LogDebug("AddressBook::Contact", "Got deprecated notification with type 'text'");
	}
	else LogDebug("AddressBook::Contact", "Unknown notification type '" + type + "'");
}

MessageQueue::Selection AddressBook::Contact::selectMessages(void) const
{
	MessageQueue *messageQueue = mAddressBook->user()->messageQueue();
	if(isSelf()) return messageQueue->select();
	else return messageQueue->select(peering());
}

void AddressBook::Contact::sendMessages(const MessageQueue::Selection &selection, int offset, int count) const
{
	if(!count) return;

	LogDebug("AddressBook::Contact", "Synchronization: Sending messages: " + String::number(offset) + ", " + String::number(count));

	Array<Message> messages;
	selection.getRange(offset, count, messages);
	
	for(int i=0; i<messages.size(); ++i)
		messages[i].send(peering());
}

void AddressBook::Contact::sendMessagesChecksum(const MessageQueue::Selection &selection, int offset, int count, bool recursion) const
{
	int total = selection.count();
	offset = bounds(offset, 0, total);
	count = bounds(count, 0, total - offset); 
	
	LogDebug("AddressBook::Contact", "Synchronization: Sending checksum: " + String::number(offset) + ", " + String::number(count) + " (recursion " + (recursion ? "enabled" : "disabled") + ")");

	ByteString result;
	selection.checksum(offset, count, result);
	
	StringMap parameters;
	parameters["type"] << "checksum";
	parameters["offset"] << offset;
	parameters["count"] << count;
	parameters["total"] << total;
	parameters["recursion"] << recursion;
	
	String base = selection.baseStamp();
	if(!base.empty()) parameters["base"] << base;
		
	Notification notification(result.toString());
	notification.setParameters(parameters);
	notification.send(peering());
}

void AddressBook::Contact::sendUnread(void) const
{
	MessageQueue::Selection selection = selectMessages();
	
	StringArray unreadStamps;
	selection.getUnreadStamps(unreadStamps);
	
	String tmp;
	YamlSerializer serializer(&tmp);
	serializer.output(unreadStamps);

	Notification notification(tmp);
	notification.setParameter("type", "unread");
	notification.send(peering());
}

void AddressBook::Contact::request(Request *request)
{
	Assert(request);
	if(!mDeleted) request->execute(mAddressBook->user());
	else request->executeDummy();
}

bool AddressBook::Contact::send(const Notification &notification)
{
	return notification.send(peering());
}

bool AddressBook::Contact::send(const Message &message)
{
	return message.send(peering());
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
	mAddressBook->user()->setOnline();

	try {
		if(request.url.empty() || request.url == "/")
		{
			if(request.get.contains("json"))
			{
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();
				
				JsonSerializer json(response.sock);
				
				String strName = name();
				String strTracker = tracker();
				String strStatus = status();
				
				MessageQueue *messageQueue = mAddressBook->user()->messageQueue();
				ConstSerializableWrapper<int> messagesWrapper(messageQueue->select(peering()).unreadCount());
				ConstSerializableWrapper<bool> newMessagesWrapper(messageQueue->hasNew());
				
				Array<String> instances;
 				getInstancesNames(instances);
				StringMap instancesMap;
				for(int i=0; i<instances.size(); ++i)
        			{
                			if(isConnected(instances[i])) instancesMap[instances[i]] = "connected";
					else instancesMap[instances[i]] = "disconnected";
                		}
				
				SerializableMap<String, Serializable*> map;
				map["name"] = &strName;
				map["tracker"] = &strTracker;
				map["status"] = &strStatus;
				map["messages"] = &messagesWrapper;
				map["newmessages"] = &newMessagesWrapper;
				map["instances"] = &instancesMap;
				json.output(map);
				return;
			}
		  
			Http::Response response(request,200);
			response.send();

			Html page(response.sock);
			if(isSelf()) page.header("Myself");
			else {
				page.header("Contact: "+name());
				page.open("div", "topmenu");
				page.span(status().capitalized(), "status.button");
				page.close("div");
			}


			// TODO: insert here profile infos
			
			page.open("div",".box");
			
			page.open("table", ".menu");
			page.open("tr");
			page.open("td");
				page.openLink(prefix + "/files/");
				page.image("/icon_files.png", "Files", ".bigicon");
				page.closeLink();
			page.close("td");
			page.open("td", ".title");
				page.text("Files");
			page.close("td");
			page.close("tr");
			page.open("tr");
			page.open("td");
				page.openLink(prefix + "/search/");
				page.image("/icon_search.png", "Search", ".bigicon");
				page.closeLink();
			page.close("td");
			page.open("td", ".title");
				page.text("Search");
			page.close("td");
			page.close("tr");
			page.open("tr");
			page.open("td");
				page.openLink(profile()->urlPrefix());
				page.image("/icon_profile.png", "Profile", ".bigicon");
				page.closeLink();
			page.close("td");
			page.open("td", ".title");
				page.text("Profile");
			page.close("td");
			page.close("tr");
			
			if(!isSelf())
			{
				page.open("tr");
				page.open("td");
					page.openLink(prefix + "/chat/");
					page.image("/icon_chat.png", "Chat", ".bigicon");
					page.closeLink();
				page.close("td");
				page.open("td",".title");
					page.text("Chat");
					page.span("", "messagescount.messagescount");
				page.close("td");
				page.close("tr");
			}
			
			page.close("table");
			page.close("div");
			
			page.open("div",".box");
			page.open("h2");
			page.text("Instances");
			page.close("h2");
			page.open("table", "instances");
			page.close("table");
			page.close("div");
			
			unsigned refreshPeriod = 5000;
			page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				var msg = '';\n\
				if(info.messages != 0) msg = ' ('+info.messages+')';\n\
				transition($('#messagescount'), msg);\n\
				$('#instances').empty();\n\
				if($.isEmptyObject(info.instances)) $('#instances').text('No connected instance');\n\
				else $.each(info.instances, function(instance, status) {\n\
					$('#instances').append($('<tr>')\n\
						.addClass(status)\n\
						.append($('<td>').addClass('name').text(instance))\n\
						.append($('<td>').addClass('status').text(status.capitalize())));\n\
				});\n\
			});");
			
			// Recent files
			page.open("div",".box");
			page.open("h2");
			page.text("Recent files");
			page.close("h2");
			page.div("", "recent");
			page.close("div");
			
			int maxAge = 60*60*24*7;	// 7 days
			int count = 20;
			page.javascript("listDirectory('"+prefix+"/search?json&maxage="+String::number(maxAge)+"&count="+String::number(count)+"','#recent',false);");
			
			page.footer();
			return;
		}
		else {
			String url = request.url;
			String directory = url;
			directory.ignore();		// remove first '/'
			url = "/" + directory.cut('/');
			if(directory.empty()) throw 404;
			  
			if(request.get.contains("play"))
			{			  	
				String host;
				if(!request.headers.get("Host", host))
				host = String("localhost:") + Config::Get("interface_port");
					 
				Http::Response response(request, 200);
				response.headers["Content-Disposition"] = "attachment; filename=\"stream.m3u\"";
				response.headers["Content-Type"] = "audio/x-mpegurl";
				response.send();
				
				String link = "http://" + host + prefix + request.url + "?file=1";
				String instance;
				if(request.get.get("instance", instance))
					link+= "&instance=" + instance; 
				
				response.sock->writeLine("#EXTM3U");
				response.sock->writeLine(String("#EXTINF:-1, ") + APPNAME + " stream");
				response.sock->writeLine(link);
				return;
			}
			
			if(directory == "files")
			{
				String target(url);
				Assert(!target.empty());
				
				if(request.get.contains("json") || request.get.contains("playlist"))
				{
					// Query resources
					Resource::Query query(mAddressBook->user()->store(), target);

					SerializableSet<Resource> resources;
					bool success = query.submitRemote(resources, peering());
					if(isSelf()) success|= query.submitLocal(resources);
					if(!success) throw 404;
					
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
				
				// if it seems to be a file
				if(target[target.size()-1] != '/')
				{
					String instance;
					request.get.get("instance", instance);
					
					Identifier instancePeering(peering());
					if(!instancePeering.empty()) instancePeering.setName(instance);
					
					Resource resource(instancePeering, url, mAddressBook->user()->store());
					try {
						resource.fetch(isSelf());	// we might find a better way to access it
					}
					catch(const Exception &e)
					{
						LogWarn("AddressBook::Contact::http", String("Resource lookup failed: ") + e.what());
						throw 404;
					}
					
					// redirect if it's a directory
					if(resource.isDirectory())
					{
						if(request.get.contains("download"))
							throw 404;
						
						Http::Response response(request, 301);	// Moved permanently
						response.headers["Location"] = prefix + request.url + '/';
						response.send();
						return;
					}
					
					// Get range
					int64_t rangeBegin = 0;
					int64_t rangeEnd = 0;
					bool hasRange = request.extractRange(rangeBegin, rangeEnd, resource.size());
					int64_t rangeSize = rangeEnd - rangeBegin;
					
					// Get resource accessor
					Resource::Accessor *accessor = resource.accessor();
					if(!accessor) throw 404;
					
					// Forge HTTP response header
					Http::Response response(request, 200);
					if(!hasRange) response.headers["Content-SHA512"] << resource.digest();
					response.headers["Content-Length"] << rangeSize;
					response.headers["Content-Name"] = resource.name();
					response.headers["Last-Modified"] = resource.time().toHttpDate();
					response.headers["Accept-Ranges"] = "bytes";
					
					if(request.get.contains("download"))
					{
						response.headers["Content-Disposition"] = "attachment; filename=\"" + resource.name() + "\"";
						response.headers["Content-Type"] = "application/octet-stream";
					}
					else {
						response.headers["Content-Disposition"] = "inline; filename=\"" + resource.name() + "\"";
						response.headers["Content-Type"] = Mime::GetType(resource.name());
					}
					
					response.send();
					if(request.method == "HEAD") return;
					
					try {
						// Launch transfer
						if(hasRange) accessor->seekRead(rangeBegin);
						accessor->readBinary(*response.sock, rangeSize);	// let's go !
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
				else {
					Http::Response response(request, 200);
					response.send();
					
					Html page(response.sock);
					if(target == "/") page.header(name()+": Browse files");
					else page.header(name()+": "+target.substr(1));
					page.open("div","topmenu");
					if(!isSelf()) page.span(status().capitalized(), "status.button");
					page.link(prefix+"/search/","Search files",".button");
					page.link(prefix+request.url+"?playlist","Play all",".button");
					page.close("div");

					unsigned refreshPeriod = 5000;
					page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
						transition($('#status'), info.status.capitalize());\n\
						$('#status').removeClass().addClass('button').addClass(info.status);\n\
						if(info.newnotifications) playNotificationSound();\n\
					});");
				
					page.div("","list.box");
					page.javascript("listDirectory('"+prefix+request.url+"?json','#list',true);");
					page.footer();
				}
				
				return;
			}
			else if(directory == "search")
			{
				if(url != "/") throw 404;
				
				String match;
				if(!request.post.get("query", match))
					request.get.get("query", match);
				match.trim();
				
				if(request.get.contains("json") || request.get.contains("playlist"))
				{
					String tmp;
					
					int minAge = 0;
					int maxAge = 0;
					if(request.get.get("minage", tmp)) tmp >> minAge;
					if(request.get.get("maxage", tmp)) tmp >> maxAge;
					
					int count = 0;
					if(request.get.get("count", tmp)) tmp >> count;
					
					if(match.empty() && maxAge <= 0) throw 400;
					
					Resource::Query query(mAddressBook->user()->store());
					if(!match.empty()) query.setMatch(match);
					if(minAge > 0) query.setMinAge(minAge);
					if(maxAge > 0) query.setMaxAge(maxAge);
					if(count > 0) query.setLimit(count);
					
					SerializableSet<Resource> resources;
					bool success = query.submitRemote(resources, peering());
					if(isSelf()) success|= query.submitLocal(resources);
					if(!success) throw 404;
					
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
				
				if(match.empty()) page.header(name() + ": Search");
				else page.header(name() + ": Searching " + match);
				
				page.open("div","topmenu");
				if(!isSelf()) page.span(status().capitalized(), "status.button");
				page.openForm(prefix + "/search", "post", "searchForm");
				page.input("text", "query", match);
				page.button("search","Search");
				page.closeForm();
				page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
				if(!match.empty()) page.link(prefix+request.url+"?query="+match.urlEncode()+"&playlist","Play all",".button");
				page.close("div");

				unsigned refreshPeriod = 5000;
				page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
					transition($('#status'), info.status.capitalize());\n\
					$('#status').removeClass().addClass('button').addClass(info.status);\n\
					if(info.newnotifications) playNotificationSound();\n\
				});");
				
				if(!match.empty())
				{
					page.div("", "list.box");
					page.javascript("listDirectory('"+prefix+request.url+"?query="+match.urlEncode()+"&json','#list');");
				}
				
				page.footer();
				return;
			}
			else if(directory == "chat")
			{
				if(isSelf()) throw 404;
			  
				Http::Response response(request, 303);
                                response.headers["Location"] = mAddressBook->user()->urlPrefix() + "/messages/" + uniqueName().urlEncode() + "/";
                                response.send();
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
		LogWarn("AddressBook::Contact::http", e.what());
		throw 500;
	}
	
	throw 404;
}

void AddressBook::Contact::copy(const AddressBook::Contact *contact)
{
	Synchronize(mAddressBook);
	Assert(contact);
	
	mUniqueName = contact->mUniqueName;
	mName = contact->mName;
	mTracker = contact->mTracker;
	mSecret = contact->mSecret;
	mPeering = contact->mPeering;
	mRemotePeering = contact->mRemotePeering;
	mTime = contact->mTime;
	mDeleted = contact->mDeleted;
	
	addAddresses(contact->addresses());
}

void AddressBook::Contact::serialize(Serializer &s) const
{
	Synchronize(mAddressBook);
	
	StringMap map;
	map["uname"] << mUniqueName;
	map["name"] << mName;
	map["tracker"] << tracker();
	map["secret"] << mSecret;
	map["peering"] << mPeering;
	map["remote"] << mRemotePeering;
	map["time"] << mTime;
	map["deleted"] << mDeleted;
	
	s.outputMapBegin(2);
	s.outputMapElement(String("info"),map);
	s.outputMapElement(String("addrs"),mAddrs);
	s.outputMapEnd();
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	Synchronize(mAddressBook);
	
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
	AssertIO(s.inputMapElement(key, mAddrs) && key == "addrs");

	map["uname"] >> mUniqueName;
	map["name"] >> mName;
	map["tracker"] >> mTracker;
	map["secret"] >> mSecret;
	map["peering"] >> mPeering;
	map["remote"] >> mRemotePeering;

	if(map.contains("time")) map["time"] >> mTime;
	else mTime = Time::Now();
	
	if(map.contains("deleted")) map["deleted"] >> mDeleted;
	else mDeleted = false;
	
	// TODO: checks
	
	mFound = false;
	createProfile();
	return true;
}

bool AddressBook::Contact::isInlineSerializable(void) const
{
	return false; 
}

}
