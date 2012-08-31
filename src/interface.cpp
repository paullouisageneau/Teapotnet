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

Interface *Interface::Instance = new Interface(8080);	// TODO

Interface::Interface(int port) :
		Http::Server(port)
{

}

Interface::~Interface(void)
{

}

void Interface::add(const String &directory, HttpInterfaceable *interfaceable)
{
	Assert(interfaceable != NULL);
	mMutex.lock();
	mDirectories.insert(directory, interfaceable);
	mMutex.unlock();
}

void Interface::remove(const String &directory)
{
	mMutex.lock();
	mDirectories.erase(directory);
	mMutex.unlock();
}

void Interface::process(Http::Request &request)
{
	String url(request.url);
	if(url.empty()) throw 404;
	if(url[0] != '/') throw 404;
	url.ignore();
	url.cut('/');

	HttpInterfaceable *interfaceable;
	mMutex.lock();
	if(!mDirectories.get(url,interfaceable)) 
	{
		mMutex.unlock();
		throw 404;
	}
	mMutex.unlock();

	interfaceable->http(request);
}

}
