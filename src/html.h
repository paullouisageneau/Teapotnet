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

#ifndef ARC_HTML_H
#define ARC_HTML_H

#include "include.h"
#include "stream.h"
#include "serializable.h"
#include "map.h"

namespace arc
{

class Html
{
public:
	Html(Stream *stream);	// stream WON'T be destroyed
	~Html(void);

	void header(const String &title = "");
	void footer(void);

	void raw(const String &str);
	void text(const String &str);
	void object(const Serializable &s);

	void open(const String &type, String id = "");
	void close(const String &type);

	void link(	const String &url,
			const String &txt,
			String id = "");

	void image(	const String &url,
			const String &alt = "",
			String id = "");

	void br(void);

	void openForm(	const String &action = "#",
			const String &method = "post",
		     	const String &name = "");
	void closeForm(void);
	void input(const String &type, const String &name, const String &value = "");
	void checkbox(const String &name, const String &value, bool checked = false);
	void textarea(const String &name, const String &value = "");
	void select(const String &name, const StringMap &options, const String &def = "");
	void button(const String &name);
	
	Stream *stream(void);

private:
	Stream *mStream;
};

}

#endif
