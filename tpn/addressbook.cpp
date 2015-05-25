/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tpn/addressbook.h"
#include "tpn/user.h"
#include "tpn/network.h"
#include "tpn/config.h"
#include "tpn/cache.h"
#include "tpn/request.h"
#include "tpn/resource.h"
#include "tpn/indexer.h"
#include "tpn/html.h"
#include "tpn/portmapping.h"
#include "tpn/httptunnel.h"

#include "pla/crypto.h"
#include "pla/random.h"
#include "pla/file.h"
#include "pla/directory.h"
#include "pla/yamlserializer.h"
#include "pla/jsonserializer.h"
#include "pla/binaryserializer.h"
#include "pla/mime.h"

namespace tpn
{

AddressBook::AddressBook(User *user) :
	mUser(user)
{
	Assert(mUser != NULL);
	
	mFileName = mUser->profilePath() + "contacts";
	if(File::Exist(mFileName))
	{
		try {
			File file(mFileName, File::Read);
			JsonSerializer serializer(&file);
			serializer.input(*this);
			file.close();
		}
		catch(const Exception &e)
		{
			LogError("AddressBook", String("Loading failed: ") + e.what());
		}
	}
	
	Interface::Instance->add(urlPrefix(), this);
	
	mScheduler.schedule(this, 5.);
	mScheduler.repeat(this, 300.);
}

AddressBook::~AddressBook(void)
{
  	Interface::Instance->remove(urlPrefix(), this);
	NOEXCEPTION(clear());
}

User *AddressBook::user(void) const
{
 	return mUser; 
}

String AddressBook::userName(void) const
{
	Synchronize(this);
	return mUser->name(); 
}

String AddressBook::urlPrefix(void) const
{
	Synchronize(this);
	return mUser->urlPrefix() + "/contacts";
}

void AddressBook::clear(void)
{
	Synchronize(this);
	mContactsByIdentifier.clear();
	mContacts.clear();
}

void AddressBook::save(void) const
{
	Synchronize(this);

	SafeWriteFile file(mFileName);
	JsonSerializer serializer(&file);
	serializer.output(*this);
	file.close();
	
	const Contact *self = getSelf();
	if(self)
	{
		Resource resource;
		resource.process(mFileName, "contacts", "contacts", self->secret());
		
		mDigest = resource.digest();
		
		Notification notification;
		notification["type"] << "contacts";
		notification["digest"] << mDigest;
		
		DesynchronizeStatement(this, Network::Instance->send(user()->identifier(), self->identifier(), notification));
	}
}

String AddressBook::addContact(const String &name, const Rsa::PublicKey &pubKey)
{
	Synchronize(this);
	
	if(name.empty())
		throw Exception("Contact name is empty");
	
	if(!name.isAlphanumeric())
		throw Exception("Contact name is invalid: " + name);
	
	String uname = name;
	unsigned i = 1;
	while((mContacts.contains(uname))
		|| uname == userName())	// userName reserved for self
	{
		uname = name;
		uname << ++i;
	}
	
	Map<String, Contact>::iterator it = mContacts.insert(uname, Contact(this, uname, name, pubKey));
	mContactsByIdentifier.insert(it->second.identifier(), &it->second);
	Interface::Instance->add(it->second.urlPrefix(), &it->second);	
	it->second.init();
	
	save();
	
	return uname;
}

bool AddressBook::removeContact(const String &uname)
{
	Synchronize(this);
	
	Map<String, Contact>::iterator it = mContacts.find(uname);
	if(it == mContacts.end()) return false;
	
	mContactsByIdentifier.erase(it->second.identifier());
	mContacts.erase(it);
	save();
	return true;
}

AddressBook::Contact *AddressBook::getContact(const String &uname)
{
	Synchronize(this);
  
	Map<String, Contact>::iterator it = mContacts.find(uname);
	if(it != mContacts.end()) return &it->second;
	return NULL;
}

const AddressBook::Contact *AddressBook::getContact(const String &uname) const
{
	Synchronize(this);
  
	Map<String, Contact>::const_iterator it = mContacts.find(uname);
	if(it != mContacts.end()) return &it->second;
	return NULL;
}

int AddressBook::getContacts(Array<AddressBook::Contact*> &result)
{
	Synchronize(this);
	mContactsByIdentifier.getValues(result);
	return result.size();
}

int AddressBook::getContactsIdentifiers(Array<Identifier> &result) const
{
	Synchronize(this);
	mContactsByIdentifier.getKeys(result);
	return result.size();
}

void AddressBook::setSelf(const Rsa::PublicKey &pubKey)
{
	Synchronize(this);
	
	String uname = userName();
	Map<String, Contact>::iterator it = mContacts.insert(uname, Contact(this, uname, uname, pubKey));
	mContactsByIdentifier.insert(it->second.identifier(), &it->second);
	it->second.init();
	
	save();
}

AddressBook::Contact *AddressBook::getSelf(void)
{
	Synchronize(this);
	return getContact(userName());
}

const AddressBook::Contact *AddressBook::getSelf(void) const
{
	Synchronize(this);
	return getContact(userName());
}

Identifier AddressBook::getSelfIdentifier(void) const
{
	Synchronize(this);
	const Contact *self = getSelf();
	if(self) return self->identifier();
	else return Identifier::Null;
}

bool AddressBook::hasIdentifier(const Identifier &identifier) const
{
	Synchronize(this);
	return mContactsByIdentifier.contains(identifier);
}

bool AddressBook::send(const Notification &notification)
{
	Array<Identifier> keys;
	SynchronizeStatement(this, mContactsByIdentifier.getKeys(keys));

	bool success = false;
	for(int i=0; i<keys.size(); ++i)
        {
                Contact *contact = getContact(keys[i]);
		if(contact) success|= contact->send(notification);
	}
	
	return success;
}

bool AddressBook::send(const Mail &mail)
{
	Array<Identifier> keys;
	SynchronizeStatement(this, mContactsByIdentifier.getKeys(keys));

	bool success = false;
	for(int i=0; i<keys.size(); ++i)
        {
                Contact *contact = getContact(keys[i]);
		if(contact) success|= contact->send(mail);
	}
	
	return success;
}

void AddressBook::serialize(Serializer &s) const
{
	Synchronize(this);
	
	Serializer::ConstObjectMapping mapping;
	mapping["contacts"] = &mContacts;
	mapping["invitations"] = &mInvitations;
	
	s.outputObject(mapping);
}

bool AddressBook::deserialize(Serializer &s)
{
	Synchronize(this);
	
	Serializer::ObjectMapping mapping;
	mapping["contacts"] = &mContacts;
	mapping["invitations"] = &mInvitations;
	
	if(!s.inputObject(mapping)) return false;
	
	// Fill mContactsByIdentifier and set up contacts
	for(Map<String, Contact>::iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		Contact *contact = &it->second;
		contact->setAddressBook(this);
		contact->init();
		mContactsByIdentifier.insert(contact->identifier(), contact);
	}
	
	// Set addressbook in invitations
	for(int i=0; i<mInvitations.size(); ++i)
	{
		Invitation *invitation = &mInvitations[i];
		invitation->setAddressBook(this);
		invitation->init();
	}
	
	return true;
}

bool AddressBook::isInlineSerializable(void) const
{
	return false; 
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	Assert(!request.url.empty());
	user()->setOnline();
	
	try {
		if(request.url == "/")
		{
			if(request.method == "POST")
			{
				try {
					Synchronize(this);
					
					if(!user()->checkToken(request.post["token"], "contact")) 
						throw 403;
					
					String action = request.post["action"];
					/*if(action == "createinvitation")
					{
						String salt   = String::random(8,  Random::Key);
						String secret = String::random(16, Random::Key);
						String check  = Sha256().compute(salt + secret).toString().toLower().substr(0, 8);
						String code   = salt + secret + check;
						
						LogDebug("AddressBook::http", "Generating new invitation: " + salt);
						Assert(code.size() == 32);
						
						mInvitations.push_back(Invitation(this, salt, secret));
						(--mInvitations.end())->init();
						save();
						
						Http::Response response(request, 200);
						response.send();
						
						Html page(response.stream);
						page.header("New invitation");
						page.open("div", "invitation");
						page.text("Here is a new invitation secret code. Your contact must enter this code in his teapotnet instance to accept the invitation.");
						page.div(code, ".code");
						page.link(prefix + '/', "OK", ".button");
						page.close("div");
						page.footer();
						return;
					}
					else if(action == "acceptinvitation")
					{
						String code;
						request.post["code"] >> code;
						
						code.trim();
						if(code.size() != 32)
							throw Exception("Invalid invitation code");
						
						String salt   = code.substr(0, 8);
						String secret = code.substr(8, 16);
						String check  = Sha256().compute(salt + secret).toString().toLower().substr(0, 8);
						if(check != code.substr(24, 8))
							throw Exception("Invalid invitation code");

						LogDebug("AddressBook::http", "Accepting invitation: " + salt);
						mInvitations.push_back(Invitation(this, salt, secret));
						(--mInvitations.end())->init();
						save();
					}
					else if(action == "createsynchronization")
					{
						String secret = String::random(16, Random::Key);
						String check  = Sha256().compute(secret).toString().toLower().substr(0, 8);
						String code = secret + check;
						
						LogDebug("AddressBook::http", "Generating new synchronization secret");
						Invitation invitation(this, user()->name(), secret);
						invitation.setSelf(true);
						mInvitations.push_back(invitation);
						(--mInvitations.end())->init();
						save();
						
						Http::Response response(request, 200);
						response.send();
						
						Html page(response.stream);
						page.header("New synchronization");
						page.open("div", "invitation");
						page.div(code, ".code");
						page.link(prefix + '/', "OK", ".button");
						page.close("div");
						page.footer();
						return;
					}
					else if(action == "acceptsynchronization")
					{
						String code = request.post["code"];
						
						code.trim();
						if(code.size() != 24)
							throw Exception("Invalid synchronization code");
						
						String secret = code.substr(0, 16);
						String check  = Sha256().compute(secret).toString().toLower().substr(0, 8);
						if(check != code.substr(16, 8))
							throw Exception("Invalid synchronization code");
						
						LogDebug("AddressBook::http", "Accepting synchronization");
						Invitation invitation(this, user()->name(), secret);
						invitation.setSelf(true);
						mInvitations.push_back(invitation);
						(--mInvitations.end())->init();
						save();
						
						Http::Response response(request, 303);
						response.headers["Location"] = user()->urlPrefix();
						response.send();
						return;
					}
					else */if(action == "deletecontact")
					{
						Synchronize(this);
						String uname = request.post["argument"];
						removeContact(uname);
					}
					else if(action == "deleteinvitation")
					{
						Synchronize(this);
						BinaryString id;
						request.post["argument"].extract(id);
						
						// TODO: function removeInvitation
						for(int i=0; i<mInvitations.size(); ++i)
							if(mInvitations[i].identifier() == id)
							{
								mInvitations.erase(i);
								break;
							}
						save();
					}
					else {
						// TODO
						throw 500;
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

				JsonSerializer json(response.stream);
				json.setOptionalOutputMode(true);
				json.output(*this);
				return;
			}
			
			Http::Response response(request, 200);
			response.send();
			
			Html page(response.stream);
			page.header("Contacts");

			String token = user()->generateToken("contact");
			
			page.open("div",".box");
			page.open("h2");
			page.text("Add Contacts / Send invitations");
			page.close("h2");

			page.open("div", ".howtorequests");
			page.text("Here you can either send or accept invitations. Inviting someone is as easy as getting a new invitation secret code here and sending it to your contact. To accept an invitation, simply enter the secret code in the appropriate section.");
			page.close("div");

			page.close("div");

			// TODO: deprecated
			
			page.open("div",".box");
			page.openForm(prefix + "/", "post", "createinvitation");
			page.open("h2"); page.text("Invitation"); page.close("h2");
			page.input("hidden", "token", token);
			page.input("hidden", "action", "createinvitation");
			page.label("generate"); page.button("generate", "Generate invitation");
			page.closeForm();
			page.close("div");
			
			page.open("div",".box");
			page.openForm(prefix + "/", "post", "acceptinvitation");
			page.open("h2"); page.text("Invitation reply"); page.close("h2");
			page.input("hidden", "token", token);
			page.input("hidden", "action", "acceptinvitation");
			page.label("code", "Code"); page.input("text", "code"); page.br();
			page.label("accept"); page.button("accept", "Accept invitation");
			page.closeForm();
			page.close("div");
			
			page.open("div",".box");
			page.openForm(prefix + "/", "post", "createsynchronization");
			page.open("h2"); page.text("Synchronize device"); page.close("h2");
			page.input("hidden", "token", token);
			page.input("hidden", "action", "createsynchronization");
			page.label("generate"); page.button("generate", "Generate synchronization secret");
			page.closeForm();
			page.close("div");
			
			page.openForm(prefix+"/", "post", "actionForm");
			page.input("hidden", "action");
			page.input("hidden", "argument");
			page.input("hidden", "token", token);
			page.closeForm();
			
			// Synchronized block
			{
				Synchronize(this);
			
				Array<Contact*> contacts;
				getContacts(contacts);
				
				if(!contacts.empty())
				{
					page.open("div",".box");
					page.open("h2");
					page.text("Contacts");
					page.close("h2");
					
					page.open("table",".contacts");
					
					for(int i=0; i<contacts.size(); ++i)
					{
						Contact *contact = contacts[i];
						if(contact->isSelf()) continue;
						
						page.open("tr");
						page.open("td",".name");
						page.text(contact->name());
						page.close("td");
						page.open("td",".uname");
						page.link(contact->urlPrefix(), contact->uniqueName());
						page.close("td");
						page.open("td",".actions");
						page.openLink('#', ".deletelink");
						page.image("/delete.png", "Delete");
						page.closeLink();
						page.close("td");
						page.close("tr");
					}
					
					page.close("table");
					page.close("div");
					
					page.javascript("$('.contacts .deletelink').css('cursor', 'pointer').click(function(event) {\n\
						event.stopPropagation();\n\
						var uname = $(this).closest('tr').find('td.uname').text();\n\
						if(confirm('Do you really want to delete '+uname+' ?')) {\n\
							document.actionForm.action.value = 'deletecontact';\n\
							document.actionForm.argument.value = uname;\n\
							document.actionForm.submit();\n\
						}\n\
					});");
				}
				
				if(!mInvitations.empty())
				{
					page.open("div",".box");
					page.open("h2");
					page.text("Pending invitations");
					page.close("h2");
					page.open("table",".invitations");
					
					for(int i=0; i<mInvitations.size(); ++i)
					{
						Invitation *invitation = &mInvitations[i];
						
						page.open("tr");
						page.open("td",".name");
						if(invitation->isSelf()) page.text("(synchronization)");
						else page.text(invitation->identifier().toString());
						page.close("td");
						page.open("td",".actions");
						page.openLink('#', ".deletelink");
						page.image("/delete.png", "Delete");
						page.closeLink();
						page.close("td");
						page.close("tr");
					}
					
					page.close("table");
					page.close("div");
					
					page.javascript("$('.invitations .deletelink').css('cursor', 'pointer').click(function(event) {\n\
						event.stopPropagation();\n\
						var peering = $(this).closest('tr').find('td.peering').text();\n\
						if(confirm('Do you really want to delete this invitation ?')) {\n\
							document.actionForm.action.value = 'deleteinvitation';\n\
							document.actionForm.argument.value = peering;\n\
							document.actionForm.submit();\n\
						}\n\
					});");
				}
			}
			
			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("AddressBook::http", e.what());
		throw 500;
	}
	
	throw 404;
}

AddressBook::Invitation::Invitation(void) :
	mAddressBook(NULL),
	mIsSelf(false),
	mFound(false)
{

}

AddressBook::Invitation::Invitation(const Invitation &invitation) :
	mAddressBook(NULL),
	mIdentifier(invitation.mIdentifier),
	mIsSelf(invitation.mIsSelf),
	mFound(false)
{
	setAddressBook(invitation.mAddressBook);
}

AddressBook::Invitation::Invitation(AddressBook *addressBook, const Identifier &identifier) :
	mAddressBook(NULL),
	mIdentifier(identifier),
	mIsSelf(false),
	mFound(false)
{
	setAddressBook(addressBook);
}

AddressBook::Invitation::~Invitation(void)
{

}

void AddressBook::Invitation::setAddressBook(AddressBook *addressBook)
{
	Assert(!mAddressBook);
	mAddressBook = addressBook;
}

void AddressBook::Invitation::init(void)
{
	if(!mAddressBook) return;
	
	listen(identifier());
}

Identifier AddressBook::Invitation::identifier(void) const
{
	Synchronize(mAddressBook);
	return mIdentifier;
}

void AddressBook::Invitation::setSelf(bool self)
{
	Synchronize(mAddressBook);
	mIsSelf = self;
}

bool AddressBook::Invitation::isSelf(void) const
{
	Synchronize(mAddressBook);
	return mIsSelf;
}

bool AddressBook::Invitation::isFound(void) const
{
	Synchronize(mAddressBook);
	return mFound;
}

void AddressBook::Invitation::seen(const Identifier &peer)
{
	Synchronize(mAddressBook);
	
	// TODO
}

void AddressBook::Invitation::connected(const Identifier &peer)
{
	LogDebug("AddressBook::Invitation", "Connected");
	
	Notification notification;
	notification["type"] << "hello";
	notification["name"] << mAddressBook->user()->name();
	notification["publickey"] << mAddressBook->user()->publicKey();
	
	if(isSelf())
		notification["contacts"] << mAddressBook->mContacts.size();
	
	if(!Network::Instance->send(mAddressBook->user()->identifier(), peer, notification))
		throw Exception("Unable to send hello");
}

bool AddressBook::Invitation::recv(const Identifier &peer, const Notification &notification)
{
	Synchronize(mAddressBook);
	
	String type;
	notification.get("type", type);
	LogDebug("AddressBook::Invitation", "Incoming notification (type='" + type + "')");
	
	if(type == "hello")
	{
		String name;
		if(!notification.get("name", name))
			throw Exception("Missing contact name");
		
		if(!notification.contains("publickey"))
			throw Exception("Missing contact public key");
		
		Rsa::PublicKey pubKey;
		notification.get("publickey").extract(pubKey);
		
		if(pubKey.digest() != identifier())
			throw Exception("Wrong public key received from invited contact " + name);
		
		if(isSelf())
		{
			Assert(name == mAddressBook->userName());
			
			unsigned localContacts  = mAddressBook->mContacts.size();
			unsigned remoteContacts = 0;
			notification.get("contacts").extract(remoteContacts);
			
			LogDebug("AddressBook::Invitation", "Synchronization: local is " + String::hexa(Network::Instance->getNumber()) + " (" + String::number(localContacts) + " contacts), remote is " + String::hexa(peer.number()) + " (" + String::number(remoteContacts) + " contacts)");
			
			if(remoteContacts >= localContacts
				&& (remoteContacts != localContacts || Network::Instance->getNumber() > peer.number()))
			{
				LogDebug("AddressBook::Invitation", "Synchronization: mode is slave");
				
				mAddressBook->setSelf(pubKey);	// calls save()
				return true;			// so invitation is not deleted
			}
			
			LogDebug("AddressBook::Invitation", "Synchronization: mode is master, sending user");
			
			BinaryString randomSecret;
			Random rnd;
			rnd.read(randomSecret, 32);
			
			Resource resource;
			resource.process(mAddressBook->user()->fileName(), mAddressBook->user()->name(), "user", randomSecret.toString());
			
			Notification notification;
			notification["type"] << "user";
			notification["digest"] << resource.digest();
			notification["secret"] << randomSecret.toString();
			
			if(!Network::Instance->send(mAddressBook->user()->identifier(), peer, notification))
				throw Exception("Unable to send user");
			
			// Add self
			mAddressBook->setSelf(mAddressBook->user()->publicKey());	// calls save()
		}
		else {
			// Add contact
			mAddressBook->addContact(name, pubKey);				// calls save()
		}
	}
	else if(type == "user")
	{
		if(!isSelf())
			throw Exception("Received user from other than self");
		
		if(!notification.contains("digest"))
			throw Exception("Missing user digest");
		
		BinaryString digest;
		notification.get("digest").extract(digest);
		
		String secret;
		notification.get("secret", secret);
		
		LogDebug("AddressBook::Invitation", "Synchronization: receiving user");
		
		class ImportTask : public Task
		{
		public:
			ImportTask(User *user, const BinaryString &digest, const String &secret)
			{
				this->user = user;
				this->digest = digest;
				this->secret = secret;
			}
			
			void run(void)
			{
				Resource resource(digest);
				Resource::Reader reader(&resource, secret);
				JsonSerializer serializer(&reader);
				serializer.read(*user);
				
				LogDebug("AddressBook::Invitation", "Synchronization: user loaded: " + user->identifier().toString());
				user->save();
				delete this;	// autodelete
			}
			
		private:
			User *user;
			BinaryString digest;
			String secret;
		};
		
		Scheduler::Global->schedule(new ImportTask(mAddressBook->user(), digest, secret));
	}
	
	// Erase invitation and save
	{
		Synchronize(mAddressBook);
		
		LogDebug("AddressBook::Invitation", "Deleting invitation: " + identifier().toString());
		for(int i=0; i<mAddressBook->mInvitations.size(); ++i)
		{
			if(&mAddressBook->mInvitations[i] == this)
			{
				mAddressBook->mInvitations.erase(i);
				break;
			}
		}
		
		SafeWriteFile file(mAddressBook->mFileName);
		JsonSerializer serializer(&file);
		serializer.output(*mAddressBook);
		file.close();
	}
	
	// WARNING: Invitation is deleted here
	return true;
}

bool AddressBook::Invitation::auth(const Identifier &peer, const Rsa::PublicKey &pubKey)
{
	Synchronize(mAddressBook);
	
	if(pubKey.digest() == mIdentifier)
	{
		return true;
	}
	
	return false;
}

void AddressBook::Invitation::serialize(Serializer &s) const
{
	Synchronize(mAddressBook);  
	
	ConstSerializableWrapper<bool> isSelfWrapper(mIsSelf);
	
	Serializer::ConstObjectMapping mapping;
	mapping["identifier"] = &mIdentifier;
	mapping["isself"] = &isSelfWrapper;
	
	s.outputObject(mapping); 
}

bool AddressBook::Invitation::deserialize(Serializer &s)
{
	Synchronize(mAddressBook);

	SerializableWrapper<bool> isSelfWrapper(&mIsSelf);
	
	Serializer::ObjectMapping mapping;
	mapping["identifier"] = &mIdentifier;
	mapping["isself"] = &isSelfWrapper;
	
	if(!s.inputObject(mapping))
		return false;
	
	listen(identifier());
	return true;
}

bool AddressBook::Invitation::isInlineSerializable(void) const
{
	return false;
}

AddressBook::Contact::Contact(void) :
	mAddressBook(NULL),
	mBoard(NULL),
	mPrivateBoard(NULL)
{	

}

AddressBook::Contact::Contact(const Contact &contact) :
	mAddressBook(NULL),
	mBoard(NULL),
	mPrivateBoard(NULL),
	mUniqueName(contact.mUniqueName),
	mName(contact.mName),
	mPublicKey(contact.mPublicKey),
	mRemoteSecret(contact.mRemoteSecret),
	mInstances(contact.mInstances)
{
	setAddressBook(contact.mAddressBook);
	// no init
}

AddressBook::Contact::Contact(	AddressBook *addressBook, 
				const String &uname,
				const String &name,
			        const Rsa::PublicKey &pubKey) :
	mAddressBook(NULL),
	mUniqueName(uname),
	mName(name),
	mPublicKey(pubKey),
	mBoard(NULL),
	mPrivateBoard(NULL)
{
	Assert(!uname.empty());
	Assert(!name.empty());
	
	setAddressBook(addressBook);
}

AddressBook::Contact::~Contact(void)
{
	if(mAddressBook)
	{
		Interface::Instance->remove(urlPrefix(), this);
		mAddressBook->mScheduler.cancel(this);
	}
	
	delete mBoard;
}

void AddressBook::Contact::init(void)
{
	if(!mAddressBook) return;
	
	mAddressBook->mScheduler.schedule(this, 5.);
	mAddressBook->mScheduler.repeat(this, 300.);
		
	Interface::Instance->add(urlPrefix(), this);
	
	if(!mBoard)
	{
		mBoard = new Board("/" + identifier().toString(), "", name());	// Public board
	}
	
	listen(identifier());
}

void AddressBook::Contact::run(void)
{
	if(!mAddressBook) return;
	
	// TODO: hint core with new instance addresses
}

void AddressBook::Contact::setAddressBook(AddressBook *addressBook)
{
	Assert(!mAddressBook);
	mAddressBook = addressBook;
}

const Rsa::PublicKey &AddressBook::Contact::publicKey(void) const
{
	Synchronize(mAddressBook);
	return mPublicKey; 
}

Identifier AddressBook::Contact::identifier(void) const
{
	Synchronize(mAddressBook);
	return mPublicKey.digest();
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

String AddressBook::Contact::urlPrefix(void) const
{
	Synchronize(mAddressBook);
	if(mUniqueName.empty()) return "";
	if(isSelf()) return mAddressBook->user()->urlPrefix()+"/myself";
	return mAddressBook->urlPrefix()+"/"+mUniqueName;
}

BinaryString AddressBook::Contact::secret(void) const
{
	Synchronize(mAddressBook);
	
	if(isSelf())
	{
		return mAddressBook->user()->secret();
	}
	else {
		return localSecret() ^ remoteSecret();
	}
}

BinaryString AddressBook::Contact::localSecret(void) const
{
	Synchronize(mAddressBook);
	return mAddressBook->user()->getSecretKey(identifier().toString());
}

BinaryString AddressBook::Contact::remoteSecret(void) const
{
	return mRemoteSecret;
}

bool AddressBook::Contact::Contact::isSelf(void) const
{
	return (mUniqueName == mAddressBook->userName());
}

bool AddressBook::Contact::isConnected(void) const
{
	return Network::Instance->hasLink(mAddressBook->user()->identifier(), identifier()); 
}

bool AddressBook::Contact::isConnected(const Identifier &instance) const
{
	return Network::Instance->hasLink(mAddressBook->user()->identifier(), instance);
}

bool AddressBook::Contact::hasInstance(const Identifier &instance) const
{
	Synchronize(mAddressBook);
	return mInstances.contains(instance);
}

int AddressBook::Contact::getInstances(Set<Identifier> &result) const
{
	Synchronize(mAddressBook);
	return mInstance.getKeys(result);
}

bool AddressBook::Contact::getInstanceAddresses(const Identifier &instance, Set<Address> &result) const
{
	Synchronize(mAddressBook);
	
	InstancesMap::const_iterator it = mInstances.find(instance);
	if(it != mInstances.end())
	{
		it->second.getAddresses(result);
		return true;
	}
	
	return false;
}

bool AddressBook::Contact::send(const Notification &notification)
{
	return notification.send(identifier());
}

bool AddressBook::Contact::send(const Mail &mail)
{
	// TODO
	return false;
}

void AddressBook::Contact::seen(const Identifier &peer)
{
	Synchronize(mAddressBook);
	Assert(peer == identifier());
  
	LogDebug("AddressBook::Contact", "Contact " + uniqueName() + " is seen");
	mInstances[peer.number()].setSeen();
}

void AddressBook::Contact::connected(const Identifier &peer)
{
	Synchronize(mAddressBook);
	Assert(peer == identifier());
	
	LogDebug("AddressBook::Contact", "Connected");
  
	Notification notification;
	notification["type"] << "hello";
	notification["secret"] << localSecret();
	notification["instance"] << Network::Instance->getName();
	
	if(!Network::Instance->send(mAddressBook->user()->identifier(), peer, notification))
		throw Exception("Unable to send hello");
}

bool AddressBook::Contact::recv(const Identifier &peer, const Notification &notification)
{
	// Not synchronized
  
	Assert(peer == identifier());
	
	String type;
	notification.get("type", type);
	LogDebug("AddressBook::Contact", "Incoming notification from " + uniqueName() + " (type='" + type + "')");
	
	if(type == "hello")
	{
		Synchronize(mAddressBook);
		
		if(!notification.contains("secret"))
			throw Exception("Missing contact secret");
		
		BinaryString secret;
		notification.get("secret").extract(secret);
		
		String instance;
		if(!notification.get("instance", instance))
			throw Exception("Missing instance name");
		
		mInstances[peer.number()].setName(instance);
		
		if(secret != mRemoteSecret)
			mRemoteSecret = secret;
		
		mAddressBook->save();
	}
	if(type == "contacts")
	{
		if(!isSelf()) throw Exception("Received contacts notification from other than self");
		
		BinaryString digest;
		notification.get("digest").extract(digest);
		
		class ImportTask : public Task
		{
		public:
			ImportTask(AddressBook *addressBook, const BinaryString &digest)
			{
				this->addressBook = addressBook;
				this->digest = digest;
			}
			
			void run(void)
			{
				Resource resource(digest);
				Resource::Reader reader(&resource, addressBook->user()->secret());
				JsonSerializer serializer(&reader);
				serializer.input(*addressBook);
				addressBook->save();
				delete this;	// autodelete
			}
			
		private:
			AddressBook *addressBook;
			BinaryString digest;
		};
		
		if(digest != mAddressBook->mDigest)
		{
			Scheduler::Global->schedule(new ImportTask(mAddressBook, digest));
			mAddressBook->mDigest = digest;
		}
	}
	
	return true;
}

bool AddressBook::Contact::auth(const Identifier &peer, const Rsa::PublicKey &pubKey)
{
	return (pubKey == publicKey());
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
	mAddressBook->user()->setOnline();

	try {
		String url = request.url;
		if(url.empty() || url[0] != '/') throw 404;
		
		if(url == "/")
		{
			if(request.get.contains("json"))
			{
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();
				
				JsonSerializer json(response.stream);
				json.setOptionalOutputMode(true);
				json.output(*this);
				return;
			}
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.stream);
			if(isSelf()) page.header("Myself");
			else {
				page.header("Contact: "+name());
				page.open("div", "topmenu");
				page.span("TODO", "status.button");	// TODO
				page.close("div");
			}
			
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
			
			if(!isSelf())
			{
				page.open("tr");
				page.open("td");
					page.openLink(prefix + "/board/");
					page.image("/icon_board.png", "Board", ".bigicon");
					page.closeLink();
				page.close("td");
				page.open("td",".title");
					page.text("Board");
					//page.span("", "mailscount.mailscount");
				page.close("td");
				page.close("tr");
				
				page.open("tr");
				page.open("td");
					page.openLink(prefix + "/chat/");
					page.image("/icon_chat.png", "Messages", ".bigicon");
					page.closeLink();
				page.close("td");
				page.open("td",".title");
					page.text("Messages");
					//page.span("", "mailscount.mailscount");
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
			
			// TODO: display instances names
			unsigned refreshPeriod = 5000;
			page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				var msg = '';\n\
				if(info.mails != 0) msg = ' ('+info.mails+')';\n\
				transition($('#mailscount'), msg);\n\
				$('#instances').empty();\n\
				if($.isEmptyObject(info.instances)) $('#instances').text('No connected instance');\n\
				else $.each(info.instances, function(number, data) {\n\
					$('#instances').append($('<tr>')\n\
						.append($('<td>').addClass('name').text(data.name)));\n\
				});\n\
			});");
			
			// Recent files
			/*page.open("div",".box");
			page.open("h2");
			page.text("Recent files");
			page.close("h2");
			page.div("", "recent");
			page.close("div");
			
			int maxAge = 60*60*24*7;	// 7 days
			int count = 20;
			page.javascript("listDirectory('"+prefix+"/search?json&maxage="+String::number(maxAge)+"&count="+String::number(count)+"','#recent',false,true);");
			*/
			page.footer();
			return;
		}
		
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
			
			response.stream->writeLine("#EXTM3U");
			response.stream->writeLine(String("#EXTINF:-1, ") + APPNAME + " stream");
			response.stream->writeLine(link);
			return;
		}
		
		if(directory == "files")
		{
			Request *req = new Request("/files/" + identifier().toString() + url);
			String reqPrefix = req->urlPrefix();
			req->autoDelete();
			
			Http::Response response(request, 200);
			response.send();
			
			Html page(response.stream);
			
			if(url == "/") page.header(name() + ": Browse files");
			else page.header(name() + ": " + url.substr(1));
			
			page.open("div","topmenu");
			if(!isSelf()) page.span("Unknown", "status.button");
			page.link(prefix+"/search/", "Search files", ".button");
			page.link(reqPrefix+"?playlist", "Play all", "playall.button");
			page.close("div");

			unsigned refreshPeriod = 5000;
			page.javascript("setCallback('"+prefix+"/?json', "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				if(info.newmails) playMailSound();\n\
			});");
		
			page.div("","list.box");
			page.javascript("listDirectory('"+reqPrefix+"','#list',true,true);");
			page.footer();
			return;
		}
		else if(directory == "search")
		{
			if(url != "/") throw 404;
			
			String match;
			if(!request.post.get("query", match))
				request.get.get("query", match);
			match.replace('/', ' ');
			match.trim();
			
			String reqPrefix;
			if(!match.empty())
			{
				Request *req = new Request("/files/" + identifier().toString() + "?" + match, identifier(), false);
				reqPrefix = req->urlPrefix();
				req->autoDelete();
			}
			
			Http::Response response(request, 200);
			response.send();
				
			Html page(response.stream);
			
			if(match.empty()) page.header(name() + ": Search");
			else page.header(name() + ": Searching " + match);
			
			page.open("div","topmenu");
			if(!isSelf()) page.span("TODO", "status.button");	// TODO
			page.openForm(prefix + "/search", "post", "searchForm");
			page.input("text", "query", match);
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			if(!match.empty()) page.link(reqPrefix+"?playlist","Play all",".button");
			page.close("div");

			unsigned refreshPeriod = 5000;
			page.javascript("setCallback('"+prefix+"/?json', "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				if(info.newmails) playMailSound();\n\
			});");
			
			if(!match.empty())
			{
				page.div("", "list.box");
				page.javascript("listDirectory('"+reqPrefix+"','#list',true,true);");
			}
			
			page.footer();
			return;
		}
		else if(directory == "avatar")
		{
			// TODO
			throw 404;
		}
		else if(directory == "board")
		{
			if(isSelf()) throw 404;
			
			Http::Response response(request, 301);	// Moved permanently
			response.headers["Location"] = mBoard->urlPrefix();
			response.send();
			return;
		}
		else if(directory == "chat")
		{
			if(isSelf()) throw 404;
			
			if(!mPrivateBoard)
			{
				BinaryString id = mAddressBook->user()->identifier().digest() ^ identifier().digest();
				mPrivateBoard = new Board("/" + id.toString(), secret().toString(), name());
			}
			
			Http::Response response(request, 301);	// Moved permanently
			response.headers["Location"] = mPrivateBoard->urlPrefix();
			response.send();
			return;
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

void AddressBook::Contact::serialize(Serializer &s) const
{
	Synchronize(mAddressBook);
	
	Serializer::ConstObjectMapping mapping;
	mapping["publickey"] = &mPublicKey;
	mapping["uname"] = &mUniqueName;
	mapping["name"] = &mName;
	mapping["instances"] = &mInstances;
	
	String prefix, status;
	ConstSerializableWrapper<uint32_t> messages(uint32_t(0));	// TODO
	if(s.optionalOutputMode())
	{
		prefix = urlPrefix();
		status = (isConnected() ? "connected" : "disconnected");
		
		mapping["prefix"] = &prefix;
		mapping["status"] = &status;
		mapping["messages"] = &messages;
	}
	else {
		mapping["secret"] = &mRemoteSecret;
		
	}
	
	s.outputObject(mapping);
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	Synchronize(mAddressBook);
	
	Serializer::ObjectMapping mapping;
	mapping["publickey"] = &mPublicKey;
	mapping["uname"] = &mUniqueName;
	mapping["name"] = &mName;
	mapping["instances"] = &mInstances;
	mapping["secret"] = &mRemoteSecret;
	
	if(!s.inputObject(mapping))
		return false;
	
	// TODO: sanity checks
	return true;
}

bool AddressBook::Contact::isInlineSerializable(void) const
{
	return false; 
}

AddressBook::Contact::Instance::Instance(void) :
	mNumber(0),
	mLastSeen(0)
{
  
}

AddressBook::Contact::Instance::Instance(const Identifier &id) :
	mIdentifier(id),
	mLastSeen(0)
{
	Assert(mNumber != 0);
}

AddressBook::Contact::Instance::~Instance()
{
  
}

Identifier &AddressBook::Contact::Instance::identifier(void) const
{
	return mIdentifier;
}

String AddressBook::Contact::Instance::name(void) const
{
	return mName;
}

void AddressBook::Contact::Instance::setName(const String &name)
{
	mName = name;
}

Time AddressBook::Contact::Instance::lastSeen(void) const
{
	return mLastSeen;
}

void AddressBook::Contact::Instance::setSeen(void)
{
	mLastSeen = Time::Now(); 
}

void AddressBook::Contact::Instance::addAddress(const Address &addr)
{
	mAddrs.insert(addr, Time::Now());
}

void AddressBook::Contact::Instance::addAddresses(const Set<Address> &addrs)
{
	for(Set<Address>::iterator it = addrs.begin();
		it != addrs.end();
		++it)
	{
		addAddress(*it);
	}
}

int AddressBook::Contact::Instance::getAddresses(Set<Address> &result) const
{
	mAddrs.getKeys(result);
	return result.size();
}

void AddressBook::Contact::Instance::serialize(Serializer &s) const
{
	ConstSerializableWrapper<uint64_t> numberWrapper(mNumber);
	
	Serializer::ConstObjectMapping mapping;
	mapping["number"] = &numberWrapper;
	mapping["name"] = &mName;
	mapping["addresses"] = &mAddrs;
	
	s.outputObject(mapping);
}

bool AddressBook::Contact::Instance::deserialize(Serializer &s)
{
	SerializableWrapper<uint64_t> numberWrapper(&mNumber);
	
	Serializer::ObjectMapping mapping;
	mapping["number"] = &numberWrapper;
	mapping["name"] = &mName;
	mapping["addresses"] = &mAddrs;
	
	// TODO: sanity checks
	return s.inputObject(mapping);
}

bool AddressBook::Contact::Instance::isInlineSerializable(void) const
{
	return false;
}

}
