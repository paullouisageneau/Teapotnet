/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/notification.h"
#include "tpn/core.h"
#include "tpn/bytestring.h"

namespace tpn
{

Notification::Notification(const String &content) :
	mTime(Time::Now()),
	mContent(content)
{

}

Notification::~Notification(void)
{
  
}

Time Notification::time(void) const
{
	return mTime; 
}

const String &Notification::content(void) const
{
	return mContent;
}

const StringMap &Notification::parameters(void) const
{
	return mParameters;
}

bool Notification::parameter(const String &name, String &value) const
{
	return mParameters.get(name, value);
}

String Notification::parameter(const String &name) const
{
	String value;
	if(mParameters.get(name, value)) return value;
	else return String();
}

void Notification::setContent(const String &content)
{
	mContent = content;
}

void Notification::setParameters(const StringMap &parameters)
{
	mParameters = parameters;
}

void Notification::setParameter(const String &name, const String &value)
{
	mParameters[name] = value; 
}

bool Notification::send(const Identifier &peering) const
{
	mPeering = peering;
	return Core::Instance->sendNotification(*this);
}

}
