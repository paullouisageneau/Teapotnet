/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#include "tpn/include.hpp"
#include "tpn/user.hpp"
#include "tpn/interface.hpp"
#include "tpn/network.hpp"
#include "tpn/board.hpp"
#include "tpn/mail.hpp"

#include "pla/serializable.hpp"
#include "pla/http.hpp"
#include "pla/address.hpp"
#include "pla/socket.hpp"
#include "pla/scheduler.hpp"
#include "pla/array.hpp"
#include "pla/map.hpp"
#include "pla/set.hpp"

namespace tpn
{

class User;

class AddressBook : public Serializable, public HttpInterfaceable
{
public:
	AddressBook(User *user);
	~AddressBook(void);
	
	User *user(void) const;
	String userName(void) const;
	String urlPrefix(void) const;
	
	void clear(void);
	void save(void) const;
	
	bool send(const String &type, const Serializable &object) const;
	
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
		
		Identifier identifier(void) const;
		String uniqueName(void) const;
		String name(void) const;
		String urlPrefix(void) const;
		BinaryString secret(void) const;
		
		bool isSelf(void) const;
		bool isConnected(void) const;
		bool isConnected(const Identifier &instance) const;
		
		bool send(const String &type, const Serializable &object) const;
		bool send(const Identifier &instance, const String &type, const Serializable &object) const;
	
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
		void init(void);
		void uninit(void);
		
		BinaryString localSecret(void) const;
		BinaryString remoteSecret(void) const;
	  
		AddressBook *mAddressBook;
		sptr<Board> mBoard, mPrivateBoard;
		
		String mUniqueName, mName;
		Identifier mIdentifier;
		BinaryString mRemoteSecret;
		
		Map<Identifier, String> mInstances;
		
		mutable std::mutex mMutex;
		
		friend class AddressBook;
	};
	
	String addContact(const String &name, const Identifier &identifier);	// returns uname
	bool removeContact(const String &uname);
	sptr<Contact> getContact(const String &uname);
	sptr<const Contact> getContact(const String &uname) const;
	int getContacts(std::vector<sptr<AddressBook::Contact> > &result);
	int getContactsIdentifiers(Array<Identifier> &result) const;
	bool hasIdentifier(const Identifier &identifier) const;
	
	void setSelf(const Identifier &identifier);
	sptr<Contact> getSelf(void);
	sptr<const Contact> getSelf(void) const;
	
	void addInvitation(const Identifier &remote, const String &name);
	
private:
	Time time(void) const;
	BinaryString digest(void) const;
	
	User *mUser;
	String mFileName;
	Map<String, sptr<Contact> > mContacts;	// Sorted by unique name
	Map<Identifier, sptr<Contact> > mContactsByIdentifier;
	Map<Identifier, String> mInvitations;
	Time mTime;	// modification time
		
	Scheduler mScheduler;
	
	mutable std::mutex mMutex;
	mutable BinaryString mDigest;
};

}

#endif
