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

#include "pla/lineserializer.hpp"
#include "pla/exception.hpp"
#include "pla/serializable.hpp"

namespace pla
{

LineSerializer::LineSerializer(Stream *stream) :
	mStream(stream)
{
	Assert(stream);
}

LineSerializer::~LineSerializer(void)
{
 
}
 
bool LineSerializer::read(Serializable &s)
{
	if(s.isInlineSerializable())
	{
		std::string line;
		if(!read(line) || line.empty()) return false;
		s.fromString(String(line));
		return true;
	}
	else {
		return s.deserialize(*this);
	}
}

bool LineSerializer::read(std::string &str)
{
	String tmp;
	bool ret = mStream->readUntil(tmp, String(Stream::NewLine) + "=");
	str = tmp;
	return ret;
}

void LineSerializer::write(const Serializable &s)
{
	if(s.isInlineSerializable())
	{
		write(static_cast<const std::string &>(s.toString()));
	}
	else {
		s.serialize(*this);
	}
}

void LineSerializer::write(const std::string &str)
{
	mStream->write(String(str));

	if(mKey) 
	{
		mStream->write('=');
		mKey = false;
	}
	else {
		mStream->write(Stream::NewLine);
	}
}

bool LineSerializer::readArrayBegin(void)
{
	return !mStream->atEnd();
}

bool LineSerializer::readArrayNext(void)
{
	return !mStream->atEnd();
}

bool LineSerializer::readMapBegin(void)
{
	return !mStream->atEnd();
}

bool LineSerializer::readMapNext(void)
{
	return !mStream->atEnd();
}

void LineSerializer::writeArrayBegin(size_t size)
{
	// Dummy
}

void LineSerializer::writeArrayNext(size_t i)
{
	mKey = false;
}

void LineSerializer::writeArrayEnd(void)
{
	mStream->newline();
}

void LineSerializer::writeMapBegin(size_t size)
{
	// Dummy
}

void LineSerializer::writeMapNext(size_t i)
{
	mKey = true;
}

void LineSerializer::writeMapEnd(void)
{
	mStream->newline();
}

}

