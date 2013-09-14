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

#ifndef TPN_USER_H
#define TPN_USER_H

#include "tpn/include.h"
#include "tpn/thread.h"
#include "tpn/http.h"
#include "tpn/interface.h"
#include "tpn/identifier.h"
#include "tpn/addressbook.h"
#include "tpn/messagequeue.h"
#include "tpn/store.h"
#include "tpn/mutex.h"
#include "tpn/map.h"
#include "tpn/yamlserializer.h"
#include "tpn/html.h"
#include "resource.h"

namespace tpn
{

class User : public Thread, protected Synchronizable, public HttpInterfaceable
{
public:

	static unsigned Count(void);
	static void GetNames(Array<String> &array);
  	static bool Exist(const String &name);
	static User *Get(const String &name);
	static User *Authenticate(const String &name, const String &password);
	static void UpdateAll(void);
	
	User(const String &name, const String &password = "");
	~User(void);
	
	const String &name(void) const;
	String profilePath(void) const;
	String urlPrefix(void) const;
	AddressBook *addressBook(void) const;
	MessageQueue *messageQueue(void) const;
	Store *store(void) const;
	
	bool isOnline(void) const;
	void setOnline(void);
	void setInfo(const StringMap &info);
	void sendInfo(const Identifier &identifier = Identifier::Null);
	
	void http(const String &prefix, Http::Request &request);

	class Profile : public Serializable, public HttpInterfaceable
	{
		public:
			Profile(User *user, const String &uname = "");
			~Profile(void);
			
			String urlPrefix(void) const;
			
			void load(void);
			void save(void);
			void clear(void);
			
			// Serializable
			void serialize(Serializer &s) const;
			bool deserialize(Serializer &s);
			
			// HttpInterfaceable
			void http(const String &prefix, Http::Request &request);

			//Getters
			String getStatus(void);
			String getProfilePhoto(void);
			
		private:
			String infoPath(void) const;
			
			void displayProfileInfo(Html &page, const String &fieldText, const String &fieldName, String &field);
			void displayProfileInfo(Html &page, const String &fieldText, const String &showPunctuation, const String &emptyQuestion, const String &fieldName, String &field);
			void updateField(String &key, String &value);
			
			User *mUser;
			String mName;
			String mFileName;
			
			String mFirstName;
			String mMiddleName;
			String mLastName;
			String mBirthday; // TODO : Make it a Time
			String mGender;
			String mReligion;
			String mRelationship;
			String mDescription;
			String mStatus;

			String mCity;
			String mAddress;
			String mMail;
			String mPhone;
			String mCollege;
			String mUniversity;
			String mJob;
			String mProfilePhoto;
			String mBooks;
			String mHobbies;
			String mMovies;
			String mPolitics;
			String mInternship;
			String mComputer;
			String mResume;
	
	};

	Profile *profile(void) const;
	
private:
	void run(void);
	
	String mName;
	ByteString mHash;
	AddressBook *mAddressBook;
	MessageQueue *mMessageQueue;
	Store *mStore;
	StringMap mInfo;
	Time mLastOnlineTime;

	Profile *mProfile;

	static Map<String, User*>	UsersByName;
	static Map<ByteString, User*>	UsersByAuth;
	static Mutex			UsersMutex;

};

}

#endif
