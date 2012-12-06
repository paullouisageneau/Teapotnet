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

#include "file.h"
#include "exception.h"

namespace tpot
{

bool File::Exist(const String &filename)
{
	if(filename.empty()) return false;
  
	/*std::fstream file;
	file.open(filename.c_str(), std::ios_base::in);
	return file.is_open();*/
	
	stat_t st;
	if(stat(filename.c_str(), &st)) return false;
	if(!S_ISDIR(st.st_mode)) return true;
	else return false;
}

bool File::Remove(const String &filename)
{
	return (std::remove(filename.c_str()) == 0);
}

void File::Rename(const String &source, const String &destination)
{
	if(!Exist(source)) throw IOException(String("Rename: source file does not exist: ") + source);
	if(std::rename(source.c_str(), destination.c_str()) == 0) return;
	
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
	if(stat(filename.c_str(), &st)) throw IOException("File does not exist: "+filename);
	return uint64_t(st.st_size);
}

tpot::Time File::Time(const String &filename)
{
	stat_t st;
	if(stat(filename.c_str(), &st)) throw IOException("File does not exist: "+filename);
	return tpot::Time(st.st_mtime);
}

String File::TempName(void)
{
	String tempPath;
#ifdef WINDOWS
	char buffer[MAX_PATH+1];
	Assert(GetTempPath(MAX_PATH+1,buffer) != 0);
	tempPath = buffer;
#else
	tempPath = "/tmp/";
#endif
	 
	String fileName;
	do fileName = tempPath + String::random(16);
	while(File::Exist(fileName));
	
	return fileName;
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
	close();
}

void File::open(const String &filename, OpenMode mode)
{
	close();
	if(filename.empty()) throw IOException("Empty file name");

	mName = filename;

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

	std::fstream::open(filename.c_str(), m|std::ios_base::binary);
	if(!std::fstream::is_open() || std::fstream::bad()) throw IOException(String("Unable to open file: ")+filename);
}

void File::close(void)
{
	// mName MUST NOT be changed here
	if(std::fstream::is_open())
		std::fstream::close();
	
	std::fstream::clear();	// clear state
}

void File::seekRead(uint64_t position)
{
	uint64_t step = std::numeric_limits<std::streamoff>::max();
	std::fstream::seekg(0, std::fstream::beg);
	do {
		uint64_t offset = std::min(position, step);
		position-= offset;
		std::fstream::seekg(std::streamoff(offset), std::fstream::cur);
	}
	while(position != 0);
}

void File::seekWrite(uint64_t position)
{
	uint64_t step = std::numeric_limits<std::streamoff>::max();
	std::fstream::seekp(0, std::fstream::beg);
	do {
		uint64_t offset = std::min(position, step);
		position-= offset;
		std::fstream::seekp(std::streamoff(offset), std::fstream::cur);
	}
	while(position != 0);  
}

String File::name(void) const
{
	return mName; 
}

uint64_t File::size(void) const
{
	return Size(mName);
}

size_t File::readData(char *buffer, size_t size)
{
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

ByteStream *File::pipeIn(void)
{
	// Somehow using Append here can result in a write failure
	File *file = new File(mName, Write);
	file->seekWrite(size());
	return file;
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
		Rename(mName, mTarget);
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
