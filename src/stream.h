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

#ifndef TPOT_STREAM_H
#define TPOT_STREAM_H

#include "include.h"

#include <sstream>

namespace tpot
{

class Serializable;
class String;
class ByteStream;

class Stream
{
public:
	static const String IgnoredCharacters;
	static const String BlankCharacters;
	static const char NewLine;
	static const char Space;
	
	Stream(void);
	virtual ~Stream(void);

	// Settings
	bool hexaMode(void);
	bool hexaMode(bool enabled);
		
	// Data-level access
	virtual size_t readData(char *buffer, size_t size) = 0;
	virtual void writeData(const char *data, size_t size) = 0;
	size_t readData(ByteStream &s, size_t max);
	size_t writeData(ByteStream &s, size_t max);
	
	// Atomic
	bool get(char &chr);
	bool ignore(int n = 1);
	void put(char chr);
	void space(void);
	void newline(void);

	char last(void) const;
	bool atEnd(void) const;
	
	// Conditionnal
	bool ignoreUntil(char delimiter);
	bool ignoreUntil(const String &delimiters);
	bool ignoreWhile(const String &chars);
	bool readUntil(Stream &output, char delimiter);
	bool readUntil(Stream &output, const String &delimiters);
	
	// Reading
	size_t	read(Stream &s);
	bool	read(Serializable &s);
	size_t	read(Stream &s, size_t max);
	bool	read(String &s);
	bool	read(bool &b);

	inline bool	read(char &c) 			{ return readData(&c,1); }
	inline bool	read(signed char &i) 		{ return readStd(i); }
	inline bool	read(signed short &i) 		{ return readStd(i); }
	inline bool	read(signed int &i) 		{ return readStd(i); }
	inline bool	read(signed long &i) 		{ return readStd(i); }
	inline bool	read(unsigned char &i) 		{ return readStd(i); }
	inline bool	read(unsigned short &i) 	{ return readStd(i); }
	inline bool	read(unsigned int &i) 		{ return readStd(i); }
	inline bool	read(unsigned long &i) 		{ return readStd(i); }
	inline bool	read(unsigned long long &i) 	{ return readStd(i); }
	inline bool	read(float &f) 			{ return readStd(f); }
	inline bool	read(double &f) 		{ return readStd(f); }

	double	readDouble(void);
	float 	readFloat(void);
	int 	readInt(void);
	bool 	readBool(void);

	// Writing
	void	write(Stream &s);
	void	write(const Serializable &s);
	void	write(const String &s);
	void	write(const char *s);
	void	write(const std::string &s);
	void	write(bool b);
	
	inline void	write(char c) 			{ writeData(&c,1); }
	inline void	write(signed char i) 		{ writeStd(i); }
	inline void	write(signed short i) 		{ writeStd(i); }
	inline void	write(signed int i) 		{ writeStd(i); }
	inline void	write(signed long i) 		{ writeStd(i); }
	inline void	write(unsigned char i) 		{ writeStd(i); }
	inline void	write(unsigned short i) 	{ writeStd(i); }
	inline void	write(unsigned int i) 		{ writeStd(i); }
	inline void	write(unsigned long i) 		{ writeStd(i); }
	inline void	write(unsigned long long i) 	{ writeStd(i); }
	inline void	write(float f) 			{ writeStd(f); }
	inline void	write(double f) 		{ writeStd(f); }

	// Flow operators
	template<typename T> Stream& operator>>(T &val);
	template<typename T> Stream& operator<<(const T &val);
	Stream &operator<<(Stream &s);
	
	// Parsing
	bool assertChar(char chr);
	bool readChar(char &chr);
	bool readLine(String &str);
	bool readString(String &str);
	template<typename T> bool readLine(T &output);
	template<typename T> bool writeLine(const T &input);
	
protected:
	char mLast;
	bool mHexa;
	bool mEnd;

private:
	bool readStdString(std::string &output);
	void error(void);

	template<typename T> bool readStd(T &val);
	template<typename T> void writeStd(const T &val);
};

// NB: String is not defined here, as it herits from Stream.
// Threrefore, it cannot be used in template functions defined here.
// Template functions readLine() and writeLine() are defined in string.h

class IOException;

template<typename T> bool Stream::readStd(T &val)
{
	std::string str;
	if(!readStdString(str) || str.empty()) return false;
	std::istringstream iss(str);
	if(mHexa) iss>>std::hex;
	else iss>>std::dec;
	if(!(iss>>val)) error();
	return true;
}

template<typename T> void Stream::writeStd(const T &val)
{
	std::ostringstream oss;
	if(mHexa) oss<<std::hex<<std::uppercase;
	else oss<<std::dec;
	if(!(oss<<val)) error();
	write(oss.str());
}

template<typename T> Stream& Stream::operator>>(T &val)
{
	if(!read(val)) error();
	return (*this);
}

template<typename T> Stream& Stream::operator<<(const T &val)
{
	write(val);
	return (*this);
}

}

#endif // STREAM_H

