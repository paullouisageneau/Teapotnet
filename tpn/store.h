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

#ifndef TPN_STORE_H
#define TPN_STORE_H

#include "tpn/include.h"
#include "tpn/thread.h"
#include "tpn/synchronizable.h"
#include "tpn/serializable.h"
#include "tpn/resource.h"
#include "tpn/file.h"
#include "tpn/map.h"
#include "tpn/set.h"
#include "tpn/array.h"
#include "tpn/list.h"
#include "tpn/http.h"
#include "tpn/interface.h"
#include "tpn/mutex.h"
#include "tpn/database.h"

namespace tpn
{
  
class Store : protected Synchronizable
{
public:
	static Store *Instance;
	
	Store(void);
	~Store(void);
	
	// Resource-level access
	bool get(const BinaryString &digest, Resource &resource);
	
	// Block-level access
	void waitBlock(const BinaryString &digest);
	File *getBlock(const BinaryString &digest, int64_t &size);
	void notifyBlock(const BinaryString &digest, const String &filename, int64_t offset, int64_t size);
	void notifyFileErasure(const String &filename);

private:
	Database *mDatabase;
};

}

#endif
