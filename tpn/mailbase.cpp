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

