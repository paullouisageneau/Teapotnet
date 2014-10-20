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

#ifndef TPN_STORE_H
#define TPN_STORE_H

#include "tpn/include.h"
#include "tpn/database.h"
#include "tpn/fountain.h"

#include "pla/synchronizable.h"
#include "pla/binarystring.h"
#include "pla/file.h"
#include "pla/map.h"

namespace tpn
{
  
class Store : protected Synchronizable
{
public:
	static Store *Instance;
	
	Store(void);
	~Store(void);
	
	bool push(const BinaryString &digest, Stream &input);
	bool pull(const BinaryString &digest, Stream &output, unsigned *tokens = NULL);
	
	bool hasBlock(const BinaryString &digest);
	void waitBlock(const BinaryString &digest);
	bool waitBlock(const BinaryString &digest, double &timeout);
	File *getBlock(const BinaryString &digest, int64_t &size);
	void notifyBlock(const BinaryString &digest, const String &filename, int64_t offset, int64_t size);
	void notifyFileErasure(const String &filename);

private:
	Database *mDatabase;
	String mCacheDirectory;
	Map<BinaryString,Fountain::Sink> mSinks;
};

}

#endif
