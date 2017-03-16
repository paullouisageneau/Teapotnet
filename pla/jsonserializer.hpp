/*************************************************************************
 *   Copyright (C) 2011-2016 by Paul-Louis Ageneau                       *
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

#ifndef PLA_JSONSERIALIZER_H
#define PLA_JSONSERIALIZER_H

#include "pla/serializer.hpp"
#include "pla/stream.hpp"
#include "pla/string.hpp"

namespace pla
{

class JsonSerializer : public Serializer
{
public:
	JsonSerializer(Stream *stream);	// stream WON'T be destroyed
	~JsonSerializer(void);

private:
	bool read(Serializable &s);
	bool read(std::string &str);
	bool read(int8_t &i)	{ return readValue(i); }
	bool read(int16_t &i)	{ return readValue(i); }
	bool read(int32_t &i)	{ return readValue(i); }
	bool read(int64_t &i)	{ return readValue(i); }
	bool read(uint8_t &i)	{ return readValue(i); }
	bool read(uint16_t &i)	{ return readValue(i); }
	bool read(uint32_t &i)	{ return readValue(i); }
	bool read(uint64_t &i)	{ return readValue(i); }
	bool read(bool &b)	{ return readValue(b); }
	bool read(float &f)	{ return readValue(f); }
	bool read(double &f)	{ return readValue(f); }

	void write(const Serializable &s);
	void write(const std::string &str);
	void write(int8_t i)	{ writeValue(i); }
	void write(int16_t i)	{ writeValue(i); }
	void write(int32_t i)	{ writeValue(i); }
	void write(int64_t i)	{ writeValue(i); }
	void write(uint8_t i)	{ writeValue(i); }
	void write(uint16_t i)	{ writeValue(i); }
	void write(uint32_t i)	{ writeValue(i); }
	void write(uint64_t i)	{ writeValue(i); }
	void write(bool b)	{ writeValue(b); }
	void write(float f)	{ writeValue(f); }
	void write(double f)	{ writeValue(f); }

	bool readArrayBegin(void);
	bool readArrayNext(void);
	bool readMapBegin(void);
	bool readMapNext(void);

	void writeArrayBegin(size_t size);
	void writeArrayNext(size_t i);
	void writeArrayEnd(void);
	void writeMapBegin(size_t size);
	void writeMapNext(size_t i);
	void writeMapEnd(void);

	template<typename T> bool readValue(T &value);
	template<typename T> void writeValue(const T &value);

	Stream *mStream;
	int mLevel = 0;
	bool mKey = false;
};

template<typename T>
bool JsonSerializer::readValue(T &value)
{
	String tmp;
	if(!Serializer::read(tmp)) return false;
	AssertIO(tmp.read(value));
	return true;
}

template<typename T>
void JsonSerializer::writeValue(const T &value)
{
	mStream->space();
	mStream->write(value);
	if(mKey) *mStream << ':' << Stream::Space;
	else if(mLevel == 0) *mStream << Stream::NewLine;
	mKey = false;
}

}

#endif
