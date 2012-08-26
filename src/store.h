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

#ifndef ARC_STORE_H
#define ARC_STORE_H

#include "include.h"
#include "identifier.h"
#include "map.h"

namespace arc
{

class Store
{
public:
	static const String DatabaseDirectory;
	static const size_t ChunkSize;

	static Store *Instance;

	Store(void);
	~Store(void);

	void addDirectory(const String &path);
	void removeDirectory(const String &path);

	void refresh(void);
	void refreshDirectory(const String &directoryPath);

	ByteStream *get(const Identifier &identifier);
	ByteStream *get(const String &url);

	bool info(const Identifier &identifier, StringMap &map);
	bool info(const String &url, StringMap &map);

protected:
	Set<String> mDirectories;
	Map<Identifier,String> mFiles;
};

}

#endif
