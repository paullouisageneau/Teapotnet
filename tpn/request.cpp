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
#include "tpn/config.h"

#include "pla/jsonserializer.h"
#include "pla/mime.h"

namespace tpn
{

Request::Request(const Identifier &peer, const String &target) : Subscriber(peer)
{
	mUrlPrefix = "/request/" + String::random(32);
	LogDebug("Request", "Creating request: " + mUrlPrefix);
	
	Interface::Instance->add(mUrlPrefix, this);
	
	subscribe(target);
}

Request::Request(const String &match)
{
	mUrlPrefix = "/request/" + String::random(32);
	LogDebug("Request", "Creating request: " + mUrlPrefix);
	
	Interface::Instance->add(mUrlPrefix, this);
	
	if(!match.empty())
	{
		String m(match);
		m.replace('/', ' ');
		subscribe("/$match/"+m);
	}
}

Request::~Request(void)
{
	//LogDebug("Request", "Deleting request: " + mUrlPrefix);
	Interface::Instance->remove(mUrlPrefix, this);
	
	// TODO: wait for http requests to finish
	
	// unsubscribe is automatic
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

void Request::setAutoDelete(double timeout)
{
	// TODO
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
	
	if(request.get.contains("playlist"))
	{
		// Playlist
		
		Http::Response response(request, 200);
		response.headers["Content-Disposition"] = "attachment; filename=\"playlist.m3u\"";
		response.headers["Content-Type"] = "audio/x-mpegurl";
		response.send();
					
		String host;
		request.headers.get("Host", host);
		createPlaylist(response.stream, host);
	}
	else {
		// JSON
		
		Http::Response response(request, 200);
		response.headers["Content-Type"] = "application/json";
		response.send();
		
		JsonSerializer json(response.stream);
		json.outputArrayBegin();
		for(int i = next; i < int(mResults.size()); ++i)
			json.outputArrayElement(mResults[i]);
		json.outputArrayEnd();
	}
}

bool Request::incoming(const String &path, const BinaryString &target)
{
	Synchronize(this);
  
	// TODO: delegate to thread
	Resource resource(target);
	addResult(resource);
	return true;
}

void Request::createPlaylist(Stream *output, String host)
{
	Synchronize(this);
	Assert(output);
	
	if(host.empty()) 
		host = String("localhost:") + Config::Get("interface_port");
	
	output->writeLine("#EXTM3U");
	for(int i = 0; i < int(mResults.size()); ++i)
	{
		const Resource &resource = mResults[i];
		if(resource.isDirectory() || resource.digest().empty()) continue;
		if(!Mime::IsAudio(resource.name()) && !Mime::IsVideo(resource.name())) continue;
		String link = "http://" + host + "/" + resource.digest().toString();
		output->writeLine("#EXTINF:-1," + resource.name().beforeLast('.'));
		output->writeLine(link);
	}
}

}
