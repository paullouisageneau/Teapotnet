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

#ifndef TPOT_CONFIG_H
#define TPOT_CONFIG_H

#include "include.h"
#include "string.h"
#include "mutex.h"
#include "file.h"
#include "map.h"
#include "address.h"

namespace tpot
{

class Config
{
public:
	static String Get(const String &key);
	static void Put(const String &key, const String &value);
	static void Default(const String &key, const String &value);
	static void Load(const String &filename);
	static void Save(const String &filename);
	
	static void GetExternalAddresses(List<Address> &list);
	
private:
	static StringMap Params;
	static Mutex ParamsMutex;
	
	Config(void);
	~Config(void);
};

}

#endif
