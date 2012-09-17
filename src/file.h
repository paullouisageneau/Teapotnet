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

#ifndef TPOT_FILE_H
#define TPOT_FILE_H

#include "stream.h"
#include "bytestream.h"
#include "string.h"

#include <fstream>

namespace tpot
{

class File : public Stream, public ByteStream, public std::fstream
{
public:
	using Stream::read;
	using Stream::write;
	using ByteStream::ignore;

	static bool Exist(const String &filename);
	static bool Remove(const String &filename);
	static void Rename(const String &source, const String &destination);

	enum OpenMode { Read, Write, ReadWrite, Append, Truncate };

	File(void);
	File(const String &filename, OpenMode mode = ReadWrite);
	virtual ~File(void);

	virtual void open(const String &filename, OpenMode mode = ReadWrite);
	virtual void close(void);
	
	size_t size(void);

	// Stream, ByteStream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);

protected:
	ByteStream *pipeIn(void);
	String mName;
};

class SafeWriteFile : public File
{
public:
	SafeWriteFile(void);
	SafeWriteFile(const String &filename);
	~SafeWriteFile(void);
	
	void open(const String &filename);
	void close(void);

private:
	String mTarget;
};

}

#endif
