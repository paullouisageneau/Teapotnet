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

#include "pla/serializer.hpp"
#include "pla/string.hpp"
#include "pla/binarystring.hpp"
#include "pla/exception.hpp"

namespace pla
{

bool Serializer::Serializer::read(Serializable &s)
{
	return s.deserialize(*this);
}

void Serializer::Serializer::write(const Serializable &s)
{
	s.serialize(*this);
}

bool Serializer::read(String &s)
{
	return read(static_cast<Serializable&>(s));
}

void Serializer::write(const String &s)
{
	write(static_cast<const Serializable&>(s));
}

bool Serializer::read(BinaryString &s)
{
	return read(static_cast<Serializable&>(s));
}

void Serializer::write(const BinaryString &s)
{
	write(static_cast<const Serializable&>(s));
}

bool Serializer::read(uint8_t &i)
{
	uint64_t t = 0;
	if(!read(t)) return false; 
	i = uint8_t(t);
	return true;
}

bool Serializer::read(uint16_t &i)
{
	uint64_t t = 0;
	if(!read(t)) return false; 
	i = uint16_t(t);
	return true;
}

bool Serializer::read(uint32_t &i)
{
	uint64_t t = 0;
	if(!read(t)) return false; 
	i = uint32_t(t);
	return true;
}

bool Serializer::read(uint64_t &i)
{
	int64_t t = 0;
	if(!read(t)) return false; 
	i = uint64_t(t);
	return true;
}

bool Serializer::read(int8_t &i)
{
	int64_t t = 0;
	if(!read(t)) return false; 
	i = int8_t(t);
	return true;
}

bool Serializer::read(int16_t &i)
{
	int64_t t = 0;
	if(!read(t)) return false; 
	i = int16_t(t);
	return true;
}

bool Serializer::read(int32_t &i)
{
	int64_t t = 0;
	if(!read(t)) return false; 
	i = int8_t(t);
	return true;
}

bool Serializer::read(float &f)
{
	double t = 0;
	if(!read(t)) return false; 
	f = float(t);
	return true;
}

bool Serializer::read(bool &b)
{ 
	uint8_t t = 0;
	if(!read(t)) return false; 
	b = (t != 0);
	return true;
}
	
void Serializer::write(uint8_t i)
{
	write(uint64_t(i));
}

void Serializer::write(uint16_t i)
{
	write(uint64_t(i));
}

void Serializer::write(uint32_t i)
{
	write(uint64_t(i));
}

void Serializer::write(uint64_t i)
{
	write(int64_t(i));
}

void Serializer::write(int8_t i)
{
	write(int64_t(i));
}

void Serializer::write(int16_t i)
{
	write(int64_t(i));
}

void Serializer::write(int32_t i)
{
	write(int64_t(i));
}

void Serializer::write(float f)
{
	write(double(f));
}

void Serializer::write(bool b)
{ 
	write(uint8_t(b ? 1 : 0)); 
}

bool Serializer::skip(void)
{
	std::string dummy;
	return read(dummy);
}

}
