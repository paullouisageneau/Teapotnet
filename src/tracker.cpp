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

#include "tracker.h"
#include "yamlserializer.h"

namespace tpot
{

const double Tracker::EntryLife = 3600.;	// seconds
  
Tracker::Tracker(int port) :
		Http::Server(port)
{
	mStorage.cleaner   = mStorage.map.begin();
	mAlternate.cleaner = mAlternate.map.begin();
}

Tracker::~Tracker(void)
{

}

void Tracker::process(Http::Request &request)
{
	Synchronize(this);
	//Log("Tracker", "URL " + request.url);	

	try {
		if(request.url != "/tracker" && request.url != "/tracker/")
			throw 404;
		
		if(!request.get.contains("id"))
			throw Exception("Missing identifier");
		
		Identifier identifier;
		try { 
			request.get["id"].extract(identifier);
		}
		catch(...)
		{
			throw Exception("Invalid identifier");
		}
		
		if(identifier.getDigest().size() != 64)
			throw Exception("Invalid indentifier size");
		
		bool alternate = false;
		if(request.get.contains("alternate")) alternate = true;
		
		if(request.method == "POST")
		{
			String instance;
			if(request.post.get("instance", instance)) identifier.setName(instance);
			if(identifier.getName().empty()) identifier.setName("default");
			
			String port;
			if(request.post.get("port", port))
			{
				String host;
				if(!request.post.get("host", host))
				{
					if(request.headers.contains("X-Forwarded-For")) 
						host = request.headers["X-Forwarded-For"];
					else host = request.sock->getRemoteAddress().host();
				}
				
				Address addr(host, port);
				insert(mStorage, identifier, addr);
				//Log("Tracker", "POST " + identifier.toString() + " -> " + addr.toString());
			}
			
			String addresses;
			if(request.post.get("addresses", addresses))
			{
				List<String> list;
				addresses.explode(list, ',');
				
				for(	List<String>::iterator it = list.begin();
					it != list.end();
					++it)
				try {
				      Address addr(*it);
				      if(alternate) insert(mAlternate, identifier, addr);
				      else insert(mStorage, identifier, addr);
				      //Log("Tracker", "POST " + identifier.toString() + " -> " + addr.toString());
				}
				catch(...)
				{
				  
				}
			}

			if(!alternate && request.post.get("alternate", addresses))
			{
				List<String> list;
				addresses.explode(list, ',');
				
				for(	List<String>::iterator it = list.begin();
					it != list.end();
					++it)
				try {
				      Address addr(*it);
				      insert(mAlternate, identifier, addr);
				      //Log("Tracker", "POST " + identifier.toString() + " -> " + addr.toString() + " (alternate)");
				}
				catch(...)
				{
				  
				}
			}
			
			Http::Response response(request,200);
			response.send();
		}
		else {
			//Log("Tracker", "GET " + identifier.toString());

			Http::Response response(request, 200);
			response.headers["Content-Type"] = "text/plain";
			response.send();
			
			if(alternate) retrieve(mAlternate, identifier, *response.sock);
			else retrieve(mStorage, identifier, *response.sock);
		}
	}
	catch(int code)
	{
		Http::Response response(request,code);
                response.send();
	}
	catch(const Exception &e)
	{
		Log("Tracker", String("Error: ") + e.what());
		Http::Response response(request,400);
                response.send();
	}
}

void Tracker::insert(Tracker::Storage &s, const Identifier &identifier, const Address &addr)
{
	int nbr = 1;
	if(nbr > s.map.size()) nbr = s.map.size();
	for(int i=0; i<nbr; ++i)
	{
	  	if(s.cleaner == s.map.end()) s.cleaner = s.map.begin();

		Map<Address,Time> &submap = s.cleaner->second;
		Map<Address,Time>::iterator it = submap.begin();
		while(it != submap.end())
		{
			if(Time::Now() - it->second >= EntryLife) submap.erase(it++);
			else it++;
		}
		
		if(submap.empty()) s.map.erase(s.cleaner++);
		else s.cleaner++;	
	}
	
	Map<Address,Time> &submap = s.map[identifier];
	submap[addr] = Time::Now();
}

void Tracker::retrieve(Tracker::Storage &s, const Identifier &identifier, Stream &output) const
{
	map_t::const_iterator it = s.map.lower_bound(identifier);
	if(it == s.map.end() || it->first != identifier) return;

	YamlSerializer serializer(&output);
	serializer.outputMapBegin();
	
	while(it != s.map.end() && it->first == identifier)
	{
		SerializableArray<Address> array;
		it->second.getKeys(array);
		std::random_shuffle(array.begin(), array.end());
		serializer.outputMapElement(it->first.getName(), array);
		++it;
	}
	
	serializer.outputMapEnd();
}

bool Tracker::contains(Storage &s, const Identifier &identifier)
{
	return s.map.contains(identifier);  
}

}
