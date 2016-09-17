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

#include "pla/exception.hpp"
#include "pla/string.hpp"

namespace pla
{

Exception::Exception(const std::string &message) :
		mMessage(message)
{

}

Exception::~Exception() throw()
{

}

const char* Exception::what() const throw()
{
	if(!mMessage.empty()) return mMessage.c_str();
	else return "(No information)";

}

AssertException::AssertException(const std::string& File, int Line, const std::string &message)
{
	String strLine;
	strLine << Line;
	mMessage = message + " in " + File + " at line " + strLine;
}

RuntimeException::RuntimeException(const std::string &message)
{
	mMessage = "Runtime exception: " + message;
}

LoadingFailed::LoadingFailed(const std::string& File, const std::string &message)
{
	mMessage = "Loading failed for \"" + File + "\": " + message;
}

Unsupported::Unsupported(const std::string &feature)
{
	mMessage = "Unsupported";
	if(!feature.empty()) mMessage+= ": " + feature;
	else mMessage+= " feature";
}

InvalidData::InvalidData(const std::string &message)
{
	mMessage = "Invalid Data";
	if(!message.empty()) mMessage+= ": " + message;
}

IOException::IOException(const std::string &message) :
	Exception(std::string() + message)
{
	mMessage = "Unexpected end of input";
	if(!message.empty()) mMessage+= ": " + message;
}

OutOfMemory::OutOfMemory(void) :
	Exception("Insufficient memory")
{

}

DivideByZero::DivideByZero(void) :
	Exception("Division by zero")
{
	
}

OutOfBounds::OutOfBounds(const std::string &message) :
	Exception(std::string("Out of bounds: ") + message)
{

}

NetException::NetException(const std::string &message) :
	Exception(std::string("Network error: ") + message)
{

}

SerializeException::SerializeException(const std::string &problem) :
	Exception(std::string("Unexpected data: ") + problem)
{

}

Timeout::Timeout(void) :
	Exception("Timeout")
{

}


}
