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

#ifndef TPN_REQUEST_H
#define TPN_REQUEST_H

#include "tpn/include.hpp"
#include "tpn/network.hpp"
#include "tpn/interface.hpp"
#include "tpn/resource.hpp"

#include "pla/binarystring.hpp"
#include "pla/alarm.hpp"

namespace tpn
{

class Request :	protected Network::Subscriber, public HttpInterfaceable
{
public:
	Request(Resource &resource);
	Request(const String &path, bool listDirectories = true);
	Request(const String &path, const Identifier &local, const Identifier &remote, bool listDirectories = true);
	Request(const String &path, const Network::Link &link, bool listDirectories = true);
	virtual ~Request(void);

	bool addTarget(const BinaryString &target, bool finish = false);

	String urlPrefix(void) const;
	int resultsCount(void) const;
	void addResult(Resource &resource, bool finish = false);
	void addResult(const Resource::DirectoryRecord &record);
	void getResult(int i, Resource::DirectoryRecord &record) const;

	void autoDelete(duration timeout = seconds(300.));

	// HttpInterfaceable
	void http(const String &prefix, Http::Request &request);

protected:
	void createPlaylist(Stream *output, String host = "", int start = -1, int stop = -1);

	// Network::Subscriber
	bool incoming(const Network::Link &link, const String &prefix, const String &path, const BinaryString &target);

private:
	static int timeParamToSeconds(String param);

	String mPath;
	String mUrlPrefix;
	Array<Resource::DirectoryRecord> mResults;	// We don't store resources but durectory records (see addResult)
	Set<BinaryString> mDigests;
	bool mListDirectories;
	bool mFinished, mFinishedAfterTarget;
	duration mAutoDeleteTimeout;
	Alarm mAutoDeleter;

	mutable std::mutex mMutex;
	mutable std::condition_variable mCondition;
};

}

#endif
