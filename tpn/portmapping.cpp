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
#include "tpn/scheduler.h"

namespace tpn
{

PortMapping *PortMapping::Instance = NULL;

PortMapping::PortMapping(void) :
	mProtocol(NULL),
	mEnabled(false)
{
	
}

PortMapping::~PortMapping(void)
{

}

void PortMapping::enable(void)
{
	Synchronize(this);
	mEnabled = true;
	
	Scheduler::Global->repeat(this, 600.);
	Scheduler::Global->schedule(this);
}

void PortMapping::disable(void)
{
	Synchronize(this);
	mEnabled = false;
	
	Scheduler::Global->remove(this);
}

bool PortMapping::isEnabled(void) const
{
	Synchronize(this);
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

void PortMapping::add(Protocol protocol, uint16_t internal, uint16_t suggested)
{
	Synchronize(this);
	
	remove(protocol, internal);
	
	Entry &entry = mMap[Descriptor(protocol, internal)];
	entry.suggested = suggested;
	entry.external = suggested;
	
	if(mProtocol) mProtocol->add(protocol, internal, entry.external);
}

void PortMapping::remove(Protocol protocol, uint16_t internal)
{
	Synchronize(this);
	mMap.erase(Descriptor(protocol, internal));
	
	if(mProtocol) mProtocol->remove(protocol, internal);
}

bool PortMapping::get(Protocol protocol, uint16_t internal, uint16_t &external) const
{
	Synchronize(this);
	
	external = internal;
	
	if(!mProtocol) return false;
	
	Entry entry;
	if(mMap.get(Descriptor(protocol, internal), entry)) return false;
	if(!entry.external) return false;
	
	external = entry.external;
	return true;
}

void PortMapping::run(void)
{
	if(mProtocol)
	{
		if(!mProtocol->check(mExternalHost))
		{
			delete mProtocol;
			mProtocol = NULL;
		}
	}
	
	if(!mProtocol)
	{
		mExternalHost.clear();
		
		for(int i=0; i<3; ++i)
		{
			try {
				switch(i)
				{
				case 0: mProtocol = new NatPMP;		break;
				case 1: mProtocol = new UPnP;		break;
				case 2: mProtocol = new FreeboxAPI;	break;
				}
				
				if(mProtocol->check(mExternalHost)) break;
			}
			catch(const Exception &e)
			{
				LogWarn("PortMapping", e.what());
			}
			
			delete mProtocol;
			mProtocol = NULL;
		}
		
		if(mProtocol) LogInfo("PortMapping", "Port mapping is available !");
	}
	
	if(mProtocol)
	{
		for(Map<Descriptor, Entry>::iterator it = mMap.begin();
			it != mMap.end();
			++it)
		{
			it->second.external = it->second.suggested;
			if(!mProtocol->add(it->first.protocol, it->first.port, it->second.external))
				LogWarn("PortMapping", "Mapping failed");
		}
	}
}


PortMapping::NatPMP::NatPMP(void)
{
	mSock.bind(5350, true);
	mGatewayAddr.set("255.255.255.255", 5351);	// TODO
}

PortMapping::NatPMP::~NatPMP(void)
{
	
}

bool PortMapping::NatPMP::check(String &host)
{
	LogInfo("PortMapping", "Looking for NAT-PMP...");
	
	ByteString query;
	query.writeBinary(uint8_t(0));	// version
	query.writeBinary(uint8_t(0));	// op
	
	int attempts = 3;
	double timeout = 0.250;
	for(int i=0; i<attempts; ++i)
	{
		ByteString dgram = query;
		mSock.write(dgram, mGatewayAddr);
		
		Address sender;
		double time = timeout;
		while(mSock.read(dgram, sender, time))
		{
			LogDebug("PortMapping", String("Got response from ") + sender.toString());
			if(parse(dgram, 0))
			{
				LogInfo("PortMapping", "NAT-PMP is available");
				mGatewayAddr = sender;
				host = mExternalHost;
				return true;
			}
		}
		
		timeout*= 2;
	}
	
	LogDebug("PortMapping", "NAT-PMP is not available");
	return false;
}

bool PortMapping::NatPMP::add(Protocol protocol, uint16_t internal, uint16_t &external)
{
	return request((protocol == TCP ? 2 : 1), internal, external, 7200, &external);	// 2h, recommended
}

bool PortMapping::NatPMP::remove(Protocol protocol, uint16_t internal)
{
	return request((protocol == TCP ? 2 : 1), internal, 0, 0, NULL);
}

bool PortMapping::NatPMP::request(uint8_t op, uint16_t internal, uint16_t suggested, uint32_t lifetime, uint16_t *external)
{
	if(!op) return false;
	
	ByteString query;
	query.writeBinary(uint8_t(0));	// version
	query.writeBinary(op);		// op
	query.writeBinary(uint16_t(0));	// reserved
	query.writeBinary(internal);
	query.writeBinary(suggested);
	query.writeBinary(lifetime);
	
	const int attempts = 3;
	double timeout = 0.250;
	for(int i=0; i<attempts; ++i)
	{
		ByteString dgram = query;
		mSock.write(dgram, mGatewayAddr);
		
		Address sender;
		double time = timeout;
		while(mSock.read(dgram, sender, time))
			if(parse(dgram, op, internal))
				return true;
			
		timeout*= 2;
	}
	
	return false;
}

bool PortMapping::NatPMP::parse(ByteString &dgram, uint8_t reqOp, uint16_t reqInternal, uint16_t *retExternal)
{
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
			
			if(!internal) return false;
			if(reqInternal && (reqInternal != internal)) return false;
			  
			if(retExternal) *retExternal = external;
			return true;
		}
	}
	
	return false;
}


PortMapping::UPnP::UPnP(void)
{
	mSock.bind(1900, true);
}

PortMapping::UPnP::~UPnP(void)
{
	
}

bool PortMapping::UPnP::check(String &host)
{
	Address addr("239.255.255.250", 1900);
	
	String message;
	message << "M-SEARCH * HTTP/1.1\r\n";
	message << "HOST: "<<addr<<"\r\n";
	message << "MAN: ssdp:discover\r\n";
	message << "MX: 10\r\n";
	message << "ST: ssdp:all\r\n";
	
	int attempts = 3;
	double timeout = 0.250;
	for(int i=0; i<attempts; ++i)
	{
		ByteString dgram(message);
		mSock.write(dgram, addr);
		
		Address sender;
		double time = timeout;
		while(mSock.read(dgram, sender, time))
		{
			LogDebug("PortMapping", String("Got response from ") + sender.toString());
			if(parse(dgram))
			{
				LogInfo("PortMapping", "UPnP is available");
				mGatewayAddr = sender;
				host = mExternalHost;
				return true;
			}
		}
		
		timeout*= 2;
	}
	
	LogDebug("PortMapping", "UPnP is not available");
}

bool PortMapping::UPnP::add(Protocol protocol, uint16_t internal, uint16_t &external)
{
	
}

bool PortMapping::UPnP::remove(Protocol protocol, uint16_t internal)
{
	
}

bool PortMapping::UPnP::parse(ByteString &dgram)
{
	String message(dgram.begin(), dgram.end());
	
	// TODO
	VAR(message);
	return false;
}

PortMapping::FreeboxAPI::FreeboxAPI(void)
{
	
}

PortMapping::FreeboxAPI::~FreeboxAPI(void)
{
	
}

bool PortMapping::FreeboxAPI::check(String &host)
{
	
}

bool PortMapping::FreeboxAPI::add(Protocol protocol, uint16_t internal, uint16_t &external)
{
	
}

bool PortMapping::FreeboxAPI::remove(Protocol protocol, uint16_t internal)
{
	
}

}

