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

#ifndef TPN_INTERFACE_H
#define TPN_INTERFACE_H

#include "tpn/include.h"
#include "tpn/http.h"
#include "tpn/mutex.h"
#include "tpn/map.h"

namespace tpn
{
  
class HttpInterfaceable
{
public:
	virtual void http(const String &prefix, Http::Request &request) = 0;
};

class Interface : public Http::Server
{
public:
	static Interface *Instance;

	Interface(int port);
        ~Interface();
	
	void add(const String &prefix, HttpInterfaceable *interfaceable);
	void remove(const String &prefix, HttpInterfaceable *interfaceable = NULL);

private:
	void process(Http::Request &request);

	Map<String,HttpInterfaceable*>	mPrefixes;
	Mutex mMutex;
};

}

#endif
