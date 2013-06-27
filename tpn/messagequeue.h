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
#include "tpn/interface.h"
#include "tpn/identifier.h"
#include "tpn/string.h"
#include "tpn/set.h"
#include "tpn/array.h"
#include "tpn/map.h"

namespace tpn
{

class User;
	
class MessageQueue : public Synchronizable, public HttpInterfaceable
{
public:
	MessageQueue(User *user);
	~MessageQueue(void);

	User *user(void) const;
	
	bool hasNew(void) const;
	bool add(const Message &message);
	bool get(const String &stamp, Message &result);
	void markRead(const String &stamp);
	void ack(const Array<Message> &messages);

	void http(const String &prefix, Http::Request &request);
	
	class Selection
	{
	public:
		Selection(void);
		Selection(const MessageQueue *messageQueue, const Identifier &peering = Identifier::Null);
		~Selection(void);
		
		unsigned count(void) const;
		unsigned unreadCount(void) const;

		bool getRange(int offset, int count, Array<Message> &result) const;
		
		bool getLast(int count, Array<Message> &result) const;
		bool getLast(const Time &time, int max, Array<Message> &result) const;
		bool getLast(const String &oldLast, int count, Array<Message> &result) const;
	
		bool getUnread(Array<Message> &result) const;
		bool getUnreadStamps(StringArray &result) const;
		void markRead(const String &stamp);

		int checksum(int offset, int count, ByteStream &result) const;
	
	private:
		String filter(void) const;
		
		const MessageQueue *mMessageQueue;
		Identifier mPeering;
	};
	
	Selection select(const Identifier &peering) const;
	Selection selectAll(void) const;
	
private:
	User *mUser;
	Database *mDatabase;
	
	mutable bool mHasNew;
};

}

#endif

