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

#include "tpn/mailbase.h"

namespace tpn
{
/*
MailBase::MailBase(User *user) :
	mUser(user),
	mHasNew(false)
{
	if(mUser) mDatabase = new Database(mUser->profilePath() + "mail.db");
	else mDatabase = new Database("mail.db");
		
	// WARNING: Additionnal fields in mail should be declared in erase()
	mDatabase->execute("CREATE TABLE IF NOT EXISTS mail\
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
	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON mail (stamp)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS contact ON mail (contact)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS time ON mail (time)");
	
	Interface::Instance->add(mUser->urlPrefix()+"/mail", this);
}

MailBase::~MailBase(void)
{
	Interface::Instance->remove(mUser->urlPrefix()+"/mail", this);
	
	delete mDatabase;
}

void MailBase::markReceived(const String &stamp, const String &uname)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("INSERT OR IGNORE INTO received (stamp, contact, time) VALUES (?1,?2,?3)");
	statement.bind(1, stamp);
	statement.bind(2, uname);
	statement.bind(3, Time::Now());
	statement.execute();
}

void MailBase::markRead(const String &stamp)
{
	setFlag(stamp, "read", true);
}

void MailBase::markPassed(const String &stamp)
{
	setFlag(stamp, "passed", true);
}

void MailBase::markDeleted(const String &stamp)
{
	setFlag(stamp, "deleted", true);
}

bool MailBase::isRead(const String &stamp) const
{
	return getFlag(stamp, "read");
}

bool MailBase::isPassed(const String &stamp) const
{
	return getFlag(stamp, "passed");
}

bool MailBase::isDeleted(const String &stamp) const
{
	return getFlag(stamp, "deleted");
}

void MailBase::setFlag(const String &stamp, const String &name, bool value)
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

bool MailBase::getFlag(const String &stamp, const String &name) const
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

void MailBase::http(const String &prefix, Http::Request &request)
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
		
		int count = 100; // TODO: 100 mail selected is max
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

	page.open("div", "chatmail");
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
		setMailsReceiver('"+Http::AppendGet(request.fullUrl, "json")+"','#chatmail');");

	unsigned refreshPeriod = 5000;
	page.javascript("setCallback(\""+contact->urlPrefix()+"/?json\", "+String::number(refreshPeriod)+", function(info) {\n\
		transition($('#status'), info.status.capitalize());\n\
		$('#status').removeClass().addClass('button').addClass(info.status);\n\
		if(info.newmail) playMailSound();\n\
	});");

	page.footer();
	return;
}

MailBase::Selection MailBase::select(const String &uname) const
{
	return Selection(this, uname, true, true, true);
}

MailBase::Selection MailBase::selectPrivate(const String &uname) const
{
	return Selection(this, uname, true, false, true);
}

MailBase::Selection MailBase::selectPublic(const String &uname, bool includeOutgoing) const
{
	return Selection(this, uname, false, true, includeOutgoing);
}

MailBase::Selection MailBase::selectChilds(const String &parentStamp) const
{
	return Selection(this, parentStamp);
}

MailBase::Selection::Selection(void) :
	mMailBase(NULL),
	mBaseTime(time_t(0)),
	mIncludePrivate(true),
	mIncludePublic(true),
	mIncludeOutgoing(true)
{
	
}

MailBase::Selection::Selection(const MailBase *board, const String &uname, 
				   bool includePrivate, bool includePublic, bool includeOutgoing) :
	mMailBase(board),
	mContact(uname),
	mBaseTime(time_t(0)),
	mIncludePrivate(includePrivate),
	mIncludePublic(includePublic),
	mIncludeOutgoing(includeOutgoing)
{
	
}

MailBase::Selection::Selection(const MailBase *board, const String &parent) :
	mMailBase(board),
	mBaseTime(time_t(0)),
	mIncludePrivate(true),
	mIncludePublic(true),
	mIncludeOutgoing(true),
	mParentStamp(parent)
{
	
}

MailBase::Selection::~Selection(void)
{
	
}

void MailBase::Selection::setParentStamp(const String &stamp)
{
	mParentStamp = stamp;
}

bool MailBase::Selection::setBaseStamp(const String &stamp)
{
	if(!stamp.empty())
	{
		Mail base;
		if(mMailBase->get(stamp, base))
		{
			mBaseStamp = base.stamp();
			mBaseTime = base.time();
			return true;
		}
	}
	
	mBaseStamp.clear();
	return false;
}

String MailBase::Selection::baseStamp(void) const
{
	return mBaseStamp;
}

void MailBase::Selection::includePrivate(bool enabled)
{
	mIncludePrivate = enabled;
}

void MailBase::Selection::includePublic(bool enabled)
{
	mIncludePublic = enabled;
}

void MailBase::Selection::includeOutgoing(bool enabled)
{
	mIncludeOutgoing = enabled;
}

int MailBase::Selection::count(void) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	
	int count = 0;
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("COUNT(*) AS count")+" WHERE "+filter());
	filterBind(statement);
	if(statement.step())
		statement.input(count);
	statement.finalize();
	return count;
}

int MailBase::Selection::unreadCount(void) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	
	int count = 0;
        Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("COUNT(*) AS count")+" WHERE "+filter()+" AND mail.incoming=1 AND IFNULL(flags.read,0)=0");
	filterBind(statement);
	if(statement.step())
		statement.input(count);
        statement.finalize();
        return count;
}

bool MailBase::Selection::contains(const String &stamp) const
{
        Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("id")+" WHERE mail.stamp=@stamp AND "+filter());
	filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
	bool found = statement.step();
        statement.finalize();
        return found;
}

bool MailBase::Selection::getOffset(int offset, Mail &result) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" ORDER BY mail.time,mail.stamp LIMIT @offset,1");
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

bool MailBase::Selection::getRange(int offset, int count, List<Mail> &result) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	result.clear();
	
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" ORDER BY mail.time,mail.stamp LIMIT @offset,@count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("offset"), offset);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
        return (!result.empty());
}

bool MailBase::Selection::getLast(int count, List<Mail> &result) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	result.clear();

	if(count == 0) return false;

	// Fetch the highest id
        int64_t maxId = 0;
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("MAX(mail.id) AS maxid")+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0");
        if(!statement.step())
        {
                statement.finalize();
                return false;
        }
        statement.input(maxId);
        statement.finalize();

	// Find the time of the last mail counting only mail without a parent
	Time lastTime = Time(0);
	statement = mMailBase->mDatabase->prepare("SELECT "+target("time")+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND NULLIF(mail.parent,'') IS NULL ORDER BY mail.time DESC,mail.id DESC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
	while(statement.step())
		statement.input(lastTime);
	statement.finalize();

	// Fetch mail by time
	const String fields = "mail.*, mail.id AS number, flags.read, flags.passed";
	statement = mMailBase->mDatabase->prepare("SELECT "+target(fields)+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND (mail.id=@maxid OR mail.time>=@lasttime OR (mail.incoming=1 AND IFNULL(flags.read,0)=0)) ORDER BY mail.time,mail.id");
	filterBind(statement);
	statement.bind(statement.parameterIndex("maxid"), maxId);
	statement.bind(statement.parameterIndex("lasttime"), lastTime);
        statement.fetch(result);
	statement.finalize();

	if(!result.empty())
	{
		if(mIncludePrivate) mMailBase->mHasNew = false;
		return true;
	}
        return false;
}

bool MailBase::Selection::getLast(int64_t nextNumber, int count, List<Mail> &result) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	result.clear();
	
	if(nextNumber <= 0) 
		return getLast(count, result);
	
	const String fields = "mail.*, mail.id AS number, flags.read, flags.passed";
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target(fields)+" WHERE "+filter()+" AND IFNULL(flags.deleted,0)=0 AND mail.id>=@id ORDER BY mail.id ASC LIMIT @count");
	filterBind(statement);
	statement.bind(statement.parameterIndex("id"), nextNumber);
	statement.bind(statement.parameterIndex("count"), count);
        statement.fetch(result);
	statement.finalize();
	
	if(!result.empty())
	{
		if(mIncludePrivate) mMailBase->mHasNew = false;
		return true;
	}
        return false;
}

bool MailBase::Selection::getUnread(List<Mail> &result) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	result.clear();
	
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("*")+" WHERE "+filter()+" AND mail.incoming=1 AND IFNULL(flags.read,0)=0");
        filterBind(statement);
	statement.fetch(result);
	statement.finalize();
        return (!result.empty());
}

bool MailBase::Selection::getUnreadStamps(StringList &result) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	result.clear();
	
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" AND mail.incoming=1 AND IFNULL(flags.read,0)=0");
        filterBind(statement);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

bool MailBase::Selection::getPassedStamps(StringList &result, int count) const
{
	Assert(mMailBase);
	Synchronize(mMailBase);
	result.clear();
	
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" AND IFNULL(flags.passed,0)=1 ORDER by flags.time DESC LIMIT @count");
        filterBind(statement);
	statement.bind(statement.parameterIndex("count"), count);
	statement.fetchColumn(0, result);
	statement.finalize();
        return (!result.empty());
}

void MailBase::Selection::markRead(const String &stamp)
{
        Assert(mMailBase);
        Synchronize(mMailBase);

	// Check the mail is in selection
	Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("id")+" WHERE "+filter()+" AND mail.stamp=@stamp");
        filterBind(statement);
	statement.bind(statement.parameterIndex("stamp"), stamp);
	bool found = statement.step();
	statement.finalize();
	
	if(found)
	{
		statement = mMailBase->mDatabase->prepare("INSERT OR IGNORE INTO flags (stamp) VALUES (?1)");
		statement.bind(1, stamp);
		statement.execute();
		
		statement = mMailBase->mDatabase->prepare("UPDATE flags SET read=?2, time=?3 WHERE stamp=?1");
		statement.bind(1, stamp);
		statement.bind(2, true);
		statement.bind(3, Time::Now());
		statement.execute();
	}
}

int MailBase::Selection::checksum(int offset, int count, BinaryString &result) const
{
	StringList stamps;

	{
		Assert(mMailBase);
		Synchronize(mMailBase);
		result.clear();
	
		Database::Statement statement = mMailBase->mDatabase->prepare("SELECT "+target("stamp")+" WHERE "+filter()+" ORDER BY mail.time,mail.stamp LIMIT @offset,@count");
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

String MailBase::Selection::target(const String &columns) const
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

String MailBase::Selection::table(void) const
{
	String joining = "mail AS mail LEFT JOIN flags ON flags.stamp=mail.stamp";
		
	if(!mContact.empty())
		joining+=" LEFT JOIN mail AS parent ON parent.stamp=NULLIF(mail.parent,'')";
	
	return joining;
}

String MailBase::Selection::filter(void) const
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

void MailBase::Selection::filterBind(Database::Statement &statement) const
{
	statement.bind(statement.parameterIndex("contact"), mContact);
	statement.bind(statement.parameterIndex("parentstamp"), mParentStamp);
	statement.bind(statement.parameterIndex("basestamp"), mBaseStamp);
	statement.bind(statement.parameterIndex("basetime"), mBaseTime);
}
*/
}

