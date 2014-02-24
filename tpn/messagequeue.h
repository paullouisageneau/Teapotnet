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
	bool add(Message &message);
	bool get(const String &stamp, Message &result) const;
	bool getChilds(const String &stamp, List<Message> &result) const;
	void ack(const List<Message> &messages);
	void pass(const List<Message> &messages);
	void erase(const String &uname);

	void markReceived(const String &stamp, const String &uname);
	void markRead(const String &stamp);
	void markPassed(const String &stamp);
	void markDeleted(const String &stamp);

	bool isRead(const String &stamp) const;
	bool isPassed(const String &stamp) const;
	bool isDeleted(const String &stamp) const;
	
	void http(const String &prefix, Http::Request &request);
	
	class Selection
	{
	public:
		Selection(void);
		Selection(const MessageQueue *messageQueue, const String &uname, bool includePrivate, bool includePublic, bool includeOutgoing);
		Selection(const MessageQueue *messageQueue, const String &parent);
		~Selection(void);
		
		void setParentStamp(const String &stamp);
		bool setBaseStamp(const String &stamp);
		String baseStamp(void) const;
		void includePrivate(bool enabled);
		void includePublic(bool enabled);
		void includeOutgoing(bool enabled);
		
		int count(void) const;
		int unreadCount(void) const;
		bool contains(const String &stamp) const;
		
		bool getOffset(int offset, Message &result) const;
		bool getRange(int offset, int count, List<Message> &result) const;
		
		bool getLast(int count, List<Message> &result) const;
		bool getLast(int64_t nextNumber, int count, List<Message> &result) const;
	
		bool getUnread(List<Message> &result) const;
		bool getUnreadStamps(StringList &result) const;
		bool getPassedStamps(StringList &result, int count) const;
		
		void markRead(const String &stamp);	// used to enforce access rights based on selected messages
		
		int checksum(int offset, int count, Stream &result) const;
		// TODO: synchro flags
	
	private:
		String target(const String &columns) const;
		String table(void) const;
		String filter(void) const;
		void filterBind(Database::Statement &statement) const;
		
		const MessageQueue *mMessageQueue;
		String mContact;
		
		String mParentStamp;
		String mBaseStamp;
		Time mBaseTime;
		
		bool mIncludePrivate;
		bool mIncludePublic;
		bool mIncludeOutgoing;
	};
	
	Selection select(const String &uname = "") const;
	Selection selectPrivate(const String &uname = "") const;
	Selection selectPublic(const String &uname = "", bool includeOutgoing = true) const;
	Selection selectChilds(const String &parentStamp) const;
	
private:
	void setFlag(const String &stamp, const String &name, bool value = true);	// Warning: name is not escaped
	bool getFlag(const String &stamp, const String &name) const;
	
	User *mUser;
	Database *mDatabase;
	
	mutable bool mHasNew;
};

}

#endif

