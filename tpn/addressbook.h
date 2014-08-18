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
#include "tpn/synchronizable.h"
#include "tpn/serializable.h"
#include "tpn/interface.h"
#include "tpn/http.h"
#include "tpn/core.h"
#include "tpn/address.h"
#include "tpn/socket.h"
#include "tpn/identifier.h"
#include "tpn/crypto.h"
#include "tpn/user.h"
#include "tpn/profile.h"
#include "tpn/mailqueue.h"
#include "tpn/scheduler.h"
#include "tpn/task.h"
#include "tpn/array.h"
#include "tpn/map.h"
#include "tpn/set.h"

namespace tpn
{

class User;
class Profile;
  
class AddressBook : private Synchronizable, public Serializable, public HttpInterfaceable, public Core::Listener
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
	
	class Contact : public Serializable, public Core::Listener, public HttpInterfaceable, public Task
	{
	public:
	  	Contact(	AddressBook *addressBook,
				const String &uname,
				const String &name,
				const String &tracker,
				const String &secret);
		Contact(AddressBook *addressBook);
		~Contact(void);

		String uniqueName(void) const;
		String name(void) const;
		String tracker(void) const;
		Identifier peering(void) const;
		Time time(void) const;
		uint32_t peerChecksum(void) const;
		String urlPrefix(void) const;
		
		Profile *profile(void) const;

		bool isSelf(void) const;
		bool isFound(void) const;
		bool isConnected(void) const;
		bool isConnected(const String &instance) const;
		bool isOnline(void) const;
		String status(void) const;
		int getIdentifiers(Array<Identifier> &result) const;
		int getInstancesNames(Array<String> &array) const;
		
		// These functions return true if addr is successfully connected
		bool connectAddress(const Address &addr, const String &instance, bool save = true);
		bool connectAddresses(const AddressMap &map, bool save = true, bool shuffle = false);
		
		void update(bool alternate = false);
		void createProfile(void);
		
		// TODO
		void connected(const Identifier &peer, bool incoming);
		void disconnected(const Identifier &peer);
		
		bool send(const Notification &notification);
		bool send(const Mail &mail);
		void copy(const Contact *contact);
		
		// Core::Listener
		void seen(const Identifier &peer);
		bool recv(const Identifier &peer, const Notification &notification);
		
		// HttpInterfaceable
		void http(const String &prefix, Http::Request &request);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	private:
		class Instance : public Serializable
		{
			Instance(void);
			Instance(const Rsa::PublicKey &key);
			~Instance();
			
			const Rsa::PublicKey &publicKey(void) const;
			Identifier identifier(void) const;
			String name(void) const;
			void setName(const String &name);
			
			void addAddress(const Address &addr);
			void addAddresses(const List<Address> &addrs);
			
			// TODO: addresses access
			
			// Serializable
			void serialize(Serializer &s) const;
			bool deserialize(Serializer &s);
			bool isInlineSerializable(void) const;
			
		private:
			Rsa::PublicKey mPublicKey;
			String mName;
			
			typedef SerializableMap<Address, Time> AddressBlock;
			AddressBlock mAddrs;
		};
		
		BinaryString secret(void) const;
		void run(void);
		
		MailQueue::Selection selectMails(bool privateOnly = false) const;
		void sendMails(const Instance &instance, const MailQueue::Selection &selection, int offset, int count) const;
		void sendMailsChecksum(const Instance &instance, const MailQueue::Selection &selection, int offset, int count, bool recursion) const;
		void sendUnread(const Instance &instance) const;
		void sendPassed(const Instance &instance) const;
		
		AddressBook *mAddressBook;
		Profile *mProfile;
		String mUniqueName, mName, mTracker;
		Identifier mPeering;
		BinaryString mSecret;
		Time mTime;
		
		typedef SerializableMap<Identifier, Instance> InstancesMap;
		InstancesMap mInstances;
		
		// TODO
		bool mFound;
		Set<String> mExcludedInstances;
		Set<String> mOnlineInstances;
	};
	
	Identifier addContact(String name, const String &secret);
	void removeContact(const Identifier &id);
	Contact *getContact(const Identifier &id);
	const Contact *getContact(const Identifier &id) const;
	Contact *getContactByUniqueName(const String &uname);
	const Contact *getContactByUniqueName(const String &uname) const;
	void getContacts(Array<Contact *> &array);
	
	Identifier setSelf(const String &secret);
	Contact *getSelf(void);
	const Contact *getSelf(void) const;
	
private:
	static const double StartupDelay;	// time before starting connections (useful for UPnP)
	static const double UpdateInterval;	// for contacts connection attemps
	static const double UpdateStep;		// between contacts
	static const int MaxChecksumDistance;	// for mail synchronization
	
	void registerContact(Contact *contact, int ordinal = 0);
	void unregisterContact(Contact *contact);
	bool publish(const Identifier &remotePeering);
	bool query(const Identifier &peer, const String &tracker, AddressMap &output, bool alternate = false);	
	
	User *mUser;
	String mUserName;
	String mFileName;
	SerializableMap<Identifier, Contact> mContacts;
	SerializableMap<String, Contact*> mContactsByUniqueName;	// Sorted by unique name
	Scheduler mScheduler;
	
	Set<String> mBogusTrackers;
};

}

#endif
