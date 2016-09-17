/*************************************************************************
 *   Copyright (C) 2011-2015 by Paul-Louis Ageneau                       *
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

#ifndef TPN_CACHE_H
#define TPN_CACHE_H

#include "tpn/include.hpp"

#include "pla/scheduler.hpp"
#include "pla/string.hpp"
#include "pla/binarystring.hpp"
#include "pla/set.hpp"

namespace tpn
{

class Cache
{
public:
	static Cache *Instance;

	Cache(void);
	~Cache(void);
	
	bool prefetch(const BinaryString &target);	// Asynchronous resource prefetching (true is already available)
	String move(const String &filename);
	String path(const BinaryString &digest) const;
	
	// TODO: cleaning
	
private:
	String mDirectory;
	Scheduler mScheduler;
};

}

#endif
