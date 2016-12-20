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

#include "tpn/tracker.hpp"

#include "pla/jsonserializer.hpp"

namespace tpn
{

const duration Tracker::EntryLife = seconds(3600.);	// seconds
  
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
	if(node.empty())
		throw 400;
	
	unsigned count = 10;
	if(request.get.contains("count"))
		request.get["count"].extract(count);
	
	Map<BinaryString, Set<Address> > result;
	
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
			if(addr.isPublic() || (addr.isPrivate() && request.remoteAddress.isPrivate()))
			{
				insert(node, addr);
				++count;
				//LogDebug("Tracker", "POST " + node.toString() + " -> " + addr.toString());
			}
			
			result[node].insert(addr);
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
				
				if(addr.isPublic() || (addr.isPrivate() && request.remoteAddress.isPrivate()))
				{
					insert(node, addr);
					++count;
					//LogDebug("Tracker", "POST " + node.toString() + " -> " + addr.toString());
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
	
	// result not cleared by retrieve()
	retrieve(node, count, result);
	
	Http::Response response(request, 200);
	response.headers["Content-Type"] = "application/json";
	response.send();
	
	JsonSerializer(response.stream) << result;
}

void Tracker::clean(int count)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	if(count < 0 || count > mMap.size())
		count = mMap.size();
	
	for(int i=0; i<count; ++i)
	{
		if(mCleaner == mMap.end()) mCleaner = mMap.begin();
		
		auto it = mCleaner->second.begin();
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
	std::unique_lock<std::mutex> lock(mMutex);
	
	mMap[node][addr] = Time::Now();
}

void Tracker::retrieve(const BinaryString &node, int count, Map<BinaryString, Set<Address> > &result) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	Assert(!node.empty());
	
	// Note: Do not clear result
	
	/*
	auto it = mMap.upper_bound(node);
	auto jt = it;
	
	int n;
	n = count;   while(it != mMap.begin() && n--) --it;
	n = count+1; while(jt != mMap.end()   && n--) ++jt;
	
	while(it != jt && result.size() < count)
	{
		if(it->first != node)
		{
			Set<Address> addrs;
			it->second.getKeys(addrs);
			result.insert(it->second, addrs);
		}

		++it;
	}
	*/
	
	// Linear, but returns the correct potential neighbors
	Map<BinaryString, BinaryString> sorted;
	for(auto it = mMap.begin(); it != mMap.end(); ++it)
	{
		if(it->first != node)
			sorted[it->first ^ node] = it->first;
	}
	
	for(auto it = sorted.begin(); it != sorted.end() && result.size() < count; ++it)
	{
		Set<Address> addrs;
		mMap.get(it->second).getKeys(addrs);
		result.insert(it->second, addrs);
	}
	//
}

}
