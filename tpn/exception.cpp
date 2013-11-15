/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/exception.h"

namespace tpn
{

Exception::Exception(const String &message) :
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

AssertException::AssertException(const String& File, int Line, const String &message)
{
	mMessage<<message<<" in "<<File<<" at line "<<Line;
}

LoadingFailed::LoadingFailed(const String& File, const String &message)
{
	mMessage<<"loading failed for \""<<File<<"\": "<<message;
}

Unsupported::Unsupported(const String &feature)
{
	mMessage = "Unsupported";
	if(!feature.empty()) mMessage+= ": " + feature;
	else mMessage+= " feature";
}

InvalidData::InvalidData(const String &dataName)
{
	mMessage = "Invalid Data";
	if(!dataName.empty()) mMessage+= ": " + dataName;
}

IOException::IOException(const String &message) :
	Exception(String() + message)
{
	mMessage = "IO error";
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

OutOfBounds::OutOfBounds(const String &message) :
	Exception(String("Out of bounds: ") + message)
{

}

NetException::NetException(const String &message) :
	Exception(String("Network error: ") + message)
{

}

SerializeException::SerializeException(const String &problem) :
	Exception(String("Unexpected data: ") + problem)
{

}

Timeout::Timeout(void) :
	Exception("Timeout")
{

}


}
