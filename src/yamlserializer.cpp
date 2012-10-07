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

#include "yamlserializer.h"
#include "exception.h"
#include "serializable.h"
#include "map.h"
#include "array.h"

namespace tpot
{

YamlSerializer::YamlSerializer(Stream *stream) :
	mStream(stream),
	mLevel(0)
{
	Assert(stream);
}

YamlSerializer::~YamlSerializer(void)
{
	 
}
 
bool YamlSerializer::input(Serializable &s)
{
	return s.deserialize(*mStream);
}

bool YamlSerializer::input(Element &element)
{
	char chr;
	if(!mStream->readChar(chr)) return false;
	
	if(chr == '.')
	{
		  AssertIO(mStream->assertChar('.'));
		  AssertIO(mStream->assertChar('.'));
		  return false;
	}
	
	AssertIO(chr == '-');
	AssertIO(mStream->readChar(chr));
	
	if(chr == '-')
	{
		 AssertIO(mStream->assertChar('-'));
		 String line;
		 mStream->readLine(line);
		 throw End();
	}
	
	AssertIO(chr == ' ' || chr == '\t');
	AssertIO(element.deserialize(*this));
	return true;
}


bool YamlSerializer::input(Pair &pair)
{
	char chr;
	if(!mStream->readChar(chr)) return false;
	
	if(chr == '.')
	{
		  AssertIO(mStream->assertChar('.'));
		  AssertIO(mStream->assertChar('.'));
		  return false;
	}
	
	if(chr == '-')
	{
		 AssertIO(mStream->assertChar('-'));
		 String line;
		 mStream->readLine(line);
		 throw End();
	}
  
  	String key(chr);
	AssertIO(mStream->readUntil(key,":"));
	
	YamlSerializer keySerializer(&key);
	AssertIO(pair.deserializeKey(keySerializer));
	AssertIO(pair.deserializeValue(*this));
	return true;
}

bool YamlSerializer::input(String &str)
{
	str.clear();
  
	char chr;
	if(!mStream->get(chr)) return false;

	while(Stream::BlankCharacters.contains(chr))
		if(!mStream->get(chr)) return false;
	
	bool quotes = (chr == '\'' || chr == '\"');
		
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
		if(!mStream->get(chr)) return true;
	}

	while(!delimiters.contains(chr))
	{
		if(chr == '\\')
		{
			AssertIO(mStream->get(chr));
			switch(chr)
			{
			case 'n': 	chr = '\n';	break;
			case 'r': 	chr = '\r';	break;
			/*case 'u':
				String tmp;
				AssertIO(read(tmp, 4));
				unsigned u;
				tmp >> u;
				if(u >= 0x80) continue;	// TODO
				chr = u;
				break;*/
			}
		}
		
		str+= chr;
		if(!mStream->get(chr))
		{
			if(quotes) throw IOException();
			else break;
		}
	}
	
	return true; 
}

void YamlSerializer::output(const Serializable &s)
{
	s.serialize(*mStream);
}

void YamlSerializer::output(const Element &element)
{
	*mStream<<String(mLevel-1, ' ')<<"- ";
	element.serialize(*this);
	*mStream<<Stream::NewLine;
}

void YamlSerializer::output(const Pair &pair)
{
	*mStream<<String(mLevel-1, ' ');
	pair.serializeKey(*this);
	*mStream<<": ";
	pair.serializeValue(*this);
	*mStream<<Stream::NewLine;
}

void YamlSerializer::output(const String &str)
{
	bool quotes = str.contains(' ') 
			|| str.contains('\t') 
			|| str.contains('\n');
  
	if(quotes) *mStream<<'\"';
  	for(size_t i=0; i<str.size(); ++i)
	{
	  	char chr = str[i];
		
		switch(chr)
		{
		case '\"': mStream->write("\\\""); break;
		case '\'': mStream->write("\\\'"); break;
		case '\n': mStream->write("\\n"); break;
		case '\r': mStream->write("\\r"); break;
		default: mStream->put(chr); break;
		}
	}
	if(quotes) *mStream<<'\"';
}

bool YamlSerializer::inputArrayBegin(void)
{
	return true;	
}

bool YamlSerializer::inputArrayElement(void)
{
	return true;	
}

bool YamlSerializer::inputMapBegin(void)
{
	return true;
}

bool YamlSerializer::inputMapElement(void)
{
  	return true;
}
	
void YamlSerializer::outputArrayBegin(int size)
{
	++mLevel;
}

void YamlSerializer::outputArrayEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
	*mStream<<"---"<<Stream::NewLine;
}

void YamlSerializer::outputMapBegin(int size)
{
	++mLevel;
}

void YamlSerializer::outputMapEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
	*mStream<<"---"<<Stream::NewLine;
}

}
