/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Plateform.                                     *
 *                                                                       *
 *   Plateform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Plateform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Plateform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef PLA_DIRECTORY_H
#define PLA_DIRECTORY_H

#include "pla/include.hpp"
#include "pla/string.hpp"
#include "pla/time.hpp"
#include "pla/map.hpp"

namespace pla
{

class Directory
{
public:
	static const char Separator;

	static bool Exist(const String &path);
	static bool Remove(const String &path, bool recursive = false);
	static void Create(const String &path);
	static uint64_t GetAvailableSpace(const String &path);
	static String GetHomeDirectory(void);
	static void ChangeCurrent(const String &path);

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
	Time fileTime(void) const;
	uint64_t fileSize(void) const;
	bool fileIsDirectory(void)  const;

	void getFileInfo(StringMap &map) const;	// Get all attributes

private:
	static String fixPath(String path);

	String mPath;
	DIR *mDir;
	struct dirent *mDirent;
};

}

#endif
