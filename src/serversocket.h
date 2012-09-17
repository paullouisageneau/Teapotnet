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

#ifndef ARC_SERVERSOCKET_H
#define ARC_SERVERSOCKET_H

#include "include.h"
#include "./socket.h"

namespace arc
{

class ServerSocket
{
public:
	ServerSocket(void);
	ServerSocket(int port);
	~ServerSocket(void);

	bool isListening(void) const;
	int getPort(void) const;
	Address getLocalAddress(void) const;

	void listen(int port);
	void close(void);
	void accept(Socket &sock);

private:
	socket_t	mSock;
	int		mPort;
};

}

#endif
