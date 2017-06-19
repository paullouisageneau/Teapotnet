/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#include "tpn/addressbook.hpp"
#include "tpn/user.hpp"
#include "tpn/network.hpp"
#include "tpn/config.hpp"
#include "tpn/cache.hpp"
#include "tpn/request.hpp"
#include "tpn/resource.hpp"
#include "tpn/indexer.hpp"
#include "tpn/html.hpp"
#include "tpn/portmapping.hpp"
#include "tpn/httptunnel.hpp"

#include "pla/crypto.hpp"
#include "pla/random.hpp"
#include "pla/file.hpp"
#include "pla/directory.hpp"
#include "pla/yamlserializer.hpp"
#include "pla/jsonserializer.hpp"
#include "pla/binaryserializer.hpp"
#include "pla/object.hpp"
#include "pla/mime.hpp"

namespace tpn
{

AddressBook::AddressBook(User *user) :
	mUser(user),
	mTime(0)
{
	Assert(mUser != NULL);

	mFileName = mUser->profilePath() + "contacts";
	if(File::Exist(mFileName))
	{
		try {
			File file(mFileName, File::Read);
			JsonSerializer serializer(&file);
			serializer >> *this;
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
	if(mUser) return mUser->name();
	else return "";
}

String AddressBook::urlPrefix(void) const
{
	if(mUser) return mUser->urlPrefix() + "/contacts";
	else return "";
}

void AddressBook::clear(void)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mContactsByIdentifier.clear();
	mContacts.clear();
}

void AddressBook::save(void) const
{
	SafeWriteFile file(mFileName);
	JsonSerializer serializer(&file);
	serializer << *this;
	file.close();

	sptr<const Contact> self = getSelf();
	if(self)
	{
		Resource resource;
		Resource::Specs specs;
		specs.name = "contacts";
		specs.type = "contacts";
		specs.secret = self->secret;
		resource.process(mFileName, specs);

		{
			std::unique_lock<std::mutex> lock(mMutex);
			mDigest = resource.digest();
		}

		self->send("contacts", Object()
				.insert("digest", resource.digest())
				.insert("time", time()));
	}
}

int AddressBook::count(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mContacts.size();
}

String AddressBook::addContact(const String &name, const Identifier &identifier)
{
	if(name.empty())
		throw Exception("Contact name is empty");

	if(!name.isAlphanumeric())
		throw Exception("Contact name is invalid: " + name);

	if(identifier == user()->identifier())
		throw Exception("Cannot add self as contact");

	String uname = name;
	{
		std::unique_lock<std::mutex> lock(mMutex);

		unsigned i = 1;
		while((mContacts.contains(uname))
			|| uname == userName())	// userName reserved for self
		{
			uname = name;
			uname << ++i;
		}

		sptr<Contact> contact = std::make_shared<Contact>(this, uname, name, identifier);
		mContacts.insert(uname, contact);
		mContactsByIdentifier.insert(identifier, contact);
		mTime = Time::Now();
	}

	save();
	return uname;
}

bool AddressBook::removeContact(const String &uname)
{
	if(uname == userName())
		return false;

	{
		std::unique_lock<std::mutex> lock(mMutex);

		auto it = mContacts.find(uname);
		if(it == mContacts.end()) return false;

		sptr<Contact> contact = it->second;
		mContactsByIdentifier.erase(contact->identifier());
		mContacts.erase(it);
		mTime = Time::Now();
	}

	save();
	return true;
}

sptr<AddressBook::Contact> AddressBook::getContact(const String &uname)
{
	std::unique_lock<std::mutex> lock(mMutex);

	auto it = mContacts.find(uname);
	if(it != mContacts.end()) return it->second;
	else return NULL;
}

sptr<const AddressBook::Contact> AddressBook::getContact(const String &uname) const
{
	std::unique_lock<std::mutex> lock(mMutex);

	auto it = mContacts.find(uname);
	if(it != mContacts.end()) return it->second;
	else return NULL;
}

int AddressBook::getContacts(std::vector<sptr<AddressBook::Contact> > &result)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mContactsByIdentifier.getValues(result);
	return result.size();
}

int AddressBook::getContactsIdentifiers(Array<Identifier> &result) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	mContactsByIdentifier.getKeys(result);
	return result.size();
}

bool AddressBook::hasIdentifier(const Identifier &identifier) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mContactsByIdentifier.contains(identifier);
}

void AddressBook::setSelf(const Identifier &identifier)
{
	String uname = userName();

	{
		std::unique_lock<std::mutex> lock(mMutex);

		auto it = mContacts.find(uname);
		if(it != mContacts.end())
		{
			// Delete
			sptr<Contact> contact = it->second;
			if(contact->identifier() == identifier) return;
			mContactsByIdentifier.erase(contact->identifier());
			mContacts.erase(it);
		}

		// Create
		sptr<Contact> contact = std::make_shared<Contact>(this, uname, uname, identifier);
		mContacts.insert(uname, contact);
		mContactsByIdentifier.insert(identifier, contact);

		// Setting self DO NOT update mTime
	}

	save();
}

sptr<AddressBook::Contact> AddressBook::getSelf(void)
{
	return getContact(userName());
}

sptr<const AddressBook::Contact> AddressBook::getSelf(void) const
{
	return getContact(userName());
}

void AddressBook::addInvitation(const Identifier &remote, const String &name)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(!mContactsByIdentifier.contains(remote))
		mInvitations.insert(remote, name);
}

String AddressBook::removeInvitation(const Identifier &remote)
{
	std::unique_lock<std::mutex> lock(mMutex);

	String name;
	mInvitations.get(remote, name);
	mInvitations.erase(remote);
	return name;
}

Time AddressBook::time(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mTime;
}

BinaryString AddressBook::digest(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mDigest;
}

bool AddressBook::send(const String &type, const Serializable &object) const
{
	std::vector<Identifier> keys;
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mContactsByIdentifier.getKeys(keys);
	}

	bool success = false;
	for(int i=0; i<keys.size(); ++i)
	{
		sptr<const Contact> contact = getContact(keys[i]);
		if(contact) success|= contact->send(type, object);
	}

	return success;
}

void AddressBook::serialize(Serializer &s) const
{
	std::unique_lock<std::mutex> lock(mMutex);

	s << Object()
		.insert("contacts", mContacts)
		.insert("time", mTime);
}

bool AddressBook::deserialize(Serializer &s)
{
	Map<String, Contact> temp;
	Time time;

	if(!(s >> Object()
		.insert("contacts", temp)
		.insert("time", time)))
		return false;

	StringSet toDelete;
	mContacts.getKeys(toDelete);

	for(auto it = temp.begin(); it != temp.end(); ++it)
	{
		Assert(!it->second.uniqueName().empty());
		Assert(!it->second.identifier().empty());
		toDelete.erase(it->first);

		sptr<Contact> contact = getContact(it->first);
		if(contact)
		{
			if(contact->identifier() != it->second.identifier())
			{
				{
					std::unique_lock<std::mutex> lock(mMutex);
					mContacts.erase(contact->uniqueName());
					mContactsByIdentifier.erase(contact->identifier());
				}

				contact->setAddressBook(NULL);
				contact.reset(); // This will trigger if(!contact)
			}
		}

		if(!contact)
		{
			contact = std::make_shared<Contact>(it->second);
			contact->setAddressBook(this);

			{
				std::unique_lock<std::mutex> lock(mMutex);
				mContacts.insert(contact->uniqueName(), contact);
				mContactsByIdentifier.insert(contact->identifier(), contact);
			}
		}
	}

	for(String uname : toDelete)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		auto it = mContacts.find(uname);
		if(it == mContacts.end()) continue;
		mContactsByIdentifier.erase(it->second->identifier());
		mContacts.erase(it);
	}

	mTime = time;
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

					if(action == "add" || action == "create")
					{
						String id = (request.post.contains("argument") ? request.post.get("argument") : request.post.get("id"));
						String name = request.post["name"];
						id.trim();
						name.trim();

						Identifier identifier;
						id.extract(identifier);
						if(identifier.size() != 32)
							throw Exception("Invalid identifier");

						String invitationName = removeInvitation(identifier);
						if(name.empty()) name = invitationName;
						if(name.empty()) throw Exception("Missing contact name");

						addContact(name, identifier);
					}
					else if(action == "delete")
					{
						String uname = (request.post.contains("argument") ? request.post.get("argument") : request.post.get("uname"));
						removeContact(uname);
					}
					else if(action == "deleteinvitation")
					{
						String id = (request.post.contains("argument") ? request.post.get("argument") : request.post.get("id"));

						Identifier identifier;
						id.extract(identifier);
						if(identifier.size() != 32)
							throw Exception("Invalid identifier");

						removeInvitation(identifier);
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
				response.headers["Location"] = request.post.getOrDefault("redirect", prefix + "/");
				response.send();
				return;
			}

			if(request.get.contains("json"))
			{
				if(request.get.contains("id"))
				{
					Identifier identifier;
					request.get["id"].extract(identifier);

					sptr<Contact> contact;
					{
						std::unique_lock<std::mutex> lock(mMutex);

						if(!mContactsByIdentifier.get(identifier, contact))
						{
							if(identifier == mUser->identifier())
							{
								String name = mUser->name();
								String prefix = mUser->urlPrefix();
								String status = "disconnected";

								Http::Response response(request, 200);
								response.headers["Content-Type"] = "application/json";
								response.send();

								JsonSerializer json(response.stream);
								json << Object()
									.insert("identifier", identifier)
									.insert("uname", name)
									.insert("name", name)
									.insert("prefix", prefix)
									.insert("status", status)
									.insert("messages", 0)
									.insert("newmessages", false);
								return;
							}

							throw 404;
						}
					}

					Http::Response response(request, 200);
					response.headers["Content-Type"] = "application/json";
					response.send();

					JsonSerializer json(response.stream);
					json.setOptionalOutputMode(true);
					json << *contact;
					return;
				}

				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();

				JsonSerializer json(response.stream);
				json.setOptionalOutputMode(true);
				json << *this;
				return;
			}

			Http::Response response(request, 200);
			response.send();

			Html page(response.stream);
			page.header("Contacts");

			String token = user()->generateToken("contact");

			page.openForm(prefix + "/", "post", "create");
			page.openFieldset("Follow new contact");
			page.input("hidden", "token", token);
			page.input("hidden", "action", "create");
			page.label("id", "Identifier"); page.input("text", "id"); page.br();
			page.label("name", "Name"); page.input("text", "name"); page.br();
			page.label("add"); page.button("add", "Add contact"); page.br(); page.br();
			page.closeFieldset();
			page.closeForm();

			page.open("div",".box");
			page.open("h2");
			page.text("Identifier");
			page.close("h2");
			page.text(user()->identifier().toString());
			page.close("div");

			page.openForm(prefix+"/", "post", "actionForm");
			page.input("hidden", "action");
			page.input("hidden", "argument");
			page.input("hidden", "token", token);
			page.closeForm();

			// Contacts
			std::vector<sptr<Contact> > contacts;
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
					sptr<Contact> contact = contacts[i];

					page.open("tr");
					page.open("td",".name");
					page.link(contact->urlPrefix(), (contact->isSelf() ? "Myself" : contact->name()));
					page.close("td");
					page.open("td",".uname");
					page.text(contact->uniqueName());
					page.close("td");
					page.open("td",".id");
					page.text(contact->identifier().toString());
					page.close("td");
					page.open("td",".actions");
					if(!contact->isSelf())
					{
						page.openLink('#', ".deletelink");
						page.image("/static/delete.png", "Delete");
						page.closeLink();
					}
					page.close("td");
					page.close("tr");
				}

				page.close("table");
				page.close("div");

				page.javascript("$('.contacts .deletelink').css('cursor', 'pointer').click(function(event) {\n\
					event.stopPropagation();\n\
					var uname = $(this).closest('tr').find('td.uname').text();\n\
					if(confirm('Do you really want to delete '+uname+' ?')) {\n\
						document.actionForm.action.value = 'delete';\n\
						document.actionForm.argument.value = uname;\n\
						document.actionForm.submit();\n\
					}\n\
					return false;\n\
				});");
			}

			// Invitations
			{
				std::unique_lock<std::mutex> lock(mMutex);

				if(!mInvitations.empty())
				{
					page.open("div",".box");
					page.open("h2");
					page.text("Invitations");
					page.close("h2");

					page.open("table",".invitations");

					for(auto it = mInvitations.begin(); it != mInvitations.end(); ++it)
					{
						page.open("tr");
						page.open("td",".name");
						page.text(it->second);
						page.close("td");
						page.open("td",".id");
						page.text(it->first.toString());
						page.close("td");
						page.open("td",".actions");
						page.openLink('#', ".acceptlink");
						page.image("/static/add.png", "Accept");
						page.closeLink();
						page.openLink('#', ".deletelink");
						page.image("/static/delete.png", "Delete");
						page.closeLink();
						page.close("td");
						page.close("tr");
					}

					page.close("table");
					page.close("div");

					page.javascript("$(document).ready(function () {\n\
						$('.invitations .acceptlink').css('cursor', 'pointer').click(function(event) {\n\
							event.stopPropagation();\n\
							var name = $(this).closest('tr').find('td.name').text();\n\
							var id = $(this).closest('tr').find('td.id').text();\n\
							if(confirm('Do you really want to add '+name+' ?')) {\n\
								document.actionForm.action.value = 'create';\n\
								document.actionForm.argument.value = id;\n\
								document.actionForm.submit();\n\
							}\n\
							return false;\n\
						});\n\
						$('.invitations .deletelink').css('cursor', 'pointer').click(function(event) {\n\
							event.stopPropagation();\n\
							var name = $(this).closest('tr').find('td.name').text();\n\
							var id = $(this).closest('tr').find('td.id').text();\n\
							if(confirm('Do you really want to delete invitation from '+name+' ?')) {\n\
								document.actionForm.action.value = 'deleteinvitation';\n\
								document.actionForm.argument.value = id;\n\
								document.actionForm.submit();\n\
							}\n\
							return false;\n\
						});\n\
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
}

AddressBook::Contact::Contact(	AddressBook *addressBook,
		const String &uname,
		const String &name,
		const Identifier &identifier) :
	mAddressBook(NULL),
	mBoard(NULL),
	mPrivateBoard(NULL),
	mUniqueName(uname),
	mName(name),
	mIdentifier(identifier)
{
	Assert(!uname.empty());
	Assert(!name.empty());
	Assert(!identifier.empty());

	setAddressBook(addressBook);
}

AddressBook::Contact::~Contact(void)
{
	uninit();
}

void AddressBook::Contact::setAddressBook(AddressBook *addressBook)
{
	// Private, no sync

	if(mAddressBook != addressBook)
	{
		if(mAddressBook) uninit();
		mAddressBook = addressBook;
		if(mAddressBook) init();
	}
}

void AddressBook::Contact::init(void)
{
	// Private, no sync

	mBoard.reset();
	mPrivateBoard.reset();

	if(mAddressBook)
	{
		if(!nIsSelf())
		{
			if(!mBoard) mBoard = std::make_shared<Board>(mIdentifier.toString(), "", mName);	// Public board
			mAddressBook->user()->mergeBoard(mBoard);

			if(!mPrivateBoard)
			{
				BinaryString boardId = mAddressBook->user()->identifier() ^ identifier();
				mPrivateBoard = std::make_shared<Board>(boardId.toString(), secret().toString(), mName + " (Private)");
			}
		}

		Interface::Instance->add(nUrlPrefix(), this);
		listen(mAddressBook->user()->identifier(), mIdentifier);
	}
}

void AddressBook::Contact::uninit(void)
{
	// Private, no sync

	if(mAddressBook)
	{
		if(mBoard) mAddressBook->user()->unmergeBoard(mBoard);

		Interface::Instance->remove(nUrlPrefix(), this);
		ignore();       // stop listening
	}

	mBoard.reset();
	mPrivateBoard.reset();
}

bool AddressBook::Contact::nIsSelf(void) const
{
	if(!mAddressBook) return false;
	return (mUniqueName == mAddressBook->userName());
}

bool AddressBook::Contact::nIsConnected(const Identifier &instance) const
{
	if(!mAddressBook) return false;
	Network::Link link(mAddressBook->user()->identifier(), mIdentifier, instance);
	return Network::Instance->hasLink(link);
}

String AddressBook::Contact::nUrlPrefix(void) const
{
	if(!mAddressBook || mUniqueName.empty()) return "";
	if(nIsSelf()) return mAddressBook->user()->urlPrefix()+"/myself";
	return mAddressBook->urlPrefix()+"/"+mUniqueName;
}

BinaryString AddressBook::Contact::localSecret(void) const
{
	if(!mAddressBook) return "";
	return mAddressBook->user()->getSecretKey(identifier().toString());
}

BinaryString AddressBook::Contact::remoteSecret(void) const
{
	return mRemoteSecret;
}

Identifier AddressBook::Contact::identifier(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mIdentifier;
}

String AddressBook::Contact::uniqueName(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mUniqueName;
}

String AddressBook::Contact::name(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mName;
}

String AddressBook::Contact::urlPrefix(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return nUrlPrefix();
}

BinaryString AddressBook::Contact::secret(void) const
{
	if(isSelf())
	{
		Assert(mAddressBook);
		return mAddressBook->user()->secret();
	}
	else {
		return localSecret() ^ remoteSecret();
	}
}

bool AddressBook::Contact::Contact::isSelf(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return nIsSelf();
}

bool AddressBook::Contact::isConnected(const Identifier &instance) const
{
	std::unique_lock<std::mutex>(mMutex);
	return nIsConnected();
}

bool AddressBook::Contact::send(const String &type, const Serializable &object) const
{
	if(!mAddressBook) return false;
	Network::Link link(mAddressBook->user()->identifier(), identifier());
	return Network::Instance->send(link, type, object);
}

bool AddressBook::Contact::send(const Identifier &instance, const String &type, const Serializable &object) const
{
	if(!mAddressBook) return false;
	Network::Link link(mAddressBook->user()->identifier(), identifier(), instance);
	return Network::Instance->send(link, type, object);
}

void AddressBook::Contact::seen(const Network::Link &link)
{
	if(!mAddressBook) return;

	if(!isConnected(link.node))
	{
		//LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": " + link.node.toString() + " is seen");
		Network::Instance->connect(link.node, link.remote, mAddressBook->user());
	}
}

void AddressBook::Contact::connected(const Network::Link &link, bool status)
{
	if(!mAddressBook) return;

	if(status)
	{
		LogInfo("AddressBook::Contact", "Contact connected: " + uniqueName() + " on " + link.node.toString());

		{
			std::unique_lock<std::mutex> lock(mMutex);
			if(!mInstances.contains(link.node))
				mInstances[link.node] = link.node.toString(); // default name
		}

		send(link.node, "info", Object()
			.insert("instance", Network::Instance->overlay()->localName())
			.insert("secret", localSecret()));

		// TODO: should be sent once to each instance
		send(link.node, "invite", Object()
			.insert("name", mAddressBook->userName()));

		if(isSelf() && !mAddressBook->digest().empty())
		{
			send(link.node, "contacts", Object()
				.insert("digest", mAddressBook->digest())
				.insert("time", mAddressBook->time()));
		}
	}
	else {
		LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": " + link.node.toString() + " is disconnected");

		{
			std::unique_lock<std::mutex> lock(mMutex);
			mInstances.erase(link.node);
		}
	}
}

bool AddressBook::Contact::recv(const Network::Link &link, const String &type, Serializer &serializer)
{
	if(!mAddressBook) return false;

	LogDebug("AddressBook::Contact", "Contact " + uniqueName() + ": received message (type=\"" + type + "\")");

	if(type == "info")
	{
		String instance;
		BinaryString remoteSecret;
		serializer >> Object()
			.insert("instance", instance)
			.insert("secret", remoteSecret);

		sptr<Board> board;
		if(!isSelf())
		{
			BinaryString boardId = mAddressBook->user()->identifier() ^ identifier();
			board = std::make_shared<Board>(boardId.toString(), secret().toString(), name());
		}

		LogDebug("AddressBook::Contact", "Remote instance name: \"" + instance + "\"");

		{
			std::unique_lock<std::mutex> lock(mMutex);
			mInstances[link.node] = instance;
			mRemoteSecret = remoteSecret;
			mPrivateBoard = board;
		}

		mAddressBook->save();
	}
	else if(type == "contacts")
	{
		if(!isSelf()) throw Exception("Received contacts from other than self");

		BinaryString digest;
		Time time = 0;
		serializer >> Object()
			.insert("digest", digest)
			.insert("time", time);

		if(!digest.empty() && digest != mAddressBook->digest() && time > mAddressBook->time())
			mAddressBook->mScheduler.schedule(Scheduler::clock::now(), Resource::ImportTask(mAddressBook, digest, "contacts", secret()));
	}
	else {
		LogWarn("AddressBook::Contact::recv", "Unknown message type \"" + type + "\"");
		return false;
	}

	return true;
}

bool AddressBook::Contact::auth(const Network::Link &link, const Rsa::PublicKey &pubKey)
{
	if(!mAddressBook) return false;
	return (pubKey.digest() == identifier());
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
	if(!mAddressBook) return;
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
				json << *this;
				return;
			}

			Http::Response response(request,200);
			response.send();

			Html page(response.stream);
			if(isSelf()) page.header("Myself");
			else {
				page.header("Contact: "+name());
				page.open("div", "topmenu");
				page.span((isConnected() ? "Connected" : "Disconnected"), "status.button");
				page.close("div");
			}

			page.open("div",".box");

			page.open("table", ".menu");
			page.open("tr");
			page.open("td");
				page.openLink(prefix + "/files/");
				page.image("/static/icon_files.png", "Files", ".bigicon");
				page.closeLink();
			page.close("td");
			page.open("td", ".title");
				page.text("Files");
			page.close("td");
			page.close("tr");
			page.open("tr");
			page.open("td");
				page.openLink(prefix + "/search/");
				page.image("/static/icon_search.png", "Search", ".bigicon");
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
					page.image("/static/icon_board.png", "Board", ".bigicon");
					page.closeLink();
				page.close("td");
				page.open("td",".title");
					page.text("Board");
				page.close("td");
				page.close("tr");

				page.open("tr");
				page.open("td");
					page.openLink(prefix + "/chat/");
					page.image("/static/icon_chat.png", "Messages", ".bigicon");
					page.closeLink();
				page.close("td");
				page.open("td",".title");
					page.text("Messages");
					page.span("", "messagescount.messagescount");
				page.close("td");
				page.close("tr");
			}

			page.close("table");
			page.close("div");

			page.open("div",".box");
			page.open("h2");
			page.text("Identifier");
			page.close("h2");
			page.text(identifier().toString());
			page.close("div");

			page.open("div",".box");
			page.open("h2");
			page.text("Instances");
			page.close("h2");
			page.open("table", "instances");
			page.close("table");
			page.close("div");

			unsigned refreshPeriod = 10000;
			page.javascript("setCallback(\""+prefix+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				var msg = '';\n\
				if(info.messages != 0) msg = ' ('+info.messages+')';\n\
				transition($('#messagescount'), msg);\n\
				$('#instances').empty();\n\
				if($.isEmptyObject(info.instances)) $('#instances').text('No connected instance');\n\
				else $.each(info.instances, function(id, name) {\n\
					$('#instances').append($('<tr>')\n\
						.append($('<td>').addClass('name').text(name)));\n\
				});\n\
			});");

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
			BinaryString digest;
			if(request.get.contains("digest"))
			{
				try {
					request.get["digest"].extract(digest);
				}
				catch(...)
				{
					LogWarn("AddressBook::Contact::http", "Invalid digest");
				}
			}

			String requestPath = "/files/" + identifier().toString() + url;
			Request *req = new Request(requestPath, mAddressBook->user()->identifier(), identifier(), true);
			if(!digest.empty()) req->addTarget(digest, true);	// finished after target
			String reqPrefix = req->urlPrefix();
			req->autoDelete();

			// JSON
			if(request.get.contains("json"))
			{
				Http::Response response(request, 307);
				response.headers["Location"] = reqPrefix + (request.get.contains("next") ? "?next=" + request.get["next"] : "");
				response.send();
				return;
			}

			Http::Response response(request, 200);
			response.send();

			Html page(response.stream);

			if(url == "/") page.header(name() + ": Browse files");
			else page.header(name() + ": " + url.substr(1));

			page.open("div","topmenu");
			if(!isSelf()) page.span((isConnected() ? "Connected" : "Disconnected"), "status.button");
			page.link(prefix+"/search/", "Search files", ".button");
			page.link(reqPrefix+"?playlist", "Play all", "playall.button");
			page.close("div");

			unsigned refreshPeriod = 10000;
			page.javascript("setCallback('"+prefix+"/?json', "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				if(info.newmessages) playMailSound();\n\
			});");

			page.div("","list.box");
			page.javascript("listDirectory('"+reqPrefix+"', '#list', '"+requestPath+"');");
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
			if(!isSelf()) page.span((isConnected() ? "Connected" : "Disconnected"), "status.button");
			page.openForm(prefix + "/search", "post", "searchForm");
			page.input("text", "query", match);
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			if(!match.empty()) page.link(reqPrefix+"?playlist","Play all",".button");
			page.close("div");

			unsigned refreshPeriod = 10000;
			page.javascript("setCallback('"+prefix+"/?json', "+String::number(refreshPeriod)+", function(info) {\n\
				transition($('#status'), info.status.capitalize());\n\
				$('#status').removeClass().addClass('button').addClass(info.status);\n\
				if(info.newmessages) playMailSound();\n\
			});");

			if(!match.empty())
			{
				page.div("", "list.box");
				page.javascript("listDirectory('"+reqPrefix+"', '#list', '');");
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
			if(isSelf() || !mBoard) throw 404;

			Http::Response response(request, 301);	// Moved permanently
			if(mBoard) response.headers["Location"] = mBoard->urlPrefix();
			else response.headers["Location"] = mAddressBook->user()->board()->urlPrefix();
			response.send();
			return;
		}
		else if(directory == "chat")
		{
			if(isSelf() || !mPrivateBoard) throw 404;

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
	std::unique_lock<std::mutex> lock(mMutex);

	Object object;
	object.insert("identifier", mIdentifier);
	object.insert("uname", mUniqueName);
	object.insert("name", mName);

	if(s.optionalOutputMode())
	{
		object.insert("instances", mInstances);
		object.insert("prefix", nUrlPrefix());
		object.insert("status", String(nIsConnected() ? "connected" : "disconnected"));
		object.insert("messages", mPrivateBoard ? mPrivateBoard->unread() : 0);
		object.insert("newmessages", mPrivateBoard ? mPrivateBoard->hasNew() : false);
	}
	else {
		object.insert("secret", mRemoteSecret);
	}

	s << object;
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(!(s >> Object()
		.insert("identifier", mIdentifier)
		.insert("uname", mUniqueName)
		.insert("name", mName)
		.insert("secret", mRemoteSecret)))
		return false;

	if(mIdentifier.empty()) return false;
	if(mUniqueName.empty()) return false;
	if(mName.empty()) mName = mUniqueName;
	return true;
}

bool AddressBook::Contact::isInlineSerializable(void) const
{
	return false;
}

}
