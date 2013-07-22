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

#ifndef TPN_ADDRESSBOOK_H
#define TPN_ADDRESSBOOK_H

#include "tpn/include.h"
#include "tpn/http.h"
#include "tpn/interface.h"
#include "tpn/address.h"
#include "tpn/socket.h"
#include "tpn/identifier.h"
#include "tpn/messagequeue.h"
#include "tpn/core.h"
#include "tpn/scheduler.h"
#include "tpn/task.h"
#include "tpn/array.h"
#include "tpn/map.h"
#include "tpn/set.h"

namespace tpn
{

class User;
  
class AddressBook : public Thread, public Synchronizable, public HttpInterfaceable
{
public:
	AddressBook(User *user);
	~AddressBook(void);
	
	User *user(void) const;
	String userName(void) const;
	
	void clear(void);
	void load(Stream &stream);
	void save(Stream &stream) const;
	void save(void) const;
	void update(void);
	
	void sendNotification(const Notification &notification, const Identifier &receiver);
	void receiveNotification(const Notification &notification);
	
	void http(const String &prefix, Http::Request &request);
	
	typedef SerializableMap<Address, Time> AddressBlock;
	typedef SerializableMap<String, AddressBlock> AddressMap;
	
	class Contact : protected Synchronizable, public Serializable, public HttpInterfaceable, public Task, public Core::Listener
	{
	public:
	  	Contact(	AddressBook *addressBook,
				const String &uname,
				const String &name,
	     			const String &tracker,
	     			const ByteString &secret);
		Contact(AddressBook *addressBook);
		~Contact(void);

		String uniqueName(void) const;
		String name(void) const;
		String tracker(void) const;
		Identifier peering(void) const;
		Identifier remotePeering(void) const;
		Time time(void) const;
		uint32_t peeringChecksum(void) const;
		String urlPrefix(void) const;
		bool isSelf(void) const;
		bool isFound(void) const;
		bool isConnected(void) const;
		bool isConnected(const String &instance) const;
		bool isOnline(void) const;
		String status(void) const;
		AddressMap addresses(void) const;
		bool isDeleted(void) const;
		void setDeleted(void);
		void getInstancesNames(Array<String> &array);
		
		bool addAddresses(const AddressMap &map);
		
		// These functions return true if addr is successfully connected
		bool connectAddress(const Address &addr, const String &instance, bool save = true);
		bool connectAddresses(const AddressMap &map, bool save = true, bool shuffle = false);
		
		void update(bool alternate = false);

		void connected(const Identifier &peering, bool incoming);
		void disconnected(const Identifier &peering);
		void notification(Notification *notification);
		void request(Request *request);
		void http(const String &prefix, Http::Request &request);
		
		void copy(const Contact *contact);
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	private:
		void run(void);
		
		MessageQueue::Selection selectMessages(void) const;
		void sendMessages(const MessageQueue::Selection &selection, int offset, int count) const;
		void sendMessagesChecksum(const MessageQueue::Selection &selection, int offset, int count, bool recursion) const;
		void sendUnread(void) const;
		
	  	AddressBook *mAddressBook;
		String mUniqueName, mName, mTracker;
		Identifier mPeering, mRemotePeering;
		ByteString mSecret;
		Time mTime;
		bool mDeleted;
		
		bool mFound;
		AddressMap mAddrs;
		StringMap mInfo;
	};
	
	Identifier addContact(String name, const ByteString &secret);
	void removeContact(const Identifier &peering);
	Contact *getContact(const Identifier &peering);
	const Contact *getContact(const Identifier &peering) const;
	Contact *getContactByUniqueName(const String &uname);
	const Contact *getContactByUniqueName(const String &uname) const;
	void getContacts(Array<Contact *> &array);
	
	Identifier setSelf(const ByteString &secret);
	Contact *getSelf(void);
	const Contact *getSelf(void) const;
	
private:
	static const int MaxChecksumDistance;	// for message synchronization
	
	void registerContact(Contact *contact);
	void unregisterContact(Contact *contact);
	bool publish(const Identifier &remotePeering);
	bool query(const Identifier &peering, const String &tracker, AddressMap &output, bool alternate = false);	
	void run(void);
	
	User *mUser;
	String mFileName;
	Map<Identifier, Contact*> mContacts;			// Sorted by peering
	Map<String, Contact*> mContactsByUniqueName;		// Sorted by unique name
	Scheduler mScheduler;
	
	unsigned mUpdateCount;
	Set<String> mBogusTrackers;
};

}

#endif
