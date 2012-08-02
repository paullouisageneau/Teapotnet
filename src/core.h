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
#include "thread.h"
#include "mutex.h"
#include "signal.h"
#include "handler.h"
#include "synchronizable.h"

namespace arc
{

class Core
{
public:
	Core(void);
	~Core(void);

	void add(Socket *sock);

protected:
	class Pipe : public Thread, public Serializable
	{
	public:
		Pipe(Core *core, Stream *stream);
		~Pipe(void);

	protected:
		void run(void);

	private:
		Core	*mCore;
		Stream  *mStream;
		Handler *mHandler;
	};

	void add(const Address &addr, Pipe *pipe);
	void remove(const Address &addr, Pipe *pipe);

	Map<Address,Array<Pipe*> > mPipes;

	friend class Pipe;
};

}

#endif
