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

#ifndef TPN_HTTPTUNNEL_H
#define TPN_HTTPTUNNEL_H

#include "tpn/include.h"
#include "tpn/synchronizable.h"
#include "tpn/stream.h"
#include "tpn/bytestream.h"
#include "tpn/address.h"
#include "tpn/socket.h"
#include "tpn/http.h"
#include "tpn/task.h"

namespace tpn
{

class HttpTunnel
{
public:
	class Client;
	class Server;
	
	static Server *Incoming(Socket *sock);
	
	class Client : protected Synchronizable, public Stream, public ByteStream
	{
	public:
		Client(const Address &a);
		~Client(void);

		// Stream, ByteStream
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void flush(void);
		
	private:
		void updatePostSize(size_t left);
		
		Address mAddress;
		Socket *mUpSock, *mDownSock;
		ByteStream::FlushTask mFlushTask;
		uint32_t mSession;
		size_t mPostSize, mPostLeft;
	};
	
	class Server : protected Synchronizable, public Stream, public ByteStream
	{
	public:
		~Server(void);
		
		// Stream, ByteStream
                size_t readData(char *buffer, size_t size);
                void writeData(const char *data, size_t size);
		void flush(void);
		
	private:
		Server(uint32_t session);	// instanciated by Incoming() only
		
		Socket *mUpSock, *mDownSock;
		ByteStream::FlushTask mFlushTask;
		Http::Request mUpRequest;
		uint32_t mSession;
		size_t mPostBlockLeft;
		bool mClosed;
		
		friend Server *HttpTunnel::Incoming(Socket *sock);;
	};

	static String UserAgent;
        static size_t DefaultPostSize;
	static size_t MaxPostSize;
	static double ConnTimeout;
	static double ReadTimeout;
	static double FlushTimeout;
private:
	HttpTunnel(void);

	static Map<uint32_t,Server*> 	Sessions;
	static Mutex			SessionsMutex;
	static uint32_t			LastSession;

	static const uint8_t TunnelOpen		= 0x01;
	static const uint8_t TunnelData		= 0x02;
	static const uint8_t TunnelPadding	= 0x03;
	static const uint8_t TunnelError	= 0x04;
	static const uint8_t TunnelPad		= 0x45;
	static const uint8_t TunnelClose	= 0x46;
	static const uint8_t TunnelDisconnect	= 0x47;
};

}

#endif
