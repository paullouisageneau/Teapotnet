/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef PLA_SERVERSOCKET_H
#define PLA_SERVERSOCKET_H

#include "pla/include.hpp"
#include "pla/set.hpp"
#include "pla/socket.hpp"

namespace pla
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
	void getLocalAddresses(Set<Address> &set) const;

	void listen(int port);
	void close(void);
	void accept(Socket &sock);

private:
	socket_t	mSock;
	int			mPort;
};

}

#endif
