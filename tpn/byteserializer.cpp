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

#include "tpn/byteserializer.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"
#include "tpn/map.h"
#include "tpn/array.h"

namespace tpn
{

ByteSerializer::ByteSerializer(ByteStream *stream) :
	mStream(stream)
{
	Assert(stream);
}

ByteSerializer::~ByteSerializer(void)
{
	 
}

bool ByteSerializer::input(String &str)
{
	uint32_t size;
	if(!input(size)) return false;	

	str.clear();
	str.reserve(size);
	
	while(size--)
	{
		uint8_t c;
		AssertIO(input(c));
		str+= char(c);
	}

	return true;
}

void ByteSerializer::output(const String &str)
{
	output(uint32_t(str.size()));
	
	for(int i=0; i<str.size(); ++i)
		output(uint8_t(str[i]));
}

bool ByteSerializer::input(bool &b)
{
	uint8_t i;
	if(!input(i)) return false;
	b = (i != 0);
	return true;
}

void ByteSerializer::output(bool b)
{
	uint8_t i;
	if(b) i = 1;
	else i = 0;
	output(i);
}

bool ByteSerializer::inputArrayBegin(void)
{
	uint32_t size;
	if(!input(size)) return false;
	mLeft.push(size);
	return true;
}

bool ByteSerializer::inputArrayCheck(void)
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

bool ByteSerializer::inputMapBegin(void)
{
	uint32_t size;
	if(!input(size)) return false;
	mLeft.push(size);
	return true;  
}

bool ByteSerializer::inputMapCheck(void)
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
	
void ByteSerializer::outputArrayBegin(int size)
{
	output(uint32_t(size));
}

void ByteSerializer::outputArrayEnd(void)
{
 	// Nothing to do 
}

void ByteSerializer::outputMapBegin(int size)
{
	output(uint32_t(size));
}

void ByteSerializer::outputMapEnd(void)
{
 	// Nothing to do   
}

}

