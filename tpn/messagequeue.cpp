/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/messagequeue.h"
#include "tpn/user.h"
#include "tpn/html.h"
#include "tpn/sha512.h"
#include "tpn/yamlserializer.h"
#include "tpn/jsonserializer.h"
#include "tpn/notification.h"
#include "tpn/splicer.h"

namespace tpn
{

MessageQueue::MessageQueue(User *user) :
	mUser(user),
	mHasNew(false)
{
	if(mUser) mDatabase = new Database(mUser->profilePath() + "messages.db");
	else mDatabase = new Database("messages.db");
		
	// WARNING: Additionnal fields in messages should be declared in erase()
	mDatabase->execute("CREATE TABLE IF NOT EXISTS messages\
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
	Database::Statement statement = mDatabase->prepare("PRAGMA table_info(messages)");
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
		LogInfo("MessageQueue", "Updating messages table to new format...");
		
		try {
			mDatabase->execute("BEGIN TRANSACTION");
			
			mDatabase->execute("INSERT OR IGNORE INTO flags (stamp,read) SELECT stamp,isread FROM messages");
			
			mDatabase->execute("CREATE TEMPORARY TABLE messages_backup\
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
			
			mDatabase->execute("INSERT INTO messages_backup SELECT id,stamp,parent,headers,content,author,signature,contact,time,public,incoming,relayed FROM messages");
			mDatabase->execute("DROP TABLE messages");
			
			mDatabase->execute("CREATE TABLE messages\
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
					
			mDatabase->execute("INSERT INTO messages SELECT * FROM messages_backup");
			mDatabase->execute("DROP TABLE messages_backup");
			mDatabase->execute("COMMIT");
	
			LogInfo("MessageQueue", "Finished updating messages table");
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
	
	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON messages (stamp)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS contact ON messages (contact)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS time ON messages (time)");
	
	Interface::Instance->add("/"+mUser->name()+"/messages", this);
}

MessageQueue::~MessageQueue(void)
{
	Interface::Instance->remove("/"+mUser->name()+"/messages", this);

	delete mDatabase;
}

User *MessageQueue::user(void) const
{
	return mUser;
}

bool MessageQueue::hasNew(void) const
{
	Synchronize(this);

	bool old = mHasNew;
	mHasNew = false;
	return old;
}

bool MessageQueue::add(Message &message)
{
	Synchronize(this);
	
	bool deleted = false;
	if(message.stamp().empty())
	{
		if(message.isIncoming()) 
		{
			LogWarn("MessageQueue::add", "Message with empty stamp, dropping");
			return false;
		}
		
		message.writeSignature(user());
	}
	else {
		if(!message.checkStamp())
		{
			LogWarn("MessageQueue::add", "Message with invalid stamp");
			deleted = true;
		}
	}
	
	bool exist = false;
	Message oldMessage;
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE stamp=?1");
	statement.bind(1, message.stamp());
	if(statement.step())
	{
		exist = true;
		statement.input(oldMessage);
	}
	statement.finalize();
	
	LogDebug("MessageQueue::add", "Adding message '"+message.stamp()+"'"+(exist ? " (already in queue)" : ""));

	if(exist && (message.isRelayed() || !oldMessage.isRelayed()))
	{
		if(message.isIncoming())
		{
			// Allow resetting time to prevent synchronization problems
			Database::Statement statement = mDatabase->prepare("UPDATE messages SET time=?1 WHERE stamp=?2 and contact=?3");
			statement.bind(1, message.time());
			statement.bind(2, message.stamp());
			statement.bind(3, message.contact());
			statement.execute();
		}
		
		return false;
	}
	
	mDatabase->insert("messages", message);
	
	if(deleted) 
	{
		markDeleted(message.stamp());
	}
	else if(!exist)
	{
		if(message.isIncoming() && !message.isPublic()) mHasNew = true;
		notifyAll();
		SyncYield(this);
		
		// Broadcast messages when parent is broadcasted or passed 
		if(!message.parent().empty())
		{
			Message parent;
			if(get(message.parent(), parent)
				&& (parent.contact().empty() || isPassed(parent.stamp())))
			{
				// TODO: we shouldn't resend it to the source
				user()->addressBook()->send(message);
			}
		}
		
		String attachment = message.header("attachment");
		if(!attachment.empty())
		try {
			BinaryString target;
			target.fromString(attachment);
			Splicer::Prefetch(target);
		}
		catch(const Exception &e)
		{
			LogWarn("MessageQueue::add", String("Attachment prefetching failed: ") + e.what());
		}
	}
	
	return true;
}

bool MessageQueue::get(const String &stamp, Message &result) const
{
	Synchronize(this);
 
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE stamp=?1 LIMIT 1");
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

bool MessageQueue::getChilds(const String &stamp, List<Message> &result) const
{
	Synchronize(this);

	result.clear();
	if(stamp.empty()) return false;

	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE parent=?1 LIMIT 1");
	statement.bind(1, stamp);
	statement.fetch(result);                
        statement.finalize();
        return !result.empty();
}

void MessageQueue::ack(const List<Message> &messages)
{
	Map<String, StringList> stamps;
	for(List<Message>::const_iterator it = messages.begin();
		it != messages.end();
		++it)
	{
		const Message &message = *it;
		if(message.isIncoming() && !isRead(message.stamp()))
		{
			stamps[message.contact()].push_back(message.stamp());
			markRead(message.stamp());
			Assert(isRead(message.stamp()));
		}
	}
	
	if(!stamps.empty())
	{
		LogDebug("MessageQueue::ack", "Sending Acknowledgements");
		
		for(Map<String, StringList>::iterator it = stamps.begin();
			it != stamps.end();
			++it)
		{
			// TODO: ACKs are sent but ignored at reception for public messages

			String tmp;
			YamlSerializer serializer(&tmp);
			serializer.output(it->second);

			Notification notification(tmp);
			notification.setParameter("type", "ack");
			
			AddressBook::Contact *contact = mUser->addressBook()->getContactByUniqueName(it->first);
			if(contact)
				contact->send(notification);
			
			AddressBook::Contact *self = mUser->addressBook()->getSelf();
			if(self && self != contact) 
				self->send(notification);
		}
	}
}

void MessageQueue::pass(const List<Message> &messages)
{
	SerializableList<Message> list;

	for(List<Message>::const_iterator it = messages.begin();
		it != messages.end();
		++it)
	{
		if(isPassed(it->stamp())) continue;

		// Mark as passed
		markPassed(it->stamp());

		// Broadcast the message
		// TODO: we shouldn't resend it to the source
		user()->addressBook()->send(*it);

		// Broadcast its childs
		List<Message> childs;
		if(getChilds(it->stamp(), childs))
		{
			for(List<Message>::const_iterator jt = childs.begin();
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
			notification.setParameter("type", "pass");
			self->send(notification);
		}
	}
}

void MessageQueue::erase(const String &uname)
{
	Synchronize(this);

	// Additionnal fields in messages should be added here
	Database::Statement statement = mDatabase->prepare("INSERT OR REPLACE INTO messages \
(id, stamp, parent, headers, content, author, signature, contact, time, public, incoming, relayed) \
SELECT m.id, m.stamp, m.parent, m.headers, m.content, m.author, '', p.contact, m.time, m.public, m.incoming, 1 \
FROM messages AS m LEFT JOIN messages AS p ON p.stamp=NULLIF(m.parent,'') WHERE m.contact=?1 AND p.contact IS NOT NULL");
	statement.bind(1, uname);
	statement.execute();

        statement = mDatabase->prepare("DELETE FROM messages WHERE contact=?1");
        statement.bind(1, uname);
        statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM received WHERE contact=?1");
        statement.bind(1, uname);
        statement.execute();
	
	statement = mDatabase->prepare("DELETE FROM flags WHERE NOT EXISTS(SELECT 1 FROM messages WHERE messages.stamp=flags.stamp)");
        statement.execute();
}

void MessageQueue::markReceived(const String &stamp, const String &uname)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO received (stamp, contact, time) VALUES (?1,?2,?3)");
	statement.bind(1, stamp);
	statement.bind(2, uname);
	statement.bind(3, Time::Now());
	statement.execute();
}

void MessageQueue::markRead(const String &stamp)
{
	setFlag(stamp, "read", true);
}

void MessageQueue::markPassed(const String &stamp)
{
	setFlag(stamp, "passed", true);
}

void MessageQueue::markDeleted(const String &stamp)
{
	setFlag(stamp, "deleted", true);
}

bool MessageQueue::isRead(const String &stamp) const
{
	return getFlag(stamp, "read");
}

bool MessageQueue::isPassed(const String &stamp) const
{
	return getFlag(stamp, "passed");
}

bool MessageQueue::isDeleted(const String &stamp) const
{
	return getFlag(stamp, "deleted");
}

void MessageQueue::setFlag(const String &stamp, const String &name, bool value)
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

bool MessageQueue::getFlag(const String &stamp, const String &name) const
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

void MessageQueue::http(const String &prefix, Http::Request &request)
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
		contact = mUser->addressBook()->getContactByUniqueName(uname);
		if(!contact) throw 404;
	}
	
	AddressBook::Contact *self = mUser->addressBook()->getSelf();
	
	if(request.method == "POST")
        {
		if(!user()->checkToken(request.post["token"], "message")) 
			throw 403;
		
		String action;
		if(request.post.get("action", action))
		{
			String stamp;
			if(!request.post.get("stamp", stamp))
				throw 400;	// Missing stamp
			
			Message message;
			if(!get(stamp, message))
				throw 400;	// Unknown message
			
			if(action == "pass")
			{
				List<Message> list;
				list.push_back(message);
				pass(list);
	
				Http::Response response(request, 200);
				response.send();
				return;
			}

			// Unknown action
			throw 400;
		}
		
                if(!request.post.contains("message") || request.post["message"].empty())
			throw 400;

		bool isPublic = request.post["public"].toBool();
		
		try {
			Message message(request.post["message"]);
			message.setIncoming(false);
			message.setPublic(isPublic);
			if(!isPublic) message.setContact(uname);
			
			String parent;
			request.post.get("parent", parent);
			if(!parent.empty()) message.setParent(parent);
			
			String attachment;
			request.post.get("attachment", attachment);
			if(!attachment.empty()) message.setHeader("attachment", attachment);
	
			add(message);	// signs the message
			
			if(contact)
			{
				contact->send(message);
				if(self && self != contact)
					self->send(message);
			}
			else {
				user()->addressBook()->send(message);
			}
		}
		catch(const Exception &e)
		{
			LogWarn("AddressBook::Contact::http", String("Cannot post message: ") + e.what());
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
		
		int count = 100; // TODO: 100 messages selected is max
		if(request.get.contains("count")) request.get.get("count").extract(count);
		
		SerializableList<Message> list;
		while(!selection.getLast(next, count, list))
			if(!wait(60.)) return;

		ack(list);
		
		JsonSerializer serializer(response.sock);
		serializer.output(list);
		return;
	}
	
	if(!contact) throw 400;
	String name = contact->name();
	String status = contact->status();
	
	bool isPopup = request.get.contains("popup");

	Http::Response response(request, 200);
	response.send();

	Html page(response.sock);
	
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

	page.open("div", "chatmessages");
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
			var message = $(document.chatform.chatinput).val();\n\
			var attachment = $(document.chatform.attachment).val();\n\
			if(!message) return false;\n\
			var fields = {};\n\
			fields['message'] = message;\n\
			fields['token'] = '"+user()->generateToken("message")+"';\n\
			if(attachment) fields['attachment'] = attachment;\n\
			$.post('"+prefix+url+"', fields)\n\
			.fail(function(jqXHR, textStatus) {\n\
				alert('The message could not be sent.');\n\
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
		setMessagesReceiver('"+Http::AppendGet(request.fullUrl, "json")+"','#chatmessages');");

	unsigned refreshPeriod = 5000;
	page.javascript("setCallback(\""+contact->urlPrefix()+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
		transition($('#status'), info.status.capitalize());\n\
		$('#status').removeClass().addClass('button').addClass(info.status);\n\
		if(info.newmessages) playMessageSound();\n\
	});");

	page.footer();
	return;
}

MessageQueue::Selection MessageQueue::select(const String &uname) const
{
	return Selection(this, uname, true, true, true);
}

MessageQueue::Selection MessageQueue::selectPrivate(const String &uname) const
{
	return Selection(this, uname, true, false, true);
}

MessageQueue::Selection MessageQueue::selectPublic(const String &uname, bool includeOutgoing) const
{
	return Selection(this, uname, false, true, includeOutgoing);
}

MessageQueue::Selection MessageQueue::selectChilds(const String &parentStamp) const
{
	return Selection(this, parentStamp);
}

MessageQueue::Selection::Selection(void) :
	mMessageQueue(NULL),
	mBaseTime(time_t(0)),
	mIncludePrivate(true),
	mIncludePublic(true),
	mIncludeOutgoing(true)
{
	
}

MessageQueue::Selection::Selection(const MessageQueue *messageQueue, const String &uname, 
				   bool includePrivate, bool includePublic, bool includeOutgoing) :
	mMessageQueue(messageQueue),
	mContact(uname),
	mBaseTime(time_t(0)),
	mIncludePrivate(includePrivate),
	mIncludePublic(includePublic),
	mIncludeOutgoing(includeOutgoing)
{
	
}

MessageQueue::Selection::Selection(const MessageQueue *messageQueue, const String &parent) :
	mMessageQueue(messageQueue),
	mBaseTime(time_t(0)),
	mIncludePrivate(true),
	mIncludePublic(true),
	mIncludeOutgoing(true),
	mParentStamp(parent)
{
	
}

MessageQueue::Selection::~Selection(void)
{
	
}

void MessageQueue::Selection::setParentStamp(const String &stamp)
{
	mParentStamp = stamp;
}

bool MessageQueue::Selection::setBaseStamp(const String &stamp)
{
	if(!stamp.empty())
	{
		Message base;
		if(mMessageQueue->get(stamp, base))
		{
			mBaseStamp = base.stamp();
			mBaseTime = base.time();
			return true;
		}
	}
	
	mBaseStamp.clear();
	return false;
}

String MessageQueue::Selection::baseStamp(void) const
{
	return mBaseStamp;
}

void MessageQueue::Selection::includePrivate(bool enabled)
{
	mIncludePrivate = enabled;
}

void MessageQueue::Selection::includePublic(bool enabled)
{
	mIncludePublic = enabled;
}

void MessageQueue::Selection::includeOutgoing(bool enabled)
{
	mIncludeOutgoing = enabled;
}

int MessageQueue::Selection::count(void) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	int count = 0;
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("COUNT(*) AS count")+" WHERE "+filter());
	filterBind(statement);
	if(statement.step())
		statement.input(count);
	statement.finalize();
	return count;
}

int MessageQueue::Selection::unreadCount(void) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	int count = 0;
        Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("COUNT(*) AS count")+" WHERE "+filter()+" AND message.incoming=1 AND IFNULL(flags.read,0)=0");
	filterBind(statement);
	if(statement.step())
		statement.input(count);
        statement.finalize();
        return count;
}

bool MessageQueue::Selection::contains(const String &stamp) const
{
        Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("id")+" WHERE message.stamp=@stamp AND "+filter());
	filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
	bool found = statement.step();
        statement.finalize();
        return found;
}

bool MessageQueue::Selection::getOffset(int offset, Message &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" ORDER BY message.time,message.stamp LIMIT @offset,1");
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

bool MessageQueue::Selection::getRange(int offset, int count, List<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" ORDER BY message.time,message.stamp LIMIT @offset,@count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("offset"), offset);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
        return (!result.empty());
}

bool MessageQueue::Selection::getLast(int count, List<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();

	if(count == 0) return false;

	// Fetch the highest id
        int64_t maxId = 0;
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("MAX(message.id) AS maxid")+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0");
        if(!statement.step())
        {
                statement.finalize();
                return false;
        }
        statement.input(maxId);
        statement.finalize();

	// Find the time of the last message counting only messages without a parent
	Time lastTime = Time(0);
	statement = mMessageQueue->mDatabase->prepare("SELECT "+target("time")+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND NULLIF(message.parent,'') IS NULL ORDER BY message.time DESC,message.id DESC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
	while(statement.step())
		statement.input(lastTime);
	statement.finalize();

	// Fetch messages by time
	const String fields = "message.*, message.id AS number, flags.read, flags.passed";
	statement = mMessageQueue->mDatabase->prepare("SELECT "+target(fields)+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND (message.id=@maxid OR message.time>=@lasttime OR (message.incoming=1 AND IFNULL(flags.read,0)=0)) ORDER BY message.time,message.id");
	filterBind(statement);
	statement.bind(statement.parameterIndex("maxid"), maxId);
	statement.bind(statement.parameterIndex("lasttime"), lastTime);
        statement.fetch(result);
	statement.finalize();

	if(!result.empty())
	{
		if(mIncludePrivate) mMessageQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MessageQueue::Selection::getLast(int64_t nextNumber, int count, List<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	if(nextNumber <= 0) 
		return getLast(count, result);
	
	const String fields = "message.*, message.id AS number, flags.read, flags.passed";
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target(fields)+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND message.id>=@id ORDER BY message.id ASC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("id"), nextNumber);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
	if(!result.empty())
	{
		if(mIncludePrivate) mMessageQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MessageQueue::Selection::getUnread(List<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" AND message.incoming=1 AND IFNULL(flags.read,0)=0");
        filterBind(statement);
	statement.fetch(result);
	statement.finalize();
        return (!result.empty());
}

bool MessageQueue::Selection::getUnreadStamps(StringList &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" AND message.incoming=1 AND IFNULL(flags.read,0)=0");
        filterBind(statement);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

bool MessageQueue::Selection::getPassedStamps(StringList &result, int count) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" AND IFNULL(flags.passed,0)=1 ORDER by flags.time DESC LIMIT @count");
        filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

void MessageQueue::Selection::markRead(const String &stamp)
{
        Assert(mMessageQueue);
        Synchronize(mMessageQueue);

	// Check the message is in selection
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("id")+" WHERE "+filter()+" AND message.stamp=@stamp");
        filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
	bool found = statement.step();
	statement.finalize();
	
	if(found)
	{
		statement = mMessageQueue->mDatabase->prepare("INSERT OR IGNORE INTO flags (stamp) VALUES (?1)");
		statement.bind(1, stamp);
		statement.execute();
		
		statement = mMessageQueue->mDatabase->prepare("UPDATE flags SET read=?2, time=?3 WHERE stamp=?1");
		statement.bind(1, stamp);
		statement.bind(2, true);
		statement.bind(3, Time::Now());
		statement.execute();
	}
}

int MessageQueue::Selection::checksum(int offset, int count, Stream &result) const
{
	StringList stamps;

	{
		Assert(mMessageQueue);
		Synchronize(mMessageQueue);
		result.clear();
	
		Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" ORDER BY message.time,message.stamp LIMIT @offset,@count");
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

String MessageQueue::Selection::target(const String &columns) const
{
	StringList columnsList;
	columns.trimmed().explode(columnsList, ',');
	for(StringList::iterator it = columnsList.begin(); it != columnsList.end(); ++it)
		if(!it->contains('.') && !it->contains(' ') && !it->contains('(') && !it->contains(')')) 
			*it = "message." + *it;

	String target;
	target.implode(columnsList, ',');	

	target+= " FROM " + table();

        return target;
}

String MessageQueue::Selection::table(void) const
{
	String joining = "messages AS message LEFT JOIN flags ON flags.stamp=message.stamp";
		
	if(!mContact.empty())
		joining+=" LEFT JOIN messages AS parent ON parent.stamp=NULLIF(message.parent,'')";
	
	return joining;
}

String MessageQueue::Selection::filter(void) const
{
        String condition;
        if(mContact.empty()) condition = "1=1";
	else {
		condition = "((message.contact='' OR message.contact=@contact)\
			OR (NOT message.relayed AND NULLIF(message.parent,'') IS NOT NULL AND (parent.contact='' OR parent.contact=@contact))\
			OR EXISTS(SELECT 1 FROM received WHERE received.contact=@contact AND received.stamp=message.stamp)\
			OR (IFNULL(flags.passed,0)=1 OR EXISTS(SELECT 1 FROM flags WHERE stamp=NULLIF(message.parent,'') AND IFNULL(flags.passed,0)=1)))";
	}

        if(!mBaseStamp.empty()) condition+= " AND (message.time>@basetime OR (message.time=@basetime AND message.stamp>=@basestamp))";

        if(!mParentStamp.empty()) condition+= " AND message.parent=@parentstamp";

        if( mIncludePrivate && !mIncludePublic) condition+= " AND message.public=0";
        if(!mIncludePrivate &&  mIncludePublic) condition+= " AND message.public=1";
        if(!mIncludeOutgoing) condition+= " AND message.incoming=1";

        return condition;
}

void MessageQueue::Selection::filterBind(Database::Statement &statement) const
{
	statement.bind(statement.parameterIndex("contact"), mContact);
	statement.bind(statement.parameterIndex("parentstamp"), mParentStamp);
	statement.bind(statement.parameterIndex("basestamp"), mBaseStamp);
	statement.bind(statement.parameterIndex("basetime"), mBaseTime);
}

}

