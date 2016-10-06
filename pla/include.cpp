/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Platform.                                     *
 *                                                                       *
 *   Platform is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Platform is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Platform.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#include "pla/include.hpp"
#include "pla/string.hpp"
#include "pla/time.hpp"

namespace pla
{

std::mutex LogMutex;
bool ForceLogToFile = false;
std::map<std::thread::id, unsigned> ThreadsMap;

#ifdef DEBUG
int	LogLevel = LEVEL_DEBUG;
#else
int	LogLevel = LEVEL_INFO;
#endif

std::string GetFormattedLogTime(void)
{
	Time time(Time::Now());
	return std::string(time.toIsoDate() + " " + time.toIsoTime());
}

}
