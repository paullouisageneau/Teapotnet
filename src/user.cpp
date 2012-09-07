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
#include "html.h"

namespace arc
{

User::User(const String &name) :
	mName(name),
	mAddressBook(new AddressBook(name))
{
	Interface::Instance->add("/"+name, this);
}

User::~User(void)
{
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
			
			page.footer();
		}
		else throw 404;
	}
	catch(Exception &e)
	{
		Log("User::http",e.what());
		throw 404;	// Httpd handles integer exceptions
	}
}

void User::run(void)
{
	synchronize(this);
	
	while(true)
	{
		mAddressBook->update();
		wait(2*60*1000);
	}
}

}
