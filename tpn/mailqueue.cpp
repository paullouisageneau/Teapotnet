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

#include "tpn/mailqueue.h"
#include "tpn/user.h"
#include "tpn/addressbook.h"
#include "tpn/notification.h"
#include "tpn/html.h"

#include "pla/yamlserializer.h"
#include "pla/jsonserializer.h"
#include "pla/crypto.h"

namespace tpn
{

MailQueue::MailQueue(User *user) :
	mUser(user),
	mHasNew(false)
{
	if(mUser) mDatabase = new Database(mUser->profilePath() + "mails.db");
	else mDatabase = new Database("mails.db");
		
	// WARNING: Additionnal fields in mails should be declared in erase()
	mDatabase->execute("CREATE TABLE IF NOT EXISTS mails\
		(id INTEGER PRIMARY KEY AUTOINCREMENT,\
		stamp TEXT UNIQUE NOT NULL,\
		parent TEXT,\
		headers TEXT,\
		content TEXT,\
		author TEXT,\
		signature TEXT,\
		contact TEXT NOT NULL,\
		time INTEGER(8) NOT NULL,\
		public INTEGER(1) NOT NULL,\
		incoming INTEGER(1) NOT NULL,\
		relayed INTEGER(1) NOT NULL)");

	// Warning: stamp is not unique in received
	mDatabase->execute("CREATE TABLE IF NOT EXISTS received\
		(stamp TEXT NOT NULL,\
		contact TEXT NOT NULL,\
		time INTEGER(8) DEFAULT 0 NOT NULL)");
	
	mDatabase->execute("CREATE UNIQUE INDEX IF NOT EXISTS contact_stamp ON received (contact,stamp)");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS flags\
		(stamp TEXT UNIQUE NOT NULL,\
		read INTEGER(1) DEFAULT 0 NOT NULL,\
		passed INTEGER(1) DEFAULT 0 NOT NULL,\
		deleted INTEGER(1) DEFAULT 0 NOT NULL,\
		time INTEGER(8) DEFAULT 0 NOT NULL)");
	
	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON flags (stamp)");
	
	// TODO: backward compatibility, should be removed (09/02/2014)
	// Populate flags table with read flags and remove isread column
	bool updateNeeded = false;
	Database::Statement statement = mDatabase->prepare("PRAGMA table_info(mails)");
	while(statement.step())
	{
		String columnName;
		statement.value(1, columnName);
		if(columnName == "isread")
		{
			updateNeeded = true;
			break;
		}
	}
	statement.finalize();
	
	if(updateNeeded)
	{
		LogInfo("MailQueue", "Updating mails table to new format...");
		
		try {
			mDatabase->execute("BEGIN TRANSACTION");
			
			mDatabase->execute("INSERT OR IGNORE INTO flags (stamp,read) SELECT stamp,isread FROM mails");
			
			mDatabase->execute("CREATE TEMPORARY TABLE mails_backup\
					(id INTEGER PRIMARY KEY AUTOINCREMENT,\
					stamp TEXT UNIQUE NOT NULL,\
					parent TEXT,\
					headers TEXT,\
					content TEXT,\
					author TEXT,\
					signature TEXT,\
					contact TEXT NOT NULL,\
					time INTEGER(8) NOT NULL,\
					public INTEGER(1) NOT NULL,\
					incoming INTEGER(1) NOT NULL,\
					relayed INTEGER(1) NOT NULL)");
			
			mDatabase->execute("INSERT INTO mails_backup SELECT id,stamp,parent,headers,content,author,signature,contact,time,public,incoming,relayed FROM mails");
			mDatabase->execute("DROP TABLE mails");
			
			mDatabase->execute("CREATE TABLE mails\
					(id INTEGER PRIMARY KEY AUTOINCREMENT,\
					stamp TEXT UNIQUE NOT NULL,\
					parent TEXT,\
					headers TEXT,\
					content TEXT,\
					author TEXT,\
					signature TEXT,\
					contact TEXT NOT NULL,\
					time INTEGER(8) NOT NULL,\
					public INTEGER(1) NOT NULL,\
					incoming INTEGER(1) NOT NULL,\
					relayed INTEGER(1) NOT NULL)");
					
			mDatabase->execute("INSERT INTO mails SELECT * FROM mails_backup");
			mDatabase->execute("DROP TABLE mails_backup");
			mDatabase->execute("COMMIT");
	
			LogInfo("MailQueue", "Finished updating mails table");
		}
		catch(const Exception &e)
		{
			throw Exception(String("Database update failed: ") + e.what());
		}
	}
	
	// Add deleted column to flags if it doesn't exist
	updateNeeded = true;
	statement = mDatabase->prepare("PRAGMA table_info(flags)");
	while(statement.step())
	{
		String columnName;
		statement.value(1, columnName);
		if(columnName == "deleted")
		{
			updateNeeded = false;
			break;
		}
	}
	statement.finalize();
	
	if(updateNeeded)
		mDatabase->execute("ALTER TABLE flags ADD COLUMN deleted INTEGER(1) DEFAULT 0 NOT NULL");
	
	// Rebuild clean received table
	updateNeeded = true;
	statement = mDatabase->prepare("PRAGMA table_info(received)");
	while(statement.step())
	{
		String columnName;
		statement.value(1, columnName);
		if(columnName == "time")
		{
			updateNeeded = false;
			break;
		}
	}
	statement.finalize();
	
	if(updateNeeded)
	{
		mDatabase->execute("DROP TABLE received");
		mDatabase->execute("CREATE TABLE received\
		(stamp TEXT NOT NULL,\
		contact TEXT NOT NULL,\
		time INTEGER(8) DEFAULT 0 NOT NULL)");
		mDatabase->execute("CREATE UNIQUE INDEX contact_stamp ON received (contact,stamp)");
	}
	
	// End of backward compatibility code
	
	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON mails (stamp)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS contact ON mails (contact)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS time ON mails (time)");
	
	Interface::Instance->add("/"+mUser->name()+"/mails", this);
}

MailQueue::~MailQueue(void)
{
	Interface::Instance->remove("/"+mUser->name()+"/mails", this);

	delete mDatabase;
}

User *MailQueue::user(void) const
{
	return mUser;
}

bool MailQueue::hasNew(void) const
{
	Synchronize(this);

	bool old = mHasNew;
	mHasNew = false;
	return old;
}

bool MailQueue::add(Mail &mail)
{
	Synchronize(this);
	
	bool deleted = false;
	if(mail.stamp().empty())
	{
		if(mail.isIncoming()) 
		{
			LogWarn("MailQueue::add", "Mail with empty stamp, dropping");
			return false;
		}
		
		mail.writeSignature(user());
	}
	else {
		if(!mail.checkStamp())
		{
			LogWarn("MailQueue::add", "Mail with invalid stamp");
			deleted = true;
		}
	}
	
	bool exist = false;
	Mail oldMail;
	Database::Statement statement = mDatabase->prepare("SELECT * FROM mails WHERE stamp=?1");
	statement.bind(1, mail.stamp());
	if(statement.step())
	{
		exist = true;
		statement.input(oldMail);
	}
	statement.finalize();
	
	LogDebug("MailQueue::add", "Adding mail '"+mail.stamp()+"'"+(exist ? " (already in queue)" : ""));

	if(exist && (mail.isRelayed() || !oldMail.isRelayed()))
	{
		if(mail.isIncoming())
		{
			// Allow resetting time to prevent synchronization problems
			Database::Statement statement = mDatabase->prepare("UPDATE mails SET time=?1 WHERE stamp=?2 and contact=?3");
			statement.bind(1, mail.time());
			statement.bind(2, mail.stamp());
			statement.bind(3, mail.contact());
			statement.execute();
		}
		
		return false;
	}
	
	mDatabase->insert("mails", mail);
	
	if(deleted) 
	{
		markDeleted(mail.stamp());
	}
	else if(!exist)
	{
		if(mail.isIncoming() && !mail.isPublic()) mHasNew = true;
		notifyAll();
		SyncYield(this);
		
		// Broadcast mails when parent is broadcasted or passed 
		if(!mail.parent().empty())
		{
			Mail parent;
			if(get(mail.parent(), parent)
				&& (parent.contact().empty() || isPassed(parent.stamp())))
			{
				// TODO: we shouldn't resend it to the source
				user()->addressBook()->send(mail);
			}
		}
		
		String attachment = mail.header("attachment");
		if(!attachment.empty())
		try {
			BinaryString target;
			target.fromString(attachment);
			//TODO: prefetch target;
		}
		catch(const Exception &e)
		{
			LogWarn("MailQueue::add", String("Attachment prefetching failed: ") + e.what());
		}
	}
	
	return true;
}

bool MailQueue::get(const String &stamp, Mail &result) const
{
	Synchronize(this);
 
	Database::Statement statement = mDatabase->prepare("SELECT * FROM mails WHERE stamp=?1 LIMIT 1");
        statement.bind(1, stamp);
        if(statement.step())
	{
		result.deserialize(statement);
        	statement.finalize(); 
		return true;
	}

	statement.finalize();
        return false;
}

bool MailQueue::getChilds(const String &stamp, List<Mail> &result) const
{
	Synchronize(this);

	result.clear();
	if(stamp.empty()) return false;

	Database::Statement statement = mDatabase->prepare("SELECT * FROM mails WHERE parent=?1 LIMIT 1");
	statement.bind(1, stamp);
	statement.fetch(result);                
        statement.finalize();
        return !result.empty();
}

void MailQueue::ack(const List<Mail> &mails)
{
	Map<String, StringList> stamps;
	for(List<Mail>::const_iterator it = mails.begin();
		it != mails.end();
		++it)
	{
		const Mail &mail = *it;
		if(mail.isIncoming() && !isRead(mail.stamp()))
		{
			stamps[mail.contact()].push_back(mail.stamp());
			markRead(mail.stamp());
			Assert(isRead(mail.stamp()));
		}
	}
	
	if(!stamps.empty())
	{
		LogDebug("MailQueue::ack", "Sending Acknowledgements");
		
		for(Map<String, StringList>::iterator it = stamps.begin();
			it != stamps.end();
			++it)
		{
			// TODO: ACKs are sent but ignored at reception for public mails

			String tmp;
			YamlSerializer serializer(&tmp);
			serializer.output(it->second);

			Notification notification(tmp);
			//notification.setParameter("type", "ack");	// TODO
			
			AddressBook::Contact *contact = mUser->addressBook()->getContact(it->first);
			if(contact)
				contact->send(notification);
			
			AddressBook::Contact *self = mUser->addressBook()->getSelf();
			if(self && self != contact) 
				self->send(notification);
		}
	}
}

void MailQueue::pass(const List<Mail> &mails)
{
	SerializableList<Mail> list;

	for(List<Mail>::const_iterator it = mails.begin();
		it != mails.end();
		++it)
	{
		if(isPassed(it->stamp())) continue;

		// Mark as passed
		markPassed(it->stamp());

		// Broadcast the mail
		// TODO: we shouldn't resend it to the source
		user()->addressBook()->send(*it);

		// Broadcast its childs
		List<Mail> childs;
		if(getChilds(it->stamp(), childs))
		{
			for(List<Mail>::const_iterator jt = childs.begin();
				jt != childs.end();
				++jt)
			{
				// TODO: we shouldn't resend it to the source
				user()->addressBook()->send(*jt);
			}
		}

		list.push_back(*it);
	}

	if(!list.empty())
	{
		// Send notification
		AddressBook::Contact *self = mUser->addressBook()->getSelf();
		if(self)
		{
			String tmp;
			YamlSerializer serializer(&tmp);
			serializer.output(list);
	
			Notification notification(tmp);
			//notification.setParameter("type", "pass");	// TODO
			self->send(notification);
		}
	}
}

void MailQueue::erase(const String &uname)
{
	Synchronize(this);

	// Additionnal fields in mails should be added here
	Database::Statement statement = mDatabase->prepare("INSERT OR REPLACE INTO mails \
(id, stamp, parent, headers, content, author, signature, contact, time, public, incoming, relayed) \
SELECT m.id, m.stamp, m.parent, m.headers, m.content, m.author, '', p.contact, m.time, m.public, m.incoming, 1 \
FROM mails AS m LEFT JOIN mails AS p ON p.stamp=NULLIF(m.parent,'') WHERE m.contact=?1 AND p.contact IS NOT NULL");
	statement.bind(1, uname);
	statement.execute();

        statement = mDatabase->prepare("DELETE FROM mails WHERE contact=?1");
        statement.bind(1, uname);
        statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM received WHERE contact=?1");
        statement.bind(1, uname);
        statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM flags WHERE NOT EXISTS(SELECT 1 FROM mails WHERE mails.stamp=flags.stamp)");
        statement.execute();
}

void MailQueue::markReceived(const String &stamp, const String &uname)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO received (stamp, contact, time) VALUES (?1,?2,?3)");
	statement.bind(1, stamp);
	statement.bind(2, uname);
	statement.bind(3, Time::Now());
	statement.execute();
}

void MailQueue::markRead(const String &stamp)
{
	setFlag(stamp, "read", true);
}

void MailQueue::markPassed(const String &stamp)
{
	setFlag(stamp, "passed", true);
}

void MailQueue::markDeleted(const String &stamp)
{
	setFlag(stamp, "deleted", true);
}

bool MailQueue::isRead(const String &stamp) const
{
	return getFlag(stamp, "read");
}

bool MailQueue::isPassed(const String &stamp) const
{
	return getFlag(stamp, "passed");
}

bool MailQueue::isDeleted(const String &stamp) const
{
	return getFlag(stamp, "deleted");
}

void MailQueue::setFlag(const String &stamp, const String &name, bool value)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("SELECT "+name+" FROM flags WHERE stamp=?1");
	statement.bind(1, stamp);
	if(statement.step())
	{
		bool currentValue = false;
		statement.input(currentValue);
		statement.finalize();
		if(value == currentValue) return;
	}
	else {
		statement.finalize();
		
		statement = mDatabase->prepare("INSERT OR IGNORE INTO flags (stamp) VALUES (?1)");
		statement.bind(1, stamp);
		statement.execute();
	}
	
	statement = mDatabase->prepare("UPDATE flags SET "+name+"=?2, time=?3 WHERE stamp=?1");
	statement.bind(1, stamp);
	statement.bind(2, value);
	statement.bind(3, Time::Now());
	statement.execute();
}

bool MailQueue::getFlag(const String &stamp, const String &name) const
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("SELECT "+name+" FROM flags WHERE stamp=?1");
	statement.bind(1, stamp);
	if(statement.step())
	{
		bool value = false;
		statement.input(value);
		statement.finalize();
		return value;
	}
	
	statement.finalize();
	return false;
}

void MailQueue::http(const String &prefix, Http::Request &request)
{
	String url = request.url;
	url.ignore();	// removes first '/'
	String uname = url;
	url = uname.cut('/');
	if(!url.empty()) throw 404;
	url = "/";
	if(!uname.empty()) url+= uname + "/";
	
	AddressBook::Contact *contact = NULL;
	if(!uname.empty()) 
	{
		contact = mUser->addressBook()->getContact(uname);
		if(!contact) throw 404;
	}
	
	AddressBook::Contact *self = mUser->addressBook()->getSelf();
	
	if(request.method == "POST")
        {
		if(!user()->checkToken(request.post["token"], "mail")) 
			throw 403;
		
		String action;
		if(request.post.get("action", action))
		{
			String stamp;
			if(!request.post.get("stamp", stamp))
				throw 400;	// Missing stamp
			
			Mail mail;
			if(!get(stamp, mail))
				throw 400;	// Unknown mail
			
			if(action == "pass")
			{
				List<Mail> list;
				list.push_back(mail);
				pass(list);
	
				Http::Response response(request, 200);
				response.send();
				return;
			}

			// Unknown action
			throw 400;
		}
		
                if(!request.post.contains("mail") || request.post["mail"].empty())
			throw 400;

		bool isPublic = request.post["public"].toBool();
		
		try {
			Mail mail(request.post["mail"]);
			mail.setIncoming(false);
			mail.setPublic(isPublic);
			if(!isPublic) mail.setContact(uname);
			
			String parent;
			request.post.get("parent", parent);
			if(!parent.empty()) mail.setParent(parent);
			
			String attachment;
			request.post.get("attachment", attachment);
			if(!attachment.empty()) mail.setHeader("attachment", attachment);
	
			add(mail);	// signs the mail
			
			if(contact)
			{
				contact->send(mail);
				if(self && self != contact)
					self->send(mail);
			}
			else {
				user()->addressBook()->send(mail);
			}
		}
		catch(const Exception &e)
		{
			LogWarn("AddressBook::Contact::http", String("Cannot post mail: ") + e.what());
			throw 409;
		}

		Http::Response response(request, 200);
		response.send();
		return;
        }
        
	if(request.get.contains("json"))
	{
		int64_t next = 0;
		if(request.get.contains("next")) request.get.get("next").extract(next);

		Http::Response response(request, 200);
		response.headers["Content-Type"] = "application/json";
		response.send();

		Selection selection;
		if(request.get["public"].toBool()) selection = selectPublic(uname);
		else  selection = selectPrivate(uname);
		if(request.get["incoming"].toBool()) selection.includeOutgoing(false);
		if(request.get.contains("parent")) selection.setParentStamp(request.get["parent"]);
		
		int count = 100; // TODO: 100 mails selected is max
		if(request.get.contains("count")) request.get.get("count").extract(count);
		
		SerializableList<Mail> list;
		while(!selection.getLast(next, count, list))
			if(!wait(60.)) return;

		ack(list);
		
		JsonSerializer serializer(response.stream);
		serializer.output(list);
		return;
	}
	
	if(!contact) throw 400;
	String name = contact->name();
	String status = "Dummy"; // TODO
	
	bool isPopup = request.get.contains("popup");

	Http::Response response(request, 200);
	response.send();

	Html page(response.stream);
	
	String title= "Chat with "+name;
	page.header(title, isPopup);

	page.open("div","topmenu");	
	if(isPopup) page.span(title, ".button");
	page.span(status.capitalized(), "status.button");
	page.raw("<a class=\"button\" href=\"#\" onclick=\"createFileSelector('/"+mUser->name()+"/myself/files/?json', '#fileSelector', 'input.attachment', 'input.attachmentname','"+mUser->generateToken("directory")+"'); return false;\">Send file</a>");

// TODO: should be hidden in CSS
#ifndef ANDROID
	if(!isPopup)
	{
		String popupUrl = prefix + url + "?popup=1";
		page.raw("<a class=\"button\" href=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','/');\">Popup</a>");
	}
#endif
	page.close("div");

	page.div("", "fileSelector");	

	if(isPopup) page.open("div", "chat");
	else page.open("div", "chat.box");

	page.open("div", "chatmails");
	page.close("div");

	page.open("div", "chatpanel");
	page.div("","attachedfile");
	page.openForm("#", "post", "chatform");
	page.textarea("chatinput");
	page.input("hidden", "attachment");
	page.input("hidden", "attachmentname");
	page.closeForm();
	page.close("div");

	page.close("div");
 
	page.javascript("function post() {\n\
			var mail = $(document.chatform.chatinput).val();\n\
			var attachment = $(document.chatform.attachment).val();\n\
			if(!mail) return false;\n\
			var fields = {};\n\
			fields['mail'] = mail;\n\
			fields['token'] = '"+user()->generateToken("mail")+"';\n\
			if(attachment) fields['attachment'] = attachment;\n\
			$.post('"+prefix+url+"', fields)\n\
			.fail(function(jqXHR, textStatus) {\n\
				alert('The mail could not be sent.');\n\
			});\n\
			$(document.chatform.chatinput).val('');\n\
			$(document.chatform.attachment).val('');\n\
			$(document.chatform.attachmentname).val('');\n\
			$('#attachedfile').hide();\n\
		}\n\
		$(document.chatform).submit(function() {\n\
			post();\n\
			return false;\n\
		});\n\
		$(document.chatform.attachment).change(function() {\n\
			$('#attachedfile').html('');\n\
			$('#attachedfile').hide();\n\
			var filename = $(document.chatform.attachmentname).val();\n\
			if(filename != '') {\n\
				$('#attachedfile').append('<img class=\"icon\" src=\"/file.png\">');\n\
				$('#attachedfile').append('<span class=\"filename\">'+filename+'</span>');\n\
				$('#attachedfile').show();\n\
			}\n\
			$(document.chatform.chatinput).focus();\n\
			if($(document.chatform.chatinput).val() == '') {\n\
				$(document.chatform.chatinput).val(filename);\n\
				$(document.chatform.chatinput).select();\n\
			}\n\
		});\n\
		$(document.chatform.chatinput).keypress(function(e) {\n\
			if (e.keyCode == 13 && !e.shiftKey) {\n\
				e.preventDefault();\n\
				post();\n\
			}\n\
		});\n\
		$('#attachedfile').hide();\n\
		setMailsReceiver('"+Http::AppendGet(request.fullUrl, "json")+"','#chatmails');");

	unsigned refreshPeriod = 5000;
	page.javascript("setCallback(\""+contact->urlPrefix()+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
		transition($('#status'), info.status.capitalize());\n\
		$('#status').removeClass().addClass('button').addClass(info.status);\n\
		if(info.newmails) playMailSound();\n\
	});");

	page.footer();
	return;
}

MailQueue::Selection MailQueue::select(const String &uname) const
{
	return Selection(this, uname, true, true, true);
}

MailQueue::Selection MailQueue::selectPrivate(const String &uname) const
{
	return Selection(this, uname, true, false, true);
}

MailQueue::Selection MailQueue::selectPublic(const String &uname, bool includeOutgoing) const
{
	return Selection(this, uname, false, true, includeOutgoing);
}

MailQueue::Selection MailQueue::selectChilds(const String &parentStamp) const
{
	return Selection(this, parentStamp);
}

MailQueue::Selection::Selection(void) :
	mMailQueue(NULL),
	mBaseTime(time_t(0)),
	mIncludePrivate(true),
	mIncludePublic(true),
	mIncludeOutgoing(true)
{
	
}

MailQueue::Selection::Selection(const MailQueue *mailQueue, const String &uname, 
				   bool includePrivate, bool includePublic, bool includeOutgoing) :
	mMailQueue(mailQueue),
	mContact(uname),
	mBaseTime(time_t(0)),
	mIncludePrivate(includePrivate),
	mIncludePublic(includePublic),
	mIncludeOutgoing(includeOutgoing)
{
	
}

MailQueue::Selection::Selection(const MailQueue *mailQueue, const String &parent) :
	mMailQueue(mailQueue),
	mBaseTime(time_t(0)),
	mIncludePrivate(true),
	mIncludePublic(true),
	mIncludeOutgoing(true),
	mParentStamp(parent)
{
	
}

MailQueue::Selection::~Selection(void)
{
	
}

void MailQueue::Selection::setParentStamp(const String &stamp)
{
	mParentStamp = stamp;
}

bool MailQueue::Selection::setBaseStamp(const String &stamp)
{
	if(!stamp.empty())
	{
		Mail base;
		if(mMailQueue->get(stamp, base))
		{
			mBaseStamp = base.stamp();
			mBaseTime = base.time();
			return true;
		}
	}
	
	mBaseStamp.clear();
	return false;
}

String MailQueue::Selection::baseStamp(void) const
{
	return mBaseStamp;
}

void MailQueue::Selection::includePrivate(bool enabled)
{
	mIncludePrivate = enabled;
}

void MailQueue::Selection::includePublic(bool enabled)
{
	mIncludePublic = enabled;
}

void MailQueue::Selection::includeOutgoing(bool enabled)
{
	mIncludeOutgoing = enabled;
}

int MailQueue::Selection::count(void) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	
	int count = 0;
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("COUNT(*) AS count")+" WHERE "+filter());
	filterBind(statement);
	if(statement.step())
		statement.input(count);
	statement.finalize();
	return count;
}

int MailQueue::Selection::unreadCount(void) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	
	int count = 0;
        Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("COUNT(*) AS count")+" WHERE "+filter()+" AND mail.incoming=1 AND IFNULL(flags.read,0)=0");
	filterBind(statement);
	if(statement.step())
		statement.input(count);
        statement.finalize();
        return count;
}

bool MailQueue::Selection::contains(const String &stamp) const
{
        Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("id")+" WHERE mail.stamp=@stamp AND "+filter());
	filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
	bool found = statement.step();
        statement.finalize();
        return found;
}

bool MailQueue::Selection::getOffset(int offset, Mail &result) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" ORDER BY mail.time,mail.stamp LIMIT @offset,1");
	filterBind(statement);
	statement.bind(statement.parameterIndex("offset"), offset);
	if(!statement.step())
	{
		statement.finalize();
		return false;
	}
	
        statement.input(result);
	statement.finalize();
        return true;
}

bool MailQueue::Selection::getRange(int offset, int count, List<Mail> &result) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	result.clear();
	
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" ORDER BY mail.time,mail.stamp LIMIT @offset,@count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("offset"), offset);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
        return (!result.empty());
}

bool MailQueue::Selection::getLast(int count, List<Mail> &result) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	result.clear();

	if(count == 0) return false;

	// Fetch the highest id
        int64_t maxId = 0;
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("MAX(mail.id) AS maxid")+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0");
        if(!statement.step())
        {
                statement.finalize();
                return false;
        }
        statement.input(maxId);
        statement.finalize();

	// Find the time of the last mail counting only mails without a parent
	Time lastTime = Time(0);
	statement = mMailQueue->mDatabase->prepare("SELECT "+target("time")+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND NULLIF(mail.parent,'') IS NULL ORDER BY mail.time DESC,mail.id DESC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
	while(statement.step())
		statement.input(lastTime);
	statement.finalize();

	// Fetch mails by time
	const String fields = "mail.*, mail.id AS number, flags.read, flags.passed";
	statement = mMailQueue->mDatabase->prepare("SELECT "+target(fields)+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND (mail.id=@maxid OR mail.time>=@lasttime OR (mail.incoming=1 AND IFNULL(flags.read,0)=0)) ORDER BY mail.time,mail.id");
	filterBind(statement);
	statement.bind(statement.parameterIndex("maxid"), maxId);
	statement.bind(statement.parameterIndex("lasttime"), lastTime);
        statement.fetch(result);
	statement.finalize();

	if(!result.empty())
	{
		if(mIncludePrivate) mMailQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MailQueue::Selection::getLast(int64_t nextNumber, int count, List<Mail> &result) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	result.clear();
	
	if(nextNumber <= 0) 
		return getLast(count, result);
	
	const String fields = "mail.*, mail.id AS number, flags.read, flags.passed";
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target(fields)+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND mail.id>=@id ORDER BY mail.id ASC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("id"), nextNumber);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
	if(!result.empty())
	{
		if(mIncludePrivate) mMailQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MailQueue::Selection::getUnread(List<Mail> &result) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	result.clear();
	
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" AND mail.incoming=1 AND IFNULL(flags.read,0)=0");
        filterBind(statement);
	statement.fetch(result);
	statement.finalize();
        return (!result.empty());
}

bool MailQueue::Selection::getUnreadStamps(StringList &result) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	result.clear();
	
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" AND mail.incoming=1 AND IFNULL(flags.read,0)=0");
        filterBind(statement);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

bool MailQueue::Selection::getPassedStamps(StringList &result, int count) const
{
	Assert(mMailQueue);
	Synchronize(mMailQueue);
	result.clear();
	
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" AND IFNULL(flags.passed,0)=1 ORDER by flags.time DESC LIMIT @count");
        filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

void MailQueue::Selection::markRead(const String &stamp)
{
        Assert(mMailQueue);
        Synchronize(mMailQueue);

	// Check the mail is in selection
	Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("id")+" WHERE "+filter()+" AND mail.stamp=@stamp");
        filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
	bool found = statement.step();
	statement.finalize();
	
	if(found)
	{
		statement = mMailQueue->mDatabase->prepare("INSERT OR IGNORE INTO flags (stamp) VALUES (?1)");
		statement.bind(1, stamp);
		statement.execute();
		
		statement = mMailQueue->mDatabase->prepare("UPDATE flags SET read=?2, time=?3 WHERE stamp=?1");
		statement.bind(1, stamp);
		statement.bind(2, true);
		statement.bind(3, Time::Now());
		statement.execute();
	}
}

int MailQueue::Selection::checksum(int offset, int count, BinaryString &result) const
{
	StringList stamps;

	{
		Assert(mMailQueue);
		Synchronize(mMailQueue);
		result.clear();
	
		Database::Statement statement = mMailQueue->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" ORDER BY mail.time,mail.stamp LIMIT @offset,@count");
        	filterBind(statement);
        	statement.bind(statement.parameterIndex("offset"), offset);
		statement.bind(statement.parameterIndex("count"), count);

		while(statement.step())
        	{
			 String stamp;
                	 statement.value(0, stamp);
                	 stamps.push_back(stamp);
		}

		statement.finalize();
	}

	Sha512 sha512;
	sha512.init();
	for(StringList::iterator it = stamps.begin(); it != stamps.end(); ++it)
	{
		if(it != stamps.begin()) sha512.process(",", 1);
		sha512.process(it->data(), it->size());
	}

	sha512.finalize(result);
	return stamps.size();
}

String MailQueue::Selection::target(const String &columns) const
{
	StringList columnsList;
	columns.trimmed().explode(columnsList, ',');
	for(StringList::iterator it = columnsList.begin(); it != columnsList.end(); ++it)
		if(!it->contains('.') && !it->contains(' ') && !it->contains('(') && !it->contains(')')) 
			*it = "mail." + *it;

	String target;
	target.implode(columnsList, ',');	

	target+= " FROM " + table();

        return target;
}

String MailQueue::Selection::table(void) const
{
	String joining = "mails AS mail LEFT JOIN flags ON flags.stamp=mail.stamp";
		
	if(!mContact.empty())
		joining+=" LEFT JOIN mails AS parent ON parent.stamp=NULLIF(mail.parent,'')";
	
	return joining;
}

String MailQueue::Selection::filter(void) const
{
        String condition;
        if(mContact.empty()) condition = "1=1";
	else {
		condition = "((mail.contact='' OR mail.contact=@contact)\
			OR (NOT mail.relayed AND NULLIF(mail.parent,'') IS NOT NULL AND (parent.contact='' OR parent.contact=@contact))\
			OR EXISTS(SELECT 1 FROM received WHERE received.contact=@contact AND received.stamp=mail.stamp)\
			OR (IFNULL(flags.passed,0)=1 OR EXISTS(SELECT 1 FROM flags WHERE stamp=NULLIF(mail.parent,'') AND IFNULL(flags.passed,0)=1)))";
	}

        if(!mBaseStamp.empty()) condition+= " AND (mail.time>@basetime OR (mail.time=@basetime AND mail.stamp>=@basestamp))";

        if(!mParentStamp.empty()) condition+= " AND mail.parent=@parentstamp";

        if( mIncludePrivate && !mIncludePublic) condition+= " AND mail.public=0";
        if(!mIncludePrivate &&  mIncludePublic) condition+= " AND mail.public=1";
        if(!mIncludeOutgoing) condition+= " AND mail.incoming=1";

        return condition;
}

void MailQueue::Selection::filterBind(Database::Statement &statement) const
{
	statement.bind(statement.parameterIndex("contact"), mContact);
	statement.bind(statement.parameterIndex("parentstamp"), mParentStamp);
	statement.bind(statement.parameterIndex("basestamp"), mBaseStamp);
	statement.bind(statement.parameterIndex("basetime"), mBaseTime);
}

}

