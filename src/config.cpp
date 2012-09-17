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

#include "config.h"

namespace tpot
{

StringMap Config::Param;
Mutex Config::ParamMutex;
  
String Config::Get(const String &key)
{
	ParamMutex.lock();
	String value;
	if(!Param.get(key, value))
	{
	 	 ParamMutex.unlock();
		 throw Exception("Config: no entry for \""+key+"\"");
	}
	ParamMutex.unlock();
	return value;
}

void Config::Put(const String &key, const String &value)
{
  	ParamMutex.lock();
	Param.insert(key, value);
	ParamMutex.unlock();
}

}
