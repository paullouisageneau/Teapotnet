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
#include "array.h"
#include "map.h"

namespace arc
{

class Core;

class Request : public Synchronizable
{
public:
	Request(void);
	virtual ~Request(void);

	unsigned id() const;
	const String &target(void) const;
	void isPending() const;

	void setBroadcast(void);
	void setMulticast(const Identifier &group);
	void setUnicast(const Identifier &destination);

	void setTarget(const String &target);
	void setParameters(StringMap &params);
	void setParameter(const String &name, const String &value);		

	void submit(void);
	void cancel(void);

	void addPending();
	void removePending();

	class Response
	{
	public:
		Response(const String &status, const StringMap &parameters);

		const String &status(void) const;
		const StringMap &parameters(void) const;
		const String &parameter(const String &name) const;
		bool parameter(const String &name, String &value) const;
	
	private:
		String mStatus;
		StringMap mParameters;
	};

	int responses(void) const;
	Response *response(int num);

private:
	unsigned mId;
	Identifier mTarget;
	int mPendingCount;

	Array<Response> mResponses;
	Map<Identifier, Response*> mResponsesMap;


	friend class Core;
};

}

#endif

