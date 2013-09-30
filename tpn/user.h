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
	void sendInfo(Identifier identifier = Identifier::Null);
	
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
		
	private:
		String infoPath(void) const;		

		class Field : public Serializable
		{
		public:
			Field(const String &name, const String &displayName = "", const String &comment = "") :
				mName(name), mDisplayName(displayName), mComment(comment) {}
			virtual ~Field(void) {}

			virtual String name(void) const { return mName; }
			virtual String displayName(void) const { return (!mDisplayName.empty() ? mDisplayName : mName.capitalized()); }
			virtual String comment(void) const { return (!mComment.empty() ? mComment : displayName()); }
			virtual String value(void) const { return toString(); }

			virtual bool empty(void) const	= 0;
			virtual void clear(void)	= 0;
			
		protected:
			String mName;
			String mDisplayName;
			String mComment;
		};

		template<class T> class TypedField : public Field
		{
		public:
			TypedField(const String &name, T *ptr, const String &displayName = "", const String &comment = "", const T &value = T()) : 
				Field(name, displayName, comment), mPtr(ptr), mDefault(value) { clear(); }
			~TypedField(void) {}
			
			bool empty(void) const	{ return (*mPtr == mDefault); }
			void clear(void)	{ *mPtr = mDefault; }
			
			// Serializable
			void serialize(Serializer &s) const	{ s.output(*mPtr); }
			bool deserialize(Serializer &s)		{ s.input(*mPtr);  }
		
		private:
			T *mPtr;
			T mDefault;
		};

		void displayField(Html &page, const String &name) const;
		void displayField(Html &page, const Field *field) const;
		void updateField(const String &name, const String &value);
		
		User *mUser;
		String mName;
		String mFileName;
		
		Map<String, Field*> mFields;
		
		// Avatar
		ByteString mAvatar;
		
		// Basic Info
		String mRealName;
		Time mBirthday;
		String mGender;
		String mRelationship;
		
		// Contact
		String mLocation;
		String mEmail;
		String mPhone;
		
		// Settings
		String mTracker;
		
		// TODO
		/*String mDescription;
		String mStatus;
		String mCity;
		String mCollege;
		String mUniversity;
		String mJob;
		String mBooks;
		String mHobbies;
		String mMovies;
		String mPolitics;
		String mInternship;
		String mComputer;
		String mResume;*/
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
