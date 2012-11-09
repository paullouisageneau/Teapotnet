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

#ifndef TPOT_PORTMAPPING_H
#define TPOT_PORTMAPPING_H

#include "include.h"
#include "map.h"
#include "thread.h"
#include "synchronizable.h"
#include "datagramsocket.h"
#include "bytestring.h"

namespace tpot
{

class PortMapping : public Thread, protected Synchronizable
{
public:
	static PortMapping *Instance;
  
	PortMapping(void);
	~PortMapping(void);
	
	bool init(void);
	bool refresh(void);
	
	bool isEnabled(void) const;
	
	String  getExternalHost(void) const;
	Address getExternalAddress(uint16_t port) const;
	
	void addUdp(uint16_t internal, uint16_t suggested);
	void removeUdp(uint16_t internal);
	bool getUdp(uint16_t internal, uint16_t &external) const;
	
	void addTcp(uint16_t internal, uint16_t suggested);
	void removeTcp(uint16_t internal);
	bool getTcp(uint16_t internal, uint16_t &external) const;
	
private:
	void run(void);
	bool request(uint8_t op, uint16_t internal, uint16_t suggested);
	bool parse(ByteString &dgram, uint8_t reqOp, uint16_t reqInternal = 0);
	
	Map<uint16_t, uint16_t> mUdpMap;
	Map<uint16_t, uint16_t> mTcpMap;
	
	bool mEnabled;
	DatagramSocket mSock;
	DatagramSocket mAnnounceSock;
	Address mGatewayAddr;
	String mExternalHost;
};

}

#endif
