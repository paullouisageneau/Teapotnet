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
#include "tpn/core.h"
#include "tpn/interface.h"
#include "tpn/resource.h"
#include "tpn/identifier.h"

#include "pla/synchronizable.h"
#include "pla/binarystring.h"

namespace tpn
{

class Request :	protected Core::Subscriber, public Synchronizable, public HttpInterfaceable
{
public:
	Request(Resource &resource);
	Request(const Identifier &peer, const String &path);
	Request(const String &match);
	~Request(void);

	String urlPrefix(void) const;
	int resultsCount(void) const;
	void addResult(Resource &resource);
	void addResult(const Resource::DirectoryRecord &record);
	void getResult(int i, Resource::DirectoryRecord &record) const;
	
	void setAutoDelete(double timeout = 10.);
	
	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);
	
protected:
	void createPlaylist(Stream *output, String host = "");
	
	// Core::Subscriber
	bool incoming(const String &prefix, const String &path, const BinaryString &target);
	
private:
	String mUrlPrefix;
	Array<Resource::DirectoryRecord> mResults;	// We don't store resources but durectory records (see addResult)
	Set<BinaryString> mDigests;
	bool mListDirectories;
	bool mFinished;
};

}

#endif

