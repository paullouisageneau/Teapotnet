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

#ifndef TPN_PROFILE_H
#define TPN_PROFILE_H

#include "tpn/include.h"
#include "tpn/interface.h"
#include "tpn/identifier.h"
#include "tpn/html.h"

#include "pla/synchronizable.h"
#include "pla/serializable.h"
#include "pla/string.h"
#include "pla/binarystring.h"

namespace tpn
{

class User;
	
class Profile : public Synchronizable, public Serializable, public HttpInterfaceable
{
public:
	Profile(User *user, const String &uname = "");
	~Profile(void);
	
	void load(void);
	void save(void);
	void clear(void);
	void send(const Identifier &identifier);
	
	String name(void) const;
	String realName(void) const;
	String eMail(void) const;
	String urlPrefix(void) const;
	String avatarUrl(void) const;
	BinaryString avatar(void) const;	
	Time time(void) const;
	bool isSelf(void) const;
	
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
		void serialize(Serializer &s) const	{ static_cast<const Serializable*>(mPtr)->serialize(s); }
		bool deserialize(Serializer &s)		{ return static_cast<Serializable*>(mPtr)->deserialize(s);  }
		void serialize(Stream &s) const     	{ static_cast<const Serializable*>(mPtr)->serialize(s); }
                bool deserialize(Stream &s)         	{ return static_cast<Serializable*>(mPtr)->deserialize(s);  }
		String toString(void) const		{ return mPtr->toString(); }	
		void fromString(String str)		{ mPtr->fromString(str); }
		bool isInlineSerializable(void) const	{ return mPtr->isInlineSerializable(); }
		bool isNativeSerializable(void) const	{ return mPtr->isNativeSerializable(); }
	
	private:
		T *mPtr;
		T mDefault;
	};

	bool displayField(Html &page, const String &name, bool forceDisplay = false) const;
	bool displayField(Html &page, const Field *field, bool forceDisplay = false) const;
	bool updateField(const String &name, const String &value);
	
	User *mUser;
	String mName;
	String mFileName;
	Time mTime;	

	Map<String, Field*> mFields;
	
	// Status
	String mStatus;
	
	// Avatar
	BinaryString mAvatar;
	
	// Basic Info
	String mRealName;
	Time mBirthday;
	String mGender;
	String mRelationship;
	
	// Contact
	String mLocation;
	String mEmail;
	String mPhone;
	
	// TODO
	/*String mDescription;
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

}

#endif
