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

#ifndef TPN_INCLUDE_H
#define TPN_INCLUDE_H

//#define DEBUG			1
#define APPNAME			"Teapotnet"
#define APPVERSION		"0.9.4"
#define APPMAGIC		 0x54504f54 // "TPOT"
#define APPAUTHOR		"Paul-Louis Ageneau"
#define APPLINK			"http://teapotnet.org/"
#define SOURCELINK		"http://teapotnet.org/source"
#define HELPLINK		"http://teapotnet.org/help"
#define BUGSLINK		"mailto:bugs@teapotnet.org"
#define DOWNLOADURL		"https://teapotnet.org/download"
#define RAPTUREURL		"https://teapotnet.org/rapture/"	// Rapture must use HTTPS 

#include "pla/include.h"
#include "pla/string.h"
#include "pla/binarystring.h"

using namespace pla;

namespace tpn
{
	typedef BinaryString Identifier;
	typedef std::pair<Identifier, Identifier> IdentifierPair;
}

#endif
