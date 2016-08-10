/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef PLA_PROXY_H
#define PLA_PROXY_H

#include "pla/include.hpp"
#include "pla/string.hpp"
#include "pla/address.hpp"

namespace pla
{
	
class Proxy
{
public:
	static bool GetProxyForUrl(const String &url, Address &addr);
	static bool HasProxyForUrl(const String &url);
	static String HttpProxy;
	
private:
#ifdef WINDOWS
	static bool ParseWinHttpProxy(LPWSTR lpszProxy, Address &addr);
#endif

	Proxy(void);
};

}

#endif
