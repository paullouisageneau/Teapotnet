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

Request::Request(Resource &resource) :
	mListDirectories(true),
	mFinished(false),
	mAutoDeleteTask(this),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	addResult(resource, true);	// finished
}
  
Request::Request(const String &target, bool listDirectories) :
	mListDirectories(listDirectories),
	mFinished(false),
	mAutoDeleteTask(this),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	subscribe(target);
}

Request::Request(const String &target, const Identifier &local, const Identifier &remote, bool listDirectories) :
	Subscriber(Network::Link(local, remote)),
	mListDirectories(listDirectories),
	mFinished(false),
	mAutoDeleteTask(this),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	subscribe(target);
}
  
Request::Request(const String &target, const Network::Link &link, bool listDirectories) :
	Subscriber(link),
	mListDirectories(listDirectories),
	mFinished(false),
	mAutoDeleteTask(this),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	subscribe(target);
}

Request::~Request(void)
{
	Synchronize(this);
	Scheduler::Global->cancel(&mAutoDeleteTask);
	Interface::Instance->remove(mUrlPrefix, this);
	unsubscribeAll();
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

void Request::addResult(Resource &resource, bool finish)
{
	Synchronize(this);
	
	if(resource.isDirectory() && mListDirectories)
	{
		Desynchronize(this);

		// List directory and add records
		Resource::Reader reader(&resource);
		Resource::DirectoryRecord record;
		while(reader.readDirectory(record))
			addResult(record);
	}
	else {
		// Do not list, just add corresponding record
		addResult(resource.getDirectoryRecord());
	}
	
	if(finish) mFinished = true;
}

void Request::addResult(const Resource::DirectoryRecord &record)
{
	Synchronize(this);
	
	if(!mDigests.contains(record.digest))
	{
		//LogDebug("Request", "Adding result: " + record.digest.toString() + " (" + record.name + ")");	

		mResults.append(record);
		mDigests.insert(record.digest);
		notifyAll();
	}
}

void Request::getResult(int i, Resource::DirectoryRecord &record) const
{
	Synchronize(this);
	Assert(i < resultsCount());
	record = mResults.at(i);
}

void Request::autoDelete(double timeout)
{
	Synchronize(this);
	if(timeout >= 0.) Scheduler::Global->schedule(&mAutoDeleteTask, timeout);
	else Scheduler::Global->cancel(&mAutoDeleteTask);
	mAutoDeleteTimeout = timeout;
}

void Request::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	
	int next = 0;
	if(request.get.contains("next"))
		request.get["next"].extract(next);
	
	double timeout = milliseconds(Config::Get("request_timeout").toInt());;
	if(request.get.contains("timeout"))
		request.get["timeout"].extract(timeout);
	
	while(int(mResults.size()) <= next && !mFinished)
	{
		Scheduler::Global->cancel(&mAutoDeleteTask);
		if(!wait(timeout))
			break;
	}
	
	if(mAutoDeleteTimeout >= 0.)
		Scheduler::Global->schedule(&mAutoDeleteTask, mAutoDeleteTimeout);
	
	// Playlist
	if(request.get.contains("playlist"))
	{
		int start = -1;
		int stop  = -1;

		String startParam;
		if(request.get.get("start", startParam) || request.get.get("t", startParam))
			start = timeParamToSeconds(startParam);
	
		String stopParam;
                if(request.get.get("stop", startParam))
                        stop = timeParamToSeconds(stopParam);

		Http::Response response(request, 200);
		response.headers["Content-Disposition"] = "attachment; filename=\"playlist.m3u\"";
		response.headers["Content-Type"] = "audio/x-mpegurl";
		response.send();
					
		String host;
		request.headers.get("Host", host);
		createPlaylist(response.stream, host, start, stop);
	}
	else {
		// JSON
		
		Http::Response response(request, 200);
		response.headers["Content-Type"] = "application/json";
		response.send();
		
		JsonSerializer json(response.stream);
		json.outputArrayBegin();
		if(mResults.size() > next)
			for(int i = next; i < int(mResults.size()); ++i)
				json.outputArrayElement(mResults[i]);
		json.outputArrayEnd();
	}
}

bool Request::incoming(const Network::Link &link, const String &prefix, const String &path, const BinaryString &target)
{
	Synchronize(this);

	if(fetch(link, prefix, path, target, false))	// no content
	{		
		Resource resource(target, true);	// local only
		if(!mListDirectories || !resource.isDirectory() || fetch(link, prefix, path, target, true))
			addResult(resource);
	}

	return true;
}

void Request::createPlaylist(Stream *output, String host, int start, int stop)
{
	Synchronize(this);
	Assert(output);
	
	if(host.empty()) 
		host = String("localhost:") + Config::Get("interface_port");
	
	output->writeLine("#EXTM3U");
	for(int i = 0; i < int(mResults.size()); ++i)
	{
		const Resource::DirectoryRecord &record = mResults[i];
		if(record.type == "directory" || record.digest.empty()) continue;
		if(!Mime::IsAudio(record.name) && !Mime::IsVideo(record.name)) continue;
		String link = "http://" + host + "/file/" + record.digest.toString();
		output->writeLine("#EXTINF:-1," + record.name.beforeLast('.'));
		if(start >= 0) output->writeLine("#EXTVLCOPT:start-time=" + String::number(start));
		if(stop >= 0)  output->writeLine("#EXTVLCOPT:stop-time="  + String::number(stop));
		output->writeLine(link);
		start = stop = -1;
	}
}

int Request::timeParamToSeconds(String param)
{
	try {
		int s = 0;
	
		if(param.contains("h"))
		{
			String hours = param;
			param = hours.cut('h');
			s+= hours.toInt()*3600;
	
			if(!param.contains("m") && !param.contains("s"))
				param+= 'm';
		}
		
		if(param.contains("m"))
		{
			String minutes = param;
			param = minutes.cut('m');
			s+= minutes.toInt()*60;
		}
		
		param.cut('s');
		s+= param.toInt();
		return s;
	}
	catch(const std::exception &e)
	{
		return -1;
	}
}

}

