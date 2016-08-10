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

#ifndef TPN_HTTPTUNNEL_H
#define TPN_HTTPTUNNEL_H

#include "tpn/include.hpp"

#include "pla/stream.hpp"
#include "pla/socket.hpp"
#include "pla/address.hpp"
#include "pla/http.hpp"

namespace tpn
{

class HttpTunnel
{
public:
	class Client;
	class Server;
	
	static Server *Incoming(Socket *sock);
	
	class Client : protected Synchronizable, public Stream
	{
	public:
		Client(const Address &a, double timeout = -1.); // timeout used for first connection only
		~Client(void);

		void close(void);
		
		// Stream, Stream
		size_t readData(char *buffer, size_t size);
		void writeData(const char *data, size_t size);
		void flush(void);
		
	private:
		void writePaddingUntil(size_t left);
		void updatePostSize(size_t left);
		
		Address mAddress;
		String mReverse;
		Socket *mUpSock, *mDownSock;
		Stream::FlushTask mFlushTask;
		uint32_t mSession;
		size_t mPostSize, mPostLeft;
		double mConnTimeout;
	};
	
	class Server : protected Synchronizable, public Stream
	{
	public:
		~Server(void);
		
		void close(void);
		
		// Stream, Stream
                size_t readData(char *buffer, size_t size);
                void writeData(const char *data, size_t size);
		void flush(void);
		
	private:
		Server(uint32_t session);	// instanciated by Incoming() only
		
		Socket *mUpSock, *mDownSock;
		Stream::FlushTask mFlushTask;
		Http::Request mUpRequest;
		uint32_t mSession;
		size_t mPostBlockLeft;
		size_t mDownloadLeft;
		bool mClosed;
		
		friend Server *HttpTunnel::Incoming(Socket *sock);;
	};

	static String UserAgent;
        static size_t DefaultPostSize;
	static size_t MaxPostSize;
	static size_t MaxDownloadSize;
	static double ConnTimeout;
	static double SockTimeout;
	static double FlushTimeout;
	static double ReadTimeout;

private:
	HttpTunnel(void);

	static Map<uint32_t,Server*> 	Sessions;
	static Mutex			SessionsMutex;

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