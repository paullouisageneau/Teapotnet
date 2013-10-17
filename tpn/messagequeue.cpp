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

#include "tpn/messagequeue.h"
#include "tpn/user.h"
#include "tpn/html.h"
#include "tpn/sha512.h"
#include "tpn/yamlserializer.h"
#include "tpn/jsonserializer.h"
#include "tpn/notification.h"

namespace tpn
{

MessageQueue::MessageQueue(User *user) :
	mUser(user),
	mHasNew(false)
{
	if(mUser) mDatabase = new Database(mUser->profilePath() + "messages.db");
	else mDatabase = new Database("messages.db");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS messages\
	(id INTEGER PRIMARY KEY AUTOINCREMENT,\
	stamp TEXT UNIQUE,\
	parent TEXT,\
	headers TEXT,\
	content TEXT,\
	peering BLOB,\
	time INTEGER(8),\
	incoming INTEGER(1),\
	public INTEGER(1),\
	isread INTEGER(1))");

	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON messages (stamp)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS peering ON messages (peering)");
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

bool MessageQueue::add(const Message &message)
{
	Synchronize(this);
	
	if(message.stamp().empty()) 
	{
		LogWarn("MessageQueue::add", "Message with empty stamp, dropping");
		return false;
	}
	
	Database::Statement statement = mDatabase->prepare("SELECT id FROM messages WHERE stamp=?1");
	statement.bind(1, message.stamp());
	bool exists = statement.step();
	statement.finalize();
	
	LogDebug("MessageQueue::add", "Adding message '"+message.stamp()+"'"+(exists ? " (already in queue)" : ""));
	if(exists && !message.isIncoming()) return false;
	
	mDatabase->insert("messages", message);
	
	if(!exists)
	{
		if(message.isIncoming()) mHasNew = true;
		notifyAll();
		SyncYield(this);
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

void MessageQueue::markRead(const String &stamp)
{
	Synchronize(this);
  
	Database::Statement statement = mDatabase->prepare("UPDATE messages SET isread=1 WHERE stamp=?1");
	statement.bind(1, stamp);
	statement.execute();
}

void MessageQueue::ack(const Array<Message> &messages)
{
	Map<Identifier, StringArray> stamps;
	for(int i=0; i<messages.size(); ++i)
		if(!messages[i].isRead() && messages[i].isIncoming())
		{
			stamps[messages[i].peering()].append(messages[i].stamp());

			Database::Statement statement = mDatabase->prepare("UPDATE messages SET isread=1 WHERE stamp=?1");
        		statement.bind(1, messages[i].stamp());
        		statement.execute();
		}

	for(Map<Identifier, StringArray>::iterator it = stamps.begin();
		it != stamps.end();
		++it)
	{
		String tmp;
		YamlSerializer serializer(&tmp);
		serializer.output(it->second);

		Notification notification(tmp);
		notification.setParameter("type", "ack");
		notification.send(it->first);
		
		const AddressBook::Contact *self = mUser->addressBook()->getSelf();
		if(self && self->peering() != it->first) 
			notification.send(self->peering());
	}
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
	
	Identifier peering;
	AddressBook::Contact *contact = NULL;
	if(!uname.empty()) 
	{
		contact = mUser->addressBook()->getContactByUniqueName(uname);
		if(!contact) throw 404;
		peering = contact->peering();
	}
	
	AddressBook::Contact *self = mUser->addressBook()->getSelf();
	
	if(request.method == "POST")
        {
		if(!user()->checkToken(request.post["token"], "message")) 
			throw 403;
		
                if(!request.post.contains("message") || request.post["message"].empty())
			throw 400;
		
		bool isPublic = request.post["public"].toBool();
		
		try {
			Message message(request.post["message"]);
			message.setPeering(peering);
			message.setPublic(isPublic);
			message.setHeader("from", mUser->name());
			if(request.post.contains("parent"))
				message.setParent(request.post["parent"]);
		
			if(request.post.contains("attachment"))
				message.setHeader("attachment", request.post.get("attachment"));
	
			if(contact)
			{
				contact->send(message);
				if(self && self->peering() != peering)
					self->send(message);
			}
			else {
				user()->addressBook()->send(message);
			}
			
			add(message);
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
		String last;
		request.get.get("last", last);

		Http::Response response(request, 200);
		response.headers["Content-Type"] = "application/json";
		response.send();

		Selection selection;
		if(request.get["public"].toBool()) selection = selectPublic(peering);
		else  selection = selectPrivate(peering);
		if(request.get["incoming"].toBool()) selection.includeOutgoing(false);
		if(request.get.contains("parent")) selection.setParentStamp(request.get["parent"]);
		
		int count = 100; // TODO: 100 messages selected is max
		if(request.get.contains("count")) count = request.get["count"].toInt();
		
		SerializableArray<Message> array;
		while(!selection.getLast(last, count, array))
			if(!wait(60.)) return;

		ack(array);
		
		JsonSerializer serializer(response.sock);
		serializer.output(array);
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
	page.raw("<a class=\"button\" href=\"#\" onclick=\"createFileSelector('/"+mUser->name()+"/myself/files/?json', '#fileSelector', 'input.attachment', 'textarea.chatinput');\">Send file</a>");
// TODO: should be hidden in CSS
#ifndef ANDROID
	String popupUrl = prefix + url + "?popup=1";
	page.raw("<a class=\"button\" href=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','/');\">Popup</a>");
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
	page.closeForm();
	page.close("div");

	page.close("div");
 
	page.javascript("function post()\n\
		{\n\
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
			$(document.chatform.chatinput).val("").change();\n\
			$(document.chatform.attachment).val("").change();\n\
			$('#attachedfile').hide();\n\
		}\n\
		var status = \""+status+"\";\n\
		var statusBackup;\n\
		function checkStatus() \n\
		{\n\
			status = $('#status').text()\n\
			if(statusBackup != status) updateStatus();\n\
			statusBackup = status;\n\
			setTimeout(function() {\n\
				checkStatus();\n\
			}, 1000);\n\
		}\n\
		function updateStatus()\n\
		{\n\
			//if(status != \"Online\")\n\
			//{\n\
			//	document.chatform.chatinput.blur();\n\
			//	document.chatform.chatinput.style.color = 'grey';\n\
			//	document.chatform.chatinput.value = '"+name.capitalized()+" is not online for now, and will receive your message on his/her next connection.';\n\
			//}\n\
			//else\n\
			//{\n\
			//	document.chatform.chatinput.focus();\n\
			//}\n\
		}\n\
		document.chatform.onsubmit = function()\n\
		{\n\
			post();\n\
			return false;\n\
		}\n\
		document.chatform.attachment.onchange = function()\n\
		{\n\
			$('#attachedfile').html('');\n\
			$('#attachedfile').hide();\n\
			var filename = $(document.chatform.chatinput).val();\n\
			if(filename != '') {\n\
				$('#attachedfile').append('<img class=\"icon\" src=\"/file.png\">');\n\
				$('#attachedfile').append('<span class=\"filename\">'+filename+'</span>');\n\
				$('#attachedfile').show();\n\
			}\n\
			document.chatform.chatinput.focus();\n\
			document.chatform.chatinput.select();\n\
		}\n\
		$('textarea.chatinput').keypress(function(e) {\n\
			if (e.keyCode == 13 && !e.shiftKey) {\n\
				e.preventDefault();\n\
				post();\n\
			}\n\
		});\n\
		$('#attachedfile').hide();\n\
		$(document).ready(function() { checkStatus(); });\n\
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

MessageQueue::Selection MessageQueue::select(const Identifier &peering) const
{
	return Selection(this, peering, true, true, true);
}

MessageQueue::Selection MessageQueue::selectPrivate(const Identifier &peering) const
{
	return Selection(this, peering, true, false, true);
}

MessageQueue::Selection MessageQueue::selectPublic(const Identifier &peering, bool includeOutgoing) const
{
	return Selection(this, peering, false, true, includeOutgoing);
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

MessageQueue::Selection::Selection(const MessageQueue *messageQueue, const Identifier &peering, 
				   bool includePrivate, bool includePublic, bool includeOutgoing) :
	mMessageQueue(messageQueue),
	mPeering(peering),
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
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT COUNT(*) AS count FROM messages WHERE "+filter());
	filterBind(statement);
	if(!statement.step()) return 0;
	statement.input(count);
	statement.finalize();
	return count;
}

int MessageQueue::Selection::unreadCount(void) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	int count = 0;
        Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT COUNT(*) AS count FROM messages WHERE "+filter()+" AND isread=0 AND incoming=1");
	filterBind(statement);
	if(!statement.step()) return 0;
        statement.input(count);
        statement.finalize();
        return count;
}

bool MessageQueue::Selection::getOffset(int offset, Message &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" ORDER BY time,stamp LIMIT @offset,1");
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

bool MessageQueue::Selection::getRange(int offset, int count, Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" ORDER BY time,stamp LIMIT @offset,@count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("offset"), offset);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
        return (!result.empty());
}

bool MessageQueue::Selection::getLast(int count, Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();

	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" ORDER BY id DESC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
	if(!result.empty())
	{
		result.reverse();
		mMessageQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MessageQueue::Selection::getLast(const Time &time, int max, Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" AND time>=@time ORDER BY id DESC LIMIT @max");
	filterBind(statement);
	statement.bind(statement.parameterIndex("time"), time);
	statement.bind(statement.parameterIndex("max"), max);
        statement.fetch(result);
	statement.finalize();
	
	if(!result.empty())
	{
		result.reverse();
		mMessageQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MessageQueue::Selection::getLast(const String &oldLast, int count, Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	if(oldLast.empty()) 
		return getLast(count, result);
	
	int64_t oldLastId = -1;
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT id FROM messages WHERE stamp=?1");
	statement.bind(1, oldLast);
        if(statement.step())
	{
		statement.input(oldLastId);
		statement.finalize();
	}
	
	statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" AND id>@id LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("id"), oldLastId);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
	if(!result.empty())
	{
		mMessageQueue->mHasNew = false;
		return true;
	}
        return false;
}

bool MessageQueue::Selection::getUnread(Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" AND isread=0 ORDER BY time");
        filterBind(statement);
	statement.fetch(result);
	statement.finalize();
        return (!result.empty());
}

bool MessageQueue::Selection::getUnreadStamps(StringArray &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT stamp FROM messages WHERE "+filter()+" AND isread=0 ORDER BY time");
        filterBind(statement);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

void MessageQueue::Selection::markRead(const String &stamp)
{
        Assert(mMessageQueue);
        Synchronize(mMessageQueue);

        Database::Statement statement = mMessageQueue->mDatabase->prepare("UPDATE messages SET isread=1 WHERE "+filter()+" AND stamp=@stamp");
        filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
        statement.execute();
}

int MessageQueue::Selection::checksum(int offset, int count, ByteStream &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
        Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT stamp AS sum FROM messages WHERE "+filter()+"  ORDER BY time,stamp LIMIT @offset,@count");
        filterBind(statement);
        statement.bind(statement.parameterIndex("offset"), offset);
	statement.bind(statement.parameterIndex("count"), count);
       
	Sha512 sha512;
	sha512.init();
	count = 0;
	while(statement.step())
	{
		String stamp;
		statement.value(0, stamp);
		if(count) sha512.process(",", 1);
		sha512.process(stamp.data(), stamp.size());
		++count;
	}
	sha512.finalize(result);
	statement.finalize();
	return count;
}

String MessageQueue::Selection::filter(void) const
{
	String condition;
	if(mPeering != Identifier::Null) condition = "(peering=@peering OR peering='' OR peering IS NULL)";
	else condition = "1=1"; // TODO
	
	if(!mBaseStamp.empty()) condition+= " AND (time>@basetime OR (time=@basetime AND stamp>=@basestamp))";
	
	if(!mParentStamp.empty()) condition+= " AND parent=@parentstamp";
	
	if( mIncludePrivate && !mIncludePublic) condition+= " AND public=0";
	if(!mIncludePrivate &&  mIncludePublic) condition+= " AND public=1";
	if(!mIncludeOutgoing) condition+= " AND incoming=1";
	
	return condition;
}

void MessageQueue::Selection::filterBind(Database::Statement &statement) const
{
	statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
	statement.bind(statement.parameterIndex("parentstamp"), mParentStamp);
	statement.bind(statement.parameterIndex("basestamp"), mBaseStamp);
	statement.bind(statement.parameterIndex("basetime"), mBaseTime);
}

}

