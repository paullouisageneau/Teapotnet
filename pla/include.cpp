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

#include "pla/include.h"
#include "pla/time.h"

namespace pla
{

Mutex	LogMutex;
bool	ForceLogToFile = false;

#ifdef DEBUG
int	LogLevel = LEVEL_INFO;
#else
int	LogLevel = LEVEL_DEBUG;
#endif

std::string GetFormattedLogTime(void)
{
	Time time(Time::Now());
	return std::string(time.toIsoDate() + " " + time.toIsoTime());
}

}
