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

#include "tpn/config.h"
#include "tpn/file.h"
#include "tpn/core.h"
#include "tpn/portmapping.h"

namespace tpn
{

StringMap Config::Params;
Mutex Config::ParamsMutex;
  
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

void Config::GetExternalAddresses(List<Address> &list)
{
	list.clear();
	
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
		list.push_back(addr);
	}

	List<Address> tmp;
	Core::Instance->getAddresses(tmp);
		
	for(List<Address>::const_iterator it = tmp.begin();
		it != tmp.end();
		++it)
	{
		const Address &addr = *it;
		if(addr.addrFamily() == AF_INET)
		{
			String host = PortMapping::Instance->getExternalHost();
			if(!host.empty()) 
			{
				uint16_t port;
				PortMapping::Instance->getTcp(addr.port(), port);
				list.push_back(Address(host, port));
			}
		}
			
		String host = addr.host();
		if(host != "127.0.0.1" && host != "::1"
			&& std::find(list.begin(), list.end(), addr) == list.end())
		{
			list.push_back(addr);
		}
	}
}

bool Config::GetProxyForUrl(const String &url, Address &addr)
{
	// TODO; get proxy from OS

	String proxy = Get("http_proxy").trimmed();
        if(!proxy.empty())
        {
                addr.fromString(proxy);
                return true;
        }

	return false;
}

}

