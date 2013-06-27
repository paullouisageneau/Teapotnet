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

#include "tpn/message.h"
#include "tpn/core.h"
#include "tpn/bytestring.h"
#include "tpn/sha512.h"
#include "tpn/yamlserializer.h"

namespace tpn
{

String Message::GenerateStamp(void)
{
	return String::random(4) + String::hexa(Time::Now().toUnixTime());
}

Message::Message(const String &content) :
	mContent(content),
	mTime(Time::Now()),
	mStamp(GenerateStamp()),
	mIsPublic(false),
	mIsIncoming(false),
	mIsRead(false)
{

}

Message::~Message(void)
{
  
}

Time Message::time(void) const
{
	return mTime; 
}

Identifier Message::peering(void) const
{
	return mPeering;
}

String Message::stamp(void) const
{
	return mStamp;
}

bool Message::isPublic(void) const
{
	return mIsPublic;
}

const String &Message::content(void) const
{
	return mContent;
}

const StringMap &Message::headers(void) const
{
	return mHeaders;
}

bool Message::header(const String &name, String &value) const
{
	return mHeaders.get(name, value);
}

String Message::header(const String &name) const
{
	String value;
	if(mHeaders.get(name, value)) return value;
	else return String();
}

void Message::setPeering(const Identifier &peering)
{
	mPeering = peering.getDigest();
}

void Message::setContent(const String &content)
{
	mContent = content;
}

void Message::setHeaders(const StringMap &headers)
{
	mHeaders = headers;
}

void Message::setHeader(const String &name, const String &value)
{
	mHeaders[name] = value; 
}

void Message::setDefaultHeader(const String &name, const String &value)
{
	if(!mHeaders.contains(name))
		mHeaders[name] = value;
}

bool Message::toggleIncoming(void)
{
	mIsIncoming = !mIsIncoming;
	return mIsIncoming;
}

bool Message::isIncoming(void) const
{
	return mIsIncoming;
}

bool Message::isRead(void) const
{
	return mIsRead;  
}

void Message::markRead(bool read) const
{
	mIsRead = read; 
}

bool Message::send(void) const
{
	String tmp;
	YamlSerializer serializer(&tmp);
	serializer.output(*this);
	
	Notification notification(tmp);
	notification.setParameter("type", "message");
	return notification.send();
}

bool Message::send(const Identifier &peering) const
{
	String tmp;
	YamlSerializer serializer(&tmp);
	serializer.output(*this);

	Notification notification(tmp);
	notification.setParameter("type", "message");
	return notification.send(peering);
}

bool Message::recv(const Notification &notification)
{
	String type;
	notification.parameter("type", type);
	if(!type.empty() && type != "message") return false;
	
	String tmp = notification.content();
	VAR(tmp);
	YamlSerializer serializer(&tmp);
	serializer.input(*this);
	VAR(mContent);
	return true;
}

void Message::serialize(Serializer &s) const
{
	ConstSerializableWrapper<bool> isPublicWrapper(mIsPublic);
	ConstSerializableWrapper<bool> isIncomingWrapper(mIsIncoming);
	ConstSerializableWrapper<bool> isReadWrapper(mIsRead);
	
	Serializer::ConstObjectMapping mapping;
	mapping["headers"] = &mHeaders;
        mapping["content"] = &mContent;
        mapping["peering"] = &mPeering;
        mapping["stamp"] = &mStamp;
	mapping["parent"] = &mParent;
        mapping["time"] = &mTime;
	mapping["public"] = &isPublicWrapper;
        mapping["incoming"] = &isIncomingWrapper;
	mapping["isread"] = &isReadWrapper;
	
	s.outputObject(mapping);
}

bool Message::deserialize(Serializer &s)
{
	mHeaders.clear();
        mContent.clear();
	mStamp.clear();
	mPeering = Identifier::Null;
	mTime = Time::Now();
	mIsPublic = false;
	mIsIncoming = false;
	mIsRead = false;
	
	SerializableWrapper<bool> isPublicWrapper(&mIsPublic);
	SerializableWrapper<bool> isIncomingWrapper(&mIsIncoming);
	SerializableWrapper<bool> isReadWrapper(&mIsRead);
	
	Serializer::ObjectMapping mapping;
	mapping["headers"] = &mHeaders;
        mapping["content"] = &mContent;
	mapping["peering"] = &mPeering;
	mapping["stamp"] = &mStamp;
	mapping["parent"] = &mParent;
	mapping["time"] = &mTime;
	mapping["public"] = &isPublicWrapper;
	mapping["incoming"] = &isIncomingWrapper;
	mapping["isread"] = &isReadWrapper;
	
	return s.inputObject(mapping);
}

bool Message::isInlineSerializable(void) const
{
	return false;
}

bool operator < (const Message &m1, const Message &m2)
{
	return m1.time() < m2.time();
}

bool operator > (const Message &m1, const Message &m2)
{
	return m1.time() > m2.time();
}

bool operator == (const Message &m1, const Message &m2)
{
	return ((m1.time() != m2.time())
		&& (m1.content() != m2.content()));   
}

bool operator != (const Message &m1, const Message &m2)
{
	return !(m1 == m2);
}

}
