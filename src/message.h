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

#ifndef TPOT_MESSAGE_H
#define TPOT_MESSAGE_H

#include "include.h"
#include "string.h"
#include "identifier.h"
#include "map.h"
#include "html.h"
#include "time.h"

namespace tpot
{

class Message
{
public:
	Message(const String &content = "");
	virtual ~Message(void);

	Time time(void) const;
	const Identifier &receiver(void) const;
	const String &content(void) const;
	const StringMap &parameters(void) const;
	bool parameter(const String &name, String &value) const;

	void setContent(const String &content);
	void setParameters(StringMap &params);
	void setParameter(const String &name, const String &value);

	bool isRead(void) const;
	void markRead(bool read = true);
	
	void send(void);
	void send(const Identifier &receiver);
	
private:
  	Time mTime;
	Identifier mReceiver;
	StringMap mParameters;
	String mContent;
	bool mIsRead;

	friend class Core;
};

}

#endif

