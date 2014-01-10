/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_CONFIG_H
#define TPN_CONFIG_H

#include "tpn/include.h"
#include "tpn/string.h"
#include "tpn/mutex.h"
#include "tpn/file.h"
#include "tpn/map.h"
#include "tpn/list.h"
#include "tpn/address.h"

namespace tpn
{

class Config
{
public:
	static String Get(const String &key);
	static void Put(const String &key, const String &value);
	static void Default(const String &key, const String &value);
	static void Load(const String &filename);
	static void Save(const String &filename);

	static bool IsUpdateAvailable(void);
	static bool CheckUpdate(void);

	static void GetExternalAddresses(List<Address> &list);
	static bool GetProxyForUrl(const String &url, Address &addr);
private:
	static StringMap Params;
	static Mutex ParamsMutex;
	static bool UpdateAvailableFlag;

	Config(void);
	~Config(void);

#ifdef WINDOWS
	static bool parseWinHttpProxy(LPWSTR lpszProxy, Address &addr);
#endif
};

}

#endif
