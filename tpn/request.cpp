/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#include "tpn/request.hpp"
#include "tpn/config.hpp"

#include "pla/jsonserializer.hpp"
#include "pla/mime.hpp"

namespace tpn
{

Request::Request(Resource &resource) :
	mListDirectories(true),
	mFinished(false),
	mFinishedAfterTarget(false),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	addResult(resource, true);	// finished
}
  
Request::Request(const String &path, bool listDirectories) :
	mPath(path),
	mListDirectories(listDirectories),
	mFinished(false),
	mFinishedAfterTarget(false),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	subscribe(path);
}

Request::Request(const String &path, const Identifier &local, const Identifier &remote, bool listDirectories) :
	Subscriber(Network::Link(local, remote)),
	mPath(path),
	mListDirectories(listDirectories),
	mFinished(false),
	mFinishedAfterTarget(false),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	subscribe(path);
}
  
Request::Request(const String &path, const Network::Link &link, bool listDirectories) :
	Subscriber(link),
	mPath(path),
	mListDirectories(listDirectories),
	mFinished(false),
	mFinishedAfterTarget(false),
	mAutoDeleteTimeout(-1.)
{
	mUrlPrefix = "/request/" + String::random(32);
	Interface::Instance->add(mUrlPrefix, this);
	subscribe(path);
}

Request::~Request(void)
{
	Interface::Instance->remove(mUrlPrefix, this);
	unsubscribeAll();
	
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mAutoDeleteTimeout = seconds(-1.);
		mAutoDeleter.cancel();
		mFinished = true;
	}
	
	mCondition.notify_all();
	std::this_thread::sleep_for(seconds(1.));	// TODO
	std::unique_lock<std::mutex> lock(mMutex);
}

bool Request::addTarget(const BinaryString &target, bool finished)
{
	if(mPath.empty() || target.empty()) return false;
	
	String prefix = mPath;
	if(prefix.size() >= 2 && prefix[prefix.size()-1] == '/')
		prefix.resize(prefix.size()-1);
	
	mFinishedAfterTarget|= finished;
	
	return incoming(link(), prefix, "/", target);
}

String Request::urlPrefix(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mUrlPrefix;
}

int Request::resultsCount(void) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return int(mResults.size());
}

void Request::addResult(Resource &resource, bool finish)
{
	if(resource.isDirectory() && mListDirectories)
	{
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
	
	if(finish) 
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mFinished = true;
	}
}

void Request::addResult(const Resource::DirectoryRecord &record)
{
	std::unique_lock<std::mutex> lock(mMutex);
	
	if(!mDigests.contains(record.digest))
	{
		LogDebug("Request", "Adding result: " + record.digest.toString() + " (" + record.name + ")");	

		mResults.append(record);
		mDigests.insert(record.digest);
		lock.unlock();
		mCondition.notify_all();
	}
}

void Request::getResult(int i, Resource::DirectoryRecord &record) const
{
	std::unique_lock<std::mutex> lock(mMutex);
	Assert(i < resultsCount());
	record = mResults.at(i);
}

void Request::autoDelete(duration timeout)
{
	std::unique_lock<std::mutex> lock(mMutex);
	if(timeout >= duration::zero()) mAutoDeleter.schedule(timeout, [this]() { delete this; });
	else mAutoDeleter.cancel();
	mAutoDeleteTimeout = timeout;
}

void Request::http(const String &prefix, Http::Request &request)
{
	std::unique_lock<std::mutex> lock(mMutex);	// Request::http() is synced !
	
	int next = 0;
	if(request.get.contains("next"))
		request.get["next"].extract(next);
	
	duration timeout = milliseconds(Config::Get("request_timeout").toDouble());
	if(request.get.contains("timeout"))
		timeout = milliseconds(request.get["timeout"].toDouble());
	
	if(int(mResults.size()) <= next && !mFinished)
	{
		mAutoDeleter.cancel();
		
		mCondition.wait_for(lock, timeout, [this, next]() {
			return int(mResults.size()) > next || mFinished;
		});
	}
	
	if(mAutoDeleteTimeout >= duration::zero())
		mAutoDeleter.schedule(timeout);
	
	// Playlist
	if(request.get.contains("playlist"))
	{
		int start = -1;
		int stop  = -1;

		String startParam;
		if(request.get.get("start", startParam) || request.get.get("t", startParam))
			start = timeParamToSeconds(startParam);
	
		String stopParam;
                if(request.get.get("stop", stopParam))
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
		
		std::list<Resource::DirectoryRecord*> tmp;
		if(int(mResults.size()) > next)
			for(int i = next; i < int(mResults.size()); ++i)
				tmp.push_back(&mResults[i]);
			
		JsonSerializer(response.stream) << tmp;
	}
}

bool Request::incoming(const Network::Link &link, const String &prefix, const String &path, const BinaryString &target)
{
	// Ignore subdirectories
	if(mListDirectories && !mPath.empty() && prefix + path != mPath)
		return false;
	
	if(fetch(link, prefix, path, target, false))	// no content
	{
		Resource resource(target, true);	// local only
		if(!mListDirectories || !resource.isDirectory() || fetch(link, prefix, path, target, true))
			addResult(resource, mFinishedAfterTarget);
	}

	return true;
}

void Request::createPlaylist(Stream *output, String host, int start, int stop)
{
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

