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

#include "tpn/html.h"
#include "tpn/exception.h"
#include "tpn/request.h"
#include "tpn/user.h"
#include "tpn/mime.h"
#include "tpn/config.h"

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
	else if(title.empty()) *mStream<<"<title>"<<APPNAME<<"</title>\n";
	else *mStream<<"<title>"<<title<<" - "<<APPNAME<<"</title>\n";
	*mStream<<"<meta http-equiv=\"content-type\" content=\"text/html;charset=UTF-8\">\n";
	*mStream<<"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=1\">\n";
	*mStream<<"<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\">\n";
	*mStream<<"<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"/favicon.ico\">\n";
	if(!redirect.empty()) *mStream<<"<meta http-equiv=\"refresh\" content=\"5;URL='"+redirect+"'\">\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/jquery.min.js\"></script>\n";
	*mStream<<"<script type=\"text/javascript\" src=\"/common.js\"></script>\n";
	
	javascript("var deviceAgent = navigator.userAgent.toLowerCase();\n\
		if(deviceAgent.indexOf('android') > -1) $('head').append('<link rel=\"stylesheet\" type=\"text/css\" href=\"/android.css\">');");
	
	*mStream<<"</head>\n";
	*mStream<<"<body>\n";

	open("div","page");
	
	if(!mBlank)
	{
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
	*mStream<<"<label";
	if(!name.empty()) *mStream<<" for=\""<<name<<"\"";
	*mStream<<">"<<label<<"</label>\n";
}

void Html::input(const String &type, const String &name, const String &value, bool noautocomplete)
{
	String t(type);
	if(t == "button") t = "submit";
 	*mStream<<"<input type=\""<<t<<"\" class=\""<<name<<"\" name=\""<<name<<"\" value=\""<<value<<"\"";
 	if(noautocomplete) *mStream<<" autocomplete=\"off\"";
 	*mStream<<">\n";
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
	javascript("$('#"+id+"').css('visibility', 'hidden').css('display', 'inline').css('width', '0px');\n\
		$('#"+id+"').after('<a class=\"button\" href=\"javascript:void(0)\" onclick=\"$(\\'#"+id+"\\').click()\">"+escape(text)+"</a>');");
}

void Html::listFilesFromRequest(Request &trequest, const String &prefix, Http::Request &request, const User *user, bool playlistMode)
{
	if(!playlistMode) open("div",".box");

	try {
		bool isPlayable = false;
		Set<String> instances;
		Map<String, StringMap> files;
		for(int i=0; i<trequest.responsesCount(); ++i)
		{
			Request::Response *tresponse = trequest.response(i);
			if(tresponse->error()) continue;
		
			StringMap params = tresponse->parameters();
			instances.insert(tresponse->instance());
			params.insert("instance", tresponse->instance());

			// Check info
			if(!params.contains("type")) continue;
			if(!params.contains("name")) continue;
			if(!params.contains("hash")) params.insert("hash", "");
			if(!params.contains("path")) params.insert("path", String("/") + params.get("name"));
			
			// Get corresponding contact
			Identifier identifier = tresponse->peering();
			if(identifier == Identifier::Null)
			{
				if(!user) continue;
				const AddressBook::Contact *self = user->addressBook()->getSelf();
				if(self) params["_url"] = user->urlPrefix() + "/myself/files" + params.get("path");
				else params["_url"] = user->urlPrefix() + "/files" + params.get("path");
			}
			else {
				const AddressBook::Contact *contact = NULL;	  
				if(user) contact = user->addressBook()->getContact(identifier);
				if(contact) params["_url"] = contact->urlPrefix() + "/files" + params.get("path");
				else if(params.get("hash").empty()) continue;
			}
			
			// Sort
			// Directories with the same name appears only once
			// Files with identical content appears only once
			if(params.get("type") == "directory") files.insert("0"+params.get("name").toLower(),params);
			else files.insert("1"+params.get("name").toLower()+params.get("hash"), params);
			
			isPlayable|= ((Mime::IsAudio(params.get("name")) || Mime::IsVideo(params.get("name")))
					&& !params.get("hash").empty());
		}

		if(playlistMode)
		{
			String host;
			if(!request.headers.get("Host", host))
				host = String("localhost:") + Config::Get("interface_port");
		
			mStream->writeLine("#EXTM3U"); 
			for(Map<String, StringMap>::iterator it = files.begin();
				it != files.end();
				++it)
			{
				StringMap &map = it->second;
				if(map.get("type") == "directory") continue;
				if(!Mime::IsAudio(map.get("name")) && !Mime::IsVideo(map.get("name"))) continue;
				String link = "http://" + host + "/" + map.get("hash");
				mStream->writeLine("#EXTINF:-1," + map.get("name").beforeLast('.'));
				mStream->writeLine(link);
			}
		}
		else if(files.empty()) text("No files");
		else {
			String desc;
                        if(instances.size() == 1) desc << files.size() << " files";
                        else desc << files.size() << " files on " << instances.size() << " instances";
                        span(desc, ".button");

			if(request.post.empty())
			{
				if(request.url[request.url.size()-1] == '/') link("..", "Parent", ".button");
                                else link(".", "Parent", ".button");

				if(isPlayable)
				{
					link(prefix + request.url + "?playlist=1", "Play this directory", ".button");
				}
			}

                        br();
			
			open("table",".files");
			try {
				for(Map<String, StringMap>::iterator it = files.begin();
					it != files.end();
					++it)
				{
					StringMap &map = it->second;
					String name = map.get("name");
					String lnk;
					if(map.get("type") == "directory") lnk = map.get("_url");
					else if(!map.get("hash").empty()) lnk = "/" + map.get("hash");
					else lnk = map.get("_url") + "?instance=" + map.get("instance").urlEncode() + "&file=1";
					
					open("tr");
					open("td",".icon");
					if(map.get("type") == "directory") image("/dir.png");
					else image("/file.png");
					close("td");
					open("td",".filename");
					if(map.get("type") != "directory" && name.contains('.'))
						span(name.afterLast('.').toUpper(), ".type");
					link(lnk, name);
					close("td");
					open("td",".size"); 
					if(map.get("type") == "directory") text("directory");
					else if(map.contains("size")) text(String::hrSize(map.get("size")));
					close("td");
					open("td",".actions");
					if(map.get("type") != "directory")
					{
						openLink(Http::AppendGet(lnk,"download"));
						image("/down.png", "Download");
						closeLink();
						if(Mime::IsAudio(name) || Mime::IsVideo(name))
						{
							openLink(Http::AppendGet(lnk,"play"));
							image("/play.png", "Play");
							closeLink();
						}
					}
					close("td");
					close("tr");
				}
			}
			catch(const Exception &e)
			{
				LogWarn("AddressBook::Contact::http", String("Error while listing files: ") + e.what());
			}
			
			close("table");

		}
	}
	catch(const Exception &e)
	{
		LogWarn("AddressBook::Contact::http", String("Unable to list files: ") + e.what());
		if(!playlistMode) text("Error, unable to list files");
	}
	
	if(!playlistMode) close("div");
}

Stream *Html::stream(void)
{
	return mStream;
}

}
