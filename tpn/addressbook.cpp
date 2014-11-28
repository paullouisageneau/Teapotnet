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
#include "tpn/core.h"
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
			load(file);
			file.close();
		}
		catch(const Exception &e)
		{
			LogError("AddressBook", String("Loading failed: ") + e.what());
		}
	}
	
	Interface::Instance->add(urlPrefix(), this);
	
	mScheduler.schedule(this, 1.);
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

void AddressBook::load(Stream &stream)
{
	Synchronize(this);

	JsonSerializer serializer(&stream);
	serializer.input(*this);
	
	LogDebug("AddressBook::load", "Loaded " + String::number(mContacts.size()) + " contacts and " + String::number(mInvitations.size()) + " invitations");
}

void AddressBook::save(Stream &stream) const
{
	Synchronize(this);
  
	JsonSerializer serializer(&stream);
	serializer.output(*this);
}

void AddressBook::save(void) const
{
	Synchronize(this);

	SafeWriteFile file(mFileName);
	save(file);
	file.close();
}

void AddressBook::sendContacts(const Identifier &peer) const
{
	Synchronize(this);
	
	if(peer == Identifier::Null)
		throw Exception("Prevented AddressBook::send() to broadcast");
	
	// TODO: send file hash
}

void AddressBook::sendContacts(void) const
{
	Synchronize(this);
	
	const Contact *self = getSelf();
	if(self && self->isConnected())
		sendContacts(self->identifier());
}

String AddressBook::addContact(const String &name, const Rsa::PublicKey &pubKey, const String &tracker)
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
	
	Map<String, Contact>::iterator it = mContacts.insert(uname, Contact(this, uname, name, pubKey, tracker));
	mContactsByIdentifier.insert(it->second.identifier(), &it->second);
	Interface::Instance->add(it->second.urlPrefix(), &it->second);	
	
	save();
	sendContacts();
	
	return uname;
}

bool AddressBook::removeContact(const String &uname)
{
	Synchronize(this);
	
	Map<String, Contact>::iterator it = mContacts.find(uname);
	if(it != mContacts.end())
	{
		mContactsByIdentifier.erase(it->second.identifier());
		mContacts.erase(it);
		return false;
	}
	
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

void AddressBook::getContacts(Array<AddressBook::Contact*> &result)
{
	Synchronize(this);
  
	mContactsByIdentifier.getValues(result);
	Contact *self = getSelf();
	if(self) result.remove(self);
}

void AddressBook::setSelf(const Rsa::PublicKey &pubKey)
{
	Synchronize(this);
	
	String uname = userName();
	Map<String, Contact>::iterator it = mContacts.insert(uname, Contact(this, uname, uname, pubKey));
	mContactsByIdentifier.insert(it->second.identifier(), &it->second);
	
	save();
	sendContacts();
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
	
	// TODO: should load in a temporary map to trigger unregister and delete messages
	clear();
	
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
		mContactsByIdentifier[contact->uniqueName()] = contact;
	}
	
	// Set addressbook in invitations
	for(int i=0; i<mInvitations.size(); ++i)
	{
		Invitation *invitation = &mInvitations[i];
		invitation->setAddressBook(this);
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
					if(action == "createinvitation")
					{
						uint64_t pin = PinGenerate();
						String code = String::random(32, Random::Crypto);
						
						LogDebug("AddressBook::http", "Generating new invitation with code: " + code);
						mInvitations.append(Invitation(this, code, pin, user()->tracker()));
						save();

						Http::Response response(request, 200);
                        			response.send();

                        			Html page(response.stream);
                        			page.header("New invitation");
						page.text("Code: " + code + "\n");
						page.text("PIN: " + String::number64(pin, 10) + "\n");
						page.footer();
						return;
					}
					else if(action == "acceptinvitation")
					{
						uint64_t pin;
						String code;
						String tracker;
						request.post["pin"] >> pin;
						request.post["code"] >> code;

						tracker = request.post["tracker"];
						if(tracker.empty()) tracker = user()->tracker(); 

						code.trim();
						if(code.size() != 32) throw Exception("Invalid invitation code");
						if(!PinIsValid(pin)) throw Exception("Invalid invitation PIN");

						LogDebug("AddressBook::http", "Accepting invitation with code: " + code);
						mInvitations.append(Invitation(this, code, pin, tracker));
						save();
					}
					if(action == "createsynchronization")
					{
						String secret = String::random(32, Random::Crypto);
						
						LogDebug("AddressBook::http", "Generating new synchronization secret");
						Invitation invitation(this, user()->name(), secret, user()->tracker());
						invitation.setSelf(true);
						mInvitations.append(invitation);
						save();
						
						Http::Response response(request, 200);
						response.send();
						
						Html page(response.stream);
						page.header("New synchronization");
						page.text("Secret: " + secret + "\n");
						page.footer();
						return;
					}
					else if(action == "acceptsynchronization")
					{
						String secret;
						request.post["secret"] >> secret;
						
						secret.trim();
						if(!secret.empty())
						{
							LogDebug("AddressBook::http", "Accepting synchronization");
							Invitation invitation(this, user()->name(), secret, user()->tracker());
							invitation.setSelf(true);
							mInvitations.append(invitation);
							save();
						}
						
						Http::Response response(request, 303);
						response.headers["Location"] = user()->urlPrefix();
						response.send();
						return;
					}
					else if(action == "deletecontact")
					{
						Synchronize(this);
						String uname = request.post["argument"];
						removeContact(uname);
					}
					else if(action == "deleteinvitation")
					{
						Synchronize(this);
						String name = request.post["argument"];
						
						// TODO: function removeInvitation
						for(int i=0; i<mInvitations.size(); ++i)
							if(mInvitations[i].name() == name)
							{
								mInvitations.erase(i);
								break;
							}
						save();
					}
					else {
						// TODO
						throw 500;
						return;
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
			
			// Loading will block here if a contact is added at the same time
			Array<Contact*> contacts;
			getContacts(contacts);
			
			String token = user()->generateToken("contact");

			// Parameters that are to be sent in friend request
			const String centralizedFriendSystemUrl = RAPTUREURL;
			int lengthUrl = centralizedFriendSystemUrl.length();
			String postrequest = "postrequest.php";
			String secretgen = "secretgen.php";
			String laststep = "laststep.php";
			String finalize = "finalize.php";
			String gcontacts = "gcontacts.php";
			String tpn_id = user()->name()+"@"+user()->tracker();
			String nameProfile = user()->profile()->realName();
			String mailProfile = user()->profile()->eMail();
			
			page.open("div",".box");
			page.open("h2");
			page.text("Add Contacts / Send invitations");
			page.close("h2");

			page.open("div", ".howtorequests");
			page.text("In this section, you can either add your friends or send invitations to them. These invitations contain a personalized code that enable them to reach you and ensure your communications are encrypted. You will have to confirm request by pasting a code in the 'Accept Request' section.");
			page.close("div");

			/*page.open("div","invitationmethods");

			page.open("span",".invitationimg");
			page.openLink(centralizedFriendSystemUrl+gcontacts+"?tpn_id="+tpn_id, ".gmailimg", true);
			page.image("/gmail.png","GMail","gmailimg");
			page.closeLink();
			page.closeForm();
			page.close("span");

			page.open("span",".invitationimg");
			page.image("/mail.png","Mail","mailimg");
			page.close("span");

			page.open("span",".invitationimg");
			page.image("/facebook_by_benstein.png","Facebook","fbimg");
			page.close("span");

			page.open("span",".invitationimg");
			page.image("/spy.png","Classic way","spyimg");
			page.close("span");

			page.close("div");

			page.open("div","howtotext");
			page.close("div");*/

			page.close("div");

			// Js globals loaded from C++
			/*page.javascript("var centralizedFriendSystemUrl = \""+centralizedFriendSystemUrl+"\"; \n\
					var nameProfile = \""+nameProfile+"\";\n\
					var mailProfile = \""+mailProfile+"\";\n\
					var postUrl1 = \""+centralizedFriendSystemUrl+"\"+\""+postrequest+"\";\n\
					var tpn_id = \""+tpn_id+"\" \n\
					var prefix = \""+prefix+"\" \n\
					var token = \""+token+"\" \n\
					");
			
			page.openForm(prefix+"/","post","newcontact");
			page.open("div","newcontactdiv.box");
			page.open("h2");
			page.text("Add contact");
			page.close("h2");
			page.input("hidden", "token", token);
			page.label("name","Teapotnet ID"); page.input("text","name"); page.br();
			page.label("secret","Secret"); page.input("text","secret","",true); page.br();
			page.label("add"); page.button("add","Add contact");
			page.close("div");
			page.closeForm();

			//page.openForm(prefix+"/","post","sendrequestform");
			page.open("div","sendrequest.box");
			page.open("h2");
			page.text("Send friend request");
			page.close("h2");
			page.label("mail_sender","Your Mail"); page.input("text","mail_sender"); page.br();
			page.label("mail_receiver","Your friend's Mail"); page.input("text","mail_receiver"); page.br();
			page.label("sendinvitation"); page.button("sendinvitation","Send invitation");
			page.close("div");
			//page.closeForm();
			
			//page.openForm(prefix+"/","post","acceptrequestform");
			page.open("div","acceptrequest.box");
			page.open("h2");
			page.text("Accept request");
			page.close("h2");
			page.open("p");
			page.text("If you've received from a friend a Teapotnet code, paste it here. Your friend will automatically be added to your contacts list.");
			page.close("p");
			page.input("text","posturl");
			page.button("postfriendrequest","Go !"); 
			page.close("div");
			//page.closeForm();
			
			// Load rapture.js
			page.raw("<script type=\"text/javascript\" src=\"/rapture.js\"></script>");
			*/
			
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
			page.label("pin", "PIN"); page.input("text", "pin", "", true); page.br();
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
                                page.text("Invitations");
                                page.close("h2");
				page.open("table",".invitations");
				
				for(int i=0; i<mInvitations.size(); ++i)
				{
					Invitation *invitation = &mInvitations[i];

					page.open("tr");
					page.open("td",".name");
					page.text(invitation->name());
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
					var name = $(this).closest('tr').find('td.name').text();\n\
					if(confirm('Do you really want to delete this invitation ?')) {\n\
						document.actionForm.action.value = 'deleteinvitation';\n\
						document.actionForm.argument.value = name;\n\
						document.actionForm.submit();\n\
					}\n\
				});");
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

bool AddressBook::publish(const Identifier &identifier, const String &tracker)
{
	try {
		String url("http://" + tracker + "/tracker/?id=" + identifier.toString());
		
		Set<Address> set;
		Config::GetExternalAddresses(set);
		
		String addresses;
		for(	Set<Address>::iterator it = set.begin();
			it != set.end();
			++it)
		{
			if(!addresses.empty()) addresses+= ',';
			addresses+= it->toString();
		}
		
		StringMap post;
		post["instance"] << Core::Instance->getNumber();
		post["addresses"] = addresses;
	
		const String externalPort = Config::Get("external_port");
		if(!externalPort.empty() && externalPort != "auto")
		{
			post["port"] = externalPort;
		}
                else if(!PortMapping::Instance->isAvailable()
			|| !PortMapping::Instance->getExternalAddress(PortMapping::TCP, Config::Get("port").toInt()).isPublic())	// Cascading NATs
		{
			post["port"] = Config::Get("port");
		}
		
		// TODO: separately report known public addresses to tracker
		/*if(!Core::Instance->isPublicConnectable())
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
		}*/
		
		if(Http::Post(url, post) == 200)
			return true;
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

	return false;
}

bool AddressBook::query(const Identifier &identifier, const String &tracker, Serializable &result)
{
	String host(tracker);
	if(host.empty()) host = user()->tracker();
	
	try {
		String url = "http://" + host + "/tracker/?id=" + identifier.toString();
		  
		String json;
		if(Http::Get(url, &json) == 200) 
		{
			JsonSerializer serializer(&json);
			return serializer.input(result);
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
	
	return false;
}

void AddressBook::run(void)
{
	publish(user()->identifier(), user()->tracker()); 
}

uint64_t AddressBook::PinGenerate(void)
{
	const unsigned digits = 10;

	Random rnd(Random::Crypto);
	uint64_t max = 1;
	for(int i=0; i<digits-1; ++i) max*= 10;
	uint64_t val = rnd.uniform(uint64_t(1), max);
	return val * 10 + PinChecksum(val);
}

uint64_t AddressBook::PinChecksum(uint64_t pin)
{
	uint64_t accum = 0;
	while (pin)
	{
		accum += 3 * (pin % 10);
		pin /= 10;
		accum += pin % 10;
		pin /= 10;
	}

	return (10 - accum % 10) % 10;
}

bool AddressBook::PinIsValid(uint64_t pin)
{
	return PinChecksum(pin / 10) == (pin % 10);
}

AddressBook::Invitation::Invitation(void) :
	mAddressBook(NULL),
	mFound(false)
{

}

AddressBook::Invitation::Invitation(const Invitation &invitation) :
	mAddressBook(NULL),
	mName(invitation.mName),
	mSecret(invitation.mSecret),
	mPeering(invitation.mPeering),
	mTracker(invitation.mTracker),
	mFound(false)
{
	setAddressBook(invitation.mAddressBook);
	// no init
}

AddressBook::Invitation::Invitation(AddressBook *addressBook, const Identifier &identifier, const String &name, const String &tracker) :
	mAddressBook(NULL),
	mName(name),
	mTracker((!tracker.empty() ? tracker : addressBook->user()->tracker())),
	mFound(false)
{
	mPeering = identifier;	// In this case, peering is set to the identifier and secret is empty.
	
	setAddressBook(addressBook);
	init();
}

AddressBook::Invitation::Invitation(AddressBook *addressBook, const String &code, uint64_t pin, const String &tracker) :
	mAddressBook(NULL),
	mName(code),
	mTracker((!tracker.empty() ? tracker : addressBook->user()->tracker())),
	mFound(false)
{
	generate(code, String::number64(pin, 10));
	
	setAddressBook(addressBook);
	init();
}

AddressBook::Invitation::Invitation(AddressBook *addressBook, const String &name, const String &secret, const String &tracker) :
	mAddressBook(NULL),
	mName(name),
	mTracker((!tracker.empty() ? tracker : addressBook->user()->tracker())),
	mFound(false)
{
	String salt = "Teapotnet/" + std::min(addressBook->userName(), name) + "/" + std::max(addressBook->userName(), name);
	generate(salt, secret);
	
	setAddressBook(addressBook);
	init();
}

AddressBook::Invitation::~Invitation(void)
{
	if(mAddressBook)
		mAddressBook->mScheduler.cancel(this);
}

void AddressBook::Invitation::generate(const String &salt, const String &secret)
{
	const unsigned iterations = 10000;
	BinaryString bsalt;
	
	// Compute salt
	String tmp(salt);
	Sha256().compute(tmp, bsalt);
	
	// Compute secret
	Sha256().pbkdf2_hmac(secret, bsalt, mSecret, 32, iterations);
	
	// Compute peering
	Sha256().pbkdf2_hmac(mSecret, bsalt, mPeering, 32, iterations);
}

void AddressBook::Invitation::run(void)
{
	if(!mAddressBook) return;
	
	mAddressBook->publish(peering(), tracker());
	
	SerializableMap<uint64_t, SerializableSet<Address> > result;
	if(mAddressBook->query(peering(), tracker(), result))
	{
		for(SerializableMap<uint64_t, SerializableSet<Address> >::iterator it = result.begin();
			it != result.end();
			++it)
		{
			if(it->first != Core::Instance->getNumber())
			{
				LogDebug("AddressBook::Invitation::run", "Found " + String::number(it->second.size()) + " addresses (instance " + String::hexa(it->first) + ")");
				
				Core::Locator locator(mAddressBook->user(), it->second);
				if(mSecret.empty()) 
				{
					// If there is no secret, this is not a peering but a normal identifier.
					locator.identifier = peering();
					locator.name = name();
				}
				else {
					locator.peering = peering();
					locator.secret = secret();
				}
				
				Core::Instance->connect(locator);
			}
		}	
	}
}

void AddressBook::Invitation::setAddressBook(AddressBook *addressBook)
{
	Assert(!mAddressBook);
	mAddressBook = addressBook;
}

void AddressBook::Invitation::init(void)
{
	if(!mAddressBook) return;
	
	mAddressBook->mScheduler.schedule(this, 1.);
	mAddressBook->mScheduler.repeat(this, 300.);
	
	listen(peering());
}

String AddressBook::Invitation::name(void) const
{
	Synchronize(mAddressBook);
	return mName;
}

BinaryString AddressBook::Invitation::secret(void) const
{
	Synchronize(mAddressBook);
	return mSecret;
}

Identifier AddressBook::Invitation::peering(void) const
{
	Synchronize(mAddressBook);
	return mPeering;
}

String AddressBook::Invitation::tracker(void) const
{
	Synchronize(mAddressBook);
	return mTracker;
}

uint32_t AddressBook::Invitation::checksum(void) const
{
	Synchronize(mAddressBook);
	return mPeering.digest().checksum32() + uint32_t(mPeering.number());
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
	notification["tracker"] << mAddressBook->user()->tracker();
	notification["publickey"] << mAddressBook->user()->publicKey();
	
	if(isSelf())
		notification["contacts"] << mAddressBook->mContacts.size();
	
	// TODO: force direct
	if(!Core::Instance->send(peer, notification))
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
		
		String tracker;
		if(!notification.get("tracker", tracker))
			throw Exception("Missing tracker");
		
		if(!notification.contains("publickey"))
			throw Exception("Missing contact public key");
		
		Rsa::PublicKey pubKey;
		notification.get("publickey").extract(pubKey);
		
		if(mSecret.empty() && pubKey.digest() != peering())
			throw Exception("Wrong public key received from invited contact " + name);
		
		if(isSelf())
		{
			Assert(name == mAddressBook->userName());
			
			unsigned localContacts  = mAddressBook->mContacts.size();
			unsigned remoteContacts = 0;
			notification.get("contacts").extract(remoteContacts);
			
			if(remoteContacts >= localContacts
				&& (remoteContacts != localContacts || mAddressBook->user()->identifier() > peer))
			{
				mAddressBook->setSelf(pubKey);	// calls save()
				return true;			// so invitation is not deleted
			}
			
			Notification notification;
			notification["type"] << "self";
			notification["publickey"] << mAddressBook->user()->publicKey();
			notification["privatekey"] << mAddressBook->user()->privateKey();
				
			// TODO: force direct
			if(!Core::Instance->send(peer, notification))
				throw Exception("Unable to send self message");
			
			// TODO: tracker
			
			// Add self
			mAddressBook->setSelf(mAddressBook->user()->publicKey());	// calls save()
		}
		else {
			// Add contact
			mAddressBook->addContact(name, pubKey, tracker);		// calls save()
		}
	}
	else if(type == "self")
	{
		if(!notification.contains("publickey"))
			throw Exception("Missing self public key");
		
		if(!notification.contains("privatekey"))
			throw Exception("Missing self private key");
		
		Rsa::PublicKey pubKey;
		notification.get("publickey").extract(pubKey);
		
		Rsa::PrivateKey privKey;
		notification.get("privatekey").extract(privKey);
		
		mAddressBook->user()->setKeyPair(pubKey, privKey);
	}
	
	// Erase invitation
	for(int i=0; i<mAddressBook->mInvitations.size(); ++i)
	{
		if(mAddressBook->mInvitations[i].peering() == peering())
		{
			mAddressBook->mInvitations.erase(i);
			break;
		}
	}
	
	// WARNING: Invitation is deleted here
	return true;
}

bool AddressBook::Invitation::auth(const Identifier &peer, BinaryString &secret)
{
	Synchronize(mAddressBook);
	
	if(!mSecret.empty() && peer == mPeering)
	{
		secret = mSecret;
		return true;
	}
	
	return false;
}

bool AddressBook::Invitation::auth(const Identifier &peer, const Rsa::PublicKey &pubKey)
{
	Synchronize(mAddressBook);
	
	if(mSecret.empty() && pubKey.digest() == mPeering)
	{
		return true;
	}
	
	return false;
}

void AddressBook::Invitation::serialize(Serializer &s) const
{
	Synchronize(mAddressBook);  
	
	Serializer::ConstObjectMapping mapping;
	mapping["name"] = &mName;
	mapping["secret"] = &mSecret;
	mapping["peering"] = &mPeering;
	mapping["tracker"] = &mTracker;
	
	s.outputObject(mapping); 
}

bool AddressBook::Invitation::deserialize(Serializer &s)
{
	Synchronize(mAddressBook);
	
	mName.clear();
	mSecret.clear();
	mPeering.clear();
	mTracker.clear();

	Serializer::ObjectMapping mapping;
	mapping["name"] = &mName;
	mapping["secret"] = &mSecret;
	mapping["peering"] = &mPeering;
	mapping["tracker"] = &mTracker;
	
	if(!s.inputObject(mapping))
		return false;
	
	// TODO: sanity checks
	
	listen(peering());
	return true;
}

bool AddressBook::Invitation::isInlineSerializable(void) const
{
	return false;
}

AddressBook::Contact::Contact(void) :
	mAddressBook(NULL),
	mProfile(NULL),
	mBoard(NULL),
	mPrivateBoard(NULL)
{	

}

AddressBook::Contact::Contact(const Contact &contact) :
	mAddressBook(NULL),
	mProfile(NULL),
	mBoard(NULL),
	mPrivateBoard(NULL),
	mUniqueName(contact.mUniqueName),
	mName(contact.mName),
	mTracker(contact.mTracker),
	mPublicKey(contact.mPublicKey),
	mHalfSecret(contact.mHalfSecret),
	mSecret(contact.mSecret),
	mInstances(contact.mInstances)
{
	setAddressBook(contact.mAddressBook);
	// no init !
}

AddressBook::Contact::Contact(	AddressBook *addressBook, 
				const String &uname,
				const String &name,
			        const Rsa::PublicKey &pubKey,
				const String &tracker) :
	mAddressBook(NULL),
	mUniqueName(uname),
	mName(name),
	mTracker(tracker),
	mPublicKey(pubKey),
	mProfile(NULL),
	mBoard(NULL),
	mPrivateBoard(NULL)
{
	Assert(!uname.empty());
	Assert(!name.empty());
	
	setAddressBook(addressBook);
	init();
}

AddressBook::Contact::~Contact(void)
{
	if(mAddressBook)
	{
		Interface::Instance->remove(urlPrefix(), this);
		mAddressBook->mScheduler.cancel(this);
	}
	
	delete mProfile;
	delete mBoard;
}

void AddressBook::Contact::init(void)
{
	if(!mAddressBook) return;
	
	mAddressBook->mScheduler.schedule(this, 1.);
	mAddressBook->mScheduler.repeat(this, 300.);
		
	Interface::Instance->add(urlPrefix(), this);
	
	if(mHalfSecret.empty())
	{
		Random rnd(Random::Key);
		rnd.readBinary(mHalfSecret, 32);
	}
	
	if(!mBoard)
	{
		mBoard = new Board("/" + identifier().toString(), "", name());	// Public board
	}
	
	if(!mProfile && !isSelf())
	{
		Synchronize(mAddressBook);
		mProfile = new Profile(mAddressBook->user(), mUniqueName, mTracker);
		
		try {
			mProfile->load();
		}
		catch(const Exception &e)
		{
			LogWarn("AddressBook::Contact", String("Unable to load profile for ") + uniqueName() + ": " + e.what());
		}
	}
	
	listen(identifier());
}

void AddressBook::Contact::run(void)
{
	if(!mAddressBook) return;
	
	bool changed = false;
	SerializableMap<uint64_t, SerializableSet<Address> > result;
	if(mAddressBook->query(identifier(), tracker(), result))
	{
		for(SerializableMap<uint64_t, SerializableSet<Address> >::iterator it = result.begin();
			it != result.end();
			++it)
		{
			Identifier id(identifier(), it->first);
			
			if(!Core::Instance->hasPeer(id))
			{
				Core::Locator locator(mAddressBook->user(), it->second);
				locator.identifier = id;
				locator.name = name();
				
				if(Core::Instance->connect(locator))
				{
					mInstances[it->first].addAddresses(it->second);
					changed = true;
				}
			}
		}	
	}
	
	if(changed)
		mAddressBook->save();
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

String AddressBook::Contact::tracker(void) const
{
	Synchronize(mAddressBook);
	return (!mTracker.empty() ? mTracker : mAddressBook->user()->tracker());
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
	if(mSecret.empty())
		throw Exception("No secret for contact: " + mUniqueName);
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

bool AddressBook::Contact::Contact::isSelf(void) const
{
	return (mUniqueName == mAddressBook->userName());
}

bool AddressBook::Contact::isConnected(void) const
{
	return Core::Instance->hasPeer(identifier()); 
}

bool AddressBook::Contact::isConnected(uint64_t number) const
{
	Identifier id;
	if(!getInstanceIdentifier(number, id)) return false;
	return Core::Instance->hasPeer(id); 
}

int AddressBook::Contact::getInstanceNumbers(Array<uint64_t> &result) const
{
	Synchronize(mAddressBook);
	
	result.clear();
	result.reserve(mInstances.size());
	 
	for(InstancesMap::const_iterator it = mInstances.begin();
		it != mInstances.end();
		++it)
	{
		result.append(it->first); 
	}
	
	return result.size();
}

int AddressBook::Contact::getInstanceIdentifiers(Array<Identifier> &result) const
{
	Synchronize(mAddressBook);
	
	result.clear();
	result.reserve(mInstances.size());
	 
	for(InstancesMap::const_iterator it = mInstances.begin();
		it != mInstances.end();
		++it)
	{
		result.append(Identifier(identifier(), it->first)); 
	}
	
	return result.size();
}

bool AddressBook::Contact::getInstanceIdentifier(uint64_t number, Identifier &result) const
{
	Synchronize(mAddressBook);
	
	InstancesMap::const_iterator it = mInstances.find(number);
	if(it != mInstances.end())
	{
		result = Identifier(identifier(), number);
		return true;
	}
	
	return false;
}

bool AddressBook::Contact::getInstanceName(uint64_t number, String &result) const
{
	Synchronize(mAddressBook);
	
	InstancesMap::const_iterator it = mInstances.find(number);
	if(it != mInstances.end())
	{
		result = it->second.name();
		return true;
	}
	
	return false;
}

bool AddressBook::Contact::getInstanceAddresses(uint64_t number, Set<Address> &result) const
{
	Synchronize(mAddressBook);
	
	InstancesMap::const_iterator it = mInstances.find(number);
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
	
	LogDebug("AddressBook::Invitation", "Connected");
  
	Notification notification;
	notification["type"] << "hello";
	notification["secret"] << mHalfSecret;
	
	// TODO: force direct
	if(!Core::Instance->send(peer, notification))
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
		
		BinaryString commonSecret = mHalfSecret;
		if(secret.size() > commonSecret.size())
			std::swap(secret, commonSecret);
		
		// XOR
		for(int i=0; i<secret.size(); ++i)
			commonSecret[i]^=secret[i];
		
		if(mSecret != commonSecret)
		{
			mSecret = commonSecret;
			mAddressBook->save();
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
			
			unsigned refreshPeriod = 5000;
			page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				var msg = '';\n\
				if(info.mails != 0) msg = ' ('+info.mails+')';\n\
				transition($('#mailscount'), msg);\n\
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
			page.javascript("listDirectory('"+prefix+"/search?json&maxage="+String::number(maxAge)+"&count="+String::number(count)+"','#recent',false,true);");
			
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
			req->setAutoDelete();
			
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
			match.trim();
			
			String reqPrefix;
			if(!match.empty())
			{
				Request *req = new Request(match, identifier());
				reqPrefix = req->urlPrefix();
				req->setAutoDelete();
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
			Http::Response response(request, 303);	// See other
			response.headers["Location"] = profile()->avatarUrl(); 
			response.send();
			return;
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
	mapping["tracker"] = &mTracker;
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
		mapping["halfsecret"] = &mHalfSecret;
		mapping["secret"] = &mSecret;
	}
	
	s.outputObject(mapping);
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	Synchronize(mAddressBook);
	
	mPublicKey.clear();
	mUniqueName.clear();
	mName.clear();
	mInstances.clear();
	
	Serializer::ObjectMapping mapping;
	mapping["publickey"] = &mPublicKey;
	mapping["uname"] = &mUniqueName;
	mapping["name"] = &mName;
	mapping["tracker"] = &mTracker;
	mapping["instances"] = &mInstances;
	mapping["halfsecret"] = &mHalfSecret;
	mapping["secret"] = &mSecret;
	
	if(!s.inputObject(mapping))
		return false;
	
	// TODO: sanity checks
	
	listen(identifier());
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

AddressBook::Contact::Instance::Instance(uint64_t number) :
	mNumber(number),
	mLastSeen(0)
{
	Assert(mNumber != 0);
}

AddressBook::Contact::Instance::~Instance()
{
  
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
	mNumber = 0;
	mName.clear();
	mAddrs.clear();
	
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
