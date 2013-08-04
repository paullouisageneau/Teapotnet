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

#include "tpn/portmapping.h"
#include "tpn/datagramsocket.h"

namespace tpn
{

PortMapping *PortMapping::Instance = NULL;
  
PortMapping::PortMapping(void) :
	mEnabled(false)
{
	mSock.bind(0, true);
	
	try {
		mAnnounceSock.bind(Address("224.0.0.1", 5350), true);
	}
	catch(const Exception &e)
	{
		try {
			mAnnounceSock.bind(5350, true);
		}
		catch(const Exception &e)
		{
			LogWarn("PortMapping", e.what());
		}
	}
  
	mSock.setTimeout(0.250);
	mAnnounceSock.setTimeout(300.);			// TODO
	mGatewayAddr.set("255.255.255.255", 5351);	// TODO
}

PortMapping::~PortMapping(void)
{
  
}

bool PortMapping::init(void)
{
	Synchronize(this);
	if(mEnabled) return true;
	
	ByteString query;
	query.writeBinary(uint8_t(0));	// version
	query.writeBinary(uint8_t(0));	// op
	
	ByteString dgram;
	Address sender;
	
	const int attempts = 4;
	for(int i=0; i<attempts; ++i)
	{
		dgram = query;
		mSock.write(dgram, mGatewayAddr);
		if(mSock.read(dgram, sender))
		{
			LogDebug("PortMapping", String("Got response from ") + sender.toString());
			if(parse(dgram, 0, 0))
			{
				mEnabled = true;
				LogInfo("PortMapping", "NAT-PMP is available");
				return true;
			}
		}
	}
	
	LogDebug("PortMapping", "NAT-PMP is not available");
	return false;
}

bool PortMapping::refresh(void)
{
	if(!init()) return false;
	
	for(Map<uint16_t,uint16_t>::iterator it = mUdpMap.begin();
		it != mUdpMap.end();
		++it)
	{
		if(!request(1, it->first, it->second))
			it->second = it->first;
	}
	
	for(Map<uint16_t,uint16_t>::iterator it = mTcpMap.begin();
		it != mTcpMap.end();
		++it)
	{
		if(!request(2, it->first, it->second))
			it->second = it->first;
	}
	
	return true;
}

bool PortMapping::isEnabled(void) const
{
	return mEnabled; 
}

String PortMapping::getExternalHost(void) const
{	
	Synchronize(this);
	return mExternalHost;
}

Address PortMapping::getExternalAddress(uint16_t port) const
{
	Synchronize(this);
	return Address(mExternalHost, port);  
}

void PortMapping::addUdp(uint16_t internal, uint16_t suggested)
{
	Synchronize(this);
	mUdpMap.insert(internal, internal);
	if(mEnabled) request(1, internal, suggested);
}

void PortMapping::removeUdp(uint16_t internal)
{
	Synchronize(this);
	mUdpMap.erase(internal);
}

bool PortMapping::getUdp(uint16_t internal, uint16_t &external) const
{
	Synchronize(this);
	external = internal;
	return mUdpMap.get(internal, external);
}

void PortMapping::addTcp(uint16_t internal, uint16_t suggested)
{
	Synchronize(this);
	mTcpMap.insert(internal, internal);
	if(mEnabled) request(2, internal, suggested);  
}

void PortMapping::removeTcp(uint16_t internal)
{
	Synchronize(this);
	mTcpMap.erase(internal);
}

bool PortMapping::getTcp(uint16_t internal, uint16_t &external) const
{
	external = internal;
	return mUdpMap.get(internal, external);  
}

void PortMapping::run(void)
{
	while(true)
	{
		ByteString dgram;
		Address sender;
		if(mAnnounceSock.read(dgram, sender)) 
		{
			Synchronize(this);
			if(parse(dgram, 0, 0) && (sender.port() == mGatewayAddr.port()))
			{
				// TODO
				mGatewayAddr = sender;	
				LogDebug("PortMapping", String("Gateway internal address updated to ") + mGatewayAddr.toString());
			}
		}
		else refresh();
	}
}

bool PortMapping::request(uint8_t op, uint16_t internal, uint16_t suggested)
{
	Synchronize(this);
	if(!op || !mEnabled) return false;
		
	const time_t lifetime = 7200;	// 2h, recommended
  
	ByteString query;
	query.writeBinary(uint8_t(0));	// version
	query.writeBinary(op);		// op
	query.writeBinary(uint16_t(0));	// reserved
	query.writeBinary(internal);
	query.writeBinary(suggested);
	query.writeBinary(uint32_t(lifetime));
	
	ByteString dgram;
	Address sender;
	
	const int attempts = 4;
	for(int i=0; i<attempts; ++i)
	{
		dgram = query;
		mSock.write(dgram, mGatewayAddr);
		if(mSock.read(dgram, sender))
			if(parse(dgram, op, internal))
				return true;
	}
	return false;
}

bool PortMapping::parse(ByteString &dgram, uint8_t reqOp, uint16_t reqInternal)
{
	Synchronize(this);
	
	uint8_t version;
	uint8_t op;
	uint16_t result;
	uint32_t time;
	if(!dgram.readBinary(version))  return false;
	if(!dgram.readBinary(op))	return false;
	if(!dgram.readBinary(result))	return false;
	if(!dgram.readBinary(time))	return false;
	
	if(reqOp != op - 128) return false;
	if(result != 0) return false;
	
	switch(op)
	{
		case 128:	// address
		{
			uint8_t a,b,c,d;
			if(!dgram.readBinary(a)) return false;
			if(!dgram.readBinary(b)) return false;
			if(!dgram.readBinary(c)) return false;
			if(!dgram.readBinary(d)) return false;
		
			if(mExternalHost.empty()) LogDebug("PortMapping", "NAT-PMP compliant gateway found");
			
			mExternalHost.clear();
			mExternalHost<<a<<'.'<<b<<'.'<<c<<'.'<<d;
			LogDebug("PortMapping", "External address is " + mExternalHost);
			return true;	
		}
	
		case 129:	// UDP mapping
		case 130:	// TCP mapping
		{
			uint16_t internal;
			uint16_t external;
			uint32_t lifetime;
			if(!dgram.readBinary(internal)) return false;
		  	if(!dgram.readBinary(external)) return false;
			if(!dgram.readBinary(lifetime)) return false;
			
			if(!internal || !external || !lifetime) return false;
			if(reqInternal && (reqInternal != internal)) return false;
			  
			if(op == 129)
			{
				mUdpMap.insert(internal, external);
				LogDebug("PortMapping", String("Mapped UDP external port ") + String::number(external) 
						+ " to internal port " + String::number(internal));
			}
			else {
				mTcpMap.insert(internal, external);
				LogDebug("PortMapping", String("Mapped TCP external port ") + String::number(external) 
						+ " to internal port " + String::number(internal));
			}
			
			return true;
		}
	}
	
	return false;
}

}

