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

#ifndef PLA_YAMLSERIALIZER_H
#define PLA_YAMLSERIALIZER_H

#include "pla/serializer.hpp"
#include "pla/string.hpp"

namespace pla
{

class YamlSerializer : public Serializer
{
public:
	YamlSerializer(Stream *stream, int writeLevel = 0);	// stream WON'T be destroyed
	virtual ~YamlSerializer(void);

	bool read(Serializable &s);
	bool read(std::string &str);
	inline bool read(int8_t &i)	{ return readValue(i); }
	inline bool read(int16_t &i)	{ return readValue(i); }
	inline bool read(int32_t &i)	{ return readValue(i); }
	inline bool read(int64_t &i)	{ return readValue(i); }
	inline bool read(uint8_t &i)	{ return readValue(i); }
	inline bool read(uint16_t &i)	{ return readValue(i); }
	inline bool read(uint32_t &i)	{ return readValue(i); }
	inline bool read(uint64_t &i)	{ return readValue(i); }
	inline bool read(bool &b)	{ return readValue(b); }
	inline bool read(float &f)	{ return readValue(f); }
	inline bool read(double &f)	{ return readValue(f); }

	void write(const Serializable &s);
	void write(const std::string &str);
	inline void write(int8_t i)	{ writeValue(i); }
	inline void write(int16_t i)	{ writeValue(i); }
	inline void write(int32_t i)	{ writeValue(i); }
	inline void write(int64_t i)	{ writeValue(i); }
	inline void write(uint8_t i)	{ writeValue(i); }
	inline void write(uint16_t i)	{ writeValue(i); }
	inline void write(uint32_t i)	{ writeValue(i); }
	inline void write(uint64_t i)	{ writeValue(i); }
	inline void write(bool b)	{ writeValue(b); }
	inline void write(float f)	{ writeValue(f); }
	inline void write(double f)	{ writeValue(f); }

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

	void writeClose(void);

private:
	template<typename T> bool readValue(T &value);
	template<typename T> void writeValue(const T &value);

	Stream *mStream;
	String mLine;		// for reading
	Stack<int> mIndent;	// for reading
	int mLevel = 0;		// for writing
	bool mKey = false;	// for writing
};

template<typename T>
bool YamlSerializer::readValue(T &value)
{
	if(mIndent.empty())
	{
		if(mLine.empty() && !mStream->readLine(mLine))
			return false;

		if(mLine.trimmed() == "...") return false;
		if(mLine.trimmed() == "---") mLine.clear();
	}

	return mLine.read(value);
}

template<typename T>
void YamlSerializer::writeValue(const T &value)
{
	if(!mLevel) *mStream<<"---"<<Stream::NewLine;
	else *mStream<<Stream::Space;
	mStream->write(value);
	if(mKey) *mStream<<':';
	else if(!mLevel) *mStream << Stream::NewLine;
	mKey = false;
}

}

#endif
