/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of TeapotNet.                                     *
 *                                                                       *
 *   TeapotNet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   TeapotNet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with TeapotNet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "splicer.h"
#include "request.h"
#include "pipe.h"
#include "config.h"

namespace tpot
{
  
Splicer::Splicer(const ByteString &target, const String &filename, size_t blockSize, size_t firstBlock) :
		mTarget(target),
		mFileName(filename),
		mBlockSize(blockSize),
		mFirstBlock(firstBlock),
		mSize(0)
{
	Log("Splicer", "Requesting available sources...");
	
	Set<Identifier> sources;
	search(sources);
	
	if(sources.empty()) throw Exception("No sources found");
	if(mName.empty()) throw Exception("Unable to retrieve the file name");
	
	Log("Splicer", "Found " + String::number(int(sources.size())) + " sources");
	
	mNbStripes = sources.size();
	mRequests.fill(NULL, mNbStripes);
	mStripes.fill(NULL, mNbStripes);

	int i = 0;
	for(Set<Identifier>::iterator it = sources.begin();
	  	it != sources.end();
		++it)
	{
		query(i, *it);
		++i;
	}
	
	Log("Splicer", "Transfers launched successfully");
}

Splicer::~Splicer(void)
{
	close();
}

const String &Splicer::name(void) const
{
	return mName;
}

uint64_t Splicer::size(void) const
{
	return mSize;
}

void Splicer::process(void)
{ 
  	std::vector<int>		onError;
	std::multimap<size_t, int> 	byBlocks;
	
	for(int i=0; i<mRequests.size(); ++i)
	{
		Assert(mRequests[i]);
		Assert(mStripes[i]);
		Synchronize(mRequests[i]);
		
		byBlocks.insert(std::pair<size_t,int>(mStripes[i]->tellWriteBlock(), i));
		
		if(mRequests[i]->responsesCount())
		{
			const Request::Response *response = mRequests[i]->response(0);
			Assert(response != NULL);
				
			if(response->error())
				onError.push_back(i);
		}
	}
	
	if(onError.empty())
	{
		if(mRequests.size() >= 2)
		{
			int fastest = byBlocks.begin()->second;
			int slowest = byBlocks.rbegin()->second;
			if(mRequests[fastest]->receiver() != mRequests[slowest]->receiver())
				if(mStripes[fastest]->tellWriteBlock() > 2*mStripes[slowest]->tellWriteBlock() + 2)
					query(slowest, mRequests[fastest]->receiver());
		}
	}
	else for(int k=0; k<onError.size(); ++k)
	{
		int i = onError[k];
		
		Identifier formerSource = mRequests[i]->receiver();
		Identifier source;
		Map<size_t, int>::reverse_iterator it = byBlocks.rbegin();
		while(true)
		{
			source = mRequests[it->second]->receiver();
			if(source != formerSource) break;
			++it;
			if(it == byBlocks.rend())
			{
				Set<Identifier> sources;
				search(sources);
	
				if(sources.empty())
				{
					msleep(30000);
					return;
				}

				Set<Identifier>::iterator jt = sources.begin();
				int r = rand() % sources.size();
				for(int i=0; i<r; ++i) jt++;
				source = *jt;
				break;
			}
		}
		
		query(i, source);
	}
}

bool Splicer::finished(void) const
{
	for(int i=0; i<mRequests.size(); ++i)
	{
		Assert(mRequests[i]);
		Assert(mStripes[i]);
		Synchronize(mRequests[i]);
		
		if(mRequests[i]->responsesCount() == 0) return false;
		
		const Request::Response *response = mRequests[i]->response(0);
		Assert(response != NULL);
		Assert(response->content() != NULL);
		
		if(response->error()) return false;
		if(response->content()->is_open()) return false;
	}
	
	return true;
}

size_t Splicer::finishedBlocks(void) const
{
	size_t block = std::numeric_limits<size_t>::max();
	for(int i=0; i<mStripes.size(); ++i)
	{
		Assert(mStripes[i]);
		block = std::min(block, mStripes[i]->tellWriteBlock());
		mStripes[i]->flush();
	}

	return block;
}

void Splicer::close(void)
{
	if(!mRequests.empty())
	{
		for(int i=0; i<mRequests.size(); ++i)
			delete mRequests[i];
			
		mRequests.clear();
	}
	
	// Deleting the requests deletes the responses
	// Deleting the responses deletes the contents
	// So the stripes are already deleted
	
	mStripes.clear();
}

void Splicer::search(Set<Identifier> &sources)
{
	const unsigned timeout = Config::Get("request_timeout").toInt();
	
	Request request(mTarget.toString(), false);
	request.submit();
	request.wait(timeout);

	Synchronize(&request);
	for(int i=0; i<request.responsesCount(); ++i)
	{
		Request::Response *response = request.response(i);
		const StringMap &parameters = response->parameters();
		
		if(!response->error())
		{
			if(mName.empty() && parameters.contains("name")) 
				mName = parameters.get("name");
			
			// TODO: check size
			if(mSize == 0 && parameters.contains("size")) 
				parameters.get("size").extract(mSize);
			
			sources.insert(response->peering());
		}
	}
}

void Splicer::query(int i, const Identifier &source)
{
	Assert(i < mRequests.size());
	Assert(i < mStripes.size());
  
  	size_t block = 0;
	size_t offset = 0;

	if(mStripes[i])
	{
		block = mStripes[i]->tellWriteBlock();
		offset = 0;
		// TODO
		//offset = mStripes[i]->tellWriteOffset();
		mStripes[i]->flush();
	}
	else {
		block = mFirstBlock;
		offset = 0;
	}

	if(mRequests[i]) delete mRequests[i];
	mStripes[i] = NULL;
	mRequests[i] = NULL;
  
	File *file = new File(mFileName, File::Write);
	StripedFile *striped = new StripedFile(file, mBlockSize, mNbStripes, i);
	striped->seekWrite(block, offset);
	mStripes[i] = striped;
		
	StringMap parameters;
	parameters["block-size"] << mBlockSize;
	parameters["stripes-count"] << mNbStripes;
	parameters["stripe"] << i;
	parameters["block"] << block;
	parameters["offset"] << offset;
	
	Request *request = new Request;
	request->setTarget(mTarget.toString(),true);
	request->setParameters(parameters);
	request->setContentSink(striped);
	request->submit(source);
	mRequests[i] = request;
}

}
