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

#ifndef TPN_MAILBASE_H
#define TPN_MAILBASE_H

#include "tpn/include.h"
#include "tpn/mail.h"
#include "tpn/database.h"

#include "pla/synchronizable.h"
#include "pla/binarystring.h"
#include "pla/string.h"

namespace tpn
{
	
class MailBase : public Synchronizable
{
public:
	MailBase(void);
	~MailBase(void);
	
	void markReceived(const String &stamp, const String &uname);
	void markRead(const String &stamp);
	void markPassed(const String &stamp);
	void markDeleted(const String &stamp);
	
	bool isRead(const String &stamp) const;
	bool isPassed(const String &stamp) const;
	bool isDeleted(const String &stamp) const;
	
private:
	void setFlag(const String &stamp, const String &name, bool value = true);	// Warning: name is not escaped
	bool getFlag(const String &stamp, const String &name) const;
	
	Database *mDatabase;
};

}

#endif

