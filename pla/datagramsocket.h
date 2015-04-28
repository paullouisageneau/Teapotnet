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

#ifndef PLA_DATAGRAMSOCKET_H
#define PLA_DATAGRAMSOCKET_H

#include "pla/include.h"
#include "pla/address.h"
#include "pla/stream.h"
#include "pla/set.h"
#include "pla/map.h"
#include "pla/synchronizable.h"

namespace pla
{

class DatagramStream;

class DatagramSocket
{
public:
	static const size_t MaxDatagramSize;
	
	DatagramSocket(int port = 0, bool broadcast = false);
	DatagramSocket(const Address &local, bool broadcast = false);
	~DatagramSocket(void);

	Address getBindAddress(void) const;
	void getLocalAddresses(Set<Address> &set) const;
	void getHardwareAddresses(Set<BinaryString> &set) const;
	
	void bind(int port, bool broascast = false, int family = AF_UNSPEC);
	void bind(const Address &local, bool broadcast = false);
	void close(void);
	
	int read(char *buffer, size_t size, Address &sender, double &timeout);
	int read(char *buffer, size_t size, Address &sender, const double &timeout = -1.);
	int peek(char *buffer, size_t size, Address &sender, double &timeout);
	int peek(char *buffer, size_t size, Address &sender, const double &timeout = -1.);
	void write(const char *buffer, size_t size, const Address &receiver);
	
	bool read(Stream &stream, Address &sender, double &timeout);
	bool read(Stream &stream, Address &sender, const double &timeout = -1.);
	bool peek(Stream &stream, Address &sender, double &timeout);
	bool peek(Stream &stream, Address &sender, const double &timeout = -1.);
	void write(Stream &stream, const Address &receiver);
	
	bool wait(double &timeout);
	
	void accept(DatagramStream &stream);
	void registerStream(const Address &addr, DatagramStream *stream);
	bool unregisterStream(DatagramStream *stream);
	
private:
	int recv(char *buffer, size_t size, Address &sender, double &timeout, int flags);
	void send(const char *buffer, size_t size, const Address &receiver, int flags);
	
	socket_t mSock;
	int mPort;

	// Mapped streams
	Map<Address, DatagramStream*> mStreams;
	Mutex mStreamsMutex;
};

class DatagramStream : public Stream
{
public:
	DatagramStream(void);
	DatagramStream(DatagramSocket *sock, const Address &addr);
	~DatagramStream(void);

	Address getLocalAddress(void) const;
	Address getRemoteAddress(void) const;

	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	bool waitData(double &timeout);			
	bool nextRead(void);
	bool nextWrite(void);	
	bool isDatagram(void) const;
	
	static double ReadTimeout;
	
private:
	DatagramSocket *mSock;
	Address mAddr;
	BinaryString mBuffer, mWriteBuffer;
	size_t mBufferOffset;
	Synchronizable mBufferSync;
	
	friend class DatagramSocket;
};

}

#endif
