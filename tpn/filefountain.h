/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#ifndef TPN_FILEFOUNTAIN_H
#define TPN_FILEFOUNTAIN_H

#include "tpn/include.h"
#include "tpn/fountain.h"
#include "tpn/stream.h"
#include "tpn/file.h"
#include "tpn/synchronizable.h"

namespace tpn
{

class FileFountain : public Fountain, public Stream, public Synchronizable
{
public:
	FileFountain(File *file);	// file will be destroyed
	~FileFountain(void);

	// Fountain
	size_t readBlock(int64_t offset, char *buffer, size_t size);
	void writeBlock(int64_t offset, const char *data, size_t size);

	// Stream
	size_t readData(char *buffer, size_t size);
	void writeData(const char *buffer, size_t size);
	void seekRead(int64_t position);
	void seekWrite(int64_t position);
	void clear(void);
	void flush(void);
	
private:
	bool isWritten(int64_t offset);
	void markWritten(int64_t offset);

	File *mFile;
	File *mMapFile;

	uint64_t mReadPosition;
	uint64_t mWritePosition;
};

}

#endif
