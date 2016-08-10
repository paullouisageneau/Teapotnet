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

#include "pla/binaryserializer.hpp"
#include "pla/exception.hpp"
#include "pla/serializable.hpp"
#include "pla/map.hpp"
#include "pla/array.hpp"

namespace pla
{

BinarySerializer::BinarySerializer(Stream *stream) :
	mStream(stream)
{
	Assert(stream);
}

BinarySerializer::~BinarySerializer(void)
{
	 
}

bool BinarySerializer::read(std::string &str)
{
	uint32_t size;
	if(!read(size)) return false;	
	
	str.clear();
	str.reserve(size);
	
	while(size--)
	{
		uint8_t c;
		AssertIO(read(c));
		str+= char(c);
	}
	
	return true;
}

void BinarySerializer::write(const std::string &str)
{
	write(uint32_t(str.size()));
	
	for(int i=0; i<str.size(); ++i)
		write(uint8_t(str[i]));
}

bool BinarySerializer::read(bool &b)
{
	uint8_t i;
	if(!read(i)) return false;
	b = (i != 0);
	return true;
}

void BinarySerializer::write(bool b)
{
	write(uint8_t(b ? 1 : 0));
}

bool BinarySerializer::readArrayBegin(void)
{
	uint32_t size;
	if(!read(size)) return false;
	mLeft.push(size);
	return true;
}

bool BinarySerializer::readArrayNext(void)
{
	Assert(!mLeft.empty());
	Assert(mLeft.top() >= 0);
	
	if(mLeft.top() == 0)
	{
		mLeft.pop();
		return false;
	}
	
	--mLeft.top(); 
	return true;
}

bool BinarySerializer::readMapBegin(void)
{
	uint32_t size;
	if(!read(size)) return false;
	mLeft.push(size);
	return true;  
}

bool BinarySerializer::readMapNext(void)
{
	Assert(!mLeft.empty());
	Assert(mLeft.top() >= 0);
	
	if(mLeft.top() == 0)
	{
		mLeft.pop();
		return false;
	}
	
	--mLeft.top(); 
	return true;
}
	
void BinarySerializer::writeArrayBegin(size_t size)
{
	write(uint32_t(size));
}

void BinarySerializer::writeMapBegin(size_t size)
{
	write(uint32_t(size));
}

}

