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
#include "directory.h"
#include "html.h"

namespace arc
{

AddressBook::AddressBook(const String &name) :
	mName(name)
{
	Interface::Instance->add("/"+name+"/contacts", this);
	
	try {
	  File file(fileName(), File::Read);
	  load(file);
	  file.close();
	}
	catch(...)
	{
	  
	}
}

AddressBook::~AddressBook(void)
{
  	Interface::Instance->remove("/"+mName+"/contacts");
}

const Identifier &AddressBook::addContact(String &name, ByteString &secret)
{
	synchronize(this);
	
	Identifier peering;
	computePeering(name, secret, peering);
	
	if(mContacts.contains(peering)) throw Exception("Contact already exists");
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
	if(mContacts.contains(peering))
	{
		Core::Instance->unregisterPeering(peering);
 		mContacts.erase(peering);
		autosave();
	}
}

void AddressBook::computePeering(const String &name, const ByteString &secret, ByteStream &out)
{
  	String agregate;
	agregate.writeLine(secret);
	agregate.writeLine(mName);
	agregate.writeLine(name);
	Sha512::Hash(agregate, out, Sha512::CryptRounds);
}

void AddressBook::computeRemotePeering(const String &name, const ByteString &secret, ByteStream &out)
{
  	String agregate;
	agregate.writeLine(secret);
	agregate.writeLine(name);
	agregate.writeLine(mName);
	Sha512::Hash(agregate, out, Sha512::CryptRounds);
}

void AddressBook::load(Stream &stream)
{
	synchronize(this);
	
	Contact contact;
	while(stream.read(contact))
	{
		mContacts.insert(contact.peering, contact);
	}	
}

void AddressBook::save(Stream &stream) const
{
	synchronize(this);
  
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
  
	SafeWriteFile file(fileName());
	save(file);
	file.close();
}

void AddressBook::update(void)
{
	synchronize(this);
	Log("AddressBook::update", "Updating " + String::number(unsigned(mContacts.size())) + " contacts");
	
	for(Map<Identifier, Contact>::iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		Contact &contact = it->second;

		if(!Core::Instance->hasPeer(contact.peering))
		{
			Core::Instance->registerPeering(contact.peering, contact.remotePeering, contact.secret);
		  
			Log("AddressBook::update", "Querying tracker " + contact.tracker);
			if(query(contact.peering, contact.tracker, contact.addrs))
			{
				for(int i=0; i<contact.addrs.size(); ++i)
				{
					const Address &addr = contact.addrs[contact.addrs.size()-(i+1)];
					unlock();
					try {
						Socket *sock = new Socket(addr);
						Core::Instance->addPeer(sock, contact.peering);
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
		
	Log("AddressBook::update", "Finished");
	autosave();
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
				try {
					String name, csecret;
					request.post["name"] >> name;
					request.post["secret"] >> csecret;
				  
					ByteString secret;
					Sha512::Hash(csecret, secret, Sha512::CryptRounds);
					
					addContact(name, secret);
				}
				catch(...)
				{
					throw 400;
				}				
				
				Http::Response response(request,303);
				response.headers["Location"] = prefix + "/";
				response.send();
				return;
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
				page.link(contactUrl, contact.name+"@"+contact.tracker);
				
				int checksum = contact.peering.checksum32() + contact.remotePeering.checksum32();
				page.text(" "+String::hexa(checksum,8));
				
				String status("Not connected");
				if(Core::Instance->hasPeer(contact.peering)) status = "Connected";
				page.text(" ("+status+")");
				
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
			page.button("Add contact");
 			page.br();
			page.closeForm();
			
			page.footer();
		}
		else {
			if(url[0] == '/') url.ignore();
			String speering = url;
			url = "/" + speering.cut('/');
			
			if(url == "/" && request.url[request.url.size()-1] != '/')
			{
			 	Http::Response response(request, 301);	// Moved Permanently
				response.headers["Location"] = prefix+request.url+'/';
				response.send();
				return;
			}
			
			Identifier peering;
			speering >> peering;
		  	Contact &contact = mContacts.get(peering);
			
			String contactRoot(prefix+'/'+peering.toString()+'/');
			
			Http::Response response(request,200);
			response.send();
				
			Html page(response.sock);
			page.header("Contact: "+contact.name);
			page.open("h1");
			page.text("Contact: "+contact.name);
			page.close("h1");

			page.text("Secret: " + contact.secret.toString()); page.br();
			page.text("Peering: " + contact.peering.toString()); page.br();
			page.text("Remote peering: " + contact.remotePeering.toString()); page.br();
			page.br();
			
			try {
				String target(url);
				if(target.size() > 1 && target[target.size()-1] == '/') target = target.substr(0, target.size()-1);
				
				Request request(target);
				request.submit(contact.peering);
				request.wait();
				
				for(int i=0; i<request.responsesCount(); ++i)
				{
					Request::Response *response = request.response(i);
					StringMap parameters = response->parameters();
					
					if(!response->content()) page.text("No content...");
					else {	
						try {
							if(parameters.contains("type") && parameters["type"] == "directory")
							{
								StringMap info;
							
									String base(url.substr(url.lastIndexOf('/')+1));
									if(!base.empty()) base+= '/';
									while(response->content()->read(info))
									{
										if(info.get("type") == "directory") page.link(base + info.get("name"), info.get("name"));
										else page.link("/" + info.get("hash"), info.get("name"));
										page.br();
									}
								
							}
							else {
								page.text("Download ");
								page.link("/" + parameters.get("hash"), parameters.get("name"));
								page.br();
							}
						}
						catch(...)
						{

						}	
						
						response->content()->close();
					}
				}
			}
			catch(const std::exception &e)
			{
				Log("Store::http", "Unable to query the file list");
				page.text("Unable to retrieve the content...");
			}
			
			page.footer();
			
		}
	}
	catch(Exception &e)
	{
		Log("AddressBook::http",e.what());
		throw 404;	// Httpd handles integer exceptions
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
	catch(const std::exception &e)
	{
		Log("AddressBook::publish", e.what()); 
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
	
		String line;
		while(output.readLine(line))
		{
			line.trim();
			if(line.empty()) continue;
			Address a;
			line >> a;
			if(!addrs.contains(a))
				addrs.push_back(a);
		}
	}
	catch(const std::exception &e)
	{
		Log("AddressBook::query", e.what()); 
		return false;
	}
	return true;
}

String AddressBook::fileName(void) const
{
	return Config::Get("profiles_dir")+Directory::Separator+mName+Directory::Separator+"contacts";
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
