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

#ifndef TPN_SERVERSOCKET_H
#define TPN_SERVERSOCKET_H

#include "tpn/include.h"
#include "tpn/list.h"
#include "tpn/./socket.h"

namespace tpn
{

class ServerSocket
{
public:
	ServerSocket(void);
	ServerSocket(int port);
	~ServerSocket(void);

	bool isListening(void) const;
	int getPort(void) const;
	Address getBindAddress(void) const;
	void getLocalAddresses(List<Address> &list) const;

	void listen(int port);
	void close(void);
	void accept(Socket &sock);

private:
	socket_t	mSock;
	int		mPort;
};

}

#endif
