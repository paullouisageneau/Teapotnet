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

#ifndef PLA_STREAM_H
#define PLA_STREAM_H

#include "pla/include.hpp"

#include <sstream>

namespace pla
{

class Serializable;
class BinaryString;
class String;
class Pipe;

class Stream
{
public:
	static const String IgnoredCharacters;
	static const String BlankCharacters;
	static const String NewLine;
	static const char Space;

	Stream(void) {}
	virtual ~Stream(void) {}

	// Settings
	bool hexaMode(void);
	bool hexaMode(bool enabled);

	// Data-level access
	virtual size_t readData(char *buffer, size_t size) = 0;
	virtual void writeData(const char *data, size_t size) = 0;
	virtual bool waitData(duration timeout);
	virtual void seekRead(int64_t position);
	virtual void seekWrite(int64_t position);
	virtual int64_t tellRead(void) const;
	virtual int64_t tellWrite(void) const;
	virtual bool nextRead(void);	// next sub-stream (next datagram if isDatagram, false in normal case)
	virtual bool nextWrite(void);
	virtual void clear(void);
	virtual void flush(void);
	virtual void close(void);
	virtual bool ignore(size_t size = 1);
	virtual bool skipMark(void);
	virtual bool isDatagram(void) const;

	size_t readData(Stream &s, size_t max);
	size_t writeData(Stream &s, size_t max);
	inline void discard(void) { clear(); }

	// Atomic
	bool get(char &chr);
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
	int64_t	read(Stream &s);
	int64_t	read(Stream &s, int64_t max);
	bool	read(Serializable &s);
	bool    read(std::string &str);
	bool	read(bool &b);

	// Disambiguation
	bool	read(BinaryString &str);
	bool	read(String &str);

	inline bool	read(char &c) 				{ return readData(&c,1); }
	inline bool	read(signed char &i) 		{ return readStd(i); }
	inline bool	read(signed short &i) 		{ return readStd(i); }
	inline bool	read(signed int &i) 		{ return readStd(i); }
	inline bool	read(signed long &i) 		{ return readStd(i); }
	inline bool	read(signed long long &i)	{ return readStd(i); }
	inline bool	read(unsigned char &i) 		{ return readStd(i); }
	inline bool	read(unsigned short &i) 	{ return readStd(i); }
	inline bool	read(unsigned int &i) 		{ return readStd(i); }
	inline bool	read(unsigned long &i) 		{ return readStd(i); }
	inline bool	read(unsigned long long &i) { return readStd(i); }
	inline bool	read(float &f) 				{ return readStd(f); }
	inline bool	read(double &f) 			{ return readStd(f); }

	double	readDouble(void);
	float 	readFloat(void);
	int 	readInt(void);
	bool 	readBool(void);

	// Writing
	int64_t	write(Stream &s);
	int64_t	write(Stream &s, int64_t max);
	void	write(const Serializable &s);
	void	write(const char *str);
	void	write(const std::string &str);
	void	write(bool b);

	// Disambiguation
	void	write(const BinaryString &str);
	void	write(const String &str);

	inline void	write(char c) 				{ writeData(&c,1); }
	inline void	write(signed char i) 		{ writeStd(i); }
	inline void	write(signed short i) 		{ writeStd(i); }
	inline void	write(signed int i) 		{ writeStd(i); }
	inline void	write(signed long i) 		{ writeStd(i); }
	inline void	write(signed long long i)	{ writeStd(i); }
	inline void	write(unsigned char i) 		{ writeStd(i); }
	inline void	write(unsigned short i) 	{ writeStd(i); }
	inline void	write(unsigned int i) 		{ writeStd(i); }
	inline void	write(unsigned long i) 		{ writeStd(i); }
	inline void	write(unsigned long long i) { writeStd(i); }
	inline void	write(float f) 				{ writeStd(f); }
	inline void	write(double f) 			{ writeStd(f); }

	// Flow operators
	template<typename T> Stream& operator>>(T &val);
	template<typename T> Stream& operator<<(const T &val);
	Stream &operator<<(Stream &s);
	bool operator!(void) const 	{ return mFailed;  }
	operator bool(void) const	{ return !mFailed; }

	// Parsing
	bool assertChar(char chr);
	bool readChar(char &chr);
	bool readLine(String &str);
	bool readString(String &str);
	template<typename T> bool readLine(T &output);
	template<typename T> void writeLine(const T &input);

	// Binary reading
	int64_t	readBinary(Stream &s, int64_t max)	{ return read(s, max); }
	int64_t	readBinary(char *data, size_t size);	// blocks until size bytes are read
	bool	readBinary(BinaryString &str);
	bool	readBinary(int8_t &i);
	bool	readBinary(int16_t &i);
	bool	readBinary(int32_t &i);
	bool	readBinary(int64_t &i);
	bool	readBinary(uint8_t &i);
	bool	readBinary(uint16_t &i);
	bool	readBinary(uint32_t &i);
	bool	readBinary(uint64_t &i);
	bool	readBinary(float32_t &f);
	bool	readBinary(float64_t &f);

	template<class T> bool readBinary(T *ptr);

	// Binary writing
	int64_t	writeBinary(Stream &s, int64_t max)		{ return write(s, max); }
	void    writeBinary(const char *data, size_t size)	{ writeData(data, size); }
	void	writeBinary(const BinaryString &str);
	void	writeBinary(int8_t i);
	void	writeBinary(int16_t i);
	void	writeBinary(int32_t i);
	void	writeBinary(int64_t i);
	void	writeBinary(uint8_t i);
	void	writeBinary(uint16_t i);
	void	writeBinary(uint32_t i);
	void	writeBinary(uint64_t i);
	void	writeBinary(float32_t f);
	void	writeBinary(float64_t f);

	template<class T> void writeBinary(const T *ptr);

	void writeZero(size_t size = 1);

protected:
	char mLast   = 0;
	bool mHexa   = false;
	bool mEnd    = false;
	bool mFailed = false;

private:
	bool readStdString(std::string &output);

	template<typename T> bool readStd(T &val);
	template<typename T> void writeStd(const T &val);

	virtual Stream *pipeIn(void);	// return the write end for a pipe

	uint16_t fixEndianess(uint16_t n);
	uint32_t fixEndianess(uint32_t n);
	uint64_t fixEndianess(uint64_t n);
};

// NB: String is not defined here, as it herits from Stream.
// Therefore, it cannot be used in template functions defined here.
// Template functions readLine() and writeLine() are defined in string.h

class IOException;

template<typename T> bool Stream::readStd(T &val)
{
	std::string str;
	if(!readStdString(str) || str.empty()) return false;
	std::istringstream iss(str);
	if(mHexa) iss>>std::hex;
	else iss>>std::dec;
	return !!(iss>>val);
}

template<typename T> void Stream::writeStd(const T &val)
{
	std::ostringstream oss;
	if(mHexa) oss<<std::hex<<std::uppercase;
	else oss<<std::dec;
	oss<<val;
	write(oss.str());
}

template<typename T> Stream& Stream::operator>>(T &val)
{
	mFailed|= !read(val);
	return (*this);
}

template<typename T> Stream& Stream::operator<<(const T &val)
{
	write(val);
	return (*this);
}

}

#endif // STREAM_H
