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

File::File(const String &filename, File::OpenMode mode)
{
	ios_base::openmode m;
	switch(mode)
	{
	case Read:		m = ios_base::in;				break;
	case Write:		m = ios_base::out;				break;
	case Append:	m = ios_base::app;				break;
	default:		m = ios_base::in|ios_base::out;	break;
	}

	open(filename.c_str(), m);
	if(!is_open()) throw IOException(String("Unable to open file ")+filename);
}

File::~File(void)
{
	close();
}

int File::readData(char *buffer, size_t size)
{
	std::fstream::readsome(buffer, size);
	return gcount();
}

void File::writeData(const char *data, size_t size)
{
	std::fstream::write(data,size);
}

}
