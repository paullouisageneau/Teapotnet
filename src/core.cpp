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
	Pipe *pipe = new Pipe(this,sock);
	pipe->start();
	add(sock->getRemoteAddress(), pipe);
}

void Core::add(const Address &addr, Core::Pipe *pipe)
{
	assert(pipe != NULL);
	mPipes.insert(addr, pipe);
}

void Core::remove(const Address &addr)
{
	if(mPipes.contains(addr))
	{
		delete mPipes.get(addr);
		mPipes.erase(addr);
	}
}

Core::Pipe::Pipe(Core *core, Stream *stream) :
	mCore(core),
	mStream(stream),
	mHandler(new Handler)
{

}

Core::Pipe::~Pipe(void)
{
	delete mStream;
	delete mHandler;
}

void Core::Pipe::run(void)
{
	String line;
	while(mStream->readLine(line))
	{
		//String location = line.cut(' ');
		//String &operation = line;

	}

	//mCore->remove(this);
}

}
