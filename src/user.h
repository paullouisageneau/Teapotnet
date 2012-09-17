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

#ifndef ARC_USER_H
#define ARC_USER_H

#include "include.h"
#include "thread.h"
#include "http.h"
#include "interface.h"
#include "identifier.h"
#include "addressbook.h"
#include "store.h"
#include "mutex.h"
#include "map.h"

namespace arc
{

class User : public Thread, protected Synchronizable, public HttpInterfaceable
{
public:
  	static bool Exist(const String &name);
	static User *Get(const String &name);
	static User *Authenticate(const String &name, const String &password);
	
	User(const String &name, const String &password = "");
	~User(void);
	
	const String &name(void) const;
	String profilePath(void) const;
	
	AddressBook *addressBook(void) const;
	Store *store(void) const;
	
	void http(const String &prefix, Http::Request &request);
	
private:
	void run(void);
	
	String mName;
	Identifier mHash;
	AddressBook *mAddressBook;
	Store *mStore;
	
	static Map<String, User*>	UsersByName;
	static Map<Identifier, User*>	UsersByAuth;
	static Mutex			UsersMutex;
};

}

#endif
