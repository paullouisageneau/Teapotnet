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
#include "file.h"

namespace tpot
{

StringMap Config::Params;
Mutex Config::ParamsMutex;
  
String Config::Get(const String &key)
{
	ParamsMutex.lock();
	String value;
	if(!Params.get(key, value))
	{
	 	 ParamsMutex.unlock();
		 throw Exception("Config: no entry for \""+key+"\"");
	}
	ParamsMutex.unlock();
	return value;
}

void Config::Put(const String &key, const String &value)
{
  	ParamsMutex.lock();
	Params.insert(key, value);
	ParamsMutex.unlock();
}

void Config::Default(const String &key, const String &value)
{
	ParamsMutex.lock();
	if(!Params.contains(key)) Params.insert(key, value);
	ParamsMutex.unlock();
}

void Config::Load(const String &filename)
{
	ParamsMutex.lock();
	try {
		File file(filename, File::Read);
		file.read(Params);
		file.close();
	}
	catch(const Exception &e) 
	{
		Log("Config", String("Unable to load config: ") + e.what());
	}
	ParamsMutex.unlock();
}

void Config::Save(const String &filename)
{
	ParamsMutex.lock();
	try {
		File file(filename, File::Truncate);
		file.write(Params);
		file.close();
	}
	catch(...)
	{
		ParamsMutex.unlock();
		throw;
	}
	ParamsMutex.unlock();
}

}
