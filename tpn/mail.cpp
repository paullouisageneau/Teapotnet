/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#include "tpn/mail.hpp"
#include "tpn/network.hpp"
#include "tpn/user.hpp"

#include "pla/binarystring.hpp"
#include "pla/yamlserializer.hpp"
#include "pla/binaryserializer.hpp"
#include "pla/object.hpp"
#include "pla/crypto.hpp"

namespace tpn
{

Mail::Mail(const String &content) :
	mTime(time_t(Time::Now()))
{
	if(!content.empty()) setContent(content);
}

Mail::~Mail(void)
{

}

bool Mail::empty(void) const
{
	return mContent.empty();
}

const String &Mail::content(void) const
{
	return mContent;
}

String Mail::author(void) const
{
	  return mAuthor;
}

Identifier Mail::identifier(void) const
{
	return mIdentifier;
}

Time Mail::time(void) const
{
	return mTime;
}

BinaryString Mail::parent(void) const
{
	return mParent;
}

BinaryString Mail::digest(void) const
{
	if(mDigest.empty()) mDigest = computeDigest();
	return mDigest;
}

void Mail::setContent(const String &content)
{
	mContent = content;
	mDigest.clear();
}

void Mail::setAuthor(const String &author)
{
	mAuthor = author;
	mDigest.clear();
}

void Mail::setParent(const BinaryString &parent)
{
	mParent = parent;
	mDigest.clear();
}

void Mail::addAttachment(const BinaryString &attachment)
{
	if(!attachment.empty())
	{
		mAttachments.append(attachment);
		mDigest.clear();
	}
}

void Mail::sign(const Identifier &identifier, const Rsa::PrivateKey &privKey)
{
	Assert(!identifier.empty());
	mIdentifier = identifier;
	privKey.sign(digest(), mSignature);
}

bool Mail::check(const Rsa::PublicKey &pubKey) const
{
	return pubKey.verify(digest(), mSignature);
}

bool Mail::isSigned(void) const
{
	return !mSignature.empty();
}

void Mail::serialize(Serializer &s) const
{
	Object object;
	object.insert("content", mContent);
	object.insert("time", mTime);

	if(!mAuthor.empty()) object.insert("author", mAuthor);
	if(!mIdentifier.empty()) object.insert("identifier", mIdentifier);
	if(!mAttachments.empty()) object.insert("attachments", mAttachments);
	if(!mParent.empty()) object.insert("parent", mParent);
	if(!mSignature.empty()) object.insert("signature", mSignature);

	if(s.optionalOutputMode())
	{
		digest();	// so mDigest is computed
		object.insert("digest", mDigest);
	}

	s << object;
}

bool Mail::deserialize(Serializer &s)
{
	mContent.clear();
	mAuthor.clear();
	mAttachments.clear();
	mParent.clear();
	mSignature.clear();
	mTime = time_t(Time::Now());
	mDigest.clear();

	Object object;
	object.insert("content", mContent);
	object.insert("time", mTime);
	object.insert("author", mAuthor);
	object.insert("identifier", mIdentifier);
	object.insert("attachments", mAttachments);
	object.insert("parent", mParent);
	object.insert("signature", mSignature);

	if(!(s >> object)) return false;

	// TODO: checks
	return true;
}

BinaryString Mail::computeDigest(void) const
{
	BinaryString signature = mSignature;
	mSignature.clear();

	BinaryString tmp;
	BinarySerializer serializer(&tmp);
	serializer << *this;

	BinaryString digest;
	Sha3_256().compute(tmp, digest),

	mSignature = signature;
	return digest;
}

bool Mail::isInlineSerializable(void) const
{
	return false;
}

bool operator < (const Mail &m1, const Mail &m2)
{
	return (m1.time() < m2.time()) || (m1.time() == m2.time() && m1.digest() < m2.digest());
}

bool operator > (const Mail &m1, const Mail &m2)
{
	return (m1.time() > m2.time()) || (m1.time() == m2.time() && m1.digest() > m2.digest());
}

bool operator == (const Mail &m1, const Mail &m2)
{
	return m1.digest() == m2.digest();
}

bool operator != (const Mail &m1, const Mail &m2)
{
	return !(m1 == m2);
}

}
