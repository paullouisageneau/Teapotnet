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

#include "core.h"

namespace arc
{

Core::Core(void)
{

}

Core::~Core(void)
{

}

void Core::add(Socket *sock)
{
	assert(sock != NULL);
	Handler *handler = new Handler(this,sock);
	handler->start();
	add(sock->getRemoteAddress(), handler);
}

void Core::add(const Address &addr, Core::Handler *handler)
{
	assert(handler != NULL);
	mHandlers.insert(addr, handler);
}

void Core::remove(const Address &addr)
{
	if(mHandlers.contains(addr))
	{
		delete mHandlers.get(addr);
		mHandlers.erase(addr);
	}
}

Core::Handler::Handler(Core *core, Socket *sock) :
	mCore(core),
	mSock(sock)
{

}

Core::Handler::~Handler(void)
{
	delete mSock;
	delete mHandler;
}

void Core::Handler::run(void)
{
	String line;
	if(!mSock->readLine(line))
		return;

	String proto;
	String version;
	line.readString(proto);
	line.readString(version);

	unlock();
	while(mSock->readLine(line))
	{
		lock();

		String command;
		line.read(command);
		command = command.toUpper();

		unsigned channel, size;
		line.read(channel);
		line.read(size);

		if(command == "DATA")
		{
			ByteStream *bs;
			if(mChannels.get(channel,bs))
			{
				if(size) {
					mSock->readBinary(*bs,size);
				}
				else {
					delete bs;
					mChannels.erase(channel);
				}
			}

		}

		unlock();
	}

	//mCore->remove(this);
}

}
