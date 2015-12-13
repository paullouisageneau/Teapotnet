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
#include "pla/object.h"
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
			serializer.read(*this);
			file.close();
		}
		catch(const Exception &e)
		{
			LogError("AddressBook", String("Loading failed: ") + e.what());
		}
	}
	
	Interface::Instance->add(urlPrefix(), this);
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
	serializer.write(*this);
	file.close();
	
	const Contact *self = getSelf();
	if(self)
	{
		Resource resource;
		resource.process(mFileName, "contacts", "contacts", self->secret());
		mDigest = resource.digest();
		
		Desynchronize(this); 
		
		Network::Instance->send(Network::Link(user()->identifier(), self->identifier()), "contacts", 
			ConstObject()
				.insert("digest", mDigest));
	}
}

String AddressBook::addContact(const String &name, const Identifier &identifier)
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
	
	Map<String, Contact>::iterator it = mContacts.insert(uname, Contact(this, uname, name, identifier));
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

void AddressBook::setSelf(const Identifier &identifier)
{
	Synchronize(this);
	
	String uname = userName();

	Map<String, Contact>::iterator it = mContacts.find(uname);
	if(it != mContacts.end())
	{
		if(it->second.identifier() == identifier)
			return;
			
		mContactsByIdentifier.erase(it->second.identifier());
		mContacts.erase(it);
	}
	
	it = mContacts.insert(uname, Contact(this, uname, uname, identifier));
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

bool AddressBook::hasIdentifier(const Identifier &identifier) const
{
	Synchronize(this);
	return mContactsByIdentifier.contains(identifier);
}

BinaryString AddressBook::digest(void) const
{
	Synchronize(this);
	return mDigest;
}

bool AddressBook::send(const String &type, const Serializable &object)
{
	Array<Identifier> keys;
	SynchronizeStatement(this, mContactsByIdentifier.getKeys(keys));

	bool success = false;
	for(int i=0; i<keys.size(); ++i)
        {
                Contact *contact = getContact(keys[i]);
		if(contact) success|= contact->send(type, object);
	}
	
	return success;
}

void AddressBook::serialize(Serializer &s) const
{
	Synchronize(this);
	
	ConstObject object;
	object["contacts"] = &mContacts;
	
	s.write(object);
}

bool AddressBook::deserialize(Serializer &s)
{
	Synchronize(this);
	
	Object object;
	object["contacts"] = &mContacts;
	
	if(!s.read(object)) return false;
	
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
	
	save();
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
					if(!user()->checkToken(request.post["token"], "contact")) 
						throw 403;
					
					String action = request.post["action"];
					
					if(action == "deletecontact")
					{
						String uname = request.post.getOrDefault("uname", request.post.get("argument"));
						removeContact(uname);
					}
					else if(action == "createinvitation")
					{
						String id = request.post.getOrDefault("id", request.post.get("argument"));
						String name = request.post["name"];
						
						Identifier identifier;
						id.extract(identifier);
						
						// TODO
					}
					else if(action == "acceptinvitation")
					{
						String id = request.post.getOrDefault("id", request.post.get("argument"));
						String name = request.post["name"];
						
						Identifier identifier;
						id.extract(identifier);
						
						// TODO
					}
					else if(action == "createsynchronization")
					{
						BinaryString secret;
						Random(Random::Key).readBinary(secret, 32);
						
						Resource resource;
						resource.process(user()->fileName(), "user", "user", secret);
						BinaryString digest = resource.digest();
						
						Assert(secret.size() == 32);
						Assert(digest.size() == 32);
						String code = String(BinaryString(secret + digest).base64Encode(true));	// safe mode
						
						setSelf(user()->identifier());	// create self contact
						
						Http::Response response(request, 200);
						response.send();
						
						Html page(response.stream);
						page.header("Synchronization secret");

						page.open("div",".box");
						page.text(code);
						page.close("div");

						page.footer();
						return;
					}
					else if (action == "acceptsynchronization")
					{
						String code = request.post["code"];
						BinaryString digest;
						try {
							digest = code.base64Decode();
						}
						catch(...)
						{
							throw Exception("Invalid synchronization code");
						}
						
						BinaryString secret;
						digest.readBinary(secret, 32);
						if(digest.size() != 32)
							throw Exception("Invalid synchronization code");
						
						setSelf(user()->identifier());	// create self contact
						
						mScheduler.schedule(new Resource::ImportTask(user(), digest, "user", secret, true));	// autodelete
					}
					else {
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
				json.write(*this);
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

			/*page.open("div", ".howtorequests");
			page.text("Here you can either send or accept invitations. Inviting someone is as easy as getting a new invitation secret code here and sending it to your contact. To accept an invitation, simply enter the secret code in the appropriate section.");
			page.close("div");*/

			page.close("div");

			page.open("div",".box");
			page.openForm(prefix + "/", "post", "createsynchronization");
			page.open("h2"); page.text("Synchronize new device"); page.close("h2");
			page.input("hidden", "token", token);
			page.input("hidden", "action", "createsynchronization");
			page.label("generate"); page.button("generate", "Generate synchronization secret");
			page.closeForm();
			page.close("div");
			
			page.open("div",".box");
			page.openForm(prefix + "/", "post", "acceptsynchronization");
			page.open("h2"); page.text("Accept device synchronization"); page.close("h2");
			page.input("hidden", "token", token);
			page.input("hidden", "action", "acceptsynchronization");
			page.input("text", "code");
			page.button("validate", "Validate");
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
	mIdentifier(contact.mIdentifier),
	mRemoteSecret(contact.mRemoteSecret)
{
	setAddressBook(contact.mAddressBook);
	// no init
}

AddressBook::Contact::Contact(	AddressBook *addressBook, 
				const String &uname,
				const String &name,
			        const Identifier &identifier) :
	mAddressBook(NULL),
	mUniqueName(uname),
	mName(name),
	mIdentifier(identifier),
	mBoard(NULL),
	mPrivateBoard(NULL)
{
	Assert(!uname.empty());
	Assert(!name.empty());
	Assert(!identifier.empty());
	
	setAddressBook(addressBook);
}

AddressBook::Contact::~Contact(void)
{
	if(mAddressBook)
		Interface::Instance->remove(urlPrefix(), this);
	
	delete mBoard;
}

void AddressBook::Contact::init(void)
{
	if(!mAddressBook) return;

	Interface::Instance->add(urlPrefix(), this);
	
	if(!mBoard)
		mBoard = new Board("/" + identifier().toString(), "", name());	// Public board
	
	listen(mAddressBook->user()->identifier(), identifier());
}

void AddressBook::Contact::setAddressBook(AddressBook *addressBook)
{
	Assert(!mAddressBook);
	mAddressBook = addressBook;
}

Identifier AddressBook::Contact::identifier(void) const
{
	Synchronize(mAddressBook);
	return mIdentifier;
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
	return Network::Instance->hasLink(Network::Link(mAddressBook->user()->identifier(), identifier())); 
}

bool AddressBook::Contact::isConnected(const Identifier &instance) const
{
	return Network::Instance->hasLink(Network::Link(mAddressBook->user()->identifier(), identifier(), instance));
}

bool AddressBook::Contact::send(const String &type, const Serializable &object)
{
	return Network::Instance->send(Network::Link(mAddressBook->user()->identifier(), identifier()), type, object);
}

bool AddressBook::Contact::send(const Identifier &instance, const String &type, const Serializable &object)
{
	return Network::Instance->send(Network::Link(mAddressBook->user()->identifier(), identifier(), instance), type, object);
}

void AddressBook::Contact::seen(const Network::Link &link)
{
	Synchronize(mAddressBook);
	
	if(!Network::Instance->hasLink(link))
	{
		LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": " + link.node.toString() + " is seen");
		Network::Instance->connect(link.node, link.remote, mAddressBook->user());
	}
}

void AddressBook::Contact::connected(const Network::Link &link, bool status)
{
	Synchronize(mAddressBook);
	
	if(status)
	{
		LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": " + link.node.toString() + " is connected");
		if(!mInstances.contains(link.node))
			mInstances[link.node] = link.node.toString();	// default name
		
		Desynchronize(mAddressBook);
		
		send(link.node, "info", ConstObject()
			.insert("name", Network::Instance->overlay()->localName())
			.insert("secret", localSecret()));
		
		if(isSelf())
		{
			send(link.node, "contacts", ConstObject()
				.insert("digest", mAddressBook->digest()));
		}
	}
	else {
		LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": " + link.node.toString() + " is disconnected");
		mInstances.erase(link.node);
	}
}

bool AddressBook::Contact::recv(const Network::Link &link, const String &type, Serializer &serializer)
{
	// Not synchronized
	
	//LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": received message (type='" + type + "')");
	
	if(type == "info")
	{
		String name;
		serializer.read(Object()
			.insert("name", &name)
			.insert("secret", &mRemoteSecret));
		
		mInstances[link.node] = name;
	}
	else if(type == "contacts")
	{
		if(!isSelf()) throw Exception("Received contacts from other than self");
		
		BinaryString digest;
		serializer.read(Object()
			.insert("digest", &digest));
		
		if(digest != mAddressBook->mDigest)
		{
			mAddressBook->mScheduler.schedule(new Resource::ImportTask(mAddressBook, digest, "contacts", secret(), true));	// autodelete
			mAddressBook->mDigest = digest;
		}
	}
	else {
		LogWarn("AddressBook::Contact::recv", "Unknown message type '" + type + "'");
		return false;
	}
	
	return true;
}

bool AddressBook::Contact::auth(const Network::Link &link, const Rsa::PublicKey &pubKey)
{
	return (pubKey.digest() == identifier());
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
				json.write(*this);
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
				else $.each(info.instances, function(id, name) {\n\
					$('#instances').append($('<tr>')\n\
						.append($('<td>').addClass('name').text(name)));\n\
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
				Request *req = new Request("/files/" + identifier().toString() + "?" + match, mAddressBook->user()->identifier(), identifier(), false);
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
				BinaryString id = mAddressBook->user()->identifier() ^ identifier();
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
	
	ConstObject object;
	object["identifier"] = &mIdentifier;
	object["uname"] = &mUniqueName;
	object["name"] = &mName;
	
	String prefix, status;
	ConstSerializableWrapper<uint32_t> messages(uint32_t(0));	// TODO
	if(s.optionalOutputMode())
	{
		prefix = urlPrefix();
		status = (isConnected() ? "connected" : "disconnected");
		
		object["instances"] = &mInstances;
		object["prefix"] = &prefix;
		object["status"] = &status;
		object["messages"] = &messages;
	}
	else {
		object["secret"] = &mRemoteSecret;
		
	}
	
	s.write(object);
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	Synchronize(mAddressBook);
	
	Object object;
	object["identifier"] = &mIdentifier;
	object["uname"] = &mUniqueName;
	object["name"] = &mName;
	object["secret"] = &mRemoteSecret;
	
	if(!s.read(object))
		return false;
	
	// TODO: sanity checks
	
	return true;
}

bool AddressBook::Contact::isInlineSerializable(void) const
{
	return false; 
}

}
