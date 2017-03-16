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

#ifndef PLA_MIME_H
#define PLA_MIME_H

#include "pla/include.hpp"
#include "pla/string.hpp"
#include "pla/map.hpp"

namespace pla
{

class Mime
{
public:
	static bool IsAudio(const String &fileName);
	static bool IsVideo(const String &fileName);
	static String GetType(const String &fileName);

private:
	static void Init(void);

	static StringMap Types;
	static std::mutex TypesMutex;

	Mime(void);
	~Mime(void);
};

}

#endif
