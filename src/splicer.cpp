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
		mFileName(filename),
		mBlockSize(blockSize)
{
	Log("Splicer", "Requesting available sources...");
	
	time_t timeout = 2000;	// TODO
	
	Request request(target.toString(),false);
	request.submit();
	request.wait(timeout);

	request.lock();

	Set<Identifier> sources;
	for(int i=0; i<request.responsesCount(); ++i)
	{
		Request::Response *response = request.response(i);
		StringMap parameters = response->parameters();
		
		if(response->status() == "OK")
		{
			if(mName.empty() && parameters.contains("name")) 
				mName = parameters.get("name");
			sources.insert(response->peering());
		}
	}

	if(mName.empty()) throw Exception("Unable to retrieve the file name");
	if(sources.empty()) throw Exception("No sources found");
	
	Log("Splicer", "Found " + String::number(int(sources.size())) + " sources");
	
	StringMap parameters;
	parameters["BlockSize"] << blockSize;
	parameters["StripesCount"] << sources.size();

	int i = 0;
	for(Set<Identifier>::iterator it = sources.begin();
	  	it != sources.end();
		++it)
	{
		File *file = new File(filename, File::Write);
		StripedFile *striped = new StripedFile(file, blockSize, sources.size(), i);
		mStripes.push_back(striped);

		Request *request = new Request;
		request->setTarget(target.toString(),true);
		parameters["Stripe"] = String::number(i);
		request->setParameters(parameters);
		request->setContentSink(striped);
		request->submit(*it);
		mRequests.push_back(request);
		
		// TODO: check responses ?
		
		++i;
	}

	request.unlock();
	
	Log("Splicer", "Transferts launched successfully");
}

Splicer::~Splicer(void)
{
	for(int i=0; i<mRequests.size(); ++i)
		delete mRequests[i];

	// Deleting the requests deletes the responses
	// Deleting the responses deletes the contents
	// So the stripes are already deleted
}

const String &Splicer::name(void)
{
	return mName;
}

bool Splicer::finished(void) const
{
	for(int i=0; i<mRequests.size(); ++i)
	{
		mRequests[i]->lock();
		
		if(mRequests[i]->responsesCount() == 0)
		{
			mRequests[i]->unlock();
			return false;
		}
		
		const Request::Response *response = mRequests[i]->response(0);
		if(!response || response->content()->is_open()) 
		{
			mRequests[i]->unlock();
			return false;
		}

		mRequests[i]->unlock();
	}

	return true;
}

size_t Splicer::finishedBlocks(void) const
{
	size_t block = mStripes[0]->tellWriteBlock();
	for(int i=1; i<mStripes.size(); ++i)
		block = std::min(block, mStripes[i]->tellWriteBlock());

	return block;
}

}
