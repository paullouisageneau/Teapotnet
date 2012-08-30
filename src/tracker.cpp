/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "tracker.h"

namespace arc
{

Tracker::Tracker(int port) :
		Http::Server(port)
{

}

Tracker::~Tracker(void)
{

}

void Tracker::process(Http::Request &request)
{
	Identifier identifier;

	try {
		if(request.url[0] == '/') request.url.ignore();
		request.url >> identifier;
	}
	catch(...)
	{
		throw 400;
	}

	if(request.method == "POST")
	{
		SerializableArray<Address> addrs;
		retrieve(identifier, addrs);
		if(addrs.empty()) throw 404;

		Http::Response response(request,200);
		response.headers["Content-Type"] = "text/plain";
		response.send();
		response.sock->write(addrs);
	}
	else {
		if(!request.post.contains("port")) throw 400;
		Address addr(request.sock->getRemoteAddress().host(), request.post["port"]);
		insert(identifier,addr);

		Http::Response response(request,200);
		response.send();
	}
}

void Tracker::insert(const Identifier &identifier, const Address &addr)
{
	Map<Address,time_t> &map = mMap[identifier];
	map[addr] = time();
}

void Tracker::retrieve(const Identifier &identifier, Array<Address> &array)
{
	array.clear();

	map_t::iterator it = mMap.find(identifier);
	if(it == mMap.end()) return;

	Map<Address,time_t> &map = it->second;
	if(map.empty()) return;

	array.reserve(map.size());
	for(	Map<Address,time_t>::iterator it = map.begin();
			it != map.end();
			++it)
	{
		array.push_back(it->first);
	}

	std::random_shuffle(array.begin(), array.end());
}

}
