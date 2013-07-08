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

#ifndef TPN_MESSAGE_H
#define TPN_MESSAGE_H

#include "tpn/include.h"
#include "tpn/serializable.h"
#include "tpn/notification.h"
#include "tpn/string.h"
#include "tpn/identifier.h"
#include "tpn/map.h"
#include "tpn/time.h"

namespace tpn
{

class Message : public Serializable
{
public:
	Message(const String &content = "");
	virtual ~Message(void);

	Time time(void) const;
	Identifier peering(void) const;
	String stamp(void) const;
	String parent(void) const;
	bool isPublic(void) const;
	const String &content(void) const;
	const StringMap &headers(void) const;
	bool header(const String &name, String &value) const;
	String header(const String &name) const;
	
	void setPeering(const Identifier &peering);
	void setParent(const String &stamp);
	void setPublic(bool ispublic);
	void setContent(const String &content);
	void setHeaders(const StringMap &headers);
	void setHeader(const String &name, const String &value);
	void setDefaultHeader(const String &name, const String &value);
	
	bool toggleIncoming(void);
	bool isIncoming(void) const;

	bool isRead(void) const;
	void markRead(bool read = true) const;

	bool send(const Identifier &peering = Identifier::Null) const;
	bool recv(const Notification &notification);
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;
	
private:
	static String GenerateStamp(void);
	
	StringMap mHeaders;
        String mContent;

	Time mTime;
	ByteString mPeering;	// no instance information
	String mStamp;
	String mParent;
	bool mIsIncoming;
	bool mIsPublic;
	
	mutable bool mIsRead;
	
	friend class Core;
};

bool operator <  (const Message &m1, const Message &m2);
bool operator >  (const Message &m1, const Message &m2);
bool operator == (const Message &m1, const Message &m2);
bool operator != (const Message &m1, const Message &m2);

}

#endif

