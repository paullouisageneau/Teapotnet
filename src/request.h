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

#ifndef ARC_REQUEST_H
#define ARC_REQUEST_H

#include "include.h"
#include "synchronizable.h"
#include "identifier.h"
#include "bytestream.h"
#include "array.h"
#include "map.h"
#include "store.h"

namespace arc
{

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
	bool execute(Store *store);

	bool isPending() const;
	void addPending();
	void removePending();

	class Response
	{
	public:
		Response(const String &status);
		Response(const String &status, const StringMap &parameters, ByteStream *content = NULL);
		~Response(void);

		const Identifier &peering(void) const;
		const String &status(void) const;
		const StringMap &parameters(void) const;
		String parameter(const String &name) const;
		bool parameter(const String &name, String &value) const;
		Pipe *content(void) const;
		
	private:
		Identifier mPeering;
		String mStatus;
		StringMap mParameters;
		Pipe *mContent;
		bool mIsSent;

		int mPendingCount;
		
		friend class Core;
	};

	int responsesCount(void) const;
	Response *response(int num);

private:
	int addResponse(Response *response);

	Identifier mReceiver;
	String mTarget;
	bool mIsData;
	StringMap mParameters;
	ByteStream *mContentSink;
	Synchronizable *mResponseSender;

	unsigned mId;
	int mPendingCount;

	Array<Response*> mResponses;

	friend class Core;
};

}

#endif

