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

#include "pla/jsonserializer.h"
#include "pla/exception.h"
#include "pla/serializable.h"
#include "pla/lineserializer.h"

namespace pla
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
	String key;
	if(!input(key)) return false;
	AssertIO(mStream->last() == ':');
	LineSerializer keySerializer(&key);
	AssertIO(pair.deserializeKey(keySerializer));
	AssertIO(pair.deserializeValue(*this));
	return true;
}

bool JsonSerializer::input(String &str)
{
	const String fieldDelimiters = ",:]}";
	
	str.clear();
	
	char chr;
	if(!mStream->get(chr)) return false;
	while(Stream::BlankCharacters.contains(chr))
		if(!mStream->get(chr)) return false;
	
	if(fieldDelimiters.contains(chr))
	{
		if(chr == '}' || chr == ']')
		{
			do {
				if(!mStream->get(chr)) 
					return false;
			}
			while(Stream::BlankCharacters.contains(chr));
		}
		return false;
	}
		
	// Special case: read map or array in string
	if(chr == '{' || chr == '[')
	{
		char opening = chr;
		char closing;
		if(opening ==  '{') closing = '}';
		else closing = ']';
		
		int count = 1;
		str+= chr;
		while(count)
		{
			AssertIO(mStream->get(chr));
			str+= chr;
			
			if(chr == opening) ++count;
			else if(chr == closing) --count;
		}
		
		do {
			if(!mStream->get(chr)) 
				return true;
		}
		while(Stream::BlankCharacters.contains(chr));
		
		AssertIO(fieldDelimiters.contains(chr));
		return true;
	}
		
	bool quotes = (chr == '\'' || chr == '\"');
	
	String delimiters;
	if(quotes)
	{
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
			{
				String tmp;
				AssertIO(mStream->read(tmp, 4));
				
				unsigned u = 0;
				tmp.hexaMode(true);
				tmp >> u;

				wchar_t wstr[2];
				wstr[0] = wchar_t(u);
				wstr[1] = 0;
				
				str+= String(wstr);
				chr = 0;
				break;
			}
			default: 
				if(isalpha(chr) || isdigit(chr)) 
					chr = 0; // unknown escape sequence
				break;
			}
		}

		if(chr) str+= chr;
		if(!mStream->get(chr))
		{
			if(quotes) throw IOException();
			else break;
		}
	}
	
	if(!fieldDelimiters.contains(chr))
	{
		do {
			if(!mStream->get(chr)) 
				return true;
		}
		while(Stream::BlankCharacters.contains(chr));
		
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
	
	String key;
	LineSerializer keySerializer(&key);
	pair.serializeKey(keySerializer);
	key.trim();
	output(key);
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
	char chr;
	do if(!mStream->get(chr)) return false;
	while(Stream::BlankCharacters.contains(chr));
	
	if(chr == '[') return true;
	if(chr == '}' || chr == ']') return false;
	
	throw IOException();
}

bool JsonSerializer::inputArrayCheck(void)
{
	char chr = mStream->last();
	while(Stream::BlankCharacters.contains(chr))
		if(!mStream->get(chr)) 
			return false;

	if(chr == '[' || chr == ',') return true;
	if(chr == '}' || chr == ']')
	{
		mStream->ignore(1);
		return false;
	}
	
	throw IOException();
}

bool JsonSerializer::inputMapBegin(void)
{
	char chr;
	do if(!mStream->get(chr)) return false;
	while(Stream::BlankCharacters.contains(chr));
	
	if(chr == '{') return true;
	if(chr == '}' || chr == ']') return false;
	
	throw IOException();
}

bool JsonSerializer::inputMapCheck(void)
{
  	char chr = mStream->last();
	while(Stream::BlankCharacters.contains(chr))
		if(!mStream->get(chr)) 
			return false;
		
	if(chr == '{' || chr == ',') return true;
	if(chr == '}' || chr == ']')
	{
		mStream->ignore(1);  
		return false;
	}
	
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
