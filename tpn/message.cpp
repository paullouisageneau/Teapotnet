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

namespace tpn
{

Message::Message(const String &content) :
	mTime(Time::Now()),
	mContent(content),
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

String Message::stamp(void) const
{
	return mStamp;
}

const Identifier &Message::receiver(void) const
{
	return mReceiver;
}

const String &Message::content(void) const
{
	return mContent;
}

const StringMap &Message::parameters(void) const
{
	return mParameters;
}

bool Message::parameter(const String &name, String &value) const
{
	return mParameters.get(name, value);
}

void Message::setContent(const String &content)
{
	mContent = content;
}

void Message::setParameters(const StringMap &params)
{
	mParameters = params;
	if(!mParameters.get("stamp", mStamp))
		mStamp = String::number(Time::Now().toUnixTime()) + String::random(4);
}

void Message::setParameter(const String &name, const String &value)
{
	mParameters[name] = value; 
}

bool Message::isRead(void) const
{
	return mIsRead;  
}

void Message::markRead(bool read) const
{
	mIsRead = read; 
}

void Message::send(void)
{
	Core::Instance->sendMessage(*this);
}

void Message::send(const Identifier &receiver)
{
	mReceiver = receiver;
	Core::Instance->sendMessage(*this);
}

void Message::serialize(Serializer &s) const
{
	ConstSerializableWrapper<bool> isReadWrapper(&mIsRead);
	
	Serializer::ConstObjectMapping mapping;
	mapping["time"] = &mTime;
	mapping["receiver"] = &mReceiver;
	mapping["stamp"] = &mStamp;
	mapping["parameters"] = &mParameters;
	mapping["content"] = &mContent;
	mapping["isread"] = &isReadWrapper;
	
	s.outputObject(mapping);
}

bool Message::deserialize(Serializer &s)
{
	mTime = Time::Now();
	mReceiver.clear();
	mStamp.clear();
	mParameters.clear();
	mContent.clear();
	mIsRead = false;
	
	SerializableWrapper<bool> isReadWrapper(&mIsRead);
	
	Serializer::ObjectMapping mapping;
	mapping["time"] = &mTime;
	mapping["receiver"] = &mReceiver;
	mapping["stamp"] = &mStamp;
	mapping["parameters"] = &mParameters;
	mapping["content"] = &mContent;
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
