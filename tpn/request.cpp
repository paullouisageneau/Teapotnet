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

#include "tpn/request.h"
#include "tpn/jsonserializer.h"

namespace tpn
{

Request::Request(void)
{
	mUrlPrefix = "/requests/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
}

Request::~Request(void)
{
	Interface::Instance->remove(mUrlPrefix, this);
}

String Request::urlPrefix(void) const
{
	Synchronize(this);
	return mUrlPrefix;
}

int Request::resultsCount(void) const
{
	Synchronize(this);
	return int(mResults.size());
}

void Request::addResult(const Resource &resource)
{
	Synchronize(this);
	if(!mDigests.contains(resource.digest()))
	{
		mResults.append(resource);
		mDigests.insert(resource.digest());
		notifyAll();
	}
}

void Request::getResult(int i, Resource &resource) const
{
	Synchronize(this);
	Assert(i < resultsCount());
	resource = mResults.at(i);
}

void Request::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	
	int next = 0;
	if(request.get.contains("next"))
		request.get["next"].extract(next);
	
	double timeout = 60.;
	if(request.get.contains("timeout"))
		request.get["timeout"].extract(timeout);
	
	while(next <= int(mResults.size()))
		if(!wait(timeout))
			break;
	
	Http::Response response(request, 200);
	response.headers["Content-Type"] = "application/json";
	response.send();
	
	JsonSerializer json(response.sock);
	json.outputArrayBegin();
	for(int i = next; i < int(mResults.size()); ++i)
		json.outputArrayElement(mResults[i]);
	json.outputArrayEnd();
}

}
