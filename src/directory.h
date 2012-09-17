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

#ifndef TPOT_DIRECTORY_H
#define TPOT_DIRECTORY_H

#include "include.h"
#include "string.h"
#include "map.h"

#include "dirent.h"

namespace tpot
{

class Directory
{
public:
	static const char Separator;

	static bool Exist(const String &path);
	static bool Remove(const String &path);
	static void Create(const String &path);

	Directory(void);
	Directory(const String &path);
	~Directory(void);

	void open(const String &path);
	void close(void);

	String path(void) const;	// trailing separator added if necessary

	bool nextFile(void);

	// Path
	String filePath(void) const;

	// Attributes
	String fileName(void) const;
	time_t fileTime(void) const;
	size_t fileSize(void) const;
	bool fileIsDir(void)  const;

	void getFileInfo(StringMap &map) const;	// Get all attributes

private:
	String mPath;
	DIR *mDir;
	struct dirent *mDirent;
};

}

#endif
