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

#ifndef TPN_NOTIFICATION_H
#define TPN_NOTIFICATION_H

#include "tpn/include.h"
#include "tpn/string.h"
#include "tpn/identifier.h"
#include "tpn/map.h"
#include "tpn/time.h"

namespace tpn
{

class Notification
{
public:
	Notification(const String &content = "");
	virtual ~Notification(void);
	
	Time time(void) const;
	const Identifier &peering(void) const;
	const String &content(void) const;
	const StringMap &parameters(void) const;
	bool parameter(const String &name, String &value) const;
	String parameter(const String &name) const;
	
	void setContent(const String &content);
	void setParameters(const StringMap &parameters);
	void setParameter(const String &name, const String &value);

	bool send(void);
	bool send(const Identifier &peering);
	
private:
	Identifier mPeering;
	Time mTime;
	
	StringMap mParameters;
        String mContent;
	
	friend class Core;
};

}

#endif

