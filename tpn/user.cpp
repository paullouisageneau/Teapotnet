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

#include "tpn/user.hpp"
#include "tpn/config.hpp"
#include "tpn/addressbook.hpp"
#include "tpn/resource.hpp"
#include "tpn/html.hpp"

#include "pla/file.hpp"
#include "pla/directory.hpp"
#include "pla/crypto.hpp"
#include "pla/random.hpp"
#include "pla/jsonserializer.hpp"
#include "pla/binaryserializer.hpp"
#include "pla/object.hpp"
#include "pla/mime.hpp"


namespace tpn
{

Map<String, User*>		User::UsersByName;
Map<BinaryString, User*>	User::UsersByAuth;
Map<Identifier, User*>		User::UsersByIdentifier;
std::mutex			User::UsersMutex;

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
	const unsigned iterations = 100000;
	BinaryString auth;
	Sha256().pbkdf2_hmac(password, name, auth, 32, iterations);

	User *user = NULL;
	UsersMutex.lock();
	if(UsersByAuth.get(auth, user))
	{
	  UsersMutex.unlock();
		user->send(password);
		return user;
	}
	UsersMutex.unlock();
	LogWarn("User::Authenticate", "Authentication failed for \""+name+"\"");
	return NULL;
}

User::User(const String &name, const String &password) :
	mName(name),
	mOnline(false)
{
	if(mName.empty())
		throw Exception("Empty user name");

	if(!mName.isAlphanumeric())
		throw Exception("User name must be alphanumeric");

	// Auth digest
	if(password.empty())
	{
    try {
			File file(profilePath()+"auth", File::Read);
			file.read(mAuth);
			file.close();
		}
		catch(...)
		{
			throw Exception("Missing password");
		}
	}
	else {
		const unsigned iterations = 100000;
		Sha256().pbkdf2_hmac(password, name, mAuth, 32, iterations);
	}

	Assert(!mAuth.empty());

	// Load from config file if it exists
	mFileName = profilePath() + "keys";
	load();

	// Generate token secret
	Random rnd(Random::Key);
	rnd.readBinary(mTokenSecret, 16);

	// Register
	User *oldUser = NULL;
	{
		std::lock_guard<std::mutex> lock(UsersMutex);
		auto it = UsersByName.find(mName);
		if(it != UsersByName.end())
			oldUser = it->second;
		UsersByName.insert(mName, this);
		UsersByAuth.insert(mAuth, this);
	}

	// Register interface
	Interface::Instance->add(urlPrefix(), this);

	// Delete old user if it exists
	delete oldUser;

	mOfflineAlarm.set([this]()
	{
		setOffline();
	});
}

User::~User(void)
{
	// Unregister
	{
		std::unique_lock<std::mutex> lock(UsersMutex);
		auto it = UsersByName.find(mName);
		if(it->second == this)
		{
			UsersByName.erase(mName);
			UsersByAuth.erase(mAuth);
		}
	}

	// Unregister interface
	Interface::Instance->remove(urlPrefix(), this);

	mOfflineAlarm.cancel();
}

void User::setKeyPair(const Rsa::PublicKey &pub, const Rsa::PrivateKey &priv)
{
	Identifier identifier = pub.digest();
	Identifier oldIdentifier;
	{
		std::unique_lock<std::mutex> lock(mMutex);

		oldIdentifier = mPublicKey.digest();
		if(oldIdentifier == identifier)
			return;

		mPublicKey = pub;
		mPrivateKey = priv;
		mCertificate = std::make_shared<SecureTransport::RsaCertificate>(mPublicKey, mPrivateKey, identifier.toString());
	}

	// Order matters here
	mIndexer = std::make_shared<Indexer>(this);
	mBoard = std::make_shared<Board>("/" + identifier.toString(), "", mName);
	mAddressBook = std::make_shared<AddressBook>(this);
	mAddressBook->setSelf(identifier);	// create self contact

	// Re-register
	{
		std::lock_guard<std::mutex> lock(UsersMutex);
		UsersByIdentifier.erase(oldIdentifier);
		UsersByIdentifier.insert(identifier, this);
	}
}

void User::setSecret(const BinaryString &secret)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mSecret = secret;
}

bool User::load(void)
{
	if(!File::Exist(mFileName))
		return false;

	File file(mFileName, File::Read);
	JsonSerializer serializer(&file);
	serializer >> *this;
	file.close();

	LogDebug("User::load", "User loaded: " + name() + ", " + identifier().toString());
	return true;
}

void User::save(void) const
{
	// Save auth
	{
		std::unique_lock<std::mutex> lock(mMutex);
		File file(profilePath()+"auth", File::Truncate);
		file.write(mAuth);
		file.close();
	}
	
	// Save keys
	SafeWriteFile file(mFileName);
	JsonSerializer serializer(&file);
	serializer << *this;
	file.close();
}

bool User::recv(const String &password)
{
	Assert(!password.empty());

	const duration timeout = seconds(60);
	Set<BinaryString> values;
	Network::Instance->retrieveValue(mAuth, values, timeout);

  LogDebug("User::import", "Got " + String::number(values.size()) + " candidates");

	for(auto v : values)
	{
		try {
			Resource resource(v);
			if(resource.type() != "user")
				continue;

			Resource::Reader reader(&resource, password);
			JsonSerializer serializer(&reader);
			if(!(serializer >> *this))
				continue;

			return true;
		}
		catch(const Exception &e)
		{
			LogWarn("User::import", e.what());
		}
	}

	return false;
}

void User::send(const String &password) const
{
	Assert(!password.empty());

	Resource resource;
	resource.process(mFileName, "user", "user", password);

	Network::Instance->storeValue(mAuth, resource.digest());
}

void User::generateKeyPair(void)
{
	// Generate keypair
	Rsa rsa(4096);
	rsa.generate(mPublicKey, mPrivateKey);

  // Generate secret
	Random rnd(Random::Key);
	rnd.readBinary(mSecret, 32);

	save();
}

String User::name(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mName;
}

String User::profilePath(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	if(!Directory::Exist(Config::Get("profiles_dir"))) Directory::Create(Config::Get("profiles_dir"));
	String path = Config::Get("profiles_dir") + Directory::Separator + mName;
	if(!Directory::Exist(path)) Directory::Create(path);
	return path + Directory::Separator;
}

String User::fileName(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mFileName;
}

String User::urlPrefix(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return String("/user/") + mName;
}

BinaryString User::secret(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mSecret;
}

sptr<AddressBook> User::addressBook(void) const
{
	return mAddressBook;
}

sptr<Board> User::board(void) const
{
	return mBoard;
}

sptr<Indexer> User::indexer(void) const
{
	return mIndexer;
}

void User::invite(const Identifier &remote, const String &name)
{
	mAddressBook->addInvitation(remote, name);
}

void User::mergeBoard(sptr<const Board> board)
{
	if(!board) return;
	mBoard->addMergeUrl(board->urlPrefix());
}

void User::unmergeBoard(sptr<const Board> board)
{
	if(!board) return;
	mBoard->removeMergeUrl(board->urlPrefix());
}

bool User::isOnline(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mOnline;
}

void User::setOnline(void)
{
	std::unique_lock<std::mutex> lock(mMutex);
	if(!mOnline)
	{
		mOnline = true;
		Network::Instance->overlay()->start();
	}

	mOfflineAlarm.schedule(seconds(60.));
}

void User::setOffline(void)
{
	std::unique_lock<std::mutex> lock(mMutex);

	if(mOnline)
	{
		mOnline = false;
		//sendStatus();
	}

	mOfflineAlarm.cancel();
}

BinaryString User::getSecretKey(const String &action) const
{
	std::unique_lock<std::mutex> lock(mMutex);

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

	{
		std::unique_lock<std::mutex> lock(mMutex);
		splain << mName;
		splain << action;
		splain << salt;
		splain << mTokenSecret;
	}

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

			{
				std::unique_lock<std::mutex> lock(mMutex);
				splain << mName;
				splain << action;
				splain << salt;
				splain << mTokenSecret;
			}

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
	std::unique_lock<std::mutex> lock(mMutex);
	return mPublicKey.digest();
}

Rsa::PublicKey User::publicKey(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mPublicKey;
}

Rsa::PrivateKey User::privateKey(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mPrivateKey;
}

sptr<SecureTransport::Certificate> User::certificate(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mCertificate;
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
					if(!request.remoteAddress.isLocal()) throw 403;
					if(!Config::LaunchUpdater()) throw 500;

					Http::Response response(request, 200);
					response.send();
					Html page(response.stream);
					page.header("Please wait", true);
					page.open("div", "notification");
					page.image("/static/loading.png", "Please wait");
					page.br();
					page.open("h1",".huge");
					page.text("Updating and restarting...");
					page.close("h1");
					page.close("div");
					page.javascript("setTimeout(function() {window.location.href = \""+redirect+"\";}, 20000);");
					page.footer();
					response.stream->close();

					std::this_thread::sleep_for(seconds(1.));	// Some time for the browser to load resources

					LogInfo("User::http", "Exiting");
					exit(0);
				}
				else if(command == "shutdown")
				{
					if(!request.remoteAddress.isLocal()) throw 403;
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

			// TODO: Move this into CSS
			page.javascript("$('#page').css('max-width','100%');");

#if defined(WINDOWS)
                        if(request.remoteAddress.isLocal() && Config::IsUpdateAvailable())
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
                        if(request.remoteAddress.isLocal() && Config::IsUpdateAvailable())
                        {
                                page.open("div", "updateavailable.banner");
				page.openForm(prefix+'/', "post", "shutdownAndUpdateForm");
				page.input("hidden", "token", generateToken("admin"));
                        	page.input("hidden", "command", "shutdown");
                        	page.input("hidden", "redirect", String(DOWNLOADURL) + "?release=osx&update=1");
				page.text("New version available - ");
                                page.link(String(DOWNLOADURL) + "?release=osx&update=1", "Quit and download now", "shutdownAndUpdateLink");
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
			page.openLink("/"); page.image("/static/logo.png", APPNAME); page.closeLink();
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
			const String instance = Network::Instance->overlay()->localName().before('.');
			//page.openLink(profile()->urlPrefix());
			//page.image(profile()->avatarUrl(), "", ".avatar");	// NO alt text for avatars
			page.text(name());
#ifndef ANDROID
			if(addressBook()->getSelf() && !instance.empty()) page.text(" (" + instance + ")");
#endif
			//page.closeLink();
			page.close("h1");
			page.close("div");

			page.open("div","contacts.box");

			page.link(prefix+"/contacts/","Edit",".button");

			page.open("h2");
			page.text("Contacts");
			page.close("h2");

			if(mAddressBook->count() == 0) page.link(prefix+"/contacts/","Add contact / Accept request");
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
					page.image("/static/dir.png");

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

			page.raw("<iframe name=\"main\" src=\""+mBoard->urlPrefix()+"?frame\"></iframe>");

			page.close("div");	// rightcolumn

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
		/*else if(directory == "avatar" || request.url == "/myself/avatar")
		{
			Http::Response response(request, 303);	// See other
			response.headers["Location"] = profile()->avatarUrl();
			response.send();
			return;
		}*/
		else if(directory == "myself")
		{
			Http::Response response(request, 303);	// See other
			response.headers["Location"] = prefix + "/files" + (request.get.contains("json") ? "?json" + (request.get.contains("next") ? "&next=" + request.get["next"] : "") : "");
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
	std::unique_lock<std::mutex> lock(mMutex);

	s << Object()
		.insert("publickey", mPublicKey)
		.insert("privatekey", mPrivateKey)
		.insert("secret", mSecret);
}

bool User::deserialize(Serializer &s)
{
	Rsa::PublicKey pub;
	Rsa::PrivateKey priv;
	BinaryString secret;

	if(!(s >> Object()
		.insert("publickey", pub)
		.insert("privatekey", priv)
		.insert("secret", secret)))
		return false;

	setKeyPair(pub, priv);
	setSecret(secret);

	// Reload self contact is it exists
	if(mAddressBook && mAddressBook->getSelf())
		mAddressBook->setSelf(identifier());

	save();
	return true;
}

bool User::isInlineSerializable(void) const
{
        return false;
}

}
