/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_SOCKET_H
#define TPN_SOCKET_H

#include "tpn/include.h"
#include "tpn/stream.h"
#include "tpn/bytestream.h"
#include "tpn/address.h"

namespace tpn
{

class ServerSocket;
	
class Socket : public Stream, public ByteStream
{
public:
	using Stream::ignore;

	static void Transfer(Socket *sock1, Socket *sock2);
	
	Socket(void);
	Socket(const Address &a, double timeout = -1.);
	Socket(socket_t sock);
	virtual ~Socket(void);

	bool isConnected(void) const;	// Does not garantee the connection isn't actually lost
	bool isReadable(void) const;
	bool isWriteable(void) const;
	Address getLocalAddress(void) const;
	Address getRemoteAddress(void) const;

	void setConnectTimeout(double timeout);
	void setReadTimeout(double timeout);
	void setWriteTimeout(double timeout);
	void setTimeout(double timeout);	// connect + read
	
	void connect(const Address &addr, bool noproxy = false);
	void close(void);

	// Stream, ByteStream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

	// Socket-specific
	size_t peekData(char *buffer, size_t size);

private:
	size_t recvData(char *buffer, size_t size, int flags);
        void sendData(const char *data, size_t size, int flags);

	socket_t mSock;
	double mConnectTimeout, mReadTimeout, mWriteTimeout;
	
	Address mProxifiedAddr;
	
	friend class ServerSocket;
};

}

#endif
