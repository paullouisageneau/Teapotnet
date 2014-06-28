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

#ifndef TPN_REQUEST_H
#define TPN_REQUEST_H

#include "tpn/include.h"
#include "tpn/synchronizable.h"
#include "tpn/interface.h"
#include "tpn/binarystring.h"
#include "tpn/resource.h"
#include "tpn/core.h"

namespace tpn
{

class Request : public Synchronizable, public HttpInterfaceable
{
public:
	Request(void);
	~Request(void);

	String urlPrefix(void) const;
	int resultsCount(void) const;
	void addResult(const Resource &resource);
	void getResult(int i, Resource &resource) const;
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);
	
private:
	String mUrlPrefix;
	Array<Resources> mResults;
	Set<BinaryString> mDigests;
};

}

#endif

