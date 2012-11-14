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

#include "html.h"
#include "exception.h"

namespace tpot
{

String Html::escape(const String &str)
{
	String result;
	for(int i=0; i<str.size(); ++i)
	{
		char chr = str[i];
		switch(chr)
		{
		case '"': 	result << "&quot;";	break;
		case '\'': 	result << "&apos;";	break;
		case '<': 	result << "&lt;";	break;
		case '>': 	result << "&gt;";	break;
		case '&': 	result << "&amp;";	break;
		default:	result+=chr;		break;
		}
	}
	return result;
}
  
Html::Html(Stream *stream) :
	mStream(stream),
	mBlank(false),
	mAdmin(false)
{

}

Html::Html(Socket *sock) :
	mStream(sock),
	mBlank(false),
	mAdmin(sock->getRemoteAddress().isLocal())
{

}


Html::~Html(void)
{

}

void Html::header(const String &title, bool blank, const String &redirect)
{
	mBlank = blank;
  
	*mStream<<"<!DOCTYPE html>\n";
	*mStream<<"<html>\n";
	*mStream<<"<head>\n";
	if(blank) *mStream<<"<title>"<<title<<"</title>\n";
	else if(title.empty()) *mStream<<APPNAME<<"</title>\n";
	else *mStream<<"<title>"<<title<<" - "<<APPNAME<<"</title>\n";
	*mStream<<"<meta http-equiv=\"content-type\" content=\"text/html;charset=UTF-8\">\n";
	*mStream<<"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">\n";
	*mStream<<"<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\">\n";
	*mStream<<"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\">\n";
	if(!redirect.empty()) *mStream<<"<meta http-equiv=\"refresh\" content=\"5;URL='"+redirect+"'\">\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/jquery.min.js\"></script>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/common.js\"></script>\n";
	*mStream<<"</head>\n";
	*mStream<<"<body>\n";

	if(!mBlank)
	{
		open("div","page");
		open("div","header");
		openLink("/"); image("/logo.png", APPNAME, "logo"); closeLink();
		open("div","title");
		if(title.empty()) text(APPNAME);
		else text(title);
		close("div");
		close("div");
		open("div","content");
	}
}

void Html::footer(void)
{
	if(!mBlank)
	{
		close("div"); 
		open("div", "footer");
		link(SOURCELINK, "Source code", "", true);
		close("div");
		close("div");
	}

	*mStream<<"</body>\n";
	*mStream<<"</html>\n";
}

void Html::raw(const String &str)
{
	mStream->write(str);
}

void Html::text(const String &str)
{
	mStream->write(escape(str));
}

void Html::object(const Serializable &s)
{
	// TODO
	open("span");
	mStream->write(escape(s.toString()));
	close("span");
}

void Html::open(const String &type, String id)
{
	assert(!type.empty());
	
	if(!id.empty() && id[0] == '#') id.ignore();
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

void Html::openLink(const String &url, String id, bool newTab)
{
	if(!id.empty() && id[0] == '#') id.ignore();
	String cl = id.cut('.');

	*mStream<<"<a href=\""<<url<<"\"";
	if(!id.empty()) *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty()) *mStream<<" class=\""<<cl<<"\"";
	if(newTab) *mStream<<" target=\"_blank\"";
	*mStream<<">";
}

void Html::closeLink(void)
{
	*mStream<<"</a>";
}

void Html::span(const String &txt, String id)
{
 	open("span",id);
	text(txt);
	close("span"); 
}

void Html::div(const String &txt, String id)
{
	open("div",id);
	text(txt);
	close("div");
}

void Html::link(	const String &url,
			const String &txt,
			String id,
			bool newTab)
{
	openLink(url, id, newTab);
	text(txt);
	closeLink();
}

void Html::image(	const String &url,
			const String &alt,
			String id)
{
	if(!id.empty() && id[0] == '#') id.ignore();
  	String cl = id.cut('.');
	
	*mStream<<"<img src=\""<<url<<"\"";
	if(!alt.empty()) *mStream<<" alt=\""<<alt<<"\"";
	if(!id.empty())  *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty())  *mStream<<" class=\""<<cl<<"\"";
	*mStream<<"/>\n";
}

void Html::javascript(const String &code)
{
	*mStream<<"<script type=\"text/javascript\">\n";
	*mStream<<code<<"\n"; 
	*mStream<<"</script>\n"; 
}

void Html::space(void)
{
	*mStream<<' ';
}


void Html::br(void)
{
	*mStream<<"<br/>\n";
}

void Html::openForm(	const String &action,
			const String &method,
		     	const String &name,
			bool multipart)
{
	String enctype;
	if(multipart) enctype = "multipart/form-data";
	else enctype = "application/x-www-form-urlencoded";
	
	*mStream<<"<form";
	if(!name.empty()) *mStream<<" name=\""<<name<<"\"";
	*mStream<<" action=\""<<escape(action)<<"\" method=\""<<method<<"\"";
	if(method == "post") *mStream<<" enctype=\""<<enctype<<"\"";
	*mStream<<">\n";
}

void Html::closeForm(void)
{
	*mStream<<"</form>\n";
}

void Html::openFieldset(const String &legend)
{
	open("fieldset");
	open("legend");
	text(legend);
	close("legend");
}

void Html::closeFieldset(void)
{
  	close("fieldset");
}

void Html::label(const String &name, const String &label)
{
	*mStream<<"<label for=\""<<name<<"\">"<<label<<"</label>\n";
}

void Html::input(const String &type, const String &name, const String &value)
{
	String t(type);
	if(t == "button") t = "submit";
 	*mStream<<"<input type=\""<<t<<"\" class=\""<<name<<"\" name=\""<<name<<"\" value=\""<<value<<"\">\n";
}

void Html::checkbox(const String &name, const String &value, bool checked)
{
 	*mStream<<"<span class=\""<<name<<"\"><input type=\"checkbox\" value=\"1\" name=\""<<name<<"\" "<<(checked ? "checked" : "")<<">";
 	text(value);
 	*mStream<<"</span>\n";
}

void Html::textarea(const String &name, const String &value)
{
	*mStream<<"<textarea class=\""<<name<<"\" name=\""<<name<<"\">";
	text(value);
	*mStream<<"</textarea>\n";
}

void Html::select(const String &name, const StringMap &options, const String &def)
{
	*mStream<<"<select name=\""<<name<<"\">\n";
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

void Html::button(const String &name, const String &text)
{
  	if(text.empty()) input("submit", name, name);
	else input("submit", name, text);
}

void Html::file(const String &name)
{
 	*mStream<<"<input type=\"file\" class=\""<<name<<"\" name=\""<<name<<"\" size=\"30\">\n";
}

Stream *Html::stream(void)
{
	return mStream;
}

}
