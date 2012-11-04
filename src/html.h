/*************************************************************************
 *   Copyright (C) 2011-2012 by Paul-Louis Ageneau                       *
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

#ifndef TPOT_HTML_H
#define TPOT_HTML_H

#include "include.h"
#include "stream.h"
#include "socket.h"
#include "serializable.h"
#include "map.h"

namespace tpot
{

class Html
{
public:
	static String escape(const String &str);
  
	Html(Stream *stream);	// stream WON'T be destroyed
	Html(Socket *sock);	// sock WON'T be destroyed
	~Html(void);

	void header(const String &title = "", bool blank = false, const String &redirect = "");
	void footer(void);

	void raw(const String &str);
	void text(const String &str);
	void object(const Serializable &s);
	
	void open(const String &type, String id = "");
	void close(const String &type);
	
	void openLink(const String &url, String id = "", bool newTab = false);
	void closeLink(void);
	
	void span(const String &txt, String id = "");
	void div(const String &txt, String id = "");
	
	void link(	const String &url,
			const String &txt,
			String id = "",
			bool newTab = false);
	
	void image(	const String &url,
			const String &alt = "",
			String id = "");

	void javascript(const String &code);
	
	void space(void);
	void br(void);

	void openForm(	const String &action = "#",
			const String &method = "post",
		     	const String &name = "",
			bool multipart = false);
	void closeForm(void);
	void openFieldset(const String &legend);
	void closeFieldset(void);
	void label(const String &name, const String &label = "");
	void input(const String &type, const String &name, const String &value = "");
	void checkbox(const String &name, const String &value, bool checked = false);
	void textarea(const String &name, const String &value = "");
	void select(const String &name, const StringMap &options, const String &def = "");
	void button(const String &name, const String &text = "");
	void file(const String &name);
	
	Stream *stream(void);

private:
	Stream *mStream;
	bool mBlank;
	bool mAdmin;
};

}

#endif
