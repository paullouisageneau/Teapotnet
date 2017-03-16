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

#include "pla/jsonserializer.hpp"
#include "pla/exception.hpp"
#include "pla/serializable.hpp"
#include "pla/lineserializer.hpp"

namespace pla
{

JsonSerializer::JsonSerializer(Stream *stream) :
	mStream(stream)
{
	Assert(stream);
}

JsonSerializer::~JsonSerializer(void)
{

}

bool JsonSerializer::read(Serializable &s)
{
	if(s.isInlineSerializable() && !s.isNativeSerializable())
	{
		String str;
		if(!Serializer::read(str)) return false;
		s.fromString(str);
		return true;
	}
	else return s.deserialize(*this);
}

bool JsonSerializer::read(std::string &str)
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

void JsonSerializer::write(const Serializable &s)
{
	if(s.isInlineSerializable() && !s.isNativeSerializable()) Serializer::write(s.toString());
	else s.serialize(*this);
	if(mKey) *mStream << ':' << Stream::Space;
	mKey = false;
}

void JsonSerializer::write(const std::string &str)
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

	if(mKey) *mStream << ':' << Stream::Space;
	mKey = false;
}

bool JsonSerializer::readArrayBegin(void)
{
	char chr;
	do if(!mStream->get(chr)) return false;
	while(Stream::BlankCharacters.contains(chr));

	if(chr == '[') return true;
	if(chr == '}' || chr == ']') return false;

	throw IOException();
}

bool JsonSerializer::readArrayNext(void)
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

bool JsonSerializer::readMapBegin(void)
{
	char chr;
	do if(!mStream->get(chr)) return false;
	while(Stream::BlankCharacters.contains(chr));

	if(chr == '{') return true;
	if(chr == '}' || chr == ']') return false;

	throw IOException();
}

bool JsonSerializer::readMapNext(void)
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

void JsonSerializer::writeArrayBegin(size_t size)
{
	*mStream<<'[';
	++mLevel;
}

void JsonSerializer::writeArrayNext(size_t i)
{
	if(i > 0) *mStream<<',';
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ');
	mKey = false;
}

void JsonSerializer::writeArrayEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ')<<']';
}

void JsonSerializer::writeMapBegin(size_t size)
{
	*mStream<<'{';
	++mLevel;
}

void JsonSerializer::writeMapNext(size_t i)
{
	if(i > 0) *mStream<<',';
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ');
	mKey = true;
}

void JsonSerializer::writeMapEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
	*mStream<<Stream::NewLine;
	*mStream<<String(mLevel*2, ' ')<<'}';
}

}
