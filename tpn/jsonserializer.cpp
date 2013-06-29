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

#include "tpn/jsonserializer.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"

namespace tpn
{

JsonSerializer::JsonSerializer(Stream *stream) :
	mStream(stream),
	mLevel(0)
{
	Assert(stream);
}

JsonSerializer::~JsonSerializer(void)
{
	 
}
 
bool JsonSerializer::input(Serializable &s)
{
	if(s.isInlineSerializable() && !s.isNativeSerializable())
	{
		String str;
		if(!input(str)) return false;
		s.fromString(str);
		return true;
	}
	else return s.deserialize(*this);
}

bool JsonSerializer::input(Element &element)
{
	return element.deserialize(*this);
}

bool JsonSerializer::input(Pair &pair)
{
	AssertIO(pair.deserializeKey(*this));
	AssertIO(mStream->assertChar(':'));
	AssertIO(pair.deserializeValue(*this));
	return true;
}

bool JsonSerializer::input(String &str)
{
	str.clear();
  
	char chr;
	if(!mStream->get(chr)) return false;

	while(Stream::BlankCharacters.contains(chr))
		if(!mStream->get(chr)) return false;
	
	bool quotes = (chr == '\'' || chr == '\"');
	
	String fieldDelimiters = ",]}";
	
	String delimiters;
	if(quotes)
	{
		quotes = true;
		delimiters = String(chr);
		AssertIO(mStream->get(chr));
	}
	else {
		str+= chr;
		delimiters = Stream::BlankCharacters;
		delimiters+=fieldDelimiters;
		if(!mStream->get(chr)) return true;
	}

	while(!delimiters.contains(chr))
	{
		if(chr == '\\')
		{
			AssertIO(mStream->get(chr));
			switch(chr)
			{
			case 'b': 	chr = '\b';	break;
			case 'f': 	chr = '\f';	break;
			case 'n': 	chr = '\n';	break;
			case 'r': 	chr = '\r';	break;
			case 't': 	chr = '\t';	break;
			case 'u':
				String tmp;
				AssertIO(mStream->read(tmp, 4));
				unsigned u;
				tmp >> u;
				if(u >= 0x80) continue;	// TODO
				chr = u;
				break;
			}
		}
		
		str+= chr;
		if(!mStream->get(chr))
		{
			if(quotes) throw IOException();
			else break;
		}
	}
	
	if(!fieldDelimiters.contains(chr))
	{
		mStream->readChar(chr);
		AssertIO(fieldDelimiters.contains(chr));
	}
	
	return true; 
}

void JsonSerializer::output(const Serializable &s)
{
	if(s.isInlineSerializable() && !s.isNativeSerializable()) output(s.toString());
	else s.serialize(*this);
}

void JsonSerializer::output(const Element &element)
{
	if(!mFirst) *mStream<<',';
	mFirst = false;
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ');
	
	element.serialize(*this);
}

void JsonSerializer::output(const Pair &pair)
{
	if(!mFirst) *mStream<<',';
	mFirst = false;
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ');
	
	pair.serializeKey(*this);
	*mStream<<": ";
	pair.serializeValue(*this);
}

void JsonSerializer::output(const String &str)
{
	*mStream<<'\"';
  	for(size_t i=0; i<str.size(); ++i)
	{
	  	char chr = str[i];
		
		switch(chr)
		{
		case '\\': mStream->write("\\\\"); break;
		case '\"': mStream->write("\\\""); break;
		//case '\'': mStream->write("\\\'"); break;
		case '\b': mStream->write("\\b");  break;
		case '\f': mStream->write("\\f");  break;
		case '\n': mStream->write("\\n");  break;
		case '\r': mStream->write("\\r");  break;
		case '\t': mStream->write("\\t");  break;
		default: mStream->put(chr); break;
		}
	}
	*mStream<<'\"';
}

bool JsonSerializer::inputArrayBegin(void)
{
	return true;
}

bool JsonSerializer::inputArrayCheck(void)
{
	char chr = mStream->last();
	if(chr == '[' || chr == ',') return true;
	if(chr == ']') return false;
	throw IOException();
}

bool JsonSerializer::inputMapBegin(void)
{
	return mStream->assertChar('{');
}

bool JsonSerializer::inputMapCheck(void)
{
  	char chr = mStream->last();
	if(chr == '{' || chr == ',') return true;
	if(chr == '}') return false;
	throw IOException();
}
	
void JsonSerializer::outputArrayBegin(int size)
{
  	*mStream<<'[';
	++mLevel;
	mFirst = true;
}

void JsonSerializer::outputArrayEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ')<<']';
	mFirst = false;	// useful for higher level array or map
}

void JsonSerializer::outputMapBegin(int size)
{
	*mStream<<'{';
	++mLevel;
	mFirst = true;
}

void JsonSerializer::outputMapEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ')<<'}';
	mFirst = false;	// useful for higher level array or map
}

}
