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
	
	if(request.url != "/teapotnet/tracker" && request.url != "/teapotnet/tracker/")
		throw 404;
	
	if(!request.get.contains("id"))
		throw 400;
	
	BinaryString node;
	request.get["id"].extract(node);

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
			if(addr.isPublic())
			{
				insert(node, addr);
				++count;
				//LogDebug("Tracker", "POST " + node.toString() + " -> " + addr.toString());
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
				
				//LogDebug("Tracker", "POST " + node.toString() + " -> " + addr.toString());
				
				if(addr.isPublic() || (addr.isPrivate() && request.remoteAddress.isPrivate()))
				{
					insert(node, addr);
					++count;
				}
			}
			catch(...)
			{
				
			}
		}
			
		clean(2*count + 1);
	}
	else {
		//LogDebug("Tracker", "GET " + node.toString());
		clean(1);
	}
	
	unsigned count = 10;	// TODO: parameter
	
	SerializableArray<Address> result;
	retrieve(node, count, result);
	
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
		
		Map<Address, Time>::iterator it = mCleaner->second.begin();
		while(it != mCleaner->second.end())
		{
			if(Time::Now() - it->second >= EntryLife) mCleaner->second.erase(it++);
			else it++;
		}
		
		if(mCleaner->second.empty()) mMap.erase(mCleaner++);
		else mCleaner++;
			
	}
}

void Tracker::insert(const BinaryString &node, const Address &addr)
{
	Synchronize(this);
	
	mMap[node][addr] = Time::Now();
}

void Tracker::retrieve(const BinaryString &node, int count, Array<Address> &result) const
{
	Synchronize(this);
	Assert(!node.empty());
	
	result.clear();
	
	/*Map<BinaryString, Map<Address, Time> >::const_iterator it, jt;
	it = jt = mMap.upper_bound(node);
	
	int n;
	n = count;   while(it != mMap.begin() && n--) --it;
	n = count+1; while(jt != mMap.end()   && n--) ++jt;
	
	while(it != jt)
	{
		if(it->first != node)
		{
			Array<Address> addrs;
			it->second.getKeys(addrs);
			result.append(addrs);
		}

		++it;
	}*/
	
	// Linear, but returns the correct potential neighbors
	Map<BinaryString, BinaryString> sorted;
	for(Map<BinaryString, Map<Address, Time> >::const_iterator it = mMap.begin();
		it != mMap.end();
		++it)
	{
		if(it->first != node)
			sorted[it->first ^ node] = it->first;
	}
	
	result.reserve(count*2);
	for(Map<BinaryString, BinaryString>::iterator it = sorted.begin();
		it != sorted.end();
		++it)
	{
		Array<Address> addrs;
		mMap.get(it->second).getKeys(addrs);
		result.append(addrs);
		if(result.size() >= count*2)
			break;
	}
	//
	
	std::random_shuffle(result.begin(), result.end());
	if(result.size() > count)
		result.resize(count);
}

}
