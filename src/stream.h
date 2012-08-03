/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Arcanet.                                       *
 *                                                                       *
 *   Arcanet is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Arcanet is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Arcanet.                                         *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef ARC_STREAM_H
#define ARC_STREAM_H

#include "include.h"

#include <sstream>

namespace arc
{

class Serializable;
class String;

class Stream
{
public:
	Stream(void);
	virtual ~Stream(void);

	bool get(char &chr);
	void put(char chr);

	bool ignore(int n = 1);
	bool ignoreUntil(char delimiter);
	bool ignoreUntil(const String &delimiters, char *found = NULL);
	bool ignoreWhile(const String &chars, char *found = NULL);

	void	read(Stream &s);
	bool	read(Serializable &s);
	bool	read(String &s);
	bool	read(bool &b);

	inline bool	read(char &c) 				{ return readData(&c,1); }
	inline bool	read(signed char &i) 		{ return readStd(i); }
	inline bool	read(signed short &i) 		{ return readStd(i); }
	inline bool	read(signed int &i) 		{ return readStd(i); }
	inline bool	read(signed long &i) 		{ return readStd(i); }
	inline bool	read(unsigned char &i) 		{ return readStd(i); }
	inline bool	read(unsigned short &i) 	{ return readStd(i); }
	inline bool	read(unsigned int &i) 		{ return readStd(i); }
	inline bool	read(unsigned long &i) 		{ return readStd(i); }
	inline bool	read(float &f) 				{ return readStd(f); }
	inline bool	read(double &f) 			{ return readStd(f); }

	template<class T> bool read(T *ptr);

	double	readDouble(void);
	float 	readFloat(void);
	int 	readInt(void);
	bool 	readBool(void);

	void	write(Stream &s);
	void	write(const Serializable &s);
	void	write(const String &s);
	void	write(const char *s);
	void	write(const std::string &s);
	void	write(bool b);

	inline void	write(char c) 				{ writeData(&c,1); }
	inline void	write(signed char i) 		{ writeStd(i); }
	inline void	write(signed short i) 		{ writeStd(i); }
	inline void	write(signed int i) 		{ writeStd(i); }
	inline void	write(signed long i) 		{ writeStd(i); }
	inline void	write(unsigned char i) 		{ writeStd(i); }
	inline void	write(unsigned short i) 	{ writeStd(i); }
	inline void	write(unsigned int i) 		{ writeStd(i); }
	inline void	write(unsigned long i) 		{ writeStd(i); }
	inline void	write(float f) 				{ writeStd(f); }
	inline void	write(double f) 			{ writeStd(f); }

	template<class T> void write(const T *ptr);

	template<typename T> Stream& operator>>(T &val);
	template<typename T> Stream& operator<<(const T &val);
	Stream &operator<<(Stream &s);

	bool readLine(Stream &output);
	bool writeLine(Stream &input);
	bool readUntil(Stream &output, char delimiter);
	bool readUntil(Stream &output, const String &delimiters, char *found);
	bool readString(Stream &output, const String &delimiters, const String &ignored, bool skipBefore = false);
	bool readField(Stream &output);
	bool parseField(Stream &output, const String &delimiters, char *found = NULL);

	// TODO: operator !

	static const char Space;

protected:
	virtual int readData(char *buffer, int size) = 0;
	virtual void writeData(const char *data, int size) = 0;

private:
	bool readStdString(std::string &output);
	void error(void);

	template<typename T> bool readStd(T &val);
	template<typename T> void writeStd(const T &val);

	static const String OutputLineDelimiter;
	static const String LineDelimiters;
	static const String IgnoredCharacters;
	static const String FieldDelimiters;
};

// NB: String is not defined here, as it herits from Stream.
// Threrefore, it cannot be used in template functions defined here.

class IOException;

template<class T>
bool Stream::read(T *ptr)
{
	return read(*ptr);
}

template<class T>
void Stream::write(const T *ptr)
{
	write(*ptr);
}

template<typename T> bool Stream::readStd(T &val)
{
	std::string str;
	if(!readStdString(str)) return false;
	std::istringstream iss(str);
	if(!(iss>>val)) error();
	return true;
}

template<typename T> void Stream::writeStd(const T &val)
{
	std::ostringstream oss;
	if(!(oss<<val)) error();
	write(oss.str());
}

template<typename T> Stream& Stream::operator>>(T &val)
{
	read(val);
	return (*this);
}

template<typename T> Stream& Stream::operator<<(const T &val)
{
	write(val);
	return (*this);
}

}

#endif // STREAM_H

