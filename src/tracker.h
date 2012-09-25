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

#ifndef TPOT_TRACKER_H
#define TPOT_TRACKER_H

#include "include.h"
#include "http.h"
#include "synchronizable.h"
#include "address.h"
#include "identifier.h"
#include "array.h"
#include "map.h"

namespace tpot
{

class Tracker : protected Synchronizable, public Http::Server
{
public:
	static const time_t EntryLife;
  
	Tracker(int port = 8080);
	~Tracker(void);

private:
	void process(Http::Request &request);
	void insert(const Identifier &identifier, const Address &addr);
	void retrieve(const Identifier &identifier, Array<Address> &array);

	typedef Map<Identifier, Map<Address,time_t> > map_t;
	map_t mMap;
	map_t::iterator mIterator; 
};

}

#endif
