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

#include "pla/yamlserializer.h"
#include "pla/lineserializer.h"
#include "pla/exception.h"
#include "pla/serializable.h"

namespace pla
{

YamlSerializer::YamlSerializer(Stream *stream, int outputLevel) :
	mStream(stream),
	mLevel(outputLevel)
{
	Assert(stream);
}

YamlSerializer::~YamlSerializer(void)
{
 
}
 
bool YamlSerializer::input(Serializable &s)
{
	if(mIndent.empty())
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return false;
		
		if(mLine.trimmed() == "...") return false;
		if(mLine.trimmed() == "---") mLine.clear();
	}
	
	if(s.isInlineSerializable() && !s.isNativeSerializable())
	{
		String line;
		if(!input(line)) return false;
		s.deserialize(line);
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
			if(!mIndent.empty()) mIndent.pop();
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

		if(mIndent.top() < 0) 
		{
			mIndent.pop();
                	if((mIndent.empty() && indent == 0) || (!mIndent.empty() && mIndent.top() >= indent))
				return false;

			mIndent.push(indent);
		}
		else {
			if(indent < mIndent.top()) 
			{
				mIndent.pop();
				return false;
			}

			if(indent > mIndent.top()) 
				mIndent.top() = indent;
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
	if(!mIndent.empty()) mIndent.pop();
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
			if(!mIndent.empty()) mIndent.pop();
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
	
		if(mIndent.top() < 0)
                {
                        mIndent.pop();
                        if((mIndent.empty() && indent == 0) || (!mIndent.empty() && mIndent.top() >= indent))
                                return false;

                        mIndent.push(indent);
                }
                else {
                        if(indent < mIndent.top())
                        {
                                mIndent.pop();
                                return false;
                        }

                        if(indent > mIndent.top())
                                mIndent.top() = indent;
                }
	}
	
	mLine.trim();
	if(!mLine.contains(':')) throw IOException("Invalid associative entry, missing ':'");
	
	String key;
	while(true)
	{
		AssertIO(mLine.readUntil(key,':'));
		char chr;
		if(!mLine.get(chr) || chr == ' ') break;
		key+= ':';
		key+= chr;
	}

	LineSerializer keySerializer(&key);
	AssertIO(pair.deserializeKey(keySerializer));
	mIndent.push(-1);
	AssertIO(pair.deserializeValue(*this));
	if(!mIndent.empty()) mIndent.pop();
	return true;
}

bool YamlSerializer::input(String &str)
{
	str.clear();
 
	if(mIndent.empty())
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
		String trimmed =  mLine.trimmed();
		if(mIndent.empty() && (trimmed == "..." || trimmed == "---"))
			return i != 0;
		
		if(i == 0)
		{
			if(trimmed.empty() || trimmed == "|") keepNewLines = true;
			else if(trimmed == ">") keepNewLines = false;
			else {
				str = trimmed;
				mLine.clear();
				mStream->readLine(mLine);
				return true;
			}
		}
		else {
			if(!mIndent.empty())
			{
				int indent = 0;
				while(indent < mLine.size() && 
					(mLine[indent] == ' ' || mLine[indent] == '\t')) ++indent;
			
				if(mIndent.top() < 0)
                		{
                        		mIndent.pop();
                        		if((mIndent.empty() && indent == 0) || (!mIndent.empty() && mIndent.top() >= indent))
						return true;

                        		mIndent.push(indent);
                		}
                		else {
                        		if(indent < mIndent.top())
                        		{
                                		mIndent.pop();
						return true;
                        		}

                        		//if(indent > mIndent.top())
                                	//	mIndent.top() = indent;
                		}

				mLine.ignore(indent);
			}

			if(!str.empty())
                	{
                        	if(keepNewLines) str+= '\n';
                        	else str+= ' ';
                	}

                	str+= mLine;
		}

		mLine.clear();
		if(!mStream->readLine(mLine))
                	return true;

		++i;
	}
}

void YamlSerializer::output(const Serializable &s)
{
	if(!mLevel) *mStream<<"---";
	
	if(s.isInlineSerializable() && !s.isNativeSerializable()) output(s.toString());
	else s.serialize(*this);
}

void YamlSerializer::output(const Element &element)
{
	*mStream<<String(std::max(mLevel-1,0)*2, ' ');
	*mStream<<"-";
	element.serialize(*this);
	*mStream<<Stream::NewLine;
}

void YamlSerializer::output(const Pair &pair)
{
	*mStream<<String(std::max(mLevel-1,0)*2, ' ');
	String key;
	LineSerializer keySerializer(&key);
	pair.serializeKey(keySerializer);
	key.trim();
	*mStream<<key<<":";
	pair.serializeValue(*this);
	*mStream<<Stream::NewLine;
}

void YamlSerializer::output(const String &str)
{
	if(!mLevel) *mStream<<"---";
	
	if(str.empty())
	{
		if(mLevel) *mStream<<Stream::Space;
		return;
	}

	if(!mLevel || str.contains('\n') || str[0] == '|' || str[0] == '>')
	{
		*mStream<<" |"<<Stream::NewLine;
	  
		String copy(str);
		copy.trim();

		String line;
		while(copy.readLine(line))
		{
			*mStream<<String(mLevel*2, ' ');
			*mStream<<line;
			if(copy.last() == '\n') *mStream<<Stream::NewLine;
		}
	}
	else {
		if(mLevel) *mStream<<Stream::Space;
		*mStream<<str;
	}
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
	*mStream<<Stream::NewLine;
	++mLevel;
}

void YamlSerializer::outputArrayEnd(void)
{
	Assert(mLevel > 0);
	--mLevel;
}

void YamlSerializer::outputMapBegin(int size)
{
	*mStream<<Stream::NewLine;
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
