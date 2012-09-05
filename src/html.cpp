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

#include "html.h"
#include "exception.h"

namespace arc
{

Html::Html(Stream *stream) :
		mStream(stream)
{

}

Html::~Html(void)
{

}

void Html::header(const String &title)
{
	*mStream<<"<!DOCTYPE html>\n";
	*mStream<<"<html>\n";
	*mStream<<"<head>\n";
	*mStream<<"<title>"<<title<<"</title>\n";
	*mStream<<"</head>\n";
	*mStream<<"<body>\n";

}

void Html::footer(void)
{
	*mStream<<"</body>\n";
	*mStream<<"</html>\n";
}

void Html::raw(const String &str)
{
	mStream->write(str);
}

void Html::text(const String &str)
{
	for(int i=0; i<str.size(); ++i)
	{
		char chr = str[i];
		switch(chr)
		{
		case '"': 	mStream->write("&quot;");	break;
		case '\'': 	mStream->write("&apos;");	break;
		case '<': 	mStream->write("&lt;");		break;
		case '>': 	mStream->write("&gt;");		break;
		case '&': 	mStream->write("&amp;");	break;
		default:	mStream->put(chr);			break;
		}
	}
}

void Html::object(const Serializable &s)
{
	s.html(*mStream);
}

void Html::open(const String &type, String id)
{
	assert(!type.empty());
	String cl = id.cut('.');
	*mStream<<"<"<<type;
	if(!id.empty()) *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty()) *mStream<<" class=\""<<cl<<"\"";
	*mStream<<">";
}

void Html::close(const String &type)
{
	assert(!type.empty());
	*mStream<<"</"<<type<<">\n";
}

void Html::link(	const String &url,
			const String &txt,
			String id)
{
	String cl = id.cut('.');
	*mStream<<"<a href=\""<<url<<"\"";
	if(!id.empty()) *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty()) *mStream<<" class=\""<<cl<<"\"";
	*mStream<<">";
	text(txt);
	*mStream<<"</a>\n";
}

void Html::image(	const String &url,
			const String &alt,
			String id)
{
	String cl = id.cut('.');
	*mStream<<"<img src=\""<<url<<"\"";
	if(!alt.empty()) *mStream<<" alt=\""<<alt<<"\"";
	if(!id.empty())  *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty())  *mStream<<" class=\""<<cl<<"\"";
	*mStream<<"/>\n";
}

void Html::br(void)
{
	*mStream<<"<br/>\n";
}

void Html::openForm(	const String &action,
			const String &method,
		     	const String &name)
{
	*mStream<<"form";
	if(!name.empty()) *mStream<<" name=\""<<name<<"\"";
	*mStream<<" action=\""<<action<<"\" method=\""<<method<<"\"";
	if(method == "post") *mStream<<" enctype=\"application/x-www-form-urlencoded\"";
	*mStream<<"/>\n";
}

void Html::closeForm(void)
{
	*mStream<<"</form>\n";
}

void Html::input(const String &type, const String &name, const String &value)
{
 	*mStream<<"<input type=\""<<type<<"\" class=\""<<name<<"\" name=\""<<name<<"\" value=\""<<value<<"\"/>\n";
}

void Html::checkbox(const String &name, const String &value, bool checked)
{
 	*mStream<<"<span class=\""<<name<<"\"><input type=\"checkbox\" value=\"1\" name=\""<<name<<"\" "<<(checked ? "checked" : "")<<"/>";
 	text(value);
 	*mStream<<"</span>\n";
}

void Html::textarea(const String &name, const String &value)
{
	*mStream<<"<textarea class=\""<<name<<"\" name=\""<<name<<"\"/>";
	text(value);
	*mStream<<"</textarea>\n";
}

void Html::select(const String &name, const StringMap &options, const String &def)
{
	*mStream<<"<select name=\""<<name<<"\"/>\n";
	for(StringMap::const_iterator it=options.begin(); it!=options.end(); ++it)
	{
		 *mStream<<"<option ";
		 if(def == it->first) *mStream<<"selected ";
		 *mStream<<"value=\""<<it->first<<"\">";
		 text(it->second);
		 *mStream<<"</option>\n";
	}
	*mStream<<"</select>\n";
}

void Html::button(const String &name)
{
	*mStream<<"<input type=\"submit\" class=\"button\" name=\""<<name<<"\" value=\"name\"/>\n";
}

Stream *Html::stream(void)
{
	return mStream;
}

}
