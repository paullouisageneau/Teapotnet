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

#include "user.h"
#include "config.h"
#include "file.h"
#include "directory.h"
#include "sha512.h"
#include "html.h"

namespace arc
{

Map<Identifier, User*> User::UsersMap;
  
User *User::Authenticate(const String &name, const String &password)
{
	Identifier hash;
	String agregate;
	agregate.writeLine(name);
	agregate.writeLine(password);
	Sha512::Hash(agregate, hash, Sha512::CryptRounds);
	
	User *user = NULL;
	if(!UsersMap.get(hash, user)) throw Exception("Authentication failed");
	return user;
}

User::User(const String &name, const String &password) :
	mName(name),
	mAddressBook(new AddressBook(this))
{
	if(password.empty())
	{
		File file(profilePath()+"password", File::Read);
		file.read(mHash);
		file.close();
	}
	else {
		String agregate;
		agregate.writeLine(name);
		agregate.writeLine(password);
		Sha512::Hash(agregate, mHash, Sha512::CryptRounds);
		
		File file(profilePath()+"password", File::Write);
		file.write(mHash);
		file.close();
	}
	
	Interface::Instance->add("/"+name, this);
	UsersMap.insert(mHash, this);
}

User::~User(void)
{
	UsersMap.erase(mHash);
	Interface::Instance->remove("/"+mName);
	delete mAddressBook;
}

void User::http(const String &prefix, Http::Request &request)
{
	synchronize(this);
	
	try {
		String url = request.url;
		
		if(url.empty() || url == "/")
		{
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header(mName);
			page.open("h1");
			page.text(mName);
			page.close("h1");

			page.link(prefix+"/contacts/","Contacts");
			page.br();
			page.br();
			
			page.footer();
			return;
		}
	}
	catch(Exception &e)
	{
		Log("User::http",e.what());
		throw 404;	// Httpd handles integer exceptions
	}
			
	throw 404;
}

const String &User::name(void) const
{
	return mName; 
}

String User::profilePath(void) const
{
	if(!Directory::Exist(Config::Get("profiles_dir"))) Directory::Create(Config::Get("profiles_dir"));
	String path = Config::Get("profiles_dir") + Directory::Separator + mName;
	if(!Directory::Exist(path)) Directory::Create(path);
	return path + Directory::Separator;
}

void User::run(void)
{
	while(true)
	{
		mAddressBook->update();
		mAddressBook->wait(2*60*1000);	// warning: this must not be locked when waiting for mAddressBook
	}
}

}
