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

#ifndef TPOT_ADDRESS_H
#define TPOT_ADDRESS_H

#include "include.h"
#include "serializable.h"

namespace tpot
{

class Address : public Serializable
{
public:
	Address(void);
	Address(const String &host, const String &service);
	Address(const String &host, int port);
	Address(const String &str);
	Address(const sockaddr *addr, socklen_t addrlen);
	virtual ~Address(void);

	void set(const String &host, const String &service);
	void set(const String &host, int port);
	void set(const sockaddr *addr, socklen_t addrlen = 0);
	void setNull(void);
	bool isNull(void) const;

	String host(void) const;
	String service(void) const;
	int port(void) const;

	const sockaddr *addr(void) const;
	int addrFamily(void) const;
	socklen_t addrLen(void) const;

	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual void serialize(Stream &s) const;
	virtual bool deserialize(Stream &s);

private:
	sockaddr_storage 	mAddr;
	socklen_t			mAddrLen;
};

bool operator < (const Address &a1, const Address &a2);
bool operator > (const Address &a1, const Address &a2);
bool operator == (const Address &a1, const Address &a2);
bool operator != (const Address &a1, const Address &a2);

}

#endif
