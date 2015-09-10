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
	
	if(request.url != "/teapotnet" && request.url != "/teapotnet/")
		throw 404;
	
	if(request.method == "POST")
	{
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
				insert(addr);
				++count;
				LogDebug("Tracker", "POST " + addr.toString());
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
				
				LogDebug("Tracker", "POST " + addr.toString());
				insert(addr);
				++count;
			}
			catch(...)
			{
				
			}
		}
			
		clean(2*count + 1);
	}
	else {
		LogDebug("Tracker", "GET");
		clean(1);
	}
	
	unsigned count = 10;	// TODO: parameter
	
	SerializableArray<Address> result;
	retrieve(request.remoteAddress, count, result);
	
	Http::Response response(request, 200);
	response.headers["Content-Type"] = "application/json";
	response.send();
	
	JsonSerializer serializer(response.stream);
	serializer.write(result);
}

void Tracker::clean(int count)
{
	Synchronize(this);
	
	if(count < 0 || count > mMap.size())
		count = mMap.size();
	
	for(int i=0; i<count; ++i)
	{
		if(mCleaner == mMap.end()) mCleaner = mMap.begin();
		if(Time::Now() - mCleaner->second >= EntryLife) mMap.erase(mCleaner++);
		else mCleaner++;
	} 
}

void Tracker::insert(const Address &addr)
{
	Synchronize(this);
	
	mMap[addr] = Time::Now();
}

void Tracker::retrieve(const Address &hint, int count, Array<Address> &result) const
{
	Synchronize(this);
	
	result.clear();
	
	// TODO: this quick and dirty selection algorithm should be replaced by something better
	
	Map<Address, Time>::const_iterator it, jt;
	it = jt = mMap.upper_bound(hint);
	
	int n;
	n = count;   while(it != mMap.begin() && n--) --it;
	n = count+1; while(jt != mMap.end()   && n--) ++jt;
	
	while(it != jt)
	{
		if(it->first != hint)
			result.append(it->first);
		++it;
	}
	
	std::random_shuffle(result.begin(), result.end());
	if(result.size() > count)
		result.resize(count);
}

}
