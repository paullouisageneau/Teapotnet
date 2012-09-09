/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "splicer.h"
#include "request.h"
#include "pipe.h"

namespace arc
{

Splicer::Splicer(const Identifier &target, const String &filename, size_t blockSize) :
		mTarget(target),
		mBlockSize(blockSize)
{
	Log("Splicer", "Requesting available sources...");
	
	Request request(target.toString(),false);
	request.submit();
	request.wait();

	request.lock();

	Set<Identifier> sources;
	for(int i=0; i<request.responsesCount(); ++i)
	{
		Request::Response *response = request.response(i);
		if(response->status() == "OK")
		{
			sources.insert(response->peering());
		}
	}

	Log("Splicer", "Found " + String::number(int(sources.size())) + " sources");
	
	StringMap parameters;
	parameters["BlockSize"] << blockSize;
	parameters["StripesCount"] << sources.size();

	for(int i=0; i<sources.size(); ++i)
	{
		File *file = new File(filename, File::Write);
		StripedFile *striped = new StripedFile(file, blockSize, sources.size(), i);
		mStripes.push_back(striped);

		Request *request = new Request;
		request->setTarget(target.toString(),true);
		parameters["Stripe"] = String::number(i);
		request->setParameters(parameters);
		request->setContentSink(striped);
		mRequests.push_back(request);
	}

	request.unlock();
	
	Log("Splicer", "Transferts launched successfully");
}

Splicer::~Splicer(void)
{
	for(int i=0; i<mRequests.size(); ++i)
		delete mRequests[i];

	for(int i=0; i<mStripes.size(); ++i)
		delete mStripes[i];
}

bool Splicer::finished(void) const
{
	for(int i=0; i<mRequests.size(); ++i)
	{
		if(!mRequests[i]->responsesCount()) return false;
		const Request::Response *response = mRequests[i]->response(0);
		if(response->content()->is_open()) return false;
	}

	return true;
}

size_t Splicer::finishedBlocks(void) const
{
	size_t block = 0;
	for(int i=0; i<mStripes.size(); ++i)
		block = std::min(block, mStripes[i]->tellBlock());

	return block;
}

}
