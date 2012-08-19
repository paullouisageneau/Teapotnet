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

#ifndef ARC_CORE_H
#define ARC_CORE_H

#include "include.h"
#include "address.h"
#include "stream.h"
#include "socket.h"
#include "pipe.h"
#include "thread.h"
#include "mutex.h"
#include "signal.h"
#include "identifier.h"
#include "resource.h"
#include "synchronizable.h"
#include "map.h"
#include "array.h"

namespace arc
{

class Core
{
public:
	Core(void);
	~Core(void);

	void add(Socket *sock);

protected:
	class Handler : public Thread, public Synchronizable
	{
	public:
		Handler(Core *core, Socket *sock);
		~Handler(void);

	protected:
		void run(void);

	private:
		Core	*mCore;
		Socket  *mSock;
		Handler *mHandler;
		Map<unsigned, ByteStream*> mChannels;
	};

	void add(const Address &addr, Handler *Handler);
	void remove(const Address &addr);

	Map<Address,Handler*> mHandlers;

	friend class Handler;
};

}

#endif
