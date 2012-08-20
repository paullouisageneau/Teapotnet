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

#ifndef ARC_EXCEPTION_H
#define ARC_EXCEPTION_H

#include "include.h"
#include "string.h"
#include <exception>

namespace arc
{

// Base class for exceptions
class Exception : public std::exception
{
public:
    Exception(const String &message = "");
    virtual ~Exception() throw();

    // Returns the associated message
    virtual const char* what() const throw();

protected:
    String mMessage; // associated message
};

// If a condition is not matched
class AssertException : public Exception
{
public:
    AssertException(const String &File, int Line, const String &message);
};

#ifdef DEBUG
	#define Assert(condition) if (!(condition)) throw AssertException(__FILE__, __LINE__, "AssertIOn failed : " #condition)
#else
	inline void DoNothing(bool) {}
	#define Assert(condition) DoNothing(!(condition))
#endif

#define assert(condition) Assert(condition)

// If a file cannot be loaded
class LoadingFailed : public Exception
{
public:
    LoadingFailed(const String& File, const String &message);
};

// If a functionnality is not available
class Unsupported : public Exception
{
public:
    Unsupported(const String &feature);
};

// If a paramter or a piece of data is invalid
class InvalidData : public Exception
{
public:
    InvalidData(const String &dataName);
};

// If an IO exception occured
class IOException: public Exception
{
public:
	IOException(const String &message = "");
};

#define AssertIO(condition) {if(!(condition)) throw IOException();}

// If the system is out of memory
class OutOfMemory : public Exception
{
    OutOfMemory(void);
};

// If a division by zero has been attempted
class DivideByZero : public Exception
{
public:
	DivideByZero(void);
};

// If an out-of-bounds access occured
class OutOfBounds: public Exception
{
public:
	OutOfBounds(const String &message = "");
};

// If an OpenGL error occured
class GlError : public Exception
{
public:
	GlError(void);
};

// If a network error occured
class NetException : public Exception
{
public:
	NetException(const String &message);
};

// If a serialization error occured
class SerializeException : public Exception
{
public:
	SerializeException(const String &problem);
};

inline void AssertZero(int nbr) { if(!nbr) throw DivideByZero(); }
inline void AssertZero(float nbr) { if(std::fabs(nbr) <= std::numeric_limits<float>::epsilon()) throw DivideByZero(); }
inline void AssertZero(double nbr) { if(std::fabs(float(nbr)) <= std::numeric_limits<float>::epsilon()) throw DivideByZero(); }

template<typename T> T operator/(T x,T y)
{
	AssertZero(y);
	return x/y;
}

}

#endif
