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

#ifndef ARC_DIRECTORY_H
#define ARC_DIRECTORY_H

#include "include.h"
#include "string.h"

#include "dirent.h"

namespace arc
{

class Directory
{
public:
	static const char Separator;

	static bool Exist(const String &path);
	static bool Remove(const String &path);

	Directory(void);
	Directory(const String &path);
	~Directory(void);

	void open(const String &path);
	void close(void);

	String path(void) const;	// trailing separator added if necessary

	bool nextFile(void);
	String fileName(void) const;
	String filePath(void) const;
	time_t fileTime(void) const;
	size_t fileSize(void) const;
	bool fileIsDir(void) const;

private:
	String mPath;
	DIR *mDir;
	struct dirent *mDirent;
};

}

#endif
