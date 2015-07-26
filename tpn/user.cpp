/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#include "tpn/user.h"
#include "tpn/config.h"
#include "tpn/addressbook.h"
#include "tpn/html.h"

#include "pla/file.h"
#include "pla/directory.h"
#include "pla/crypto.h"
#include "pla/random.h"
#include "pla/jsonserializer.h"
#include "pla/binaryserializer.h"
#include "pla/mime.h"


namespace tpn
{

Map<String, User*>		User::UsersByName;
Map<BinaryString, User*>	User::UsersByAuth;
Map<Identifier, User*>		User::UsersByIdentifier;
Mutex				User::UsersMutex;

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

User *User::GetByIdentifier(const Identifier &id)
{
	User *user = NULL;
	UsersMutex.lock();
	if(UsersByIdentifier.get(id, user)) 
	{
	  	UsersMutex.unlock();
		return user;
	}
	UsersMutex.unlock();
	return NULL; 
}

User *User::Authenticate(const String &name, const String &password)
{
	BinaryString hash;
	Sha256().pbkdf2_hmac(password, name, hash, 32, 10000);
	
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
		
		// TODO
		//user->addressBook()->update();
	}
}

User::User(const String &name, const String &password) :
	mName(name),
	mOnline(false),
	mSetOfflineTask(this)
{
	if(mName.empty()) 
		throw Exception("Empty user name");
	
	if(!mName.isAlphanumeric()) 
		throw Exception("User name must be alphanumeric");
	
	// Auth digest
	if(password.empty())
	{
		File file(profilePath()+"password", File::Read);
		file.read(mAuth);
		file.close();
	}
	else {
		Sha256().pbkdf2_hmac(password, name, mAuth, 32, 10000);
	
		File file(profilePath()+"password", File::Truncate);
		file.write(mAuth);
		file.close();
	}
	
	// User config file
	mFileName = profilePath() + "user";
	
	// Load if config file exist
	load();
	
	// Generate RSA key and secret if necessary
	Random rnd(Random::Key);
	if(mPublicKey.isNull())
	{
		Rsa rsa(4096);
		rsa.generate(mPublicKey, mPrivateKey);
		rnd.readBinary(mSecret, 32);
		
		save();
	}
	
	Assert(File::Exist(mFileName));
	
	// Generate token secret
	rnd.readBinary(mTokenSecret, 16);
	
	Assert(!mPublicKey.isNull());
	Assert(!mSecret.empty());
	Assert(!mTokenSecret.empty());
	
	mMasterCertificate = NULL;
	mLocalCertificate = NULL;
	mIndexer = NULL;
	mAddressBook = NULL;
	mBoard = NULL;

	try {
		mMasterCertificate = new SecureTransport::RsaCertificate(mMasterPublicKey, mMasterPrivateKey, identifier().toString());
		mLocalCertificate = new SecureTransport::RsaCertificate(mLocalPublicKey, mLocalPrivateKey, instance().toString());
		
		mIndexer = new Indexer(this);
        	mAddressBook = new AddressBook(this);
       	 	mBoard = new Board("/" + identifier().toString(), mName);
	}
	catch(...)
	{
		delete mMasterCertificate;
		delete mLocalCertificate;
		delete mIndexer;
		delete mAddressBook;
		delete mBoard;
		throw;
	}

	UsersMutex.lock();
	UsersByName.insert(mName, this);
	UsersByAuth.insert(mAuth, this);
	UsersByIdentifier.insert(identifier(), this);
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
	Scheduler::Global->cancel(&mSetOfflineTask);
	
	delete mMasterCertificate;
	delete mLocalCertificate;
	delete mAddressBook;
	delete mBoard;
	delete mIndexer;
}

void User::load()
{
	Synchronize(this);
	
	if(!File::Exist(mFileName)) return;
	File file(mFileName, File::Read);
	JsonSerializer serializer(&file);
	serializer.read(*this);
	file.close();
}

void User::save() const
{
	Synchronize(this);
	
	SafeWriteFile file(mFileName);
	JsonSerializer serializer(&file);
	serializer.write(*this);
	file.close();
}

String User::name(void) const
{
	Synchronize(this);
	return mName; 
}

String User::profilePath(void) const
{
	Synchronize(this);
	if(!Directory::Exist(Config::Get("profiles_dir"))) Directory::Create(Config::Get("profiles_dir"));
	String path = Config::Get("profiles_dir") + Directory::Separator + mName;
	if(!Directory::Exist(path)) Directory::Create(path);
	return path + Directory::Separator;
}

String User::fileName(void) const
{
	Synchronize(this);
	return mFileName;
}

String User::urlPrefix(void) const
{
	Synchronize(this);
	return String("/user/") + mName;
}

BinaryString User::secret(void) const
{
	Synchronize(this);
	return mSecret;
}

AddressBook *User::addressBook(void) const
{
	return mAddressBook;
}

Board *User::board(void) const
{
	return mBoard;
}

Indexer *User::indexer(void) const
{
	return mIndexer;
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
		//sendStatus();
	
		// TODO	
		//DesynchronizeStatement(this, mAddressBook->update());
	}
	
	Scheduler::Global->schedule(&mSetOfflineTask, 60.);
}

void User::setOffline(void)
{
	Synchronize(this);
	
	if(mOnline) 
	{
		mOnline = false;
		//sendStatus();
	}
}

BinaryString User::getSecretKey(const String &action) const
{
	Synchronize(this);
	
	// Cache for subkeys
	BinaryString key;
	if(!mSecretKeysCache.get(action, key))
	{
		Sha256().pbkdf2_hmac(mSecret, action, key, 32, 100000);
		mSecretKeysCache.insert(action, key);
	}

	return key;
}

String User::generateToken(const String &action) const
{
	Random rnd(Random::Nonce);
	
	BinaryString salt;
	salt.writeBinary(rnd, 8);

	BinaryString plain;
	BinarySerializer splain(&plain);
	splain.output(name());
	splain.output(action);
	splain.output(salt);
	SynchronizeStatement(this, splain.output(mTokenSecret));

	BinaryString digest;
	Sha256().compute(plain, digest);
	
	BinaryString key;
	digest.readBinary(key, 8);

	BinaryString token;
	token.writeBinary(salt);	// 8 bytes
	token.writeBinary(key);		// 8 bytes
	
	Assert(token.size() == 16);
	return token;
}

bool User::checkToken(const String &token, const String &action) const
{
	if(!token.empty())
	{
		BinaryString bs;
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
			BinaryString salt, remoteKey;
			AssertIO(bs.readBinary(salt, 8));
			AssertIO(bs.readBinary(remoteKey, 8));
			
			BinaryString plain;
			BinarySerializer splain(&plain);
			splain.output(name());
			splain.output(action);
			splain.output(salt);
			SynchronizeStatement(this, splain.output(mTokenSecret));
			
			BinaryString digest;
			Sha256().compute(plain, digest);
			
			BinaryString key;
			digest.readBinary(key, 8);

			if(key == remoteKey) 
				return true;
		}
	}
	
	LogDebug("User::checkToken", String("Invalid token") + (!action.empty() ? " for action \"" + action + "\"" : ""));
	return false;
}

Identifier User::identifier(void) const
{
	Synchronize(this);
	return Identifier(mMasterPublicKey.digest());
}

Identifier User::instance(void) const
{
	Synchronize(this);
	return Identifier(mLocalPublicKey.digest());
}

const Rsa::PublicKey &User::masterPublicKey(void) const
{
	Synchronize(this);
	return mMasterPublicKey.digest();
}

const Rsa::PrivateKey &User::masterPrivateKey(void) const
{
	Synchronize(this);
	return mMasterPrivateKey.digest();
}

const Rsa::PublicKey &User::localPublicKey(void) const
{
	Synchronize(this);
	return mLocalPublicKey.digest();
}

SecureTransport::Certificate *User::masterCertificate(void) const
{
	return mMasterCertificate;
}

SecureTransport::Certificate *User::localCertificate(void) const
{
	return mLocalCertificate;
}

void User::http(const String &prefix, Http::Request &request)
{
	Assert(!request.url.empty());
	
	try {
		setOnline();
		
		String url = request.url;
		if(url == "/")
		{
			if(request.method == "POST")
			{
				if(!checkToken(request.post["token"], "admin"))
					throw 403;

				String redirect;
				request.post.get("redirect", redirect);
				if(redirect.empty()) redirect = prefix + "/";
				
				String command = request.post["command"];
				
				bool shutdown = false;

				if(command == "update")
				{
					//if(!request.sock->getRemoteAddress().isLocal()) throw 403;	// TODO
					if(!Config::LaunchUpdater()) throw 500;
					
					Http::Response response(request, 200);
					response.send();
					Html page(response.stream);
					page.header("Please wait", true);
					page.open("div", "notification");
					page.image("/loading.png", "Please wait");
					page.br();
					page.open("h1",".huge");
					page.text("Updating and restarting...");
					page.close("h1");
					page.close("div");
					page.javascript("setTimeout(function() {window.location.href = \""+redirect+"\";}, 20000);");
					page.footer();
					response.stream->close();
					
					Thread::Sleep(1.);	// Some time for the browser to load resources
					
					LogInfo("User::http", "Exiting");
					exit(0);
				}
				else if(command == "shutdown")
				{
					// TODO
					//if(!request.sock->getRemoteAddress().isLocal()) throw 403;
					shutdown = true;
				}
				else throw 400;
				
				Http::Response response(request, 303);
				response.headers["Location"] = redirect;
				response.send();
				response.stream->close();
				
				if(shutdown)
				{
					LogInfo("User::http", "Shutdown");
					exit(0);
				}

				return;
			}
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.stream);
			page.header(APPNAME, true);

			// TODO: This is awful
			page.javascript("$('#page').css('max-width','100%');");
			
#if defined(WINDOWS)
                        if(request.sock->getRemoteAddress().isLocal() && Config::IsUpdateAvailable())
                        {
                                page.open("div", "updateavailable.banner");
				page.openForm(prefix+'/', "post", "shutdownAndUpdateForm");
				page.input("hidden", "token", generateToken("admin"));
                        	page.input("hidden", "command", "update");
				page.text("New version available - ");
                                page.link("#", "Update now", "shutdownAndUpdateLink");
				page.closeForm();
				page.javascript("$('#shutdownAndUpdateLink').click(function(event) {\n\
					event.preventDefault();\n\
					document.shutdownAndUpdateForm.submit();\n\
				});");
                                page.close("div");
                        }
#endif
		
#if defined(MACOSX)
                        if(request.sock->getRemoteAddress().isLocal() && Config::IsUpdateAvailable())
                        {
                                page.open("div", "updateavailable.banner");
				page.openForm(prefix+'/', "post", "shutdownAndUpdateForm");
				page.input("hidden", "token", generateToken("admin"));
                        	page.input("hidden", "command", "shutdown");
                        	page.input("hidden", "redirect", String(SECUREDOWNLOADURL) + "?release=osx&update=1");
				page.text("New version available - ");
                                page.link(String(SECUREDOWNLOADURL) + "?release=osx&update=1", "Quit and download now", "shutdownAndUpdateLink");
				page.closeForm();
				page.javascript("$('#shutdownAndUpdateLink').click(function(event) {\n\
					event.preventDefault();\n\
					document.shutdownAndUpdateForm.submit();\n\
				});");
                                page.close("div");
                        }
#endif
		
			page.open("div", "wrapper");
			page.open("div","leftcolumn");
			page.open("div","leftpage");

			page.open("div", "logo");
			page.openLink("/"); page.image("/logo.png", APPNAME); page.closeLink();
			page.close("div");

			page.open("div","search");
			page.openForm(prefix + "/search", "post", "searchForm");
			page.input("text","query", "Search for files...");
			//page.button("search","Search");
			page.closeForm();
			//page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");	// really annoying with touchscreens
			page.javascript("$(document).ready(function() {\n\
					document.searchForm.query.style.color = 'grey';\n\
				});\n\
				$(document.searchForm.query).focus(function() {\n\
					document.searchForm.query.value = '';\n\
					document.searchForm.query.style.color = 'black';\n\
				})\n\
				$(document.searchForm.query).blur(function() {\n\
					document.searchForm.query.style.color = 'grey';\n\
					document.searchForm.query.value = 'Search for files...';\n\
				})\n");
			//page.br();
			page.close("div");

			page.open("div", "header");
			page.link("/?changeuser", "Change account", ".button");
			page.open("h1");
			const String instance = Network::Instance->getName().before('.');
			page.openLink(profile()->urlPrefix());
			page.image(profile()->avatarUrl(), "", ".avatar");	// NO alt text for avatars
			page.text(name());
#ifndef ANDROID
			if(addressBook()->getSelf() && !instance.empty()) page.text(" (" + instance + ")");
#endif
			page.closeLink();
			page.close("h1");
			page.close("div");
			
			page.open("div","contacts.box");
			
			page.link(prefix+"/contacts/","Edit",".button");
		
			page.open("h2");
			page.text("Contacts");
			page.close("h2");
		
			// TODO
			AddressBook::Contact *self = mAddressBook->getSelf();
			Array<AddressBook::Contact*> contacts;
			mAddressBook->getContacts(contacts);
			
			if(contacts.empty() && !self) page.link(prefix+"/contacts/","Add contact / Accept request");
			else {
				page.open("div", "contactsTable");
				page.open("p"); page.text("Loading..."); page.close("p");
				page.close("div");
				
				unsigned refreshPeriod = 5000;
				page.javascript("displayContacts('"+prefix+"/contacts/?json"+"','"+String::number(refreshPeriod)+"','#contactsTable')");
			}
			
			page.close("div");

			page.open("div","files.box");
			
			Array<String> directories;
			mIndexer->getDirectories(directories);
			
			page.link(prefix+"/files/","Edit",".button");
			if(!directories.empty()) page.link(prefix+"/files/?action=refresh&redirect="+String(prefix+url).urlEncode(), "Refresh", "refreshfiles.button");
			
			page.open("h2");
			page.text("Shared folders");
			page.close("h2");
			
			if(directories.empty()) page.link(prefix+"/files/","Add shared folder");
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
			
			page.open("div", "footer");
			page.text(String("Version ") + APPVERSION + " - ");
			page.link(HELPLINK, "Help", "", true);
			page.text(" - ");
			page.link(SOURCELINK, "Source code", "", true);
			page.text(" - ");
			page.link(BUGSLINK, "Report a bug", "", true);
			page.close("div");
			
			page.close("div"); // leftpage
			page.close("div"); // leftcolumn

			page.open("div", "rightcolumn");
			
			page.raw("<iframe src=\""+mBoard->urlPrefix()+"?frame\"></iframe>");
			
			page.close("div");	// rightcolumn

			page.close("div");
			page.close("div");
			
			page.footer();
			return;
		}
		
		String directory = url;
		directory.ignore();		// remove first '/'
		url = "/" + directory.cut('/');
		if(directory.empty()) throw 404;
		
		if(directory == "search")
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
				Request *req = new Request("/files?" + match, false);
				reqPrefix = req->urlPrefix();
				req->autoDelete();
			}
			
			Http::Response response(request, 200);
			response.send();
				
			Html page(response.stream);
			
			if(match.empty()) page.header(name() + ": Search");
			else page.header(name() + ": Searching " + match);
			
			page.open("div","topmenu");
			page.openForm(prefix + "/search", "post", "searchForm");
			page.input("text", "query", match);
			page.button("search","Search");
			page.closeForm();
			page.javascript("$(document).ready(function() { document.searchForm.query.focus(); });");
			if(!match.empty()) page.link(reqPrefix+"?playlist","Play all",".button");
			page.close("div");
			
			if(!match.empty())
			{
				page.div("", "list.box");
				page.javascript("listDirectory('"+reqPrefix+"','#list',true,true);");
			}
			
			page.footer();
			return;
		}
		else if(directory == "avatar" || request.url == "/myself/avatar")
		{
			Http::Response response(request, 303);	// See other
			response.headers["Location"] = profile()->avatarUrl(); 
			response.send();
			return;
		}
		else if(directory == "myself")
		{
			Http::Response response(request, 303);	// See other
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

void User::serialize(Serializer &s) const
{
	Synchronize(this);

	// TODO
	Serializer::ConstObjectMapping mapping;
	mapping["publickey"] = &mPublicKey;
	mapping["privatekey"] = &mPrivateKey;
	mapping["secret"] = &mSecret;
	
	s.outputObject(mapping);
}

bool User::deserialize(Serializer &s)
{
	Synchronize(this);
	
	Identifier oldIdentifier = identifier();
	
	mPublicKey.clear();
	mPrivateKey.clear();
	mSecret.clear();
	
	Serializer::ObjectMapping mapping;
	mapping["publickey"] = &mPublicKey;
	mapping["privatekey"] = &mPrivateKey;
	mapping["secret"] = &mSecret;
	
	if(!s.inputObject(mapping))
		return false;
	
	if(!mPublicKey.isNull() && !mPrivateKey.isNull())
	{
		// Register
		if(oldIdentifier != identifier())
		{
			UsersMutex.lock();
			UsersByIdentifier.erase(oldIdentifier);
			UsersByIdentifier.insert(identifier(), this);
			UsersMutex.unlock();
		}
		
		// Reload certificates
		delete mMasterCertificate;
		delete mLocalCertificate;
		mMasterCertificate = new SecureTransport::RsaCertificate(mMasterPublicKey, mMasterPrivateKey, identifier().toString());
		mLocalCertificate = new SecureTransport::RsaCertificate(mLocalPublicKey, mLocalPrivateKey, instance().toString());
	}
	
	return true;
}

bool User::isInlineSerializable(void) const
{
        return false;
}

}
