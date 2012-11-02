/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "addressbook.h"
#include "user.h"
#include "core.h"
#include "sha512.h"
#include "config.h"
#include "file.h"
#include "directory.h"
#include "html.h"
#include "yamlserializer.h"
#include "portmapping.h"

namespace tpot
{

AddressBook::AddressBook(User *user) :
	mUser(user)
{
	Assert(mUser != NULL);
	mFileName = mUser->profilePath() + "contacts";
	
	Interface::Instance->add("/"+mUser->name()+"/contacts", this);
	
	if(File::Exist(mFileName))
	{
		try {
			File file(mFileName, File::Read);
			load(file);
			file.close();
		}
		catch(const Exception &e)
		{
			Log("AddressBook", String("Loading failed: ") + e.what());
		}
	}
}

AddressBook::~AddressBook(void)
{
  	Interface::Instance->remove("/"+mUser->name()+"/contacts");
}

User *AddressBook::user(void) const
{
 	return mUser; 
}

String AddressBook::userName(void) const
{
 	return mUser->name(); 
}

int AddressBook::unreadMessagesCount(void) const
{
	int count = 0;
  
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		count+= contact->unreadMessagesCount();
	}
	
	return count;
}

const Identifier &AddressBook::addContact(String name, const ByteString &secret)
{
	Synchronize(this);

	String tracker = name.cut('@');
	if(tracker.empty()) tracker = Config::Get("tracker");
	
	String uname = name;
	unsigned i = 1;
	while(mContactsByUniqueName.contains(uname))
	{
		uname = name;
		uname << ++i;
	}
	
	Contact *contact = new Contact(this, uname, name, tracker, secret);
	if(mContacts.contains(contact->peering())) 
	{
		delete contact;
		throw Exception("The contact already exists");
	}
	
	mContacts.insert(contact->peering(), contact);
	mContactsByUniqueName.insert(contact->uniqueName(), contact);
	
	save();
	start();
	return contact->peering();
}

void AddressBook::removeContact(const Identifier &peering)
{
	Synchronize(this);
	
	Contact *contact;
	if(mContacts.get(peering, contact))
	{
		Core::Instance->unregisterPeering(peering);
		mContactsByUniqueName.erase(contact->uniqueName());
 		mContacts.erase(peering);
		delete contact;
		save();
		start();
	}
}

const AddressBook::Contact *AddressBook::getContact(const Identifier &peering)
{
	Contact *contact;
	if(mContacts.get(peering, contact)) return contact;
	else return NULL;
}

void AddressBook::load(Stream &stream)
{
	Synchronize(this);

	YamlSerializer serializer(&stream);
	Contact *contact = new Contact(this);
	while(serializer.input(*contact))
	{
		mContacts.insert(contact->peering(), contact);
		contact = new Contact(this);
	}
	delete contact;
	start();
}

void AddressBook::save(Stream &stream) const
{
	Synchronize(this);
  
	YamlSerializer serializer(&stream);
	for(Map<Identifier, Contact*>::const_iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		const Contact *contact = it->second;
		serializer.output(*contact);
	}	
}

void AddressBook::save(void) const
{
	Synchronize(this);
  
	SafeWriteFile file(mFileName);
	save(file);
	file.close();
}

void AddressBook::update(void)
{
	Synchronize(this);
	Log("AddressBook::update", "Updating " + String::number(unsigned(mContacts.size())) + " contacts");
	
	for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
		it != mContacts.end();
		++it)
	{
		Contact *contact = it->second;
		contact->update();
	}
		
	Log("AddressBook::update", "Finished");
	save();
}

void AddressBook::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);

	try {
		if(request.url.empty() || request.url == "/")
		{
			if(request.method == "POST")
			{
				try {
			  		String command = request.post["command"];
			  		if(command == "delete")
					{
				  		Identifier peering;
						request.post["argument"] >> peering;
						
						removeContact(peering);
					}
					else {
						String name, csecret;
						name = request.post["name"];
						csecret = request.post["secret"];
					  
						if(name.empty() || csecret.empty()) throw 400;
						
						ByteString secret;
						Sha512::Hash(csecret, secret, Sha512::CryptRounds);
						
						addContact(name, secret);
					}
				}
				catch(const Exception &e)
				{
					Log("AddressBook::http", String("Error: ") + e.what());
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
			
			if(!mContacts.empty())
			{
				page.open("div",".box");
				
				page.openForm(prefix+"/", "post", "executeForm");
				page.input("hidden", "command");
				page.input("hidden", "argument");
				page.closeForm();
				
				page.javascript("function deleteContact(name, identifier)\n\
				{\n\
					if(!confirm('Do you really want to delete '+name+' ?')) return false;\n\
					document.executeForm.command.value = 'delete';\n\
					document.executeForm.argument.value = identifier;\n\
					document.executeForm.submit();\n\
					return false;\n\
				}");
				
				page.open("table",".contacts");
				for(Map<Identifier, Contact*>::iterator it = mContacts.begin();
					it != mContacts.end();
					++it)
				{
					Contact *contact = it->second;
					String contactUrl = prefix + '/' + contact->uniqueName() + '/';
					
					page.open("tr");
					page.open("td");
					page.open("span",".contact");
					page.link(contactUrl, contact->name() + "@" + contact->tracker());
					page.close("span");
					page.close("td");
					page.open("td");
					page.text(" "+String::hexa(contact->peeringChecksum(),8));
					page.close("td");
					page.open("td");
					if(Core::Instance->hasPeer(contact->peering())) page.span("Connected", ".online");
					else page.span("Not connected", ".offline");
					page.close("td");
					page.open("td");
					int msgcount = contact->unreadMessagesCount();
					if(msgcount) page.span(String("[")+String::number(msgcount)+String(" new messages]"), ".important");
					page.close("td");
					page.open("td",".delete");
					page.openLink("javascript:deleteContact('"+contact->name()+"','"+contact->peering().toString()+"')");
					page.image("/delete.png", "Delete");
					page.closeLink();
					page.close("td");
					page.close("tr");
				}
				page.close("table");
				page.close("div");
			}
			
			page.openForm(prefix+"/","post");
			page.openFieldset("New contact");
			page.label("name","Name"); page.input("text","name"); page.br();
			page.label("secret","Secret"); page.input("text","secret"); page.br();
			page.label("add"); page.button("add","Add contact");
			page.closeFieldset();
			page.closeForm();
			
			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		Log("AddressBook::http",e.what());
		throw 500;	// Httpd handles integer exceptions
	}
	
	throw 404;
}

void AddressBook::run(void)
{
	update();
}

bool AddressBook::publish(const Identifier &remotePeering)
{
	try {
		String url("http://" + Config::Get("tracker") + "/tracker/" + remotePeering.toString());
		
		StringMap post;
		String externalAddress = Config::Get("external_address");
		if(!externalAddress.empty() && externalAddress != "auto")
		{
			if(externalAddress.contains(':'))
			{
				Address addr(externalAddress);
				post["host"] = addr.host();
				post["port"] = addr.service();
			}
			else {
				post["host"] = externalAddress;
				post["port"] = Config::Get("port");
			}
			
			if(Http::Post(url, post) != 200) return false;
		}
		else {
			List<Address> list;
			Core::Instance->getAddresses(list);
			
			bool success = false;
			for(List<Address>::const_iterator it = list.begin();
				it != list.end();
				++it)
			{
				const Address &addr = *it;
				if(addr.addrFamily() == AF_INET)
				{
					String host = PortMapping::Instance->getExternalHost();
					if(!host.empty()) 
					{
						post["host"] = host;
						uint16_t port;
						PortMapping::Instance->getTcp(addr.port(), port);
						post["port"] == String::number(unsigned(port));
						
						success|= (Http::Post(url, post) != 200);
					}
				}
				
				String host = addr.host();
				if(host != "127.0.0.1" && host != "::1")
				{
					post["host"] = host;
					post["port"] = addr.service();
					
					success|= (Http::Post(url, post) != 200);
				}
			}
			
			return success;
		}
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
	  	if(tracker.empty()) url = "http://" + Config::Get("tracker") + "/tracker/" + peering.toString();
		else url = "http://" + tracker + "/tracker/" + peering.toString();
  
		String output;
		if(Http::Get(url, &output) != 200) return false;
	
		String line;
		while(output.readLine(line))
		{
			line.trim();
			if(line.empty()) continue;
			Address a;
			try { line >> a; }
			catch(...) { continue; }
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

AddressBook::Contact::Contact(	AddressBook *addressBook, 
				const String &uname,
				const String &name,
			        const String &tracker,
			        const ByteString &secret) :
	mAddressBook(addressBook),
	mUniqueName(uname),
	mName(name),
	mTracker(tracker),
	mSecret(secret)
{	
	Assert(addressBook != NULL);
	Assert(!uname.empty());
	Assert(!name.empty());
	Assert(!tracker.empty());
	Assert(!secret.empty());
	
	// Compute peering
	String agregate;
	agregate.writeLine(mSecret);
	agregate.writeLine(mAddressBook->userName());
	agregate.writeLine(mName);
	Sha512::Hash(agregate, mPeering, Sha512::CryptRounds);
	
	// Compute Remote peering
	agregate.clear();
	agregate.writeLine(mSecret);
	agregate.writeLine(mName);
	agregate.writeLine(mAddressBook->userName());
	Sha512::Hash(agregate, mRemotePeering, Sha512::CryptRounds);
	
	Interface::Instance->add(urlPrefix(), this);
}

AddressBook::Contact::Contact(AddressBook *addressBook) :
  	mAddressBook(addressBook),
	mMessagesCount(0)
{
  
}

AddressBook::Contact::~Contact(void)
{
  	Interface::Instance->remove(urlPrefix());
}

const String &AddressBook::Contact::uniqueName(void) const
{
	return mUniqueName;
}

const String &AddressBook::Contact::name(void) const
{
	return mName;
}

const String &AddressBook::Contact::tracker(void) const
{
	return mTracker;
}

const Identifier &AddressBook::Contact::peering(void) const
{
	return mPeering;
}

const Identifier &AddressBook::Contact::remotePeering(void) const
{
	return mRemotePeering;
}

uint32_t AddressBook::Contact::peeringChecksum(void) const
{
	return mPeering.checksum32() + mRemotePeering.checksum32(); 
}

String AddressBook::Contact::urlPrefix(void) const
{
	if(mUniqueName.empty()) return "";
	return String("/")+mAddressBook->userName()+"/contacts/"+mUniqueName;
}

int AddressBook::Contact::unreadMessagesCount(void) const
{
	int count = 0;
	for(int i=mMessages.size()-1; i>=0; --i)
	{
		if(mMessages[i].isRead()) break;
		++count;
	}
	return count;
}

void AddressBook::Contact::update(void)
{
	Synchronize(this);

	if(!Core::Instance->hasPeer(mPeering))
	{
	  	//Log("AddressBook::Contact", "Looking for " + mUniqueName);
		Core::Instance->registerPeering(mPeering, mRemotePeering, mSecret, this);
		
		if(Core::Instance->hasRegisteredPeering(mRemotePeering))	// the user is local
		{
			Log("AddressBook::Contact", mUniqueName + " found locally");
		  
			Address addr("127.0.0.1", Config::Get("port"));
			try {
				Socket *sock = new Socket(addr);
				Core::Instance->addPeer(sock, mPeering);
			}
			catch(...)
			{
				Log("AddressBook::Contact", "WARNING: Unable to connect the local core");	 
			}
		}
		else {
			Log("AddressBook::Contact", "Querying tracker " + mTracker + " for " + mUniqueName);	
			if(AddressBook::query(mPeering, mTracker, mAddrs))
			{
				for(int i=0; i<mAddrs.size(); ++i)
				{
					const Address &addr = mAddrs[mAddrs.size()-(i+1)];
					unlock();
					try {
						Socket *sock = new Socket(addr, 1000);	// TODO: timeout
						Core::Instance->addPeer(sock, mPeering);
					}
					catch(...)
					{
						
					}
					lock();
				}
			}
			
			if(!Core::Instance->hasPeer(mPeering))	// a new local peer could have established a connection in parallel
			{
				Log("AddressBook::Contact", "Publishing to tracker " + mTracker + " for " + mUniqueName);
				AddressBook::publish(mRemotePeering);
			}
		}
	}
	
	if(!mMessages.empty())
	{
		time_t t = time(NULL);
		while(!mMessages.front().isRead() 
			&& mMessages.front().time() >= t + 7200)	// 2h
		{
				 mMessages.pop_front();
		}
	} 
}

void AddressBook::Contact::message(Message *message)
{
	Synchronize(this);
	
	Assert(message);
	Assert(message->receiver() == mPeering);
	mMessages.push_back(*message);
	++mMessagesCount;
	notifyAll();
}

void AddressBook::Contact::request(Request *request)
{
	Assert(request);
	Store *store = mAddressBook->user()->store();
	request->execute(store);
}

void AddressBook::Contact::http(const String &prefix, Http::Request &request)
{
  	Synchronize(this);
	
	String base(prefix+request.url);
	base = base.substr(base.lastIndexOf('/')+1);
	if(!base.empty()) base+= '/';
	
	try {
		if(request.url.empty() || request.url == "/")
		{
			Http::Response response(request,200);
			response.send();
				
			Html page(response.sock);
			page.header("Contact: "+mName);
			page.open("h1");
			page.text("Contact: "+mName);
			page.close("h1");

			page.span("Status:",".title"); page.space();
			if(Core::Instance->hasPeer(mPeering)) page.span("Connected",".online");
			else page.span("Not connected",".offline");
			page.br();
			page.br();
			
			page.link(prefix+"/files/","Files");
			page.br();
			page.link(prefix+"/search/","Search");
			page.br();
			page.link(prefix+"/chat/","Chat");
			int msgcount = unreadMessagesCount();
			if(msgcount) 
			{
				page.space();
				page.span(String("[")+String::number(msgcount)+String(" new messages]"),".important");
			}
			page.br();
			page.br();
			
			page.open("div", "info");
			page.span("Secret:",".title"); page.space(); page.span(mSecret.toString(),".value"); page.br();
			page.span("Local peering:",".title"); page.space(); page.span(mPeering.toString(),".value"); page.br();
			page.span("Remote peering:",".title"); page.space(); page.span(mRemotePeering.toString(),".value"); page.br();
			page.close("div");
			page.open("div", "show_info");
			page.close("div");
			
			page.raw("<script type=\"text/javascript\">\n\
	document.getElementById('info').style.display = 'none';\n\
	document.getElementById('show_info').innerHTML = '<a id=\"show_info\" href=\"javascript:showInfo();\">Show information</a>';\n\
	function showInfo() { document.getElementById('info').style.display=''; document.getElementById('show_info').style.display='none'; }\n\
</script>\n");
			
			page.footer();
			return;
		}
		else {
			String url = request.url;
			String directory = url;
			directory.ignore();		// remove first '/'
			url = "/" + directory.cut('/');
			if(directory.empty()) throw 404;
			  
			if(directory == "files")
			{
				String target(url);
				if(target.size() > 1 && target[target.size()-1] == '/') 
					target = target.substr(0, target.size()-1);
				
				Request trequest(target, true);
				try {
					trequest.submit(mPeering);
					trequest.wait();
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", "Cannot send request, peer not connected");
					
					Http::Response response(request,200);
                                        response.send();

                                        Html page(response.sock);
                                        page.header(mName+": Files");
                                        page.open("h1");
					page.text(mName+": Files");
                                	page.close("h1");
					page.text("Not connected...");
					page.footer();
					return;
				}
				
				if(trequest.responsesCount() == 0)
					throw Exception("No response from peer");
				
				Request::Response *tresponse = trequest.response(0);
				StringMap parameters = tresponse->parameters();
				if(tresponse->error())
				{
					if(tresponse->status() == Request::Response::NotFound) throw 404;
					else throw Exception(String("Response status code ")+String::number(tresponse->status()));
				}

				Assert(tresponse->content());
					
				try {
					if(parameters.contains("type") && parameters["type"] == "directory")
					{
						Http::Response response(request,200);
						response.send();	
				
						Html page(response.sock);
						page.header(mName+": Files");
						page.open("h1");
						page.text(mName+": Files");
						page.close("h1");
						page.link(prefix+"/search/","Search files");
						
						YamlSerializer serializer(tresponse->content());
							
						Map<String, StringMap> files;
						StringMap map;
						while(serializer.input(map))
						{
							// Check info
							if(!map.contains("type")) continue;
							if(!map.contains("name")) continue;
							if(map.get("type") != "directory" && !map.contains("hash")) continue;
							
							// Sort
							if(map.get("type") == "directory") files.insert("0"+map.get("name"),map);
							else files.insert("1"+map.get("name"),map);
						}
						
						page.open("table",".files");
						for(Map<String, StringMap>::iterator it = files.begin();
							it != files.end();
							++it)
						{
							StringMap &map = it->second;
							
							page.open("tr");
							page.open("td"); 
							if(map.get("type") == "directory") page.link(base + map.get("name"), map.get("name"));
							else page.link("/" + map.get("hash"), map.get("name"));
							page.close("td");
							page.open("td"); 
							if(map.get("type") == "directory") page.text("directory");
							else if(map.contains("size")) page.text(String::hrSize(map.get("size")));
							page.close("td");
							page.close("tr");
						}

						page.close("table");
						page.footer();
						return;
					}
					else {
						Http::Response response(request, 200);
						response.headers["Content-Type"] = "application/octet-stream";
						response.headers["Content-Disposition"] = "attachment";
						response.headers["Content-Length"] = parameters.get("size");
						response.headers["Content-Disposition"]+= "; filename=\"" + parameters.get("name") + "\"";
						response.headers["Content-SHA512"] = parameters.get("hash");
						
						// TODO: Date + Last-Modified
						response.send();
						response.sock->write(*tresponse->content());
						return;
					}
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", String("Unable to access remote file or directory: ") + e.what());
					throw;
				}
			}
			else if(directory == "search")
			{
				if(url != "/") throw 404;
				
				String query;
				if(request.post.contains("query"))
				{
					query = request.post.get("query");
					query.trim();
				}
				
				Http::Response response(request,200);
				response.send();
				
				Html page(response.sock);
				page.header(mName+": Search");
				page.open("h1");
				page.text(mName+": Search");
				page.close("h1");
				
				page.openForm(prefix + "/search", "post", "searchform");
				page.input("text","query",query);
				page.button("search","Search");
				page.closeForm();
				page.br();
				
				if(query.empty())
				{
					page.footer();
					return;
				}
				
				Request trequest("search:"+query, false);	// no data
				try {
					trequest.submit(mPeering);
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", "Cannot send request, peer not connected");
					page.text("Not connected...");
					page.footer();
					return;
				}
				
				const unsigned timeout = Config::Get("request_timeout").toInt();
				
				{
					Desynchronize(this);
					trequest.lock();
					trequest.wait(timeout);
				}
				
				try {
					page.open("table",".files");
					for(int i=0; i<trequest.responsesCount(); ++i)
					{
						Request::Response *tresponse = trequest.response(i);
						if(tresponse->error()) continue;
					
						// Check info
						StringMap map = tresponse->parameters();
						if(!map.contains("type")) continue;
						if(!map.contains("path")) continue;
						if(map.get("type") != "directory" && !map.contains("hash")) continue;
						if(!map.contains("name")) map["name"] = map["path"].afterLast('/');
						
						page.open("tr");
						page.open("td"); 
						if(map.get("type") == "directory") page.link(prefix + "files" + map.get("path"), map.get("name"));
						else page.link("/" + map.get("hash"), map.get("name"));
						page.close("td");
						page.open("td"); 
						if(map.get("type") == "directory") page.text("directory");
						else if(map.contains("size")) page.text(String::hrSize(map.get("size")));
						page.close("td");
						page.close("tr");
					}
					page.close("table");
				}
				catch(const Exception &e)
				{
					Log("AddressBook::Contact::http", String("Unable to list files: ") + e.what());
					page.close("table");
					page.text("Error, unable to list files");
				}
				
				trequest.unlock();
				page.footer();
				return;
			}
			else if(directory == "chat")
			{
				if(url != "/")
				{
				  	url.ignore();
					unsigned count = 0;
					try { url>>count; }
					catch(...) { throw 404; }
					
					Http::Response response(request,200);
					response.send();
					
					if(count == mMessagesCount)
						wait(120000);
					
					if(count < mMessagesCount && mMessagesCount-count <= mMessages.size())
					{
						Html html(response.sock);
						int i = mMessages.size() - (mMessagesCount-count);
						messageToHtml(html, mMessages[i], false);
						mMessages[i].markRead();
					}
					
					return;
				}
			  
				if(request.method == "POST")
				{
					if(request.post.contains("message") && !request.post["message"].empty())
					{
						try {
							Message message(request.post["message"]);
						  	message.send(mPeering);	// send
							
							mMessages.push_back(Message(request.post["message"]));	// thus receiver is null
							++mMessagesCount;
							notifyAll();
							
							if(request.post["ajax"].toBool())	//ajax
							{
								Http::Response response(request, 200);
								response.send();
								/*Html html(response.sock);
								messageToHtml(html, mMessages.back(), false);
								mMessages.back().markRead();*/
							}
							else {	// form submit
							 	Http::Response response(request, 303);
								response.headers["Location"] = prefix + "/chat";
								response.send();
							}
						}
						catch(...)
						{
							throw 409;
						}
						
						return;
					}
				}
			  
				Http::Response response(request,200);
				response.send();	
				
				Html page(response.sock);
				page.header("Chat with "+mName, request.get.contains("popup"));
				if(!request.get.contains("popup"))
				{
					page.open("h1");
					page.text("Chat with "+mName);
					page.close("h1");
				}
				else {
					page.open("b");
					page.text("Chat with "+mName);
					page.close("b");
					page.br();
				}
				
				page.openForm(prefix + "/chat", "post", "chatForm");
				page.input("text","message");
				page.button("send","Send");
				page.space();
				
				if(!request.get.contains("popup"))
				{
					String popupUrl = prefix + "/chat?popup=1";
					page.raw("<a href=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','"+prefix+"');\">Popup</a>");
				}
				
				page.br();
				page.br();
				page.closeForm();
	
				page.open("div", "chat");
				for(int i=mMessages.size()-1; i>=0; --i)
				{
	  				messageToHtml(page, mMessages[i], mMessages[i].isRead());
					mMessages[i].markRead();
				}
				page.close("div");
				
page.raw("<script type=\"text/javascript\">\n\
	var count = "+String::number(mMessagesCount)+";\n\
	var title = document.title;\n\
	var hasFocus = true;\n\
	var nbNewMessages = 0;\n\
	$(window).blur(function() {\n\
		hasFocus = false;\n\
		$('span.message').attr('class', 'oldmessage');\n\
	});\n\
	$(window).focus(function() {\n\
		hasFocus = true;\n\
		nbNewMessages = 0;\n\
		document.title = title;\n\
	});\n\
	function update()\n\
	{\n\
		var request = $.ajax({\n\
			url: '"+prefix+"/chat/'+count,\n\
			dataType: 'html',\n\
			timeout: 300000\n\
		});\n\
		request.done(function(html) {\n\
			if($.trim(html) != '')\n\
			{\n\
				$(\"#chat\").prepend(html);\n\
				if(!hasFocus)\n\
				{\n\
					nbNewMessages+= 1;\n\
					document.title = title+' ('+nbNewMessages+')';\n\
				}\n\
				count+= 1;\n\
			}\n\
			setTimeout('update()', 100);\n\
		});\n\
		request.fail(function(jqXHR, textStatus) {\n\
			setTimeout('update()', 10000);\n\
		});\n\
	}\n\
	function post()\n\
	{\n\
		var message = document.chatForm.message.value;\n\
		if(!message) return false;\n\
		document.chatForm.message.value = '';\n\
		var request = $.post('"+prefix+"/chat',\n\
			{ 'message': message, 'ajax': 1 });\n\
		request.fail(function(jqXHR, textStatus) {\n\
			alert('The message could not be sent. Is this user online ?');\n\
		});\n\
	}\n\
	setTimeout('update()', 1000);\n\
	document.chatForm.onsubmit = function() {post(); return false;}\n\
</script>\n");
				
				page.footer();
				return;
			}
		}
	}
	catch(const NetException &e)
	{
		throw;
	}
	catch(const Exception &e)
	{
		Log("AddressBook::Contact::http", e.what());
		throw 500;
	}
	
	throw 404;
}

void AddressBook::Contact::messageToHtml(Html &html, const Message &message, bool old) const
{
	char buffer[64];
	time_t t = message.time();
	std::strftime (buffer, 64, "%x %X", localtime(&t));
	if(old) html.open("span",".oldmessage");
	else html.open("span",".message");
	html.open("span",".date");
	html.text(buffer);
	html.close("span");
	html.text(" ");
	html.open("span",".user");
	if(message.receiver() == Identifier::Null) html.text(mAddressBook->userName());
	else html.text(mAddressBook->getContact(message.receiver())->name());
	html.close("span");
	html.text(": " + message.content());
	html.close("span");
	html.br(); 
}

void AddressBook::Contact::serialize(Serializer &s) const
{
	Synchronize(this);
	
	StringMap map;
	map["uname"] << mUniqueName;
	map["name"] << mName;
	map["tracker"] << mTracker;
	map["secret"] << mSecret;
	map["peering"] << mPeering;
	map["remote"] << mRemotePeering;
	
	s.outputMapBegin(2);
	s.outputMapElement(String("info"),map);
	s.outputMapElement(String("addr"),mAddrs);
	s.outputMapEnd();
}

bool AddressBook::Contact::deserialize(Serializer &s)
{
	Synchronize(this);
	
	if(!mUniqueName.empty())
		Interface::Instance->remove(urlPrefix());
	
	mUniqueName.clear();
  	mName.clear();
	mTracker.clear();
	mSecret.clear();
	mPeering.clear();
	mRemotePeering.clear();
	
	StringMap map;
	
	String key;
	AssertIO(s.inputMapBegin());
	AssertIO(s.inputMapElement(key,map) && key == "info");
	AssertIO(s.inputMapElement(key,mAddrs) && key == "addr");
	
	map["uname"] >> mUniqueName;
	map["name"] >> mName;
	map["tracker"] >> mTracker;
	map["secret"] >> mSecret;
	map["peering"] >> mPeering;
	map["remote"] >> mRemotePeering;
	
	// TODO: checks
	
	Interface::Instance->add(urlPrefix(), this);
	
	return true;
}

bool AddressBook::Contact::isInlineSerializable(void) const
{
	return false; 
}

}
