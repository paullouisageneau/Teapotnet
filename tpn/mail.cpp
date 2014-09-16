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

#include "tpn/mail.h"
#include "tpn/mailqueue.h"
#include "tpn/core.h"
#include "tpn/user.h"

#include "pla/binarystring.h"
#include "pla/yamlserializer.h"
#include "pla/binaryserializer.h"
#include "pla/crypto.h"

namespace tpn
{

Mail::Mail(const String &content) :
	mTime(Time::Now()),
	mIsPublic(false),
	mIsIncoming(false),
	mIsRelayed(false),
	mNumber(0)
{
	if(!content.empty()) setContent(content);
}

Mail::~Mail(void)
{
  
}

Time Mail::time(void) const
{
	return mTime; 
}

String Mail::stamp(void) const
{
	return mStamp;
}

String Mail::parent(void) const
{
	return mParent;
}

String Mail::author(void) const
{
        return mAuthor;
}

String Mail::contact(void) const
{
        return mContact;
}

bool Mail::isPublic(void) const
{
	return mIsPublic;
}

bool Mail::isIncoming(void) const
{
        return mIsIncoming;
}

bool Mail::isRelayed(void) const
{
        return mIsRelayed;
}

const String &Mail::content(void) const
{
	return mContent;
}

const StringMap &Mail::headers(void) const
{
	return mHeaders;
}

bool Mail::header(const String &name, String &value) const
{
	return mHeaders.get(name, value);
}

String Mail::header(const String &name) const
{
	String value;
	if(mHeaders.get(name, value)) return value;
	else return "";
}

void Mail::setContent(const String &content)
{
	mContent = content;
	mContent.trim();	// TODO: YamlSerializer don't support leading spaces
}

void Mail::setParent(const String &stamp)
{
	mParent = stamp;
}

void Mail::setPublic(bool ispublic)
{
	mIsPublic = ispublic;
}

void Mail::setAuthor(const String &author)
{
	mAuthor = author;
}

void Mail::setContact(const String &uname)
{
	mContact = uname;
}

void Mail::setHeaders(const StringMap &headers)
{
	mHeaders = headers;
}

void Mail::setHeader(const String &name, const String &value)
{
	mHeaders[name] = value; 
}

void Mail::setDefaultHeader(const String &name, const String &value)
{
	if(!mHeaders.contains(name))
		mHeaders[name] = value;
}

void Mail::removeHeader(const String &name)
{
	mHeaders.erase(name);
}

void Mail::writeSignature(User *user)
{
	mAuthor = user->name();

	// TODO: should be removed
	// Backward compatibility for legacy stamps 
	if(mStamp.empty() || mStamp.size() == 32)
	//
		mStamp = computeStamp();

	mSignature = computeSignature(user);
}

bool Mail::checkStamp(void) const
{
	if(mStamp.empty())
		return false;
	
	// TODO: should be removed
	// Backward compatibility for legacy stamps 
	if(mStamp.size() != 32) 
		return true;
	//

	return (mStamp == computeStamp());
}

bool Mail::checkSignature(User *user) const
{
	return (mAuthor == user->name() && mSignature == computeSignature(user));
}

void Mail::setIncoming(bool incoming)
{
	mIsIncoming = incoming;
}

void Mail::setRelayed(bool relayed)
{
        mIsRelayed = relayed;
}

bool Mail::send(const Identifier &peering) const
{
	Assert(!mStamp.empty());
	
	String tmp;
	YamlSerializer serializer(&tmp);
	serializer.output(*this);

	Notification notification(tmp);
	//notification.setParameter("type", "mail");	// TODO
	return notification.send(peering);
}

bool Mail::recv(const Notification &notification)
{
	String type;
	//notification.parameter("type", type);	// TODO
	//if(!type.empty() && type != "mail") return false;
	
	String tmp = notification.content();
	YamlSerializer serializer(&tmp);
	serializer.input(*this);
	return true;
}

void Mail::serialize(Serializer &s) const
{
	ConstSerializableWrapper<bool> isPublicWrapper(mIsPublic);
	ConstSerializableWrapper<bool> isIncomingWrapper(mIsIncoming);
	ConstSerializableWrapper<bool> isRelayedWrapper(mIsRelayed);
	
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
	
	ConstSerializableWrapper<int64_t> numberWrapper(mNumber);
	ConstSerializableWrapper<bool>    isReadWrapper(mIsRead);
	ConstSerializableWrapper<bool>    isPassedWrapper(mIsPassed);
	
	if(mNumber) 
	{
		mapping["number"] = &numberWrapper;
		mapping["read"] = &isReadWrapper;
		mapping["passed"] = &isPassedWrapper;
	}
	
	s.outputObject(mapping);
}

bool Mail::deserialize(Serializer &s)
{
	mStamp.clear();
	
	SerializableWrapper<bool> isPublicWrapper(&mIsPublic);
	SerializableWrapper<bool> isIncomingWrapper(&mIsIncoming);
	SerializableWrapper<bool> isRelayedWrapper(&mIsRelayed);

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

	SerializableWrapper<int64_t> numberWrapper(&mNumber);
	SerializableWrapper<bool>    isReadWrapper(&mIsRead);
	SerializableWrapper<bool>    isPassedWrapper(&mIsPassed);
	
	bool dummy = false;
	SerializableWrapper<bool>    dummyBoolWrapper(&dummy);
	
	mapping["number"] = &numberWrapper;
	mapping["read"] = &isReadWrapper;
	mapping["passed"] = &isPassedWrapper;
	mapping["deleted"] = &dummyBoolWrapper;
	
	bool success = s.inputObject(mapping);
	if(mStamp.empty()) throw InvalidData("Mail without stamp");
	return success;
}

void Mail::computeAgregate(BinaryString &result) const
{
	result.clear();

	// Note: contact, incoming, and relayed are NOT in the agregate
        BinarySerializer serializer(&result); 
        serializer.output(int64_t(mTime.toUnixTime()));
	serializer.output(mIsPublic);
	serializer.output(mAuthor);
        serializer.output(mParent);
	serializer.output(mHeaders);
        serializer.output(mContent);
}

String Mail::computeStamp(void) const
{
	BinaryString agregate, digest;
	computeAgregate(agregate);
	Sha512().compute(agregate, digest);
	digest.resize(24);
	
	String stamp = digest.base64Encode(true);	// safe mode
	Assert(stamp.size() == 32);	// 24 * 4/3 = 32
	return stamp;
}

String Mail::computeSignature(User *user) const
{
	Assert(user);

	// TODO: should be removed
	// Backward compatibility for legacy stamps 
	if(mStamp.size() != 32)
	{
		// Note: contact, incoming and relayed are NOT signed
		BinaryString agregate;
		BinarySerializer serializer(&agregate);
		serializer.output(mHeaders);
		serializer.output(mContent);
		serializer.output(mAuthor);
		serializer.output(mStamp);
		serializer.output(mParent);
		serializer.output(int64_t(mTime.toUnixTime()));
		serializer.output(mIsPublic);

		BinaryString signature;
		Sha512().hmac(user->getSecretKey("mail"), agregate, signature);
		signature.resize(16);

		return signature.toString();
	}
	//
	
	BinaryString agregate, hmac;
	computeAgregate(agregate);
	Sha512().hmac(user->getSecretKey("mail"), agregate, hmac);
	hmac.resize(24);
	
	String signature = hmac.base64Encode(true);	// safeMode
	Assert(signature.size() == 32);	// 24 * 4/3 = 32
	return signature;
}

bool Mail::isInlineSerializable(void) const
{
	return false;
}

bool operator < (const Mail &m1, const Mail &m2)
{
	return m1.time() < m2.time();
}

bool operator > (const Mail &m1, const Mail &m2)
{
	return m1.time() > m2.time();
}

bool operator == (const Mail &m1, const Mail &m2)
{
	return ((m1.time() != m2.time())
		&& (m1.content() != m2.content()));   
}

bool operator != (const Mail &m1, const Mail &m2)
{
	return !(m1 == m2);
}

}
