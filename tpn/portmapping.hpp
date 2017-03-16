/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#ifndef TPN_PORTMAPPING_H
#define TPN_PORTMAPPING_H

#include "tpn/include.hpp"

#include "pla/string.hpp"
#include "pla/binarystring.hpp"
#include "pla/datagramsocket.hpp"
#include "pla/address.hpp"
#include "pla/alarm.hpp"
#include "pla/map.hpp"

namespace tpn
{

class PortMapping
{
public:
	static PortMapping *Instance;

	PortMapping(void);
	~PortMapping(void);

	void enable(void);
	void disable(void);
	bool isEnabled(void) const;
	bool isAvailable(void) const;

	enum Protocol
	{
		UDP,
		TCP
	};

	String  getExternalHost(void) const;
	Address getExternalAddress(Protocol protocol, uint16_t internal) const;

	void add(Protocol protocol, uint16_t internal, uint16_t suggested);
	void remove(Protocol protocol, uint16_t internal);
	bool get(Protocol protocol, uint16_t internal, uint16_t &external) const;

private:
	void run(void);

	struct Descriptor
	{
		Protocol protocol;
		uint16_t port;

		Descriptor(Protocol protocol, uint16_t port) { this->protocol = protocol; this->port = port; }
		bool operator < (const Descriptor &d) const  { return protocol < d.protocol || (protocol == d.protocol && port < d.port); }
		bool operator == (const Descriptor &d) const { return protocol == d.protocol && port == d.port; }
	};

	struct Entry
	{
		uint16_t suggested;
		uint16_t external;

		Entry(uint16_t suggested = 0, uint16_t external = 0) { this->suggested = suggested; this->external = external; }
	};

	class MappingProtocol
	{
	public:
		MappingProtocol(void)	{}
		virtual ~MappingProtocol(void)	{}

		virtual bool check(String &host) = 0;	// true if protocol is available
		virtual bool add(Protocol protocol, uint16_t internal, uint16_t &external) = 0;
		virtual bool remove(Protocol protocol, uint16_t internal, uint16_t external) = 0;
	};

	class NatPMP : public MappingProtocol
	{
	public:
		NatPMP(void);
		~NatPMP(void);

		bool check(String &host);
		bool add(Protocol protocol, uint16_t internal, uint16_t &external);
		bool remove(Protocol protocol, uint16_t internal, uint16_t external);

	private:
		bool request(uint8_t op, uint16_t internal, uint16_t suggested, uint32_t lifetime, uint16_t *external = NULL);
		bool parse(BinaryString &dgram, uint8_t reqOp, uint16_t reqInternal = 0, uint16_t *retExternal = NULL);

		DatagramSocket mSock;
		Address mGatewayAddr;
		String mExternalHost;
	};

	class UPnP : public MappingProtocol
	{
	public:
		UPnP(void);
		~UPnP(void);

		bool check(String &host);
		bool add(Protocol protocol, uint16_t internal, uint16_t &external);
		bool remove(Protocol protocol, uint16_t internal, uint16_t external);

	private:
		bool parse(BinaryString &dgram);
		String extract(String &xml, const String &field, size_t pos = 0);

		DatagramSocket mSock;
		Address mGatewayAddr;
		String mControlUrl;
		String mExternalHost;
	};

	class FreeboxAPI : public MappingProtocol
	{
	public:
		FreeboxAPI(void);
		~FreeboxAPI(void);

		bool check(String &host);
		bool add(Protocol protocol, uint16_t internal, uint16_t &external);
		bool remove(Protocol protocol, uint16_t internal, uint16_t external);

	private:
		struct FreeboxResponse : public Serializable
		{
		public:
			FreeboxResponse(void);

			bool success;
			String errorCode;
			String message;
			StringMap result;

			void serialize(Serializer &s) const;
			bool deserialize(Serializer &s);
			bool isInlineSerializable(void) const;
		};

		bool get(const String &url, FreeboxResponse &response);
		bool put(const String &url, Serializable &data, FreeboxResponse &response);

		String mFreeboxUrl;
		Address mLocalAddr;
	};

	Map<Descriptor, Entry> mMap;		// Ports mapping
	sptr<MappingProtocol> mProtocol;	// Current mapping protocol
	String mExternalHost;
	Alarm mAlarm;
	bool mEnabled;

	mutable std::mutex mMutex;
};

}

#endif
