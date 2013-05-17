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

namespace tpn
{

MessageQueue::MessageQueue(void)
{

}

MessageQueue::~MessageQueue(void)
{
	for(int i=0; i<mMessages.size(); ++i)
		delete mMessages[i];
	
	mMessages.clear();
	mMessagesByStamp.clear();
}

unsigned MessageQueue::count(void) const
{
	return unsigned(mMessages.size());
}

unsigned MessageQueue::unreadCount(void) const
{
	return unsigned(mUnread.size());
}

bool MessageQueue::add(const Message &message)
{
	Synchronize(this);
	
	String stamp = message.stamp();
	if(mByStamp.contains(stamp))
		return false;

	Set<Message>::iterator it = mMessages.insert(mMessages.end(), message);
	mByStamp.insert(stamp, it);
	if(!message.isRead()) mUnread.insert(stamp);
	return true;
}

bool MessageQueue::markRead(const String &stamp, bool read)
{
	Synchronize(this);
  
	Map<String, Set<Message>::iterator>::iterator it = mByStamp.find(stamp);
	if(it == mByStamp.end()) return false;
	it->second->markRead(read);
	if(read) mUnread.remove(stamp);
	else mUnread.insert(stamp);
	return true;
}

bool MessageQueue::get(const String &stamp, Message &result)
{
	Synchronize(this);
  
	Map<String, Set<Message>::iterator>::iterator it = mByStamp.find(stamp);
	if(it == mByStamp.end()) return false;
	result = *it->second;
	return true;
}

bool MessageQueue::getAllStamps(StringSet &result)
{
	Synchronize(this);
	
	result.clear();
	mByStamp.getKeys(result);
	return (!result.empty());
}

bool MessageQueue::getUnreadStamps(StringSet &result)
{
	Synchronize(this);
	
	result = mUnread;
	return (!result.empty());
}

bool MessageQueue::getNewStamps(const StringSet &old, StringSet &result)
{
	Synchronize(this);
	
	result.clear();
	
	for(	Set<Message>::iterator it = mMessages.begin();
		it != mMessages.end();
		++it)
	{
		String stamp = it->getStamp();
		if(!old.contains(stamp))
			result.insert(stamp);
	}
	
	return (!result.empty());
}

}
