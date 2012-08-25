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

#ifndef ARC_REQUEST_H
#define ARC_REQUEST_H

#include "include.h"
#include "synchronizable.h"
#include "identifier.h"
#include "bytestream.h"
#include "synchronizable.h"
#include "array.h"
#include "map.h"

namespace arc
{

class Core;

class Request : public Synchronizable
{
public:
	Request(const String &target = "");
	virtual ~Request(void);

	unsigned id() const;
	const String &target(void) const;
	bool isPending() const;

	void setContentSink(ByteStream *bs);

	void setTarget(const String &target);
	void setParameters(StringMap &params);
	void setParameter(const String &name, const String &value);

	void submit(void);
	void submit(const Identifier &receiver);
	void cancel(void);
	void execute(void);

	void addPending();
	void removePending();

	class Response
	{
	public:
		Response(const String &status, const StringMap &parameters, ByteStream *sink = NULL);
		~Response(void);

		const String &status(void) const;
		const StringMap &parameters(void) const;
		String parameter(const String &name) const;
		bool parameter(const String &name, String &value) const;
		Pipe *content(void) const;

	private:
		String mStatus;
		StringMap mParameters;
		Pipe *mContent;
		bool mIsSent;

		friend class Core;
	};

	int responsesCount(void) const;
	Response *response(int num);

private:
	int addResponse(Response *response);

	Identifier mReceiver;
	String mTarget;
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

