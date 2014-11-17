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

#ifndef TPN_BOARD_H
#define TPN_BOARD_H

#include "tpn/include.h"
#include "tpn/mail.h"
#include "tpn/core.h"
#include "tpn/interface.h"
#include "tpn/identifier.h"

#include "pla/synchronizable.h"
#include "pla/binarystring.h"
#include "pla/string.h"
#include "pla/array.h"
#include "pla/map.h"

namespace tpn
{
	
class Board : public Synchronizable, public Core::Publisher, public Core::Subscriber, public HttpInterfaceable
{
public:
	Board(const String &name, const String &displayName = "");
	~Board(void);
	
	String urlPrefix(void) const;
	bool hasNew(void) const;
	bool add(Mail &mail);
	BinaryString digest(void) const;
	
	// Publisher
	bool anounce(const Identifier &peer, const String &prefix, const String &path, BinaryString &target);
	
	// Subscriber
	bool incoming(const Identifier &peer, const String &prefix, const String &path, const BinaryString &target);
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);
	
private:
	String mName;
	String mDisplayName;
	Identifier mOwner;
	Set<Mail> mMails;
	Array<const Mail*> mUnorderedMails;
	
	mutable BinaryString mDigest;
	mutable bool mHasNew;
};

}

#endif

