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
#include "file.h"
#include "html.h"

namespace arc
{

AddressBook::AddressBook(const String &name)
{
	Interface::Instance->add(name+"/contacts", this);
}

AddressBook::~AddressBook(void)
{
  
}

const Identifier &AddressBook::addContact(String &name, ByteString &secret)
{
	synchronize(this);
	
	Identifier peering;
	computePeering(name, secret, peering);
	
	Contact &contact = mContacts[peering];
	contact.name = name;
	contact.secret = secret;
	contact.peering = peering;
	contact.tracker = contact.name.cut('@');
	if(contact.tracker.empty()) contact.tracker = Config::Get("tracker");
	
	contact.remotePeering.clear();
	computeRemotePeering(name, secret, contact.remotePeering);
	
	autosave();
	notify();
	return contact.peering;
}

void AddressBook::removeContact(Identifier &peering)
{
	synchronize(this);
 	mContacts.erase(peering);
	autosave();
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

void AddressBook::load(Stream &stream)
{
	synchronize(this);
  
	stream.readLine(mLocalName);
	
	mContacts.clear();
	Contact contact;
	while(stream.read(contact))
	{
		mContacts.insert(contact.peering, contact);
	}	
}

void AddressBook::save(Stream &stream) const
{
	synchronize(this);
  
	stream.writeLine(mLocalName);
	
	for(Map<Identifier, Contact>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact &contact = it->second;
		stream.write(contact);
	}	
}

void AddressBook::autosave(void) const
{
	synchronize(this);
  
	SafeWriteFile file(mLocalName+"_contacts.txt");
	save(file);
	file.close();
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	synchronize(this);
	
	try {
		String url = request.url;
		
		if(url.empty() || url == "/")
		{
			if(request.method == "POST")
			{
				String name, csecret;
				request.post["name"] >> name;
				request.post["secret"] >> csecret;
				
				ByteString secret;
				Sha512::Hash(csecret, secret, Sha512::CryptRounds);
				
				addContact(name, secret);
			}
			
			Http::Response response(request,200);
			response.send();
			
			Html page(response.sock);
			page.header("Contacts");
			page.open("h1");
			page.text("Contacts");
			page.close("h1");

			for(Map<Identifier, Contact>::iterator it = mContacts.begin();
				it != mContacts.end();
				++it)
			{
		    		Contact &contact = it->second;
				String contactUrl;
				contactUrl << prefix << "/" << contact.peering;
				page.link(contact.name, contactUrl);
				page.br();
			}

			page.open("h2");
			page.text("Add new contact");
			page.close("h2");
			page.openForm(prefix+"/","post");
			page.text("Name");
			page.input("text","name");
			page.br();
			page.text("Secret");
			page.input("text","secret");
			page.br();
			page.closeForm();
			
			page.footer();
		}
		else {
			if(url[0] == '/') url.ignore();
			
			ByteString peering;
			url >> peering;

		  	Contact &contact = mContacts.get(peering);
			
			Http::Response response(request,200);
			response.send();
				
			Html page(response.sock);
			page.header(contact.name);
			page.open("h1");
			page.text(contact.name);
			page.close("h1");

			page.text("Secret: " + contact.secret.toString()); page.br();
			page.text("Peering: " + contact.peering.toString()); page.br();
			page.text("Remote peering: " + contact.remotePeering.toString()); page.br();
			
			page.footer();
			
		}
	}
	catch(...)
	{
		// TODO: Log
		throw 404;	// Httpd handles integer exceptions
	}
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

		    if(query(contact.peering, contact.tracker, contact.addrs))
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
					lock();
				}
			}
		    }
		      
		    publish(contact.remotePeering);
		}
		
		autosave();
		wait(10*60*1000);
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

bool AddressBook::query(const Identifier &peering, const String &tracker, Array<Address> &addrs)
{
	try {
	  	String url;
	  	if(tracker.empty()) url = "http://" + Config::Get("tracker") + "/" + peering.toString();
		else url = "http://" + tracker + "/" + peering.toString();
  
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

void AddressBook::Contact::serialize(Stream &s) const
{
	StringMap map;
	map["name"] << name;
	map["tracker"] << tracker;
	map["secret"] << secret;
	map["peering"] << peering;
	map["remotePeering"] << remotePeering;
		
	s.write(map);
	s.write(addrs);
}

void AddressBook::Contact::deserialize(Stream &s)
{
	name.clear();
	secret.clear();
	peering.clear();
	remotePeering.clear();
	
	StringMap map;
	s.read(map);
	map["name"] >> name;
	map["tracker"] >> tracker;
	map["secret"] >> secret;
	map["peering"] >> peering;
	map["remotePeering"] >> remotePeering;
	
	s.read(addrs);
}

}
