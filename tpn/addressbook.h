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
#include "tpn/core.h"
#include "tpn/identifier.h"
#include "tpn/profile.h"
#include "tpn/mailqueue.h"

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
class Profile;
  
class AddressBook : private Task, private Synchronizable, public Serializable, public HttpInterfaceable
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
	
	void sendContacts(const Identifier &peer) const;
	void sendContacts(void) const;
	
	void update(void);
	bool send(const Notification &notification);
	bool send(const Mail &mail);
	
	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isInlineSerializable(void) const;
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);
	
	class Invitation : private Task, public Serializable, public Core::Listener
	{
	public:
		Invitation(void);
		Invitation(const Invitation &invitation);
		Invitation(	AddressBook *addressBook,
				const Identifier &identifier,
				const String &tracker = "");
		Invitation(	AddressBook *addressBook,
				const String &code,
				uint64_t pin,
				const String &tracker = "");
		Invitation(	AddressBook *addressBook,
				const String &name,
				const String &secret,
				const String &tracker = "");
		~Invitation(void);
		
		void setAddressBook(AddressBook *addressBook);
		
		String name(void) const;
		BinaryString secret(void) const;
		Identifier peering(void) const;
		String tracker(void) const;
		uint32_t checksum(void) const;
		
		bool isFound(void) const;

		// Listener
		void seen(const Identifier &peer);
		void connected(const Identifier &peer);
		bool recv(const Identifier &peer, const Notification &notification);
		bool auth(const Identifier &peer, BinaryString &secret);
		bool auth(const Identifier &peer, const Rsa::PublicKey &pubKey);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	protected:
		void generate(const String &salt, const String &secret);
		void run(void);
		
		AddressBook *mAddressBook;
		String mName;
		BinaryString mSecret;
		Identifier mPeering;
		String mTracker;
		bool mFound;
		
		friend class AddressBook;
	};
	
	class Contact : private Task, public Serializable, public Core::Listener, public HttpInterfaceable
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
		
		const Rsa::PublicKey &publicKey(void) const;
		Identifier identifier(void) const;
		String uniqueName(void) const;
		String name(void) const;
		String urlPrefix(void) const;
		Profile *profile(void) const;
		
		bool isSelf(void) const;
		bool isConnected(void) const;
		bool isConnected(uint64_t number) const;
		
		int  getInstanceNumbers(Array<uint64_t> &result) const;
		int  getInstanceIdentifiers(Array<Identifier> &result) const;
		bool getInstanceIdentifier(uint64_t number, Identifier &result) const;
		bool getInstanceName(uint64_t number, String &result) const;
		bool getInstanceAddresses(uint64_t number, Set<Address> &result) const;
		
		bool send(const Notification &notification);
		bool send(const Mail &mail);
		
		// Core::Listener
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
		void run(void);
	  
		class Instance : public Serializable
		{
		public:
			Instance(void);
			Instance(uint64_t number);
			~Instance();
			
			uint64_t number(void) const;
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
			uint64_t mNumber;
			String mName;
			
			typedef SerializableMap<Address, Time> AddressBlock;
			AddressBlock mAddrs;
			
			Time mLastSeen;
		};
	  
		AddressBook *mAddressBook;
		Profile *mProfile;
		
		String mUniqueName, mName;
		Rsa::PublicKey mPublicKey;

		typedef SerializableMap<uint64_t, Instance> InstancesMap;
		InstancesMap mInstances;
		
		friend class AddressBook;
	};
	
	String addContact(const String &name, const Rsa::PublicKey &pubKey);	// returns uname
	bool removeContact(const String &uname);
	Contact *getContact(const String &uname);
	const Contact *getContact(const String &uname) const;
	void getContacts(Array<AddressBook::Contact*> &result);
	
	void setSelf(const Rsa::PublicKey &pubKey);
	Contact *getSelf(void);
	const Contact *getSelf(void) const;
	
private:
	bool publish(const Identifier &identifier, const String &tracker);
	bool query(const Identifier &identifier, const String &tracker, Serializable &result);	
	void run(void);
	
	// Pin
	static uint64_t PinGenerate(void);
	static uint64_t PinChecksum(uint64_t pin);
	static bool PinIsValid(uint64_t pin);
	
	User *mUser;
	String mFileName;
	SerializableMap<String, Contact> mContacts;	// Sorted by unique name
	SerializableMap<Identifier, Contact*> mContactsByIdentifier;
	SerializableArray<Invitation> mInvitations;
	Scheduler mScheduler;
};

}

#endif
