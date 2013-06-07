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
#include "tpn/string.h"
#include "tpn/identifier.h"
#include "tpn/map.h"
#include "tpn/html.h"
#include "tpn/time.h"

namespace tpn
{

class Message : public Serializable
{
public:
	Message(const String &content = "");
	virtual ~Message(void);

	Time time(void) const;
	String stamp(void) const;
	const Identifier &receiver(void) const;
	const String &content(void) const;
	const StringMap &parameters(void) const;
	bool parameter(const String &name, String &value) const;
	
	void setContent(const String &content);
	void setParameters(const StringMap &params);
	void setParameter(const String &name, const String &value);

	bool isRead(void) const;
	void markRead(bool read = true) const;
	
	void send(void);
	void send(const Identifier &receiver);

	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;
	
private:
  	Time mTime;
	Identifier mReceiver;
	String mStamp;
	StringMap mParameters;
	String mContent;
	mutable bool mIsRead;
	
	friend class Core;
};

bool operator <  (const Message &m1, const Message &m2);
bool operator >  (const Message &m1, const Message &m2);
bool operator == (const Message &m1, const Message &m2);
bool operator != (const Message &m1, const Message &m2);

}

#endif

