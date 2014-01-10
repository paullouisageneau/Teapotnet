/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
 *   paul-louis (at) ageneau (dot) org                                   *
 *                                                                       *
 *   This file is part of Teapotnet.                                     *
 *                                                                       *
 *   Teapotnet is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU Affero General Public License as      *
 *   published by the Free Software Foundation, either version 3 of      *
 *   the License, or (at your option) any later version.                 *
 *                                                                       *
 *   Teapotnet is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the        *
 *   GNU Affero General Public License for more details.                 *
 *                                                                       *
 *   You should have received a copy of the GNU Affero General Public    *
 *   License along with Teapotnet.                                       *
 *   If not, see <http://www.gnu.org/licenses/>.                         *
 *************************************************************************/

#ifndef TPN_SIGNAL_H
#define TPN_SIGNAL_H

#include "tpn/include.h"
#include "tpn/exception.h"
#include "tpn/mutex.h"

namespace tpn
{

class Signal
{
public:
	Signal(void);
	virtual ~Signal(void);

	void launch(void);
	void launchAll(void);
	void wait(Mutex &mutex);
	bool wait(Mutex &mutex, double &timeout);
	bool wait(Mutex &mutex, const double &timeout);
	
private:
	pthread_cond_t mCond;
};

}

#endif
