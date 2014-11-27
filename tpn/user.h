/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef TPN_USER_H
#define TPN_USER_H

#include "tpn/include.h"
#include "tpn/interface.h"
#include "tpn/identifier.h"
#include "tpn/indexer.h"
#include "tpn/profile.h"
#include "tpn/board.h"

#include "pla/synchronizable.h"
#include "pla/serializable.h"
#include "pla/securetransport.h"
#include "pla/crypto.h"
#include "pla/thread.h"
#include "pla/task.h"
#include "pla/mutex.h"
#include "pla/map.h"

namespace tpn
{

class AddressBook;
  
class User : protected Synchronizable, public Serializable, public HttpInterfaceable
{
public:
	static unsigned Count(void);
	static void GetNames(Array<String> &array);
  	static bool Exist(const String &name);
	static User *Get(const String &name);
	static User *Authenticate(const String &name, const String &password);
	static void UpdateAll(void);
	
	User(const String &name, const String &password = "", const String &tracker = "");
	~User(void);
	
	void load(void);
	void save(void) const;
	
	String name(void) const;
	String tracker(void) const;
	String profilePath(void) const;
	String urlPrefix(void) const;
	
	void setTracker(const String &tracker);
	
	AddressBook *addressBook(void) const;
	Board *board(void) const;
	Indexer *indexer(void) const;
	Profile *profile(void) const;
	
	bool isOnline(void) const;
	void setOnline(void);
	void setOffline(void);
	void sendStatus(const Identifier &identifier = Identifier::Null);
	void sendSecret(const Identifier &identifier);
	
	BinaryString getSecretKey(const String &action) const;
	
	String generateToken(const String &action = "") const;
	bool checkToken(const String &token, const String &action = "") const;
	
	Identifier identifier(void) const;
	const Rsa::PublicKey &publicKey(void) const;
	const Rsa::PrivateKey &privateKey(void) const;
	SecureTransport::Certificate *certificate(void) const;
	
	void setKeyPair(const Rsa::PublicKey &publicKey, const Rsa::PrivateKey &privateKey);
	
	void http(const String &prefix, Http::Request &request);

	void serialize(Serializer &s) const;
	bool deserialize(Serializer &s);
	bool isInlineSerializable(void) const;
	
private:
	String mName;
	String mFileName;
	BinaryString mAuth;
	AddressBook *mAddressBook;
	Board *mBoard;
	Indexer *mIndexer;
	Profile *mProfile;
	
	Rsa::PublicKey	mPublicKey;
	Rsa::PrivateKey	mPrivateKey;
	BinaryString mSecret;
	SecureTransport::Certificate *mCertificate;
	
	bool mOnline;
	BinaryString mTokenSecret;
	mutable Map<String, BinaryString> mSecretKeysCache;
	
	class SetOfflineTask : public Task
	{
	public:
		SetOfflineTask(User *user)	{ this->user = user; }
		void run(void)			{ user->setOffline(); } 
	private:
		User *user;
	};
	
	SetOfflineTask mSetOfflineTask;
	
	static Map<String, User*>	UsersByName;
	static Map<BinaryString, User*>	UsersByAuth;
	static Mutex			UsersMutex;
};

}

#endif
