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
#include "pla/scheduler.h"
#include "pla/task.h"
#include "pla/array.h"
#include "pla/map.h"
#include "pla/set.h"

namespace tpn
{

class User;

class AddressBook : protected Synchronizable, public Serializable, public HttpInterfaceable
{
public:
	AddressBook(User *user);
	~AddressBook(void);
	
	User *user(void) const;
	String userName(void) const;
	String urlPrefix(void) const;
	
	void clear(void);
	void save(void) const;
	
	bool send(const String &type, const Serializable &object);
	
	// Serializable
	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isInlineSerializable(void) const;
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);
	
	class Contact : public Serializable, public Network::Listener, public HttpInterfaceable
	{
	public:
		Contact(void);
		Contact(const Contact &contact);
		Contact(	AddressBook *addressBook,
				const String &uname,
				const String &name,
				const Identifier &identifier);
		~Contact(void);
		
		void setAddressBook(AddressBook *addressBook);
		void init(void);
		
		Identifier identifier(void) const;
		String uniqueName(void) const;
		String name(void) const;
		String urlPrefix(void) const;
		BinaryString secret(void) const;
		
		bool isSelf(void) const;
		bool isConnected(void) const;
		bool isConnected(const Identifier &instance) const;
		
		bool send(const String &type, const Serializable &object);
		bool send(const Identifier &instance, const String &type, const Serializable &object);
	
		// Network::Listener
		void seen(const Network::Link &link);
		void connected(const Network::Link &link, bool status);
		bool recv(const Network::Link &link, const String &type, Serializer &serializer);
		bool auth(const Network::Link &link, const Rsa::PublicKey &pubKey);
		
		// HttpInterfaceable
		void http(const String &prefix, Http::Request &request);
		
		// Serializable
		void serialize(Serializer &s) const;
		bool deserialize(Serializer &s);
		bool isInlineSerializable(void) const;
		
	private:
		BinaryString localSecret(void) const;
		BinaryString remoteSecret(void) const;
	  
		AddressBook *mAddressBook;
		Board *mBoard, *mPrivateBoard;
		
		String mUniqueName, mName;
		Identifier mIdentifier;
		BinaryString mRemoteSecret;
		
		SerializableMap<Identifier, String> mInstances;
		
		friend class AddressBook;
	};
	
	String addContact(const String &name, const Identifier &identifier);	// returns uname
	bool removeContact(const String &uname);
	Contact *getContact(const String &uname);
	const Contact *getContact(const String &uname) const;
	int getContacts(Array<AddressBook::Contact*> &result);
	int getContactsIdentifiers(Array<Identifier> &result) const;
	bool hasIdentifier(const Identifier &identifier) const;
	
	void setSelf(const Identifier &identifier);
	Contact *getSelf(void);
	const Contact *getSelf(void) const;
	
	void addInvitation(const Identifier &remote, const String &name);
	
private:
	BinaryString digest(void) const;
	
	User *mUser;
	String mFileName;
	SerializableMap<String, Contact> mContacts;	// Sorted by unique name
	SerializableMap<Identifier, Contact*> mContactsByIdentifier;
	Map<Identifier, String> mInvitations;
	
	Scheduler mScheduler;
	
	mutable BinaryString mDigest;
};

}

#endif
