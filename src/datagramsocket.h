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

#ifndef TPOT_DATAGRAMSOCKET_H
#define TPOT_DATAGRAMSOCKET_H

#include "include.h"
#include "address.h"
#include "bytestream.h"

namespace tpot
{

class DatagramSocket
{
public:
	static const int MaxDatagramSize;
	
	DatagramSocket(int port);
	~DatagramSocket(void);

	Address getLocalAddress(void);

	int read(char *buffer, size_t size, Address &sender);
	void write(const char *buffer, size_t size, const Address &receiver);

	void read(ByteStream &stream, Address &sender);
	void write(ByteStream &stream, const Address &receiver);
	
private:
	socket_t mSock;
	int mPort;
};

}

#endif
