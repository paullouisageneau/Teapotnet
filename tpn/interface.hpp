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

#ifndef TPN_INTERFACE_H
#define TPN_INTERFACE_H

#include "tpn/include.hpp"

#include "pla/http.hpp"
#include "pla/map.hpp"

namespace tpn
{

class User;

class HttpInterfaceable
{
public:
	virtual void http(const String &prefix, Http::Request &request) = 0;

protected:
	sptr<User> getAuthenticatedUser(Http::Request &request, String name = "");
	int getAuthenticatedUsers(Http::Request &request, Array<sptr<User> > &users);
};

class Interface : public Http::Server, public HttpInterfaceable
{
public:
	static Interface *Instance;

	Interface(int port);
        ~Interface(void);

	void add(const String &prefix, HttpInterfaceable *interfaceable);
	void remove(const String &prefix, HttpInterfaceable *interfaceable = NULL);

	void http(const String &prefix, Http::Request &request);

private:
	void process(Http::Request &request);
	void generate(Stream &out, int code, const String &message);

	Map<String,HttpInterfaceable*> mPrefixes;
	std::mutex mMutex;

  String mBadPasswordsString;
};

}

#endif
