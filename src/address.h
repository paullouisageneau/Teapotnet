/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef ARC_ADDRESS_H
#define ARC_ADDRESS_H

#include "include.h"
#include "serializable.h"

namespace arc
{

class Address : public Serializable
{
public:
	Address(void);
	Address(const String &host, const String &service);
	Address(String Address);
	Address(const sockaddr *addr, socklen_t addrlen);
	virtual ~Address(void);

	void set(const String &host, const String &service);
	void set(const sockaddr *addr, socklen_t addrlen = 0);
	void setNull(void);
	bool isNull(void) const;

	const sockaddr *getaddr(void) const;
	int getaddrFamily(void) const;
	socklen_t getaddrLen(void) const;

	// Serializable
	virtual void serialize(Stream &s) const;
	virtual void deserialize(Stream &s);
	virtual void serializeBinary(ByteStream &s) const;
	virtual void deserializeBinary(ByteStream &s);

private:
	sockaddr_storage 	maddr;
	socklen_t			maddrLen;
};

bool operator < (const Address &a1, const Address &a2);
bool operator > (const Address &a1, const Address &a2);
bool operator == (const Address &a1, const Address &a2);
bool operator != (const Address &a1, const Address &a2);

}

#endif
