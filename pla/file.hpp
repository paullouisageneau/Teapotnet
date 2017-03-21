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

#ifndef PLA_FILE_H
#define PLA_FILE_H

#include "pla/stream.hpp"
#include "pla/string.hpp"
#include "pla/time.hpp"

#include <fstream>

namespace pla
{

class File : public Stream, public std::fstream
{
public:
	using Stream::read;
	using Stream::write;

	static String TempDirectory;
	static String TempPrefix;

	static bool Exist(const String &filename);
	static bool Remove(const String &filename);
	static void Rename(const String &source, const String &destination);
	static uint64_t Size(const String &filename);
	static pla::Time Time(const String &filename);
	static String TempName(void);
	static void CleanTemp(void);

	enum OpenMode { Read, Write, ReadWrite, Append, Truncate, TruncateReadWrite };

	File(void);
	File(const String &filename, OpenMode mode = Read);
	virtual ~File(void);

	virtual void open(const String &filename, OpenMode mode = Read);
	virtual void close(void);

	OpenMode openMode(void) const;
	void reopen(OpenMode mode);

	void seekRead(int64_t position);
	void seekWrite(int64_t position);
	int64_t tellRead(void) const;
	int64_t tellWrite(void) const;

	String name(void) const;
	OpenMode mode(void) const;
	uint64_t size(void) const;

	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *data, size_t size);
	void flush(void);
	bool skipMark(void);

protected:
	static String TempPath(void);

	Stream *pipeIn(void);
	String mName;
	OpenMode mMode;
	int64_t mReadPosition, mWritePosition;
};

class SafeWriteFile : public File
{
public:
	SafeWriteFile(void);
	SafeWriteFile(const String &filename);
	~SafeWriteFile(void);

	void open(const String &filename, OpenMode mode = Truncate);	// mode MUST be Truncate
	void close(void);

private:
	String mTarget;
};

class TempFile : public File
{
public:
	TempFile(void);
	TempFile(const String &filename);
	~TempFile(void);

	void close(void);

private:
	void open(const String &filename, OpenMode mode = ReadWrite);
};

}

#endif
