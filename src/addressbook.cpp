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

#include "addressbook.h"
#include "core.h"
#include "sha512.h"
#include "config.h"

namespace arc
{

AddressBook::AddressBook(void)
{
  
}

AddressBook::~AddressBook(void)
{
  
}

const Identifier &AddressBook::addContact(String &name, String &secret)
{
	synchronize(this);
	
	Identifier peering;
	computePeering(name, secret, peering);
	
	Contact &contact = mContacts[peering];
	contact.name = name;
	contact.secret = secret;
	contact.peering = peering;
	
	contact.remotePeering.clear();
	computeRemotePeering(name, secret, contact.remotePeering);
	
	notify();
	return contact.peering;
}

void AddressBook::removeContact(Identifier &peering)
{
	synchronize(this);
 	mContacts.erase(peering);
}

void AddressBook::computePeering(const String &name, const ByteString &secret, ByteStream &out)
{
  	String agregate;
	agregate.writeLine(secret);
	agregate.writeLine(mLocalName);
	agregate.writeLine(name);
	Sha512::Hash(agregate, out, Sha512::CryptRounds);
}

void AddressBook::computeRemotePeering(const String &name, const ByteString &secret, ByteStream &out)
{
  	String agregate;
	agregate.writeLine(secret);
	agregate.writeLine(name);
	agregate.writeLine(mLocalName);
	Sha512::Hash(agregate, out, Sha512::CryptRounds);
}

void AddressBook::http(Http::Request &request)
{
	synchronize(this);
	
	
}

void AddressBook::run(void)
{
	synchronize(this);
	
	while(true)
	{
		for(Map<Identifier, Contact>::iterator it = mContacts.begin();
				      it != mContacts.end();
				      ++it)
		{
		    Contact &contact = it->second;

		    if(query(contact.peering, contact.addrs))
		    {
		    	if(!Core::Instance->hasPeer(contact.peering))
			{
				for(int i=0; i<contact.addrs.size(); ++i)
				{
					const Address &addr = contact.addrs[contact.addrs.size()-(i+1)];
					unlock();
					try {
						Socket *sock = new Socket(addr);
						Core::Instance->addPeer(sock, contact.peering, contact.remotePeering);
					}
					catch(...)
					{
					 
					}
				}
			}
		    }
		      
		    publish(contact.remotePeering);
		}
		
		wait(30*60*1000);
	}
}

bool AddressBook::publish(const Identifier &remotePeering)
{
	try {
		String url("http://" + Config::Get("tracker") + "/" + remotePeering.toString());
		
		StringMap post;
		post["port"] = Config::Get("port");
		if(Http::Post(url, post) != 200) return false;
	}
	catch(...)
	{
		// LOG
		return false;
	}
	return true;
}

bool AddressBook::query(const Identifier &peering, Array<Address> &addrs)
{
	try {
		String url("http://" + Config::Get("tracker") + "/" + peering.toString());
  
		String output;
		if(Http::Get(url, &output) != 200) return false;
	
		Address a;
		while(output.read(a))
			if(!addrs.contains(a))
				addrs.push_back(a); 
	}
	catch(...)
	{
		// LOG
		return false;
	}
	return true;
}

}
