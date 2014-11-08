/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#include "tpn/include.h"
#include "tpn/block.h"

#include "pla/synchronizable.h"
#include "pla/string.h"
#include "pla/binarystring.h"

namespace tpn
{

class Cache : public Synchronizable
{
public:
	static Cache *Instance;

	Cache(void);
	~Cache(void);
	
	// Asynchronous resource prefetching
	bool prefetch(const BinaryString &target);	// true is already available
	
	String move(const String &filename);
	
	// TODO: cleaning
	
	void storeMapping(const String &key, const BinaryString &value);
	bool retrieveMapping(const String &key, BinaryString &value); 
	
private:
	String mDirectory;
};

}

#endif
