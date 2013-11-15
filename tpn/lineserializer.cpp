/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/lineserializer.h"
#include "tpn/exception.h"
#include "tpn/serializable.h"

namespace tpn
{

LineSerializer::LineSerializer(Stream *stream) :
	mStream(stream)
{
	Assert(stream);
}

LineSerializer::~LineSerializer(void)
{
 
}
 
bool LineSerializer::input(Serializable &s)
{
	if(s.isInlineSerializable() && !s.isNativeSerializable())
	{
		String line;
		if(!input(line)) return false;
		s.fromString(line);
		return true;
	}
	else return s.deserialize(*this);
}

bool LineSerializer::input(Element &element)
{
	String line;
	if(!input(line)) return false;
	if(line.empty()) return false;
	LineSerializer serializer(&line);
	AssertIO(element.deserialize(serializer));
	return true;
}


bool LineSerializer::input(Pair &pair)
{
	String line;
	if(!input(line)) return false;
	if(line.empty()) return false;
	
	String value = line.cut('=');
	value.trim();
	
	LineSerializer keySerializer(&line);
	LineSerializer valueSerializer(&value);
	AssertIO(pair.deserializeKey(keySerializer));
	pair.deserializeValue(valueSerializer);
	return true;
}

bool LineSerializer::input(String &str)
{
	// TODO: unescape
	return mStream->read(str);
}

void LineSerializer::output(const Serializable &s)
{
	if(s.isInlineSerializable() && !s.isNativeSerializable()) output(s.toString());
	else s.serialize(*this);
	
	mStream->newline();
}

void LineSerializer::output(const Element &element)
{
	String line;
	LineSerializer serializer(&line);
	element.serialize(serializer);
	if(!line.empty()) line.resize(line.size()-1);
	output(line);
	mStream->newline();
}

void LineSerializer::output(const Pair &pair)
{
	String key, value;
	LineSerializer keySerializer(&key);
	LineSerializer valueSerializer(&value);
	pair.serializeKey(keySerializer);
	pair.serializeValue(valueSerializer);
	key.trim();
	value.trim();
	String str;
	str<<key<<'='<<value;
	output(str);
	mStream->newline();
}

void LineSerializer::output(const String &str)
{	
	// TODO: escape
	mStream->write(str);
	mStream->space();
}

bool LineSerializer::inputArrayBegin(void)
{
	return !mStream->atEnd();
}

bool LineSerializer::inputMapBegin(void)
{
	return !mStream->atEnd();
}

}
