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
#include "tpn/mailbase.h"
#include "tpn/core.h"
#include "tpn/user.h"

#include "pla/binarystring.h"
#include "pla/yamlserializer.h"
#include "pla/binaryserializer.h"
#include "pla/crypto.h"

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

const String &Mail::content(void) const
{
	return mContent;  
}

String Mail::author(void) const
{
	  return mAuthor;
}

String Mail::board(void) const
{
	  return mBoard;
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

void Mail::setBoard(const String &board)
{
	mBoard = board;
	mBoard.clear();
}

void Mail::setParent(const BinaryString &parent)
{
	mParent = parent;
	mDigest.clear();
}

void Mail::addAttachment(const BinaryString &attachment)
{
	mAttachments.append(attachment);
	mDigest.clear();
}

bool Mail::isSigned(void) const
{
	return !mSignature.empty();  
}

void Mail::sign(const Rsa::PrivateKey &privKey)
{
	privKey.sign(digest(), mSignature);
}

bool Mail::check(const Rsa::PublicKey &pubKey) const
{
	return pubKey.verify(digest(), mSignature);
}

void Mail::serialize(Serializer &s) const
{
	Serializer::ConstObjectMapping mapping;
	mapping["content"] = &mContent;
	mapping["time"] = &mTime;
	
	if(!mAuthor.empty())
		mapping["author"] = &mAuthor;
	
	if(!mAttachments.empty())
		mapping["attachments"] = &mAttachments;
	
	if(!mParent.empty())
		mapping["parent"] = &mParent;
	
	if(!mSignature.empty())
		mapping["signature"] = &mSignature;
	
	if(s.optionalOutputMode())
	{
		digest();	// so mDigest is computed
		mapping["digest"] = &mDigest;
	}
	
	s.outputObject(mapping);
}

bool Mail::deserialize(Serializer &s)
{
	mContent.clear();
	mAuthor.clear();
	mAttachments.clear();
	mParent.clear();
	mSignature.clear();
	mTime = Time::Now();
	
	Serializer::ObjectMapping mapping;
        mapping["content"] = &mContent;
	mapping["time"] = &mTime;
	mapping["author"] = &mAuthor;
	mapping["attachments"] = &mAttachments;
	mapping["parent"] = &mParent;
	mapping["signature"] = &mSignature;
	
	if(!s.inputObject(mapping))
		return false;
	
	// TODO: checks
	return true;
}

BinaryString Mail::computeDigest(void) const
{
	BinaryString signature = mSignature;
	mSignature.clear();
	
	BinaryString tmp;
	BinarySerializer serializer(&tmp);
	serializer.write(*this);
	
	BinaryString digest;
	Sha256().compute(tmp, digest),
	
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
