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

#ifndef ARC_SOCKET_H
#define ARC_SOCKET_H

#include "include.h"
#include "stream.h"
#include "bytestream.h"
#include "address.h"

namespace arc
{

class ServerSocket;
	
class Socket : public Stream, public ByteStream
{
public:
	using ByteStream::ignore;

	Socket(void);
	Socket(const Address &Address);
	Socket(socket_t sock);
	virtual ~Socket(void);

	bool isConnected(void) const;
	Address getRemoteAddress(void) const;

	void connect(const Address &Address);
	void close(void);

	// Stream, ByteStream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

private:
	socket_t mSock;
	
	friend class ServerSocket;
};

}

#endif
