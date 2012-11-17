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

#include "directory.h"
#include "exception.h"
#include "file.h"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#ifdef WINDOWS
#define stat _stat
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

namespace tpot
{

const char Directory::Separator = PATH_SEPARATOR;

bool Directory::Exist(const String &path)
{
	if(path.empty()) return false;	
  
	/*DIR *dir = opendir(path.c_str());
	if(!dir) return false;
	closedir(dir);*/
	
	stat_t st;
	if(stat(path.c_str(), &st)) return false;
	return S_ISDIR(st.st_mode);
}

bool Directory::Remove(const String &path)
{
	return (rmdir(path.c_str()) == 0);
}

void Directory::Create(const String &path)
{
	if(mkdir(path.c_str(), 0770) != 0)
		throw IOException("Cannot create directory \""+path+"\"");
}

Directory::Directory(void) :
		mDir(NULL),
		mDirent(NULL)
{

}

Directory::Directory(const String &path) :
		mDir(NULL),
		mDirent(NULL)
{
	open(path);
}

Directory::~Directory(void)
{
	close();
}

void Directory::open(const String &path)
{
	close();
	if(path.empty()) return;

	mDir = opendir(path.c_str());
	if(!mDir) throw IOException(String("Unable to open directory: ")+path);

	mPath = path;
	if(mPath[mPath.size()-1] != Separator)
		mPath+= Separator;
}

void Directory::close(void)
{
	if(mDir)
	{
		closedir(mDir);
		mDir = NULL;
		mDirent = NULL;
		mPath.clear();
	}
}

String Directory::path(void) const
{
	return mPath;
}

bool Directory::nextFile(void)
{
	if(!mDir) return false;
	mDirent = readdir(mDir);
	if(!mDirent) return false;

	if(fileName() == ".")  return nextFile();
	if(fileName() == "..") return nextFile();
	return true;
}

String Directory::filePath(void) const
{
	if(!mDirent) throw IOException("No more files in directory");
	return mPath+String(mDirent->d_name);
}


String Directory::fileName(void) const
{
	if(!mDirent) throw IOException("No more files in directory");
	return String(mDirent->d_name);
}

time_t Directory::fileTime(void) const
{
	if(!mDirent) throw IOException("No more files in directory");
	struct stat attrib;
	stat(filePath().c_str(), &attrib);
	return attrib.st_mtime;
}

size_t Directory::fileSize(void) const
{
	if(!mDirent) throw IOException("No more files in directory");
	if(fileIsDir()) return 0;
	return File::Size(filePath());
}

bool Directory::fileIsDir(void) const
{
	if(!mDirent) throw IOException("No more files in directory");
	//return (mDirent->d_type == DT_DIR);
	return Exist(filePath());
}

void Directory::getFileInfo(StringMap &map) const
{
	// Note: fileInfo must not contain path
	
	map.clear();
	map["name"] =  fileName();
	map["time"] << fileTime();
	
	if(fileIsDir()) map["type"] =  "directory";
	else {
		map["type"] =  "file";
		map["size"] << fileSize();
	}
}

}
