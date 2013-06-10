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
#include "tpn/user.h"

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
	content TEXT,\
	time INTEGER(8),\
	isread INTEGER(1))");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS stamp ON messages (stamp)");
	mDatabase->execute("CREATE INDEX IF NOT EXISTS time ON messages (time)");
}

MessageQueue::~MessageQueue(void)
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

bool MessageQueue::getLast(int count, Array<Message> &result)
{
	Synchronize(this);
	
	result.clear();
	
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages ORDER BY time DESC LIMIT ?1");
	statement.bind(1, count);
        while(statement.step())
	{
		Message message;
		message.deserialize(statement);
		result.push_back(message);
	}
	statement.finalize();
	
	result.reverse();
        return (!result.empty());
}

bool MessageQueue::getLast(const Time &time, int max, Array<Message> &result)
{
	Synchronize(this);
	
	result.clear();
	
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE time>=?1 ORDER BY time DESC LIMIT ?2");
	statement.bind(1, time);
	statement.bind(2, max);
        while(statement.step())
	{
		Message message;
		message.deserialize(statement);
		result.push_back(message);
	}
	statement.finalize();
	
	result.reverse();
        return (!result.empty());
}

bool MessageQueue::getLast(const String &oldLast, int count, Array<Message> &result)
{
	Synchronize(this);
	
	result.clear();
	
	if(oldLast.empty()) 
		return getLast(count, result);
	
	int64_t oldLastId = -1;
	Database::Statement statement = mDatabase->prepare("SELECT id FROM messages WHERE stamp=?1");
	statement.bind(1, oldLast);
        if(!statement.step())
	{
		statement.finalize();
		return getLast(count, result);
	}
	statement.value(0, oldLastId);
	statement.finalize();

	statement = mDatabase->prepare("SELECT * FROM messages WHERE id>?1 ORDER BY time DESC");
	statement.bind(1, oldLastId);
	while(statement.step())
	{
		Message message;
		message.deserialize(statement);
		result.push_back(message);
	}
	statement.finalize();
	
        result.reverse();
	return (!result.empty());
}


bool MessageQueue::getDiff(const Array<String> &oldStamps, Array<Message> &result)
{
	Synchronize(this);
	result.clear();
	
	const int maxAge = 48 + 1;	// hours
	
	Time time(Time::Now());
	time.addHours(-maxAge);
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE time>=?1 OR isread=0 ORDER BY time");
	statement.bind(1, time);
	
	Map<String, Message> tmp;
        while(statement.step())
	{
		Message message;
		message.deserialize(statement);
		tmp.insert(message.stamp(), message);
	}
	
	statement.finalize();
	
	for(int i=0; i<oldStamps.size(); ++i)
		tmp.erase(oldStamps[i]);
	
	tmp.getValues(result);
        return (!result.empty());
}

bool MessageQueue::getUnread(Array<Message> &result)
{
	Synchronize(this);
	
	result.clear();
	
	Database::Statement statement = mDatabase->prepare("SELECT * FROM messages WHERE isread=0 ORDER BY time");
        while(statement.step())
	{
		Message message;
		message.deserialize(statement);
		result.push_back(message);
	}

	statement.finalize();
        return (!result.empty());
}

}
				
