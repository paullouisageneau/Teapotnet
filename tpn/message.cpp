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

#include "tpn/message.h"
#include "tpn/core.h"
#include "tpn/user.h"
#include "tpn/bytestring.h"
#include "tpn/sha512.h"
#include "tpn/yamlserializer.h"
#include "tpn/byteserializer.h"

namespace tpn
{

String Message::GenerateStamp(void)
{
	return String::random(4) + String::hexa(Time::Now().toUnixTime());
}

Message::Message(const String &content) :
	mTime(Time::Now()),
	mIsPublic(false),
	mIsIncoming(false),
	mIsRelayed(false),
	mIsRead(false),
	mNumber(0)
{
	if(!content.empty()) setContent(content);
}

Message::~Message(void)
{
  
}

Time Message::time(void) const
{
	return mTime; 
}

String Message::stamp(void) const
{
	return mStamp;
}

String Message::parent(void) const
{
	return mParent;
}

String Message::author(void) const
{
        return mAuthor;
}

String Message::contact(void) const
{
        return mContact;
}

bool Message::isPublic(void) const
{
	return mIsPublic;
}

bool Message::isIncoming(void) const
{
        return mIsIncoming;
}

bool Message::isRelayed(void) const
{
        return mIsRelayed;
}

bool Message::isRead(void) const
{
        return mIsRead;
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
	else return "";
}

void Message::setContent(const String &content)
{
	if(mStamp.empty()) mStamp = GenerateStamp();
	mContent = content;
	mContent.trim();	// TODO: YamlSerializer don't support leading spaces
}

void Message::setPublic(bool ispublic)
{
	mIsPublic = ispublic;
}

void Message::setContact(const String &uname)
{
	mContact = uname;
}

void Message::setParent(const String &stamp)
{
	mParent = stamp;
}

void Message::setHeaders(const StringMap &headers)
{
	mHeaders = headers;
}

void Message::setTime(const String &time)
{
	mTime = Time(time);
}

void Message::setAuthor(const String &author)
{
	mAuthor = author;
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

void Message::removeHeader(const String &name)
{
	mHeaders.erase(name);
}

void Message::writeSignature(User *user)
{
	if(mStamp.empty()) mStamp = GenerateStamp();
	mAuthor = user->name();
	mSignature = computeSignature(user);
}

bool Message::checkSignature(User *user) const
{
	return (mAuthor == user->name() && mSignature == computeSignature(user));
}

void Message::setIncoming(bool incoming)
{
	mIsIncoming = incoming;
}

void Message::setRelayed(bool relayed)
{
        mIsRelayed = relayed;
}

void Message::markRead(bool read) const
{
	mIsRead = read; 
}

bool Message::send(const Identifier &peering) const
{
	Assert(!mStamp.empty());
	
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
	YamlSerializer serializer(&tmp);
	serializer.input(*this);
	return true;
}

void Message::serialize(Serializer &s) const
{
	ConstSerializableWrapper<bool> isPublicWrapper(mIsPublic);
	ConstSerializableWrapper<bool> isIncomingWrapper(mIsIncoming);
	ConstSerializableWrapper<bool> isRelayedWrapper(mIsRelayed);
	ConstSerializableWrapper<bool> isReadWrapper(mIsRead);
	ConstSerializableWrapper<int64_t> numberWrapper(&mNumber);
	
	Serializer::ConstObjectMapping mapping;
	mapping["headers"] = &mHeaders;
        mapping["content"] = &mContent;
	mapping["author"] = &mAuthor;
	mapping["signature"] = &mSignature;
        mapping["stamp"] = &mStamp;
	mapping["parent"] = &mParent;
        mapping["time"] = &mTime;
	mapping["public"] = &isPublicWrapper;

	mapping["contact"] = &mContact;
        mapping["incoming"] = &isIncomingWrapper;
	mapping["relayed"] = &isRelayedWrapper;
	mapping["isread"] = &isReadWrapper;
	
	if(mNumber) mapping["number"] = &numberWrapper;
	
	s.outputObject(mapping);
}

bool Message::deserialize(Serializer &s)
{
	mStamp.clear();
	
	SerializableWrapper<bool> isPublicWrapper(&mIsPublic);
	SerializableWrapper<bool> isIncomingWrapper(&mIsIncoming);
	SerializableWrapper<bool> isRelayedWrapper(&mIsRelayed);
	SerializableWrapper<bool> isReadWrapper(&mIsRead);
	SerializableWrapper<int64_t> numberWrapper(&mNumber);
	
	Serializer::ObjectMapping mapping;
	mapping["headers"] = &mHeaders;
        mapping["content"] = &mContent;
	mapping["author"] = &mAuthor;
	mapping["signature"] = &mSignature;
	mapping["stamp"] = &mStamp;
	mapping["parent"] = &mParent;
	mapping["time"] = &mTime;
	mapping["public"] = &isPublicWrapper;
	
	mapping["contact"] = &mContact;
	mapping["incoming"] = &isIncomingWrapper;
	mapping["relayed"] = &isRelayedWrapper;
	mapping["isread"] = &isReadWrapper;
	
	mapping["number"] = &numberWrapper;
	
	bool success = s.inputObject(mapping);
	if(mStamp.empty()) throw InvalidData("Message without stamp");
	return success;
}

String Message::computeSignature(User *user) const
{
	Assert(user);
	
	// Note: contact, incoming, relayed and isread are NOT signed
	ByteString agregate;
	ByteSerializer serializer(&agregate);
	serializer.output(mHeaders);
        serializer.output(mContent);
        serializer.output(mAuthor);
	serializer.output(mStamp);
	serializer.output(mParent);
	serializer.output(int64_t(mTime.toUnixTime()));
	serializer.output(mIsPublic);

	ByteString signature;
	Sha512::AuthenticationCode(user->getSecretKey("message"), agregate, signature);
	signature.resize(16);
	
	return signature.toString();
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
