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

#ifndef TPN_MESSAGEQUEUE_H
#define TPN_MESSAGEQUEUE_H

#include "tpn/include.h"
#include "tpn/synchronizable.h"
#include "tpn/message.h"
#include "tpn/database.h"
#include "tpn/string.h"
#include "tpn/set.h"
#include "tpn/array.h"
#include "tpn/map.h"

namespace tpn
{

class User;
	
class MessageQueue : public Synchronizable
{
public:
	MessageQueue(User *user);
	~MessageQueue(void);
	
	unsigned count(void) const;
	unsigned unreadCount(void) const;
	bool hasNew(void) const;
	
	bool add(const Message &message);
	bool get(const String &stamp, Message &result);
	bool markRead(const String &stamp, bool read = true);
	
	bool getLast(int count, Array<Message> &result);
	bool getLast(const Time &time, int max, Array<Message> &result);
	bool getLast(const String &oldLast, int count, Array<Message> &result);
	
	bool getDiff(const Array<String> &oldStamps, Array<Message> &result);
	bool getUnread(Array<Message> &result);
	
private:
	User *mUser;
	Database *mDatabase;
	
	mutable bool mHasNew;
};

}

#endif

