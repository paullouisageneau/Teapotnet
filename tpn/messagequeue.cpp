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
	
	if(exists) 
	{
		LogDebug("MessageQueue::add", "Message '" + message.stamp() + "' is already in queue");
		return false;
	}

	mDatabase->insert("messages", message);
	mHasNew = true;
	notifyAll();
	return true;
}

bool MessageQueue::get(const String &stamp, Message &result)
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
	url = "/" + uname + "/";
	
	const AddressBook::Contact *contact = mUser->addressBook()->getContactByUniqueName(uname);
	const AddressBook::Contact *self = mUser->addressBook()->getSelf();
	if(!contact) throw 404;
		
	String name = contact->name();
	String status = contact->status();
	Identifier peering = contact->peering();
	
	if(request.method == "POST")
        {
                if(request.post.contains("message") && !request.post["message"].empty())
                {
                        try {
                                Message message(request.post["message"]);
				message.setPeering(peering);
				message.setHeader("from", mUser->name());
				message.send(peering);
				if(self && self->peering() != peering) 
					message.send(self->peering());
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
        }
        
	if(request.get.contains("json"))
	{
		String last;
		request.get.get("last", last);

		Http::Response response(request, 200);
		response.headers["Content-Type"] = "application/json";
		response.send();

		const int count = 10;

		SerializableArray<Message> array;
		while(!select(peering).getLast(last, count, array))
			wait();

		ack(array);
		
		JsonSerializer serializer(response.sock);
		serializer.output(array);
		return;
	}
	
	bool isPopup = request.get.contains("popup");

	Http::Response response(request,200);
	response.send();

	Html page(response.sock);
	
	String title= "Chat with "+name;
	page.header(title, isPopup);
	
	if(isPopup)
	{
		page.open("div","topmenu");
		page.span(title, ".button");
		page.span(status.capitalized(), "status.button");
		page.close("div");
		page.open("div", "chat");
	}
	else {
		String popupUrl = prefix + url + "?popup=1";
		page.open("div","topmenu");
		page.span(status.capitalized(), "status.button");
#ifndef ANDROID
		page.raw("<a class=\"button\" ref=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','/');\">Popup</a>");
#endif
		page.close("div");
		page.open("div", "chat.box");
	}

	page.open("div", "chatmessages");
	page.close("div");

	page.open("div", "chatpanel");
	page.openForm("#", "post", "chatform");
	page.textarea("chatinput");
	//page.button("send","Send");
	//page.br();
	page.closeForm();
	page.javascript("$(document).ready(function() { document.chatform.chatinput.focus(); });");
	page.close("div");

	page.close("div");
 
	page.javascript("function post()\n\
		{\n\
			var message = document.chatform.chatinput.value;\n\
			if(!message) return false;\n\
			document.chatform.chatinput.value = '';\n\
			var request = $.post('"+prefix+url+"',\n\
				{ 'message': message });\n\
			request.fail(function(jqXHR, textStatus) {\n\
				alert('The message could not be sent.');\n\
			});\n\
		}\n\
		document.chatform.onsubmit = function()\n\
		{\n\
			post();\n\
			return false;\n\
		}\n\
		$('textarea.chatinput').keypress(function(e) {\n\
			if (e.keyCode == 13 && !e.shiftKey) {\n\
				e.preventDefault();\n\
				post();\n\
			}\n\
		});\n\
		$(document).ready( function() {\n\
			$('#chatmessages').scrollTop($('#chatmessages')[0].scrollHeight);\n\
		});\n\
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
	return Selection(this, peering);
}

MessageQueue::Selection MessageQueue::selectAll(void) const
{
	return Selection(this, Identifier::Null);
}

MessageQueue::Selection::Selection(void) :
	mMessageQueue(NULL)
{
	
}

MessageQueue::Selection::Selection(const MessageQueue *messageQueue, const Identifier &peering) :
	mMessageQueue(messageQueue),
	mPeering(peering)
{
	
}
	
MessageQueue::Selection::~Selection(void)
{
	
}

unsigned MessageQueue::Selection::count(void) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	unsigned count = 0;
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT COUNT(*) AS count FROM messages WHERE "+filter());
	statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
	if(!statement.step()) return 0;
	statement.input(count);
	statement.finalize();
	return count;
}

unsigned MessageQueue::Selection::unreadCount(void) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	
	unsigned count = 0;
        Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT COUNT(*) AS count FROM messages WHERE "+filter()+" AND isread=0");
	statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
	if(!statement.step()) return 0;
        statement.input(count);
        statement.finalize();
        return count;
}

bool MessageQueue::Selection::getRange(int offset, int count, Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" ORDER BY time,stamp LIMIT @offset,@count");
	statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
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

	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" ORDER BY time DESC LIMIT @count");
	statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
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
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" AND time>=@time ORDER BY time DESC LIMIT @max");
	statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
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
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT time FROM messages WHERE stamp=?1");
	statement.bind(1, oldLast);
        if(!statement.step())
	{
		statement.finalize();
		return getLast(count, result);
	}
	Time time;
	statement.input(time);
	statement.finalize();

	if(!getLast(time, count, result)) 
		return false;

	int i = 0;
	while(i < result.size())
		if(result[i++].stamp() == oldLast)
			break;
	result.erase(0, i);
	return (!result.empty());
}

bool MessageQueue::Selection::getUnread(Array<Message> &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
	Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT * FROM messages WHERE "+filter()+" AND isread=0 ORDER BY time");
        statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
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
        statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

void MessageQueue::Selection::markRead(const String &stamp)
{
        Assert(mMessageQueue);
        Synchronize(mMessageQueue);

        Database::Statement statement = mMessageQueue->mDatabase->prepare("UPDATE messages SET isread=1 WHERE "+filter()+" AND stamp=@stamp");
        statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
	statement.bind(statement.parameterIndex("stamp"), stamp);
        statement.execute();
}

int MessageQueue::Selection::checksum(int offset, int count, ByteStream &result) const
{
	Assert(mMessageQueue);
	Synchronize(mMessageQueue);
	result.clear();
	
        Database::Statement statement = mMessageQueue->mDatabase->prepare("SELECT stamp AS sum FROM messages WHERE "+filter()+"  ORDER BY time,stamp LIMIT @offset,@count");
        statement.bind(statement.parameterIndex("peering"), mPeering.getDigest());
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
	if(mPeering != Identifier::Null) return "peering=@peering";
	else return "1=1"; // TODO
}

}
