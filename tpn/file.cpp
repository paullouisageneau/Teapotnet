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

#include "tpn/file.h"
#include "tpn/exception.h"
#include "tpn/directory.h"
#include "tpn/config.h"

namespace tpn
{

const String File::TempPrefix = "tpot_";
  
bool File::Exist(const String &filename)
{
	if(filename.empty()) return false;
  
	/*std::fstream file;
	file.open(filename.pathEncode().c_str(), std::ios_base::in);
	return file.is_open();*/
	
	stat_t st;
	if(tpn::stat(filename.pathEncode().c_str(), &st)) return false;
	if(!S_ISDIR(st.st_mode)) return true;
	else return false;
}

bool File::Remove(const String &filename)
{
	return (std::remove(filename.pathEncode().c_str()) == 0);
}

void File::Rename(const String &source, const String &destination)
{
	if(!Exist(source)) throw IOException(String("Rename: source file does not exist: ") + source);
	if(Exist(destination)) Remove(destination);
	
	if(std::rename(source.pathEncode().c_str(), destination.pathEncode().c_str()) == 0) return;
	
	// std::rename will fail to copy between filesystems, so we need to try manually
	File sourceFile(source, Read);
	File destinationFile(destination, Truncate);
	sourceFile.read(destinationFile);
	sourceFile.close();
	destinationFile.close();
	Remove(source);
}

uint64_t File::Size(const String &filename)
{
	stat_t st;
	if(tpn::stat(filename.pathEncode().c_str(), &st)) throw IOException("File does not exist: "+filename);
	return uint64_t(st.st_size);
}

tpn::Time File::Time(const String &filename)
{
	stat_t st;
	if(tpn::stat(filename.pathEncode().c_str(), &st)) throw IOException("File does not exist: "+filename);
	return tpn::Time(st.st_mtime);
}

String File::TempName(void)
{
	String tempPath = TempPath();
	String fileName;
	do fileName = tempPath + TempPrefix + String::random(16);
	while(File::Exist(fileName));
	return fileName;
}

void File::CleanTemp(void)
{
	try {
		Directory dir(TempPath());
		while(dir.nextFile())
		try {
			if(dir.fileName().substr(0,TempPrefix.size()) == TempPrefix)
				File::Remove(dir.filePath());
		}
		catch(...)
		{
		
		}
	}
	catch(const Exception &e)
	{
		Log("File::CleanTemp", "Warning: Unable to access temp directory");
	}
}

String File::TempPath(void)
{
	String tempPath = Config::Get("temp_dir");
	if(tempPath.empty() || tempPath == "auto")
	{
		#ifdef WINDOWS
			char buffer[MAX_PATH+1];
			Assert(GetTempPath(MAX_PATH+1,buffer) != 0);
			tempPath = buffer;
		#else
			tempPath = "/tmp/";
		#endif
	}
	else {
		if(!Directory::Exist(tempPath))
			Directory::Create(tempPath);
		tempPath+= '/';
	}
	
	return tempPath;
}

File::File(void)
{

}

File::File(const String &filename, File::OpenMode mode)
{
	open(filename,mode);
}

File::~File(void)
{
	NOEXCEPTION(close());
}

void File::open(const String &filename, OpenMode mode)
{
	close();
	if(filename.empty()) throw IOException("Empty file name");

	mName = filename;
	mMode = mode;

	std::ios_base::openmode m;
	switch(mode)
	{
	case Read:		m = std::ios_base::in;						break;
	case Write:		m = std::ios_base::out;						break;
	case Append:		m = std::ios_base::app;						break;
	case Truncate:		m = std::ios_base::out|std::ios_base::trunc;			break;
	case TruncateReadWrite:	m = std::ios_base::in|std::ios_base::out|std::ios_base::trunc;	break;
	default:		m = std::ios_base::in|std::ios_base::out;			break;
	}

	std::fstream::open(filename.pathEncode().c_str(), m|std::ios_base::binary);
	if(!std::fstream::is_open() || std::fstream::bad()) throw IOException(String("Unable to open file: ")+filename);
}

void File::close(void)
{
	// mName MUST NOT be changed here
	if(std::fstream::is_open())
		std::fstream::close();
	
	std::fstream::clear();	// clear state
}

void  File::reopen(OpenMode mode)
{
	close();
	mMode = mode;
	open(mName, mode);
}

void File::seekRead(int64_t position)
{
	int64_t step = std::numeric_limits<std::streamoff>::max();	// streamoff is signed
	std::fstream::seekg(0, std::fstream::beg);
	do {
		int64_t offset = std::min(position, step);
		position-= offset;
		std::fstream::seekg(std::streamoff(offset), std::fstream::cur);
	}
	while(position != 0);
}

void File::seekWrite(int64_t position)
{
	int64_t step = std::numeric_limits<std::streamoff>::max();	// streamoff is signed
	std::fstream::seekp(0, std::fstream::beg);
	do {
		int64_t offset = std::min(position, step);
		position-= offset;
		std::fstream::seekp(std::streamoff(offset), std::fstream::cur);
	}
	while(position != 0);  
}

String File::name(void) const
{
	return mName; 
}

File::OpenMode File::mode(void) const
{
	return mMode;
}

uint64_t File::size(void) const
{
	return Size(mName);
}

size_t File::readData(char *buffer, size_t size)
{
#ifdef __clang__
	// Hack to fix a bug with readsome() not resetting gcount
	std::fstream::peek();
#endif
	
	std::fstream::clear();	// clear state
	std::fstream::readsome(buffer, size);
	if(!std::fstream::gcount() && std::fstream::good())
		std::fstream::read(buffer, size);
	
	if(std::fstream::bad()) throw IOException(String("Unable to read from file: ") + mName);
	return std::fstream::gcount();
}

void File::writeData(const char *data, size_t size)
{
	std::fstream::clear();	// clear state
	std::fstream::write(data,size);
	if(std::fstream::bad()) throw IOException(String("Unable to write to file: ") + mName);
}

void File::flush(void)
{
	std::fstream::flush(); 
}

Stream *File::pipeIn(void)
{
	// Somehow using Append here can result in a write failure
	try {
		File *file = new File(mName, ReadWrite);
		file->seekWrite(size());
		return file;
	}
	catch(...)
	{
		return NULL; 
	}
}

SafeWriteFile::SafeWriteFile(void)
{

}

SafeWriteFile::SafeWriteFile(const String &filename)
{
	open(filename);
}

SafeWriteFile::~SafeWriteFile(void)
{

}

void SafeWriteFile::open(const String &filename, OpenMode mode)
{
	close();
	if(filename.empty()) throw IOException("Empty file name");
	Assert(mode == Truncate);
	File::open(filename+".tmp", Truncate);
	mTarget = filename;
}

void SafeWriteFile::close(void)
{
	File::close();
	if(!mTarget.empty())
	{
		if(!std::fstream::bad()) Rename(mName, mTarget);
		Remove(mName);
		mName = mTarget;
		mTarget.clear();
	}
}

TempFile::TempFile(void) :
	File(TempName(), TruncateReadWrite)
{
  
}

TempFile::TempFile(const String &filename) :
	File(filename, TruncateReadWrite)
{
	  
}

TempFile::~TempFile(void)
{
	File::close();
	if(File::Exist(mName)) File::Remove(mName);  
}

void TempFile::open(const String &filename, OpenMode mode)
{
	File::open(filename, mode);
}

void TempFile::close(void)
{
	File::close();
}

}
