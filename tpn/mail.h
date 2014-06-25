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

#ifndef TPN_MAIL_H
#define TPN_MAIL_H

#include "tpn/include.h"
#include "tpn/serializable.h"
#include "tpn/notification.h"
#include "tpn/string.h"
#include "tpn/identifier.h"
#include "tpn/map.h"
#include "tpn/time.h"

namespace tpn
{

class User;
class MailQueue;
	
class Mail : public Serializable
{
public:
	Mail(const String &content = "");
	virtual ~Mail(void);

	Time time(void) const;
	String stamp(void) const;
	String parent(void) const;
	String author(void) const;
	String contact(void) const;
	bool isPublic(void) const;
	bool isIncoming(void) const;
	bool isRelayed(void) const;

	const String &content(void) const;
	const StringMap &headers(void) const;
	bool header(const String &name, String &value) const;
	String header(const String &name) const;

	// Covered by signature	
	void setContent(const String &content);
	void setParent(const String &stamp);
	void setPublic(bool ispublic);
	void setAuthor(const String &author);
	void setHeaders(const StringMap &headers);
	void setHeader(const String &name, const String &value);
	void setDefaultHeader(const String &name, const String &value);
	void removeHeader(const String &name);
	
	// Stamp and signature
	void writeSignature(User *user);
	bool checkStamp(void) const;
	bool checkSignature(User *user) const;

	// NOT covered by signature
	void setContact(const String &uname);	
	void setIncoming(bool incoming);
	void setRelayed(bool relayed);

	bool send(const Identifier &peering = Identifier::Null) const;
	bool recv(const Notification &notification);
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;
	
private:
	void computeAgregate(BinaryString &result) const;
	String computeStamp(void) const;
	String computeSignature(User *user) const;
	
	// Signed
	StringMap mHeaders;
        String mContent;
	String mAuthor;
	String mSignature;
	String mStamp;
        String mParent;
	Time mTime;
	bool mIsPublic;

	// Not signed
	String mContact;
	bool mIsIncoming;
	bool mIsRelayed;

	// Only used when MailQueue outputs to interface
	int64_t mNumber;
	bool	mIsRead;
	bool	mIsPassed;
	
	friend class Core;
};

bool operator <  (const Mail &m1, const Mail &m2);
bool operator >  (const Mail &m1, const Mail &m2);
bool operator == (const Mail &m1, const Mail &m2);
bool operator != (const Mail &m1, const Mail &m2);

}

#endif

