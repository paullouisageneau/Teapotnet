/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#ifndef PLA_SOCKETSELECT_H
#define PLA_SOCKETSELECT_H

#include "pla/include.hpp"
#include "pla/socket.hpp"
#include "pla/map.hpp"

namespace pla
{

// SocketSelect watch a number of sockets for available data
class SocketSelect
{
public:
	SocketSelect(void);
	~SocketSelect(void);

	void add(Socket *sock, std::function<void(Socket*)> reader);
	void remove(Socket *sock);

	void clear(void);
	void join(void);

protected:
	std::map<Socket*, std::function<void(Socket*) > > mSockets;
	std::thread mThread;
	std::mutex mMutex;
	bool mJoining;
};

}

#endif
