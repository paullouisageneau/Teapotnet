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

#ifndef TPOT_ADDRESSBOOK_H
#define TPOT_ADDRESSBOOK_H

#include "include.h"
#include "http.h"
#include "interface.h"
#include "address.h"
#include "socket.h"
#include "identifier.h"
#include "core.h"
#include "message.h"
#include "array.h"
#include "map.h"

namespace tpot
{

class User;
  
class AddressBook : public Thread, public Synchronizable, public HttpInterfaceable
{
public:
	AddressBook(User *user);
	~AddressBook(void);
	
	User *user(void) const;
	String userName(void) const;
	int unreadMessagesCount(void) const;
	
	void load(Stream &stream);
	void save(Stream &stream) const;
	void save(void) const;
	void update(void);
	
	void http(const String &prefix, Http::Request &request);
	
	class Contact : protected Synchronizable, public Serializable, public HttpInterfaceable, public Core::Listener
	{
	public:
	  	Contact(	AddressBook *addressBook,
				const String &uname,
				const String &name,
	     			const String &tracker,
	     			const ByteString &secret);
		Contact(AddressBook *addressBook);
		~Contact(void);
	    	
		const String &uniqueName(void) const;
		const String &name(void) const;
		const String &tracker(void) const;
		const Identifier &peering(void) const;
		const Identifier &remotePeering(void) const;
		uint32_t peeringChecksum(void) const;
		String urlPrefix(void) const;
		int unreadMessagesCount(void) const;
		bool isFound(void) const;
		bool isConnected(void) const;
		String status(void) const;
		
		bool addAddress(const Address &addr, bool forceConnection = false);
		bool removeAddress(const Address &addr);
		
		void update(void);
		
		void message(Message *message);
		void request(Request *request);
		void http(const String &prefix, Http::Request &request);
		
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	private:
	  	void messageToHtml(Html &html, const Message &message, bool old = false) const;
	  
	  	AddressBook *mAddressBook;
		String mUniqueName, mName, mTracker;
		Identifier mPeering, mRemotePeering;
		ByteString mSecret;
		
		bool mFound;
		SerializableArray<Address> mAddrs;
		Deque<Message> mMessages;
		unsigned mMessagesCount;
	};
	
	const Identifier &addContact(String name, const ByteString &secret);
	void removeContact(const Identifier &peering);
	Contact *getContact(const Identifier &peering);
	const Contact *getContact(const Identifier &peering) const;
	void getContacts(Array<Contact *> &array);
	
private:
	static bool publish(const Identifier &remotePeering);
	static bool query(const Identifier &peering, const String &tracker, Array<Address> &addrs, bool alternate = false);
	
	void run(void);
	
	User *mUser;
	String mFileName;
	Map<Identifier, Contact*> mContacts;		// Sorted by peering
	Map<String, Contact*> mContactsByUniqueName;	// Sorted by unique name
};

}

#endif
