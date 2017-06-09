/*************************************************************************
 *   Copyright (C) 2011-2017 by Paul-Louis Ageneau                       *
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

#include "tpn/html.hpp"
#include "tpn/request.hpp"
#include "tpn/user.hpp"
#include "tpn/config.hpp"

#include "pla/exception.hpp"
#include "pla/mime.hpp"

namespace tpn
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
	mBlank(false)
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
	else if(title.empty()) *mStream<<"<title>"<<APPNAME<<"</title>\n";
	else *mStream<<"<title>"<<title<<" - "<<APPNAME<<"</title>\n";
	*mStream<<"<meta http-equiv=\"content-type\" content=\"text/html;charset=UTF-8\">\n";
	*mStream<<"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">\n";
	*mStream<<"<link rel=\"stylesheet\" type=\"text/css\" href=\"/static/style.css\">\n";
	*mStream<<"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/static/favicon.ico\">\n";
	if(!redirect.empty()) *mStream<<"<meta http-equiv=\"refresh\" content=\"3;URL='"+redirect+"'\">\n";
	*mStream<<"<noscript><meta http-equiv=\"refresh\" content=\"0;url=/static/noscript.html\"></noscript>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/static/jquery.min.js\"></script>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/static/jquery.form.min.js\"></script>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/static/common.js\"></script>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/static/directory.js\"></script>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/static/mail.js\"></script>\n";
	*mStream<<"<base target=\"_parent\">\n";

	// Load specific CSS on touch devices
	javascript("var deviceAgent = navigator.userAgent.toLowerCase();\n\
if(deviceAgent.indexOf('android') >= 0 || deviceAgent.indexOf('iPhone') >= 0 || deviceAgent.indexOf('iPad') >= 0)\n\
	$('head').append('<link rel=\"stylesheet\" type=\"text/css\" href=\"/static/touchscreen.css\">');");

	// Register teapotnet handler
	if(!blank) javascript("var handler = location.protocol + '//' + location.host + '/?url=%s';\n\
navigator.registerProtocolHandler('teapotnet', handler, 'Teapotnet handler');");

	*mStream<<"</head>\n";
	*mStream<<"<body>\n";

	open("div","page");

	if(!mBlank)
	{
		open("div","header");
		openLink("/","#backlink"); image("/static/logo.png", APPNAME, "logo"); closeLink();
		open("div","title");
		if(title.empty()) text(APPNAME);
		else text(title);
		close("div");
		close("div");
		//javascript("$('#backlink').click(function(){ window.location.href = getBasePath(1); return false;});");
		open("div","content");
	}
}

void Html::footer(void)
{
	if(!mBlank)
	{
		close("div");
		open("div", "footer");
		close("div");
	}

	close("div");

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
	open("span");
	mStream->write(escape(s.toString()));
	close("span");
}

void Html::open(const String &type, String id)
{
	Assert(!type.empty());

	if(!id.empty() && id[0] == '#') id.ignore();
	String cl = id.cut('.');

	*mStream<<"<"<<type;
	if(!id.empty()) *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty()) *mStream<<" class=\""<<cl<<"\"";
	*mStream<<">";
}

void Html::close(const String &type)
{
	Assert(!type.empty());
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

void Html::link(const String &url,
			const String &txt,
			String id,
			bool newTab)
{
	openLink(url, id, newTab);
	text(txt);
	closeLink();
}

void Html::image(const String &url,
			const String &alt,
			String id)
{
	if(!id.empty() && id[0] == '#') id.ignore();
	String cl = id.cut('.');

	*mStream<<"<img src=\""<<url<<"\"";
	if(!alt.empty()) *mStream<<" alt=\""<<alt<<"\"";
	if(!id.empty())  *mStream<<" id=\""<<id<<"\"";
	if(!cl.empty())  *mStream<<" class=\""<<cl<<"\"";
	*mStream<<"/>";
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

void Html::openForm(const String &action,
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
	*mStream<<"<label";
	if(!name.empty()) *mStream<<" for=\""<<name<<"\"";
	*mStream<<">"<<label<<"</label>\n";
}

void Html::input(const String &type, const String &name, const String &value, bool readonly, bool noautocomplete)
{
	*mStream<<"<input type=\""<<(type == "button" ? "submit" : type)<<"\" name=\""<<name<<"\" value=\""<<value<<"\""<<(readonly ? " readonly" : "");
	if(noautocomplete) *mStream<<" autocomplete=\"off\"";
	*mStream<<">\n";
}

void Html::checkbox(const String &name, const String &value, bool checked, bool readonly)
{
	*mStream<<"<span class=\"checkbox\"><input type=\"checkbox\" value=\"1\" name=\""<<name<<"\""<<(checked ? " checked" : "")<<(readonly ? " readonly" : "")<<">&nbsp;";
	text(value);
	*mStream<<"</span>\n";
}

void Html::textarea(const String &name, const String &value, bool readonly)
{
	*mStream<<"<textarea name=\""<<name<<"\""<<(readonly ? " readonly" : "")<<">";
	text(value);
	*mStream<<"</textarea>\n";
}

void Html::select(const String &name, const StringMap &options, const String &def, bool readonly)
{
	*mStream<<"<select name=\""<<name<<"\""<<(readonly ? " readonly" : "")<<">\n";
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

void Html::button(const String &name, String text)
{
	if(text.empty()) input("submit", name, name);
	else input("submit", name, text);
}

void Html::file(const String &name, String text)
{
	if(text.empty()) text = name;
	String id = String("input_file_") + name;
	*mStream<<"<input type=\"file\" id=\""<<id<<"\" class=\""<<name<<"\" name=\""<<name<<"\" size=\"30\">\n";
	javascript("$('#"+id+"').css('visibility', 'hidden').css('display', 'inline').css('width', '0px').css('margin', '0px').css('padding', '0px');\n\
		$('#"+id+"').after('<a class=\"button\" href=\"javascript:void(0)\" onclick=\"$(\\'#"+id+"\\').click(); return false;\">"+escape(text)+"</a>');");
}

Stream *Html::stream(void)
{
	return mStream;
}

}
