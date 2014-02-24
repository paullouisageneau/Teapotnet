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

#include "tpn/binaryserializer.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"
#include "tpn/map.h"
#include "tpn/array.h"

namespace tpn
{

BinarySerializer::BinarySerializer(Stream *stream) :
	mStream(stream)
{
	Assert(stream);
}

BinarySerializer::~BinarySerializer(void)
{
	 
}

bool BinarySerializer::input(String &str)
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

void BinarySerializer::output(const String &str)
{
	output(uint32_t(str.size()));
	
	for(int i=0; i<str.size(); ++i)
		output(uint8_t(str[i]));
}

bool BinarySerializer::input(bool &b)
{
	uint8_t i;
	if(!input(i)) return false;
	b = (i != 0);
	return true;
}

void BinarySerializer::output(bool b)
{
	uint8_t i;
	if(b) i = 1;
	else i = 0;
	output(i);
}

bool BinarySerializer::inputArrayBegin(void)
{
	uint32_t size;
	if(!input(size)) return false;
	mLeft.push(size);
	return true;
}

bool BinarySerializer::inputArrayCheck(void)
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

bool BinarySerializer::inputMapBegin(void)
{
	uint32_t size;
	if(!input(size)) return false;
	mLeft.push(size);
	return true;  
}

bool BinarySerializer::inputMapCheck(void)
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
	
void BinarySerializer::outputArrayBegin(int size)
{
	output(uint32_t(size));
}

void BinarySerializer::outputArrayEnd(void)
{
 	// Nothing to do 
}

void BinarySerializer::outputMapBegin(int size)
{
	output(uint32_t(size));
}

void BinarySerializer::outputMapEnd(void)
{
 	// Nothing to do   
}

}

