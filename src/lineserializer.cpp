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

#include "lineserializer.h"
#include "exception.h"
#include "serializable.h"

namespace tpot
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
	String line;
	if(!input(line)) return false;
	AssertIO(s.deserialize(line));
	return true;
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
	AssertIO(pair.deserializeValue(valueSerializer));
	return true;
}

bool LineSerializer::input(String &str)
{
	// TODO: unescape
	return mStream->readLine(str);
}

void LineSerializer::output(const Serializable &s)
{
	String line;
	s.serialize(line);
	output(line);
}

void LineSerializer::output(const Element &element)
{
	String line;
	LineSerializer serializer(&line);
	element.serialize(serializer);
	if(!line.empty()) line.resize(line.size()-1);
	output(line);
}

void LineSerializer::output(const Pair &pair)
{
	String key, value;
	LineSerializer keySerializer(&key);
	LineSerializer valueSerializer(&value);
	pair.serializeKey(keySerializer);
	pair.serializeValue(valueSerializer);
	if(!key.empty()) key.resize(key.size()-1);
	if(!value.empty()) value.resize(value.size()-1);
	String str;
	str<<key<<'='<<value;
	output(str);
}

void LineSerializer::output(const String &str)
{	
	// TODO: escape
	mStream->writeLine(str);
}

}
