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

#include "tpn/include.hpp"

#include "pla/http.hpp"
#include "pla/synchronizable.hpp"
#include "pla/address.hpp"
#include "pla/binarystring.hpp"
#include "pla/array.hpp"
#include "pla/map.hpp"
#include "pla/time.hpp"

namespace tpn
{

class Tracker : protected Synchronizable, public Http::Server
{
public:
	static const double EntryLife;
  
	Tracker(int port = 8080);
	~Tracker(void);

private:
	Map<BinaryString, Map<Address, Time> > mMap;
	Map<BinaryString, Map<Address, Time> >::iterator mCleaner;
	
	void process(Http::Request &request);
	void clean(int count = -1);
	void insert(const BinaryString &node, const Address &addr);
	void retrieve(const BinaryString &node, int count, SerializableMap<BinaryString, SerializableSet<Address> > &result) const;
};

}

#endif
