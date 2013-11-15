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

#ifndef TPN_DATAGRAMSOCKET_H
#define TPN_DATAGRAMSOCKET_H

#include "tpn/include.h"
#include "tpn/address.h"
#include "tpn/bytestream.h"
#include "tpn/list.h"

namespace tpn
{

class DatagramSocket
{
public:
	static const int MaxDatagramSize;
	
	DatagramSocket(int port = 0, bool broadcast = false);
	DatagramSocket(const Address &local, bool broadcast = false);
	~DatagramSocket(void);

	Address getBindAddress(void) const;
	void getLocalAddresses(List<Address> &list) const;
	
	void bind(int port, bool broascast = false);
	void bind(const Address &local, bool broadcast = false);
	void close(void);
	
	int read(char *buffer, size_t size, Address &sender, double &timeout);
	int read(char *buffer, size_t size, Address &sender, const double &timeout = -1.);
	void write(const char *buffer, size_t size, const Address &receiver);

	bool read(ByteStream &stream, Address &sender, double &timeout);
	bool read(ByteStream &stream, Address &sender, const double &timeout = -1.);
	void write(ByteStream &stream, const Address &receiver);
	
private:
	socket_t mSock;
	int mPort;
};

}

#endif
