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

#include "tpn/tracker.h"

#include "pla/jsonserializer.h"

namespace tpn
{

const double Tracker::EntryLife = 3600.;	// seconds
  
Tracker::Tracker(int port) :
		Http::Server(port)
{
	mCleaner = mMap.begin();
}

Tracker::~Tracker(void)
{

}

void Tracker::process(Http::Request &request)
{
	//LogDebug("Tracker", "URL " + request.url);	

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
		
		if(identifier.digest().size() != 32)
			//throw Exception("Invalid indentifier size");
			throw 404;		

		if(request.method == "POST")
		{
			uint64_t instance = 0;
			String tmp;
			request.post.get("instance", tmp);
			tmp.read(instance);
			if(instance == 0)
				throw Exception("Invalid or missing instance number");
			
			identifier.setNumber(instance);
		
			int count = 0;
			String port;
			if(request.post.get("port", port))
			{
				String host;
				if(!request.post.get("host", host))
				{
					if(request.headers.contains("X-Forwarded-For")) 
						host = request.headers["X-Forwarded-For"].beforeLast(',').trimmed();
					else host = request.remoteAddress.host();
				}
				
				Address addr(host, port);
				if(!addr.isLocal())
				{
					insert(identifier, addr);
					++count;
					LogDebug("Tracker", "POST " + identifier.toString() + " -> " + addr.toString());
				}
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
				      	
					LogDebug("Tracker", "POST " + identifier.toString() + " -> " + addr.toString());
					insert(identifier, addr);
					++count;
				}
				catch(...)
				{
				  
				}
			}
			
			clean(2*count + 1);
			
			Http::Response response(request, 200);
			response.send();
		}
		else {
			Http::Response response(request, 200);
			response.headers["Content-Type"] = "application/json";
			response.send();
			
			LogDebug("Tracker", "GET " + identifier.toString());
			
			String buffer;
			retrieve(identifier, buffer);
			response.stream->write(buffer);
		}
	}
	catch(int code)
	{
		Http::Response response(request,code);
                response.send();
	}
	catch(const Exception &e)
	{
		LogWarn("Tracker", e.what());
		Http::Response response(request, 400);
                response.send();
	}
}

void Tracker::clean(int nbr)
{
	Synchronize(this);
	
	if(nbr < 0) nbr = mMap.size();
	else if(nbr > mMap.size()) nbr = mMap.size();
	for(int i=0; i<nbr; ++i)
	{
	  	if(mCleaner == mMap.end()) mCleaner = mMap.begin();

		Map<Address,Time> &submap = mCleaner->second;
		Map<Address,Time>::iterator it = submap.begin();
		while(it != submap.end())
		{
			if(Time::Now() - it->second >= EntryLife) submap.erase(it++);
			else it++;
		}
		
		if(submap.empty()) mMap.erase(mCleaner++);
		else mCleaner++;
	} 
}

void Tracker::insert(const Identifier &identifier, const Address &addr)
{
	Synchronize(this);
	
	Map<Address,Time> &submap = mMap[identifier];
	submap[addr] = Time::Now();
}

void Tracker::retrieve(const Identifier &identifier, Stream &output) const
{
	Synchronize(this);
	
	map_t::const_iterator it = mMap.lower_bound(identifier);
	if(it == mMap.end() || it->first != identifier) return;
	
	JsonSerializer serializer(&output);
	serializer.outputMapBegin();
	while(it != mMap.end() && it->first == identifier)
	{
		SerializableArray<Address> array;
		it->second.getKeys(array);
		if(!array.empty())
		{
			std::random_shuffle(array.begin(), array.end());
			serializer.outputMapElement(it->first.number(), array);
		}
		++it;
	}
	serializer.outputMapEnd();
}

bool Tracker::contains(const Identifier &identifier)
{
	Synchronize(this);
	
	return mMap.contains(identifier);  
}

}
