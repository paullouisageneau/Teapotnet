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

#include "file.h"
#include "exception.h"

namespace arc
{

bool File::Exist(const String &filename)
{
	std::fstream file;
	file.open(filename.c_str(), std::ios_base::in);
	return file.is_open();
}

bool File::Remove(const String &filename)
{
	return (std::remove(filename.c_str()) == 0);
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
	if(is_open()) close();

	mName = filename;

	std::ios_base::openmode m;
	switch(mode)
	{
	case Read:		m = std::ios_base::in;						break;
	case Write:		m = std::ios_base::out;						break;
	case Append:	m = std::ios_base::app;						break;
	case Truncate:	m = std::ios_base::trunc;					break;
	default:		m = std::ios_base::in|std::ios_base::out;	break;
	}

	std::fstream::open(filename.c_str(), m|std::ios_base::binary);
	if(!is_open()) throw IOException(String("Unable to open file: ")+filename);
}

size_t File::size(void)
{
	pos_type pos = std::fstream::tellg();
	std::fstream::seekg(0, std::ios_base::end);
	pos_type size = std::fstream::tellg();
	std::fstream::seekg(pos);
	return size_t(size);
}

size_t File::readData(char *buffer, size_t size)
{
	std::fstream::readsome(buffer, size);
	if(!std::fstream::gcount() && std::fstream::good())
		std::fstream::read(buffer, size);
	return std::fstream::gcount();
}

void File::writeData(const char *data, size_t size)
{
	std::fstream::write(data,size);
}

ByteStream *File::pipeIn(void)
{
	return new File(mName,Append);
}

}
