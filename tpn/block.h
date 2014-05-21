/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef TPN_BLOCK_H
#define TPN_BLOCK_H

#include "tpn/include.h"
#include "tpn/serializable.h"
#include "tpn/string.h"
#include "tpn/binarystring.h"
#include "tpn/time.h"
#include "tpn/directory.h"

namespace tpn
{

class Block : public Serializable
{
public:
	Block(void);
	~Block(void);
	
	void clear(void);
	void fetch(void);
	void refresh(void);
	
	BinaryString 	digest(void) const;
	String		name(void) const;
	Time		time(void) const;
	int64_t		size(void) const;
	int		type(void) const;
	
	Accessor *accessor(void) const;
	
	// Serializable
	virtual void serialize(Serializer &s) const;
	virtual bool deserialize(Serializer &s);
	virtual bool isInlineSerializable(void) const;

protected:
	BinaryString	mDigest;
	String 		mName;
	String		mType;
	Time		mTime;
	int64_t		mSize;
	
	Accessor *mAccessor;
};

}

#endif

