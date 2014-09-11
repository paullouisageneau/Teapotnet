/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#ifndef TPN_TRACKER_H
#define TPN_TRACKER_H

#include "tpn/include.h"
#include "tpn/identifier.h"

#include "pla/http.h"
#include "pla/synchronizable.h"
#include "pla/address.h"
#include "pla/array.h"
#include "pla/map.h"
#include "pla/time.h"

namespace tpn
{

class Tracker : protected Synchronizable, public Http::Server
{
public:
	static const double EntryLife;
  
	Tracker(int port = 8080);
	~Tracker(void);

private:
	typedef Map<Identifier, Map<Address, Time> > map_t;
	map_t mMap;
	map_t::iterator mCleaner;
	
	void process(Http::Request &request);
	void clean(Storage &s, int nbr = -1);
	void insert(const Identifier &identifier, const Address &addr);
	void retrieve(const Identifier &identifier, Stream &output) const;
	bool contains(const Identifier &identifier);
};

}

#endif
