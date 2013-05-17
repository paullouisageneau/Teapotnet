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

void Message::setParameters(StringMap &params)
{
	mParameters = params;
}

void Message::setParameter(const String &name, const String &value)
{
	mParameters[name] = value; 
}

bool Message::isRead(void) const
{
	return mIsRead;  
}

void Message::markRead(bool read)
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
