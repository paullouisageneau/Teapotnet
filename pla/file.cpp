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

#include "pla/file.hpp"
#include "pla/exception.hpp"
#include "pla/directory.hpp"

namespace pla
{

String File::TempDirectory = "";	// empty = auto
String File::TempPrefix    = "pla_";
  
bool File::Exist(const String &filename)
{
	if(filename.empty()) return false;
  
	/*std::fstream file;
	file.open(filename.pathEncode().c_str(), std::ios_base::in);
	return file.is_open();*/
	
	stat_t st;
	if(pla::stat(filename.pathEncode().c_str(), &st)) return false;
	if(!S_ISDIR(st.st_mode)) return true;
	else return false;
}

bool File::Remove(const String &filename)
{
	return (std::remove(filename.pathEncode().c_str()) == 0);
}

void File::Rename(const String &source, const String &destination)
{
	if(!Exist(source)) throw Exception(String("Rename: source file does not exist: ") + source);
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
	if(pla::stat(filename.pathEncode().c_str(), &st)) throw Exception("File does not exist: "+filename);
	return uint64_t(st.st_size);
}

pla::Time File::Time(const String &filename)
{
	stat_t st;
	if(pla::stat(filename.pathEncode().c_str(), &st)) throw Exception("File does not exist: "+filename);
	return pla::Time(st.st_mtime);
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
	if(!TempDirectory.empty())
	{
		return TempDirectory + "/";
	}
	else {
		String tempPath;

#ifdef WINDOWS
		char buffer[MAX_PATH+1];
		Assert(GetTempPath(MAX_PATH+1,buffer) != 0);
		tempPath = buffer;
#else
		tempPath = "/tmp/";
#endif
		
		return tempPath;
	}
}

File::File(void) :
	mReadPosition(0),
	mWritePosition(0)
{

}

File::File(const String &filename, File::OpenMode mode) :
	mReadPosition(0),
	mWritePosition(0)
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
	if(filename.empty()) throw Exception("Empty file name");

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
	if(!std::fstream::is_open() || std::fstream::bad()) throw Exception(String("Unable to open file: ")+filename);
	
	mReadPosition = 0;
	mWritePosition = 0;
	
	if(m & std::ios_base::app)
		mWritePosition = size();
}

void File::close(void)
{
	// mName MUST NOT be changed here
	if(std::fstream::is_open())
		std::fstream::close();
	
	std::fstream::clear();	// clear state
}

File::OpenMode File::openMode(void) const
{
	return mMode;  
}

void  File::reopen(OpenMode mode)
{
	close();
	mMode = mode;
	open(mName, mode);
}

void File::seekRead(int64_t position)
{
	mReadPosition = position;
	
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
	mWritePosition = position;
	
	int64_t step = std::numeric_limits<std::streamoff>::max();	// streamoff is signed
	std::fstream::seekp(0, std::fstream::beg);
	do {
		int64_t offset = std::min(position, step);
		position-= offset;
		std::fstream::seekp(std::streamoff(offset), std::fstream::cur);
	}
	while(position != 0);  
}

int64_t File::tellRead(void) const
{
	return mReadPosition;
}

int64_t File::tellWrite(void) const
{
	return mWritePosition;
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
	
	if(std::fstream::bad()) throw Exception(String("Unable to read from file: ") + mName);
	
	mReadPosition+= std::fstream::gcount();
	return std::fstream::gcount();
}

void File::writeData(const char *data, size_t size)
{
	std::fstream::clear();	// clear state
	std::fstream::write(data,size);
	if(std::fstream::bad()) throw Exception(String("Unable to write to file: ") + mName);
	
	mWritePosition+= std::fstream::gcount();
}

void File::flush(void)
{
	std::fstream::flush(); 
}

bool File::skipMark(void)
{
	if(mReadPosition == 0)
	{
		// Skip UTF-8 BOM
		char buf[3];
		if(readData(buf, 3) == 3)
		{
			if(uint8_t(buf[0]) == 0xEF
				&& uint8_t(buf[1]) == 0xBB
				&& uint8_t(buf[2]) == 0xBF)
			{
				return true;
			}
		}
		
		seekRead(0);
	}
	
	return false;
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
	if(filename.empty()) throw Exception("Empty file name");
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
