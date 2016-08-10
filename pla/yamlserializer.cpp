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

#include "pla/yamlserializer.hpp"
#include "pla/lineserializer.hpp"
#include "pla/exception.hpp"
#include "pla/serializable.hpp"

namespace pla
{

YamlSerializer::YamlSerializer(Stream *stream, int writeLevel) :
	mStream(stream),
	mLevel(writeLevel)
{
	Assert(stream);
}

YamlSerializer::~YamlSerializer(void)
{
 
}
 
bool YamlSerializer::read(Serializable &s)
{
	if(mIndent.empty())
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return false;
		
		String trimmed = mLine.trimmed();
		while(!trimmed.empty() && trimmed[0] == '#')	// comment
		{
			if(!mStream->readLine(mLine))
				return false;
			trimmed = mLine.trimmed();
		}
		
		if(trimmed == "...") return false;
		if(trimmed == "---") mLine.clear();
	}
	
	if(s.isInlineSerializable() && !s.isNativeSerializable())
	{
		String line;
		if(!Serializer::read(line)) return false;
		s.deserialize(line);
		return true;
	}
	else return s.deserialize(*this);
}

bool YamlSerializer::read(std::string &str)
{
	str.clear();
 
	if(mIndent.empty())
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return false;
		
		// Note: no comment inside litteral
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

void YamlSerializer::write(const Serializable &s)
{
	if(!mLevel) *mStream<<"---";
	
	if(s.isInlineSerializable() && !s.isNativeSerializable()) Serializer::write(s.toString());
	else s.serialize(*this);

	if(mKey) *mStream<<':';
	mKey = false;
}

void YamlSerializer::write(const std::string &str)
{
	if(!mLevel) *mStream<<"---";
	
	if(str.empty())
	{
		if(mLevel) *mStream<<Stream::Space;
		return;
	}

	if(!mLevel || str.find('\n') != std::string::npos 
			|| str[0] == '|' || str[0] == '>')
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
		if(mLevel && !mKey) *mStream<<Stream::Space;
		*mStream<<str;
	}

	if(mKey) *mStream<<':';
	mKey = false;
}

bool YamlSerializer::readArrayBegin(void)
{
	return !mStream->atEnd();
}

bool YamlSerializer::readArrayNext(void)
{
	String trimmed = mLine.trimmed();
	if(!trimmed.empty() && trimmed[0] == '#')	// comment
		trimmed.clear();
	
	while(trimmed.empty())
	{
		mLine.clear();
		if(!mStream->readLine(mLine))
		{
			if(!mIndent.empty()) mIndent.pop();
			return false;
		}
		
		trimmed =  mLine.trimmed();
		if(!trimmed.empty() && trimmed[0] == '#')	// comment
			trimmed.clear();
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
	return true;
}

bool YamlSerializer::readMapBegin(void)
{
	return !mStream->atEnd();
}

bool YamlSerializer::readMapNext(void)
{
	String trimmed = mLine.trimmed();
	if(!trimmed.empty() && trimmed[0] == '#')	// comment
		trimmed.clear();
	
	while(trimmed.empty())
	{
		mLine.clear();
		if(!mStream->readLine(mLine))
		{
			if(!mIndent.empty()) mIndent.pop();
			return false;
		}
		
		trimmed =  mLine.trimmed();
		if(!trimmed.empty() && trimmed[0] == '#')	// comment
			trimmed.clear();
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
	
	return true;
}
	
void YamlSerializer::writeArrayBegin(size_t size)
{
	*mStream<<Stream::NewLine;
	++mLevel;
}

void YamlSerializer::writeArrayNext(size_t i)
{
	if(i > 0) *mStream<<Stream::NewLine;
	*mStream<<String(std::max(mLevel-1,0)*2, ' ');
	*mStream<<"-";
}

void YamlSerializer::writeArrayEnd(void)
{
	*mStream<<Stream::NewLine;
	Assert(mLevel > 0);
	--mLevel;
}

void YamlSerializer::writeMapBegin(size_t size)
{
	*mStream<<Stream::NewLine;
	++mLevel;
}

void YamlSerializer::writeMapNext(size_t i)
{
	if(i > 0) *mStream<<Stream::NewLine;
	*mStream<<String(std::max(mLevel-1,0)*2, ' ');
	mKey = true;
}

void YamlSerializer::writeMapEnd(void)
{
	*mStream<<Stream::NewLine;
	Assert(mLevel > 0);
	--mLevel;
}

void YamlSerializer::writeClose(void)
{
	*mStream<<"..."<<Stream::NewLine;
}

}

