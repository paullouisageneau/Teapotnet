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

#include "interface.h"

namespace arc
{

Interface *Interface::Instance = NULL;

Interface::Interface(int port) :
		Http::Server(port)
{

}

Interface::~Interface(void)
{

}

void Interface::add(const String &prefix, HttpInterfaceable *interfaceable)
{
	Assert(interfaceable != NULL);
	
	String cprefix(prefix);
	if(cprefix.empty() || cprefix[0] != '/')
		cprefix = "/" + cprefix;
	
	mMutex.lock();
	mPrefixes.insert(cprefix, interfaceable);
	mMutex.unlock();
}

void Interface::remove(const String &prefix)
{
	mMutex.lock();
	mPrefixes.erase(prefix);
	mMutex.unlock();
}

void Interface::process(Http::Request &request)
{
	List<String> list;
	request.url.explode(list,'/');

	// URL must begin with /
	if(list.empty()) throw 404;
	if(!list.front().empty()) throw 404;
	list.pop_front();
	if(list.empty()) throw 404;

	mMutex.lock();
	while(!list.empty())
	{
		String prefix;
		prefix.implode(list,'/');
		prefix = "/" + prefix;
	 	list.pop_back();
		
		HttpInterfaceable *interfaceable;
		if(mPrefixes.get(prefix,interfaceable)) 
		{
			request.url.ignore(prefix.size()+1);
			interfaceable->http(prefix, request);
			mMutex.unlock();
			return;
		}
	}
	mMutex.unlock();
	throw 404;
}

}
