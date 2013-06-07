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
#include "tpn/yamlserializer.h"

namespace tpn
{

MessageQueue::MessageQueue(User *user) :
	mUser(user)
	mHasNew(false)
{
	if(mUser) mDatabase = new Database(mUser->profilePath() + "messages.db");
	else mDatabase = new Database("messages.db");
	
	mDatabase->execute("CREATE TABLE IF NOT EXISTS messages\
	(id INTEGER PRIMARY KEY AUTOINCREMENT,\
	stamp TEXT UNIQUE,\
	content TEXT,\
	time INTEGER(8),\
	isread INTEGER(1))");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON messages (stamp)");
}

MessageQueue::MessageQueue(void)
{

}

unsigned MessageQueue::count(void) const
{
	unsigned count;
	Database::Statement statement = mDatabase->prepare("SELECT COUNT(*) FROM messages");
	statement.step();
	statement.input(count);
	statement.finalize();
	return count;
}

unsigned MessageQueue::unreadCount(void) const
{
	unsigned count;
        Database::Statement statement = mDatabase->prepare("SELECT COUNT(*) FROM messages WHERE isread=1");
        statement.step();
        statement.input(count);
        statement.finalize();
        return count;
}

bool MessageQueue::hasNew(void) const
{
	bool old = mHasNew;
	mHasNew = false;
	return old;
}

bool MessageQueue::add(const Message &message)
{
	Synchronize(this);
	
	Database::Statement statement = mDatabase->prepare("SELECT id FROM messages WHERE stamp=?1");
        statement.bind(1, message.stamp());
	bool exists = statement.step();
        statement.finalize();
	if(exists) return false;

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

bool MessageQueue::markRead(const String &stamp, bool read)
{
	Synchronize(this);
  
	Database::Statement statement = mDatabase->prepare("UPDATE messages SET isread=?1 WHERE stamp=?2");
        statement.bind(1, read);
	statement.bind(2, stamp);
	statement.execute();
	return true;
}

bool MessageQueue::getAllStamps(StringArray &result)
{
	Synchronize(this);

	// TODO	
	result.clear();
	mByStamp.getKeys(result);
	return (!result.empty());
}

bool MessageQueue::getUnreadStamps(StringArray &result)
{
	Synchronize(this);
	
	result.clear();
	
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE isread=0");
        while(statement.step())
	{
		result.deserialize(statement);
        	statement.finalize(); 
		return true;
	}

	statement.finalize();
        return false;
}

bool MessageQueue::getLastStamps(const String &oldLast, StringArray &result)
{
	Synchronize(this);

	result.clear();
	
        Set<Message>::iterator it = mMessages.begin();
	if(!oldLast.empty())
	{
		Map<String, Set<Message>::iterator>::iterator jt = mByStamp.find(oldLast);
		if(jt != mByStamp.end())
		{	it = jt->second;
			++it;
		}
	}
	
	while(it != mMessages.end())
	{
		result.append(it->stamp());
		++it;
	}
	
	return (!result.empty());
}

bool MessageQueue::getNewStamps(const StringArray &oldStamps, StringArray &result)
{
	Synchronize(this);
	
	result.clear();
	
	for(	Set<Message>::iterator it = mMessages.begin();
		it != mMessages.end();
		++it)
	{
		String stamp = it->stamp();
		if(!oldStamps.contains(stamp))
			result.append(stamp);
	}
	
	return (!result.empty());
}

String MessageQueue::getFileName(int daysAgo) const
{
	Time time = Time::Now();
	time.addDays(-daysAgo);
	return mPath + time.toIsoDate();
}

String MessageQueue::getLastFileName(void) const
{
	for(int d=1; d<31; ++d)
	{
		String path = getFileName(d);
		if(File::Exist(path)) 
			return path;
	}
	
	return "";
}

}
				
