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
	if(!mLevel)
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return false;
		
		if(mLine.trimmed() == "...") return false;
		if(mLine.trimmed() == "---") mLine.clear();
	}
	
	if(s.isInlineSerializable())
	{
		String line;
		if(!input(line)) return false;
		s.fromString(line);
		return true;
	}
	else return s.deserialize(*this);
}

bool YamlSerializer::input(Element &element)
{  
  	String trimmed = mLine.trimmed();
	while(trimmed.empty())
	{
		mLine.clear();
		if(!mStream->readLine(mLine))
		{
			mIndent.pop();
			return false;
		}
		
		trimmed =  mLine.trimmed();
	}
	
	if(trimmed == "..." || trimmed == "---")
		return false;

	if(!mIndent.empty())
	{
		int indent = 0;
		while(indent < mLine.size() && 
			(mLine[indent] == ' ' || mLine[indent] == '\t')) ++indent;
	
		if(mIndent.top() < 0) mIndent.top() = indent;
		else {
			if(indent < mIndent.top()) return false;
			if(indent > mIndent.top()) mIndent.top() = indent;
		}
	}
	
	mLine.trim();
	
	char chr;
	mLine.get(chr);
	if(chr != '-') throw IOException("Invalid array entry, missing '-'");
	
	AssertIO(mLine.get(chr));
	AssertIO(chr == ' ');
	
	mIndent.push(-1);
	AssertIO(element.deserialize(*this));
	mIndent.pop();
	return true;
}


bool YamlSerializer::input(Pair &pair)
{
	String trimmed = mLine.trimmed();
	while(trimmed.empty())
	{
		mLine.clear();
		if(!mStream->readLine(mLine))
		{
			mIndent.pop();
			return false;
		}
		
		trimmed =  mLine.trimmed();
	}
	
	if(trimmed == "..." || trimmed == "---")
		return false;
	
	if(!mIndent.empty())
	{
		int indent = 0;
		while(indent < mLine.size() && 
			(mLine[indent] == ' ' || mLine[indent] == '\t')) ++indent;
	
		if(mIndent.top() < 0) mIndent.top() = indent;
		else {
			if(indent < mIndent.top()) return false;
			if(indent > mIndent.top()) mIndent.top() = indent;
		}
	}
	
	mLine.trim();
	if(!mLine.contains(':')) throw IOException("Invalid associative entry, missing ':'");
	
	String key;
	AssertIO(mLine.readUntil(key,':'));
	YamlSerializer keySerializer(&key);
	AssertIO(pair.deserializeKey(keySerializer));
	mIndent.push(-1);
	AssertIO(pair.deserializeValue(*this));
	mIndent.pop();
	return true;
}

bool YamlSerializer::input(String &str)
{
	str.clear();
 
	if(!mLevel)
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return false;
		
		if(mLine.trimmed() == "...") return false;
		if(mLine.trimmed() == "---") mLine.clear();
	}
	
	bool keepNewLines = true;
	int i = 0;
	while(true)
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return i != 0;
		
		String trimmed =  mLine.trimmed();
		if(mLevel == 0 && (trimmed == "..." || trimmed == "---"))
			return i != 0;
		
		if(i == 0)
		{
			if(trimmed.empty() || trimmed == "|")
			{
				mLine.clear();
				continue;
			}
			if(trimmed == ">") 
			{
				keepNewLines = false;
				mLine.clear();
				continue;
			}
		}
		
		if(!mIndent.empty())
		{
			int indent = 0;
			while(indent < mLine.size() && 
				(mLine[indent] == ' ' || mLine[indent] == '\t')) ++indent;
		
			if(mIndent.top() < 0) mIndent.top() = indent;
			else {
				if(indent < mIndent.top()) return i != 0;
				if(indent > mIndent.top()) mIndent.top() = indent;
			}
		}
		
		if(!str.empty()) 
		{
			if(keepNewLines) str+= '\n';
			else str+= ' ';
		}
		
		mLine.trim();
		str+= mLine;
		mLine.clear();
		++i;
	}
}

void YamlSerializer::output(const Serializable &s)
{
	if(s.isInlineSerializable()) output(s.toString());
	else s.serialize(*this);
}

void YamlSerializer::output(const Element &element)
{
	*mStream<<String(std::max(mLevel-1,0), ' ');
	*mStream<<"- ";
	element.serialize(*this);
	*mStream<<Stream::NewLine;
}

void YamlSerializer::output(const Pair &pair)
{
	*mStream<<String(std::max(mLevel-1,0), ' ');
	pair.serializeKey(*this);
	*mStream<<": ";
	pair.serializeValue(*this);
	*mStream<<Stream::NewLine;
}

void YamlSerializer::output(const String &str)
{
	if(!mLevel) *mStream<<"---";
	if(str.empty())
	{
		*mStream<<Stream::NewLine;
		return;
	}
	
	if(!mLevel || str.contains('\n') || str[0] == '|' || str[0] == '>') *mStream<<" |"<<Stream::NewLine;
  
	String copy(str);
	String line;
	while(copy.readLine(line))
	{
		*mStream<<String(std::max(mLevel-1,0), ' ');
		 *mStream<<line;
		if(copy.last() == '\n') *mStream<<Stream::NewLine;
	}
	
	if(!mLevel) *mStream<<Stream::NewLine;
	//else *mStream<<Stream::Space;
}

bool YamlSerializer::inputArrayBegin(void)
{
	return !mStream->atEnd();
}

bool YamlSerializer::inputArrayCheck(void)
{
	return true;	
}

bool YamlSerializer::inputMapBegin(void)
{
	return !mStream->atEnd();
}

bool YamlSerializer::inputMapCheck(void)
{
  	return true;
}
	
void YamlSerializer::outputArrayBegin(int size)
{
	if(!mLevel) *mStream<<"---"<<Stream::NewLine;
	++mLevel;
}

void YamlSerializer::outputArrayEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
}

void YamlSerializer::outputMapBegin(int size)
{
	if(!mLevel) *mStream<<"---"<<Stream::NewLine;
  	++mLevel;
}

void YamlSerializer::outputMapEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
}

void YamlSerializer::outputClose(void)
{
	*mStream<<"..."<<Stream::NewLine;
}

}
