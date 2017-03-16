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

#ifndef PLA_EXCEPTION_H
#define PLA_EXCEPTION_H

#include "pla/include.hpp"

#include <exception>
#include <stdexcept>

namespace pla
{

// Base class for exceptions
class Exception : public std::exception
{
public:
	Exception(const std::string &message = "");
	virtual ~Exception() throw();

	// Returns the associated message
	virtual const char* what() const throw();

protected:
	std::string mMessage; // associated message
};

// If a condition is not matched
class AssertException : public Exception
{
public:
	AssertException(const std::string &File, int Line, const std::string &message);
};

#ifdef DEBUG
	#define Assert(condition) if(!(condition)) throw AssertException(__FILE__, __LINE__, "Assertion failed : " #condition)
#else
	inline void DoNothing(bool) {}
	#define Assert(condition) DoNothing(!(condition))
#endif

//#define assert(condition) Assert(condition)

// If a code issue is detected at run-time
class RuntimeException : public Exception
{
public:
	RuntimeException(const std::string &message);
};

// If a file cannot be loaded
class LoadingFailed : public Exception
{
public:
	LoadingFailed(const std::string& File, const std::string &message);
};

// If a functionnality is not available
class Unsupported : public Exception
{
public:
	Unsupported(const std::string &feature);
};

// If a parameter or a field is invalid
class InvalidData : public Exception
{
public:
	InvalidData(const std::string &message);
};

// If an IO exception occured
// TODO: this is often used in place of a InvalidFormat/ParseError
class IOException: public Exception
{
public:
	IOException(const std::string &message = "");
};

#define AssertIO(condition) if(!(condition)) throw IOException()

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
	OutOfBounds(const std::string &message = "");
};

// If a network error occured
class NetException : public Exception
{
public:
	NetException(const std::string &message);
};

// If a serialization error occured
class SerializeException : public Exception
{
public:
	SerializeException(const std::string &problem);
};

// If a timeout occured
class Timeout : public Exception
{
public:
	Timeout(void);
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
