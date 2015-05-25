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

#ifndef TPN_ADDRESSBOOK_H
#define TPN_ADDRESSBOOK_H

#include "tpn/include.h"
#include "tpn/user.h"
#include "tpn/interface.h"
#include "tpn/network.h"
#include "tpn/board.h"
#include "tpn/mail.h"

#include "pla/synchronizable.h"
#include "pla/serializable.h"
#include "pla/http.h"
#include "pla/address.h"
#include "pla/socket.h"
#include "pla/crypto.h"
#include "pla/scheduler.h"
#include "pla/task.h"
#include "pla/array.h"
#include "pla/map.h"
#include "pla/set.h"

namespace tpn
{

class User;

class AddressBook : private Task, private Synchronizable, public Serializable, public HttpInterfaceable
{
public:
	AddressBook(User *user);
	~AddressBook(void);
	
	User *user(void) const;
	String userName(void) const;
	String urlPrefix(void) const;
	
	void clear(void);
	void save(void) const;
	
	void update(void);
	bool send(const Notification &notification);
	bool send(const Mail &mail);
	
	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isInlineSerializable(void) const;
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);
	
	class Invitation : public Serializable, public Network::Listener
	{
	public:
		Invitation(void);
		Invitation(const Invitation &invitation);
		Invitation(AddressBook *addressBook, const Identifier &identifier);
		~Invitation(void);
		
		void setAddressBook(AddressBook *addressBook);
		void init(void);
		
		Identifier identifier(void) const;
		
		void setSelf(bool self = true);
		bool isSelf(void) const;
		
		bool isFound(void) const;
		
		// Listener
		void seen(const Identifier &peer);
		void connected(const Identifier &peer);
		bool recv(const Identifier &peer, const Notification &notification);
		bool auth(const Identifier &peer, const Rsa::PublicKey &pubKey);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	protected:
		AddressBook *mAddressBook;
		Identifier mIdentifier;
		bool mIsSelf;
		bool mFound;
		
		friend class AddressBook;
	};
	
	class Contact : private Task, public Serializable, public Network::Listener, public HttpInterfaceable
	{
	public:
		Contact(void);
		Contact(const Contact &contact);
		Contact(	AddressBook *addressBook,
				const String &uname,
				const String &name,
				const Rsa::PublicKey &pubKey);
		~Contact(void);
		
		void setAddressBook(AddressBook *addressBook);
		void init(void);
		
		const Rsa::PublicKey &publicKey(void) const;
		Identifier identifier(void) const;
		String uniqueName(void) const;
		String name(void) const;
		String urlPrefix(void) const;
		BinaryString secret(void) const;
		
		bool isSelf(void) const;
		bool isConnected(void) const;
		bool isConnected(const Identifier &instance) const;
		
		bool hasInstance(const Identifier &instance) const;
		int  getInstances(Set<Identifier> &result) const;
		bool getInstanceAddresses(const Identifier &instance, Set<Address> &result) const;
		
		bool send(const Notification &notification);
		bool send(const Mail &mail);
		
		// Network::Listener
		void seen(const Identifier &peer);
		void connected(const Identifier &peer);
		bool recv(const Identifier &peer, const Notification &notification);
		bool auth(const Identifier &peer, const Rsa::PublicKey &pubKey);
		
		// HttpInterfaceable
		void http(const String &prefix, Http::Request &request);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	private:
		BinaryString localSecret(void) const;
		BinaryString remoteSecret(void) const;
		void run(void);
	  
		class Instance : public Serializable
		{
		public:
			Instance(void);
			Instance(const Identifier &id);
			~Instance();
			
			Identifier &identifier(void) const;
			String name(void) const;
			void setName(const String &name);
			Time lastSeen(void) const;
			void setSeen(void);
			
			void addAddress(const Address &addr);
			void addAddresses(const Set<Address> &addrs);
			int  getAddresses(Set<Address> &result) const;
			
			// Serializable
			void serialize(Serializer &s) const;
			bool deserialize(Serializer &s);
			bool isInlineSerializable(void) const;
			
		private:
			Identifier mIdentifier;
			uint64_t mNumber;
			String mName;
			
			typedef SerializableMap<Address, Time> AddressBlock;
			AddressBlock mAddrs;
			
			Time mLastSeen;
		};
	  
		AddressBook *mAddressBook;
		Board *mBoard, *mPrivateBoard;
		
		String mUniqueName, mName;
		Rsa::PublicKey mPublicKey;
		BinaryString mRemoteSecret;
		
		typedef SerializableMap<Identifier, Instance> InstancesMap;
		InstancesMap mInstances;
		
		friend class AddressBook;
	};
	
	String addContact(const String &name, const Rsa::PublicKey &pubKey);	// returns uname
	bool removeContact(const String &uname);
	Contact *getContact(const String &uname);
	const Contact *getContact(const String &uname) const;
	int getContacts(Array<AddressBook::Contact*> &result);
	int getContactsIdentifiers(Array<Identifier> &result) const;
	
	void setSelf(const Rsa::PublicKey &pubKey);
	Contact *getSelf(void);
	const Contact *getSelf(void) const;
	Identifier getSelfIdentifier(void) const;
	
	bool hasIdentifier(const Identifier &identifier) const;
	
private:
	void run(void);
	
	User *mUser;
	String mFileName;
	SerializableMap<String, Contact> mContacts;	// Sorted by unique name
	SerializableMap<Identifier, Contact*> mContactsByIdentifier;
	SerializableArray<Invitation> mInvitations;
	Scheduler mScheduler;
	
	mutable BinaryString mDigest;
};

}

#endif
