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

#include "tpn/include.hpp"

#include "pla/string.hpp"
#include "pla/file.hpp"
#include "pla/map.hpp"
#include "pla/set.hpp"
#include "pla/address.hpp"

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
	static void Clear(void);

	static bool IsUpdateAvailable(void);
	static bool CheckUpdate(void);
	static bool LaunchUpdater(String *commandLine = NULL);
	
	static void GetExternalAddresses(Set<Address> &set);
	
private:
	static StringMap Params;
	static Mutex ParamsMutex;
	static bool UpdateAvailableFlag;

	Config(void);
	~Config(void);
};

}

#endif
