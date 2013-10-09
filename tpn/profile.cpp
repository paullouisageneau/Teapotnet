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

#include "tpn/profile.h"
#include "tpn/user.h"
#include "tpn/config.h"
#include "tpn/file.h"
#include "tpn/directory.h"
#include "tpn/yamlserializer.h"
#include "tpn/jsonserializer.h"

namespace tpn
{

Profile::Profile(User *user, const String &uname, const String &tracker):
	mUser(user),
	mName(uname),
	mTracker(tracker)
{
	Assert(mUser);
	if(mName.empty()) mName = mUser->name();
	if(mTracker.empty()) mTracker = Config::Get("tracker");
	
	Assert(!mName.empty());
	Assert(!mTracker.empty());

	// Status
	mFields["status"]      	= new TypedField<String>("status", &mStatus, "Status");
	
	// Avatar
	mFields["avatar"]      	= new TypedField<ByteString>("avatar", &mAvatar, "Avatar");
	
	// Basic Info
	mFields["realname"]	= new TypedField<String>("realname", &mRealName, "Name", "What's your name ?");
	mFields["birthday"]     = new TypedField<Time>("birthday", &mBirthday, "Birthday", "What's your birthday ?", Time(0));
	mFields["gender"]    	= new TypedField<String>("gender", &mGender, "Gender", "What's your gender ?");	
	mFields["relationship"]	= new TypedField<String>("relationship", &mRelationship, "Relationship", "What's your relationship status ?");

	// Contact
	mFields["location"]    	= new TypedField<String>("location", &mLocation, "Address", "What's your address ?");
	mFields["email"]	= new TypedField<String>("email", &mEmail, "E-mail", "What's your email address ?");
	mFields["phone"]	= new TypedField<String>("phone", &mPhone, "Phone", "What's your phone number ?");

	// Settings
	mFields["tracker"]	= new TypedField<String>("tracker", &mTracker, "Tracker", "The TeapotNet tracker you use", mTracker);

	mFileName = infoPath() + mName;
	load();

	Interface::Instance->add(urlPrefix(), this);
}

Profile::~Profile()
{
	for(    Map<String, Field*>::iterator it = mFields.begin();
                it != mFields.end();
                ++it)
        {
		delete it->second;
        }

	mFields.clear();

	Interface::Instance->remove(urlPrefix());
}

String Profile::name(void) const
{
	Synchronize(this);
	return mName;
}

String Profile::tracker(void) const
{
	Synchronize(this);
	if(!mTracker.empty()) return mTracker;
	else return Config::Get("tracker");
}

String Profile::urlPrefix(void) const
{
	Synchronize(this);
	
	if(isSelf()) return "/"+mUser->name()+"/myself/profile";
	else return "/"+mUser->name()+"/contacts/"+mName+"/profile";
}

bool Profile::isSelf(void) const
{
	return (mName == mUser->name());
}

String Profile::infoPath(void) const
{
	Synchronize(this);
	
        String path = mUser->profilePath() + "infos";
        if(!Directory::Exist(path)) Directory::Create(path);
        return path + Directory::Separator;
}

void Profile::load()
{
	Synchronize(this);
	
	if(!File::Exist(mFileName)) return;
	File profileFile(mFileName, File::Read);
	YamlSerializer serializer(&profileFile);
	deserialize(serializer);
}

void Profile::save()
{
	Synchronize(this);
	
	String tmp;
	YamlSerializer serializer(&tmp);
	serialize(serializer);

	SafeWriteFile file(mFileName);
	file.write(tmp);
	file.close();
}

void Profile::send(const Identifier &identifier)
{
	Synchronize(this);
	
	String tmp;
	YamlSerializer serializer(&tmp);
	serialize(serializer);
	
	Notification notification(tmp);
	notification.setParameter("type", "profile");
	if(identifier != Identifier::Null) notification.send(identifier);
	else mUser->addressBook()->send(notification);
}

void Profile::clear()
{
	Synchronize(this);
	
	for(	Map<String, Field*>::iterator it = mFields.begin();
		it != mFields.end();
		++it)
	{
		it->second->clear();
	}
	
	// Do not call save() here
}

void Profile::serialize(Serializer &s) const
{
	Synchronize(this);
	
	Serializer::ConstObjectMapping fields;
	
	for(	Map<String, Field*>::const_iterator it = mFields.begin();
		it != mFields.end();
		++it)
	{
		if(!it->second->empty()) 
			fields[it->first] = it->second;
	}
	
	s.outputObject(fields);
}

bool Profile::deserialize(Serializer &s)
{	
	Synchronize(this);
	
	Serializer::ObjectMapping fields;
	
	for(	Map<String, Field*>::const_iterator it = mFields.begin();
		it != mFields.end();
		++it)
	{
		fields[it->first] = it->second;
	}

	return s.inputObject(fields);
}

void Profile::http(const String &prefix, Http::Request &request)
{
	try {
		String url = request.url;
		if(url.empty() || url[0] != '/') throw 404;

		if(request.method == "POST")
		{
			if(!isSelf()) throw 403;
			
			if(request.post.contains("clear"))	// Should be removed
			{
				clear();
				save();
			}
			else {
				String field = request.post["field"];
				String value = request.post["value"];

				updateField(field, value);
			}
			
			Http::Response response(request, 303);
			response.headers["Location"] = prefix + "/";
			response.send();
			return;
		}
		
		if(url == "/")
		{
			Http::Response response(request,200);
			response.send();
		
			Html page(response.sock);
			page.header(APPNAME, true);

			try {
				page.header(name()+"'s profile");

				page.open("div","profile.box");

				page.open("h2");
				page.text("My personal information");
				page.close("h2");

				// button() should not be used outside of a form
				//page.button("clearprofilebutton", "Clear profile");

				page.open("div", "profileheader");
				
				page.open("div","personalstatus");
				if(isSelf())
				{
					if(mStatus.empty())
					{
						page.span("(click here to post a status)", "status.empty");
					}
					else {
						page.span("“", ".statusquotemark");
						page.span(mStatus,"status.editable");
						page.span("”", ".statusquotemark");
					}
				}
				else {
					if(!mStatus.empty())
					{
						page.span("“", ".statusquotemark");
						page.span(mStatus, "status");
						page.span("”", ".statusquotemark");
					}
				}
				page.close("div");

				page.open("div", "profilephoto");

				// TODO : photo de profil
				
				page.close("div");

				page.close("div");

				
				page.open("h2");
				page.text("Personal Information");
				page.close("h2");

				bool disp = false;
				disp|= displayField(page, "realname");
				disp|= displayField(page, "birthday");
				disp|= displayField(page, "gender");
				disp|= displayField(page, "relationship");
				disp|= displayField(page, "address");
				disp|= displayField(page, "email");
				disp|= displayField(page, "phone");

				page.close("div");
				
				if(isSelf())
				{
					page.open("div","parameters.box");
					
					page.open("h2");
                                	page.text("Parameters");
                                	page.close("h2");

					displayField(page, "tracker", true);
					
					page.close("div");

					page.javascript("function postField(field, value)\n\
						{\n\
							var request = $.post('"+prefix+"/profile"+"',\n\
								{ 'field': field , 'value': value });\n\
							request.fail(function(jqXHR, textStatus) {\n\
								alert('The profile update could not be made.');\n\
							});\n\
							setTimeout(function(){location.reload();},100);\n\
						}\n\
						var blocked = false;\n\
						$('.editable,.empty').click(function() {\n\
							if(blocked) return;\n\
							blocked = true;\n\
							var currentId = $(this).attr('id');\n\
							var currentText = $(this).html();\n\
							var value = (!$(this).hasClass('empty') ? currentText : '');\n\
							$(this).html('<input type=\"text\" value=\"'+value+'\" class=\"inputprofile\">');\n\
							$('input').focus();\n\
							$('input').keypress(function(e) {\n\
								if (e.keyCode == 13 && !e.shiftKey) {\n\
									e.preventDefault();\n\
									var field = currentId;\n\
									value = $('input[type=text]').attr('value');\n\
									postField(field, value);\n\
								}\n\
							});\n\
							$('input').blur(function() {\n\
								setTimeout(function(){location.reload();},100);\n\
							});\n\
						});\n\
						$('.clearprofilebutton').click(function() {\n\
							var request = $.post('"+prefix+"/profile"+"',\n\
								{ 'clear': 1 });\n\
							request.fail(function(jqXHR, textStatus) {\n\
								alert('Profile clear could not be made.');\n\
							});\n\
							setTimeout(function(){location.reload();},100);\n\
						});");
				}
				else {
					if(!disp) page.text("The profile has not been filled yet.");
				}
			}
			catch(const IOException &e)
			{
				LogWarn("Profile::http", e.what());
				return;
			}

			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("Profile::http", e.what());
		throw 404;	// Httpd handles integer exceptions
	}
			
	throw 404;
}

bool Profile::displayField(Html &page, const String &name, bool forceDisplay) const
{
	Synchronize(this);
	
	Field *field;
	if(!mFields.get(name, field)) return false;
	return displayField(page, field, forceDisplay);
}

bool Profile::displayField(Html &page, const Field *field, bool forceDisplay) const
{
	Synchronize(this);
	
	if(!field->empty() || forceDisplay)
        {
		if(isSelf())
		{
			page.open("div", ".profileinfo");
			page.text(field->displayName() + ": ");
			page.open("span", field->name()+".editable");
			page.text(field->value());
			page.close("span");
			//page.br();
			page.close("div");
			return true;
		}
		else {
			page.open("div", ".profileinfo");
			page.text(field->displayName() + ": ");
			page.text(field->value());
			//page.br();
			page.close("div");
			return true;
		}
        }
        else {
		if(isSelf())
		{
			page.open("div", ".profileinfo");
			page.open("span", field->name()+".empty");
			page.text(field->comment());
			page.close("span");
			//page.br();
			page.close("div");
			return true;
		}
        }
        
        return false;
}

bool Profile::updateField(const String &name, const String &value)
{
	Synchronize(this);
	
	Field *field;
	if(!mFields.get(name, field)) return false;
	
	field->fromString(value);
	save();
		
	SerializableMap<String, Field*> map;
	map[name] = field;
	
	String tmp;
	YamlSerializer serializer(&tmp);
	map.serialize(serializer);
	
	Notification notification(tmp);
	notification.setParameter("type", "profilediff");
	mUser->addressBook()->send(notification);
	return true;
}

}
