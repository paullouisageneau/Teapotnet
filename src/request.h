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

#ifndef TPOT_REQUEST_H
#define TPOT_REQUEST_H

#include "include.h"
#include "synchronizable.h"
#include "identifier.h"
#include "bytestream.h"
#include "array.h"
#include "map.h"
#include "store.h"

namespace tpot
{

class AddressBook;

class Request : public Synchronizable
{
public:
	Request(const String &target = "", bool data = true);
	virtual ~Request(void);

	unsigned id() const;
	const String &target(void) const;

	void setContentSink(ByteStream *bs);
	void setTarget(const String &target, bool data);
	void setParameters(StringMap &params);
	void setParameter(const String &name, const String &value);

	void submit(void);
	void submit(const Identifier &receiver);
	void cancel(void);
	bool execute(AddressBook *addressBook, Store *store);

	const Identifier &receiver(void) const;
	
	bool isPending() const;
	void addPending(const Identifier &peering);
	void removePending(const Identifier &peering);
	
	class Response
	{
	public:
		static const int Finished = -1;
		static const int Success = 0;
		static const int Pending = 1;
		static const int Failed = 2;
		static const int NotFound = 3;
		static const int Interrupted = 4;
		static const int ReadFailed = 5;
	  
		Response(int status = 0);
		Response(int status, const StringMap &parameters, ByteStream *content = NULL);
		~Response(void);

		const Identifier &peering(void) const;
		const StringMap &parameters(void) const;
		String parameter(const String &name) const;
		bool parameter(const String &name, String &value) const;
		Pipe *content(void) const;
		
		int status(void) const;
		bool error(void) const;
		bool finished(void) const;
		
	private:
	  	int mStatus;
		Identifier mPeering;
		StringMap mParameters;
		Pipe *mContent;
		
		unsigned mChannel;	// Core uses mChannel when receiving
		bool mTransfertStarted;
		bool mTransfertFinished;
		
		friend class Core;
	};

	int responsesCount(void) const;
	Response *response(int num);
	const Response *response(int num) const;
	bool isSuccessful(void) const;

private:
	Response *createResponse(Store::Entry &entry, const StringMap &parameters, Store *store);
	int addResponse(Response *response);

	Identifier mReceiver;
	String mTarget;
	bool mIsData;
	StringMap mParameters;
	ByteStream *mContentSink;
	Synchronizable *mResponseSender;
	Address mRemoteAddr;
	
	unsigned mId;
	Set<Identifier> mPending;

	Array<Response*> mResponses;

	friend class Core;
};

}

#endif

