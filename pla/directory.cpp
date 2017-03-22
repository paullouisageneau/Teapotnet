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

#include "pla/directory.hpp"
#include "pla/exception.hpp"
#include "pla/file.hpp"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#ifdef WINDOWS
#define stat _stat
#define PATH_SEPARATOR '\\'
#else
#ifndef ANDROID
#include <sys/statvfs.h>
#else
#include <sys/vfs.h>
#define statvfs statfs
#define fstatvfs fstatfs
#endif
#include <pwd.h>
#define PATH_SEPARATOR '/'
#endif

namespace pla
{

const char Directory::Separator = PATH_SEPARATOR;

bool Directory::Exist(const String &path)
{
	/*DIR *dir = opendir(fixPath(path).pathEncode().c_str());
	if(!dir) return false;
	closedir(dir);
	return true;*/

	stat_t st;
	if(pla::stat(fixPath(path).pathEncode().c_str(), &st)) return false;
	return S_ISDIR(st.st_mode);
}

bool Directory::Remove(const String &path, bool recursive)
{
	if(recursive)
	{
		if(!Directory::Exist(path)) return false;

		try {
			Directory dir(path);
			while(dir.nextFile())
			{
				Assert(dir.fileName() != "..");
				if(dir.fileIsDirectory()) Directory::Remove(dir.filePath(), true);
				else File::Remove(dir.filePath());
			}
		}
		catch(...)
		{

		}
	}

	return (rmdir(fixPath(path).pathEncode().c_str()) == 0);
}

void Directory::Create(const String &path)
{
	if(mkdirmod(fixPath(path).pathEncode().c_str(), 0770) != 0)
		throw Exception("Cannot create directory \""+path+"\"");
}

uint64_t Directory::GetAvailableSpace(const String &path)
{
#ifdef WINDOWS
	ULARGE_INTEGER freeBytesAvailable;
	std::memset(&freeBytesAvailable, 0, sizeof(freeBytesAvailable));
	if(!GetDiskFreeSpaceEx(path.c_str(), &freeBytesAvailable, NULL, NULL))
		throw Exception("Unable to get free space for " + path);
	return uint64_t(freeBytesAvailable.QuadPart);
#else
	struct statvfs f;
	if(statvfs(path, &f)) throw Exception("Unable to get free space for " + path);
	return uint64_t(f.f_bavail) * uint64_t(f.f_bsize);
#endif
}

String Directory::GetHomeDirectory(void)
{
#ifdef WINDOWS
	char szPath[MAX_PATH];
	/*if(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, szPath) == S_OK)
		return String(szPath);*/
	if(ExpandEnvironmentStrings("%USERPROFILE%", szPath, MAX_PATH))
		return String(szPath);
#else
	const char *home = getenv("HOME");
	if(!home)
	{
		struct passwd* pwd = getpwuid(getuid());
		if(pwd) home = pwd->pw_dir;
	}

	if(home) return String(home);
#endif

	throw Exception("Unable to get home directory path");
}

void Directory::ChangeCurrent(const String &path)
{
	if(chdir(fixPath(path).pathEncode().c_str()) != 0)
		throw Exception("Cannot change current directory to \""+path+"\"");
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

	mDir = opendir(fixPath(path).pathEncode().c_str());
	if(!mDir) throw Exception(String("Unable to open directory: ")+path);

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
	if(!mDirent) throw Exception("No more files in directory");
	return mPath + String(mDirent->d_name).pathDecode();
}

String Directory::fileName(void) const
{
	if(!mDirent) throw Exception("No more files in directory");
	return String(mDirent->d_name).pathDecode();
}

Time Directory::fileTime(void) const
{
	if(!mDirent) throw Exception("No more files in directory");
	stat_t st;
	if(pla::stat(filePath().pathEncode().c_str(), &st)) return 0;
	return Time(st.st_mtime);
}

uint64_t Directory::fileSize(void) const
{
	if(!mDirent) throw Exception("No more files in directory");
	if(fileIsDirectory()) return 0;
	stat_t st;
	if(pla::stat(filePath().pathEncode().c_str(), &st)) return 0;
	return uint64_t(st.st_size);
}

bool Directory::fileIsDirectory(void) const
{
	if(!mDirent) throw Exception("No more files in directory");
	//return (mDirent->d_type == DT_DIR);
	return Exist(filePath());
}

void Directory::getFileInfo(StringMap &map) const
{
	// Note: fileInfo must not contain path

	map.clear();
	map["name"] =  fileName();
	map["time"] << fileTime();

	if(fileIsDirectory()) map["type"] =  "directory";
	else {
		map["type"] =  "file";
		map["size"] << fileSize();
	}
}

String Directory::fixPath(String path)
{
	if(path.empty()) throw Exception("Empty path");
	if(path.size() >= 2 && path[path.size()-1] == Separator) path.resize(path.size()-1);
#ifdef WINDOWS
	if(path.size() == 2 && path[path.size()-1] == ':') path+= Separator;
#endif
	return path;
}

}
