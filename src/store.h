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
#include "http.h"
#include "file.h"
#include "map.h"
#include "interface.h"

namespace arc
{

class Store : public HttpInterfaceable
{
public:
	static const String DatabaseDirectory;
	static const size_t ChunkSize;

	static Store *Instance;

	void addDirectory(const String &name, const String &path);
	void removeDirectory(const String &name);

	void refresh(void);

	struct Entry
	{
		Identifier 	identifier;
		String		url;
		String		path;
		StringMap	info;
		File		*content;	// file content
	};

	bool get(const Identifier &identifier, Entry &entry, bool content = true);
	bool get(const String &url, Entry &entry, bool content = true);

	void http(Http::Request &request);

private:
	Store(void);
	~Store(void);

	void refreshDirectory(const String &dirUrl, const String &dirPath);
	String urlToPath(const String &url) const;

	StringMap mDirectories;
	Map<Identifier,String> mFiles;
};

}

#endif
