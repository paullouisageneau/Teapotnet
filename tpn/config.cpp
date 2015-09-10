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

#include "tpn/config.h"
#include "tpn/network.h"
#include "tpn/portmapping.h"
#include "tpn/httptunnel.h"	// for user agent

#include "pla/file.h"

namespace tpn
{

StringMap Config::Params;
Mutex Config::ParamsMutex;
bool Config::UpdateAvailableFlag = false;  

String Config::Get(const String &key)
{
	ParamsMutex.lock();
	String value;
	if(Params.get(key, value))
	{
	 	 ParamsMutex.unlock();
		return value;
	}
	
	ParamsMutex.unlock();
	//throw Exception("Config: no entry for \""+key+"\"");
	return "";
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
		LogError("Config", String("Unable to load config: ") + e.what());
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

void Config::GetExternalAddresses(Set<Address> &set)
{
	set.clear();
	
	String externalAddress = Config::Get("external_address");
	if(!externalAddress.empty() && externalAddress != "auto")
	{
		Address addr;
		if(externalAddress.contains(':')) addr.set(externalAddress);
		else {
			String port = Config::Get("port");
			String externalPort = Config::Get("external_port");
                	if(!externalPort.empty() && externalPort != "auto") port = externalPort;
			addr.set(externalAddress, port);
		}
		set.insert(addr);
	}

	Set<Address> tmp;
	Network::Instance->overlay()->getAddresses(tmp);
	
	uint16_t port = 0;
	for(Set<Address>::const_iterator it = tmp.begin();
		it != tmp.end();
		++it)
	{
		const Address &addr = *it;

		if(addr.isIpv4() && addr.isPrivate())
			port = addr.port();
		
		if(!addr.isLocal())
			set.insert(addr);
	}
	
	if(port && PortMapping::Instance->isAvailable())
		set.insert(PortMapping::Instance->getExternalAddress(PortMapping::TCP, port));
}

bool Config::IsUpdateAvailable(void)
{
	return UpdateAvailableFlag;
}


bool Config::CheckUpdate(void)
{
	String release;

#if defined(WINDOWS)
        release = "win32";
#elif defined(MACOSX)
        release = "osx";
#else
	release = "src";
#endif

        try {
                LogInfo("Config::CheckUpdate", "Looking for updates...");
                String url = String(DOWNLOADURL) + "?version&release=" + release + "&current=" + APPVERSION;

                String content;
                int result = Http::Get(url, &content);
                if(result != 200)
                        throw Exception("HTTP error code " + String::number(result));

                unsigned lastVersion = content.trimmed().dottedToInt();
                unsigned appVersion = String(APPVERSION).dottedToInt();

                Assert(appVersion != 0);
                if(lastVersion > appVersion)
		{
			UpdateAvailableFlag = true;
			return true;
		}
        }
        catch(const Exception &e)
        {
                LogWarn("Config::CheckUpdate", String("Unable to look for updates: ") + e.what());
        }

        return false;
}

bool Config::LaunchUpdater(String *commandLine)
{
#if defined(WINDOWS)
	String parameters;
	if(commandLine) parameters = *commandLine;
	else parameters = "--nointerface";
	LogInfo("Config::ExitAndUpdate", "Running WinUpdater...");
	if(int(ShellExecute(NULL, NULL, "winupdater.exe", parameters.c_str(), NULL, SW_SHOW)) > 32)
		return true;
	LogWarn("Config::ExitAndUpdate", "Unable to run WinUpdater");
#endif
	
	return false;
}

}

