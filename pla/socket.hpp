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

#ifndef PLA_SOCKET_H
#define PLA_SOCKET_H

#include "pla/include.hpp"
#include "pla/stream.hpp"
#include "pla/address.hpp"

namespace pla
{

class ServerSocket;

class Socket : public Stream
{
public:
	static void Transfer(Socket *sock1, Socket *sock2);
	static Address HttpProxy;

	Socket(void);
	Socket(const Address &a, duration timeout = seconds(-1.));
	Socket(socket_t sock);
	virtual ~Socket(void);

	bool isConnected(void) const;	// Does not garantee the connection isn't actually lost
	bool isReadable(void) const;
	bool isWriteable(void) const;
	Address getLocalAddress(void) const;
	Address getRemoteAddress(void) const;

	void setConnectTimeout(duration timeout);
	void setReadTimeout(duration timeout);
	void setWriteTimeout(duration timeout);
	void setTimeout(duration timeout);	// connect + read + write

	void connect(const Address &addr, bool noproxy = false);
	void close(void);

	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	bool waitData(duration timeout);

	// Socket-specific
	size_t peekData(char *buffer, size_t size);

private:
	size_t recvData(char *buffer, size_t size, int flags);
	void sendData(const char *data, size_t size, int flags);

	socket_t mSock;
	duration mConnectTimeout, mReadTimeout, mWriteTimeout;
	Address mProxifiedAddr;

	friend class ServerSocket;
};

}

#endif
