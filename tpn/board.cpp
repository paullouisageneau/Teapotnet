/*************************************************************************
 *   Copyright (C) 2011-2014 by Paul-Louis Ageneau                       *
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

#include "tpn/board.h"
#include "tpn/resource.h"
#include "tpn/cache.h"
#include "tpn/html.h"

#include "pla/jsonserializer.h"
#include "pla/binaryserializer.h"

namespace tpn
{

Board::Board(const String &name) :
	mName(name),
	mHasNew(false)
{
	Assert(!mName.empty() && mName[0] == '/');	// TODO

	Cache::Instance->retrieveMapping("/mail" + mName, mDigest);
	
	Interface::Instance->add("/mail" + mName, this);
	
	publish("/mail" + mName);
	subscribe("/mail" + mName);
}

Board::~Board(void)
{
	Interface::Instance->remove("/mail" + mName, this);
	
	unpublish("/mail" + mName);
	unsubscribe("/mail" + mName);
}

bool Board::hasNew(void) const
{
	Synchronize(this);

	bool value = false;
	std::swap(mHasNew, value);
	return value;
}

bool Board::add(Mail &mail)
{
	Synchronize(this);
	
	if(mMails.contains(mail))
		return false;
	
	const Mail *p = &*mMails.insert(mail).first;
	mUnorderedMails.append(p);
	
	mDigest.clear();
	publish("/mail" + mName);
	notifyAll();
	return true;
}

BinaryString Board::digest(void) const
{
	if(mDigest.empty())
	{
		String tempFileName = File::TempName();
		File tempFile(tempFileName, File::Truncate);
		
		BinarySerializer serializer(&tempFile);
		for(Set<Mail>::const_iterator it = mMails.begin();
			it != mMails.end();
			++it)
		{
			serializer.write(*it);
		}
		
		tempFile.close();
		
		Resource resource;
		resource.process(Cache::Instance->move(tempFileName), mName, "mail");
		
		mDigest = resource.digest();
		
		Cache::Instance->storeMapping("/mail" + mName, mDigest);
	}
  
	return mDigest;
}

bool Board::anounce(const Identifier &peer, const String &prefix, const String &path, BinaryString &target)
{
	Synchronize(this);
	
	target = digest();
	return true;
}
	
bool Board::incoming(const String &prefix, const String &path, const BinaryString &target)
{
	Synchronize(this);
	
	if(target == digest())
		return false;
	
	if(fetch(prefix, path, target))
	{
		Resource resource(target, true);	// local only (already fetched)
		if(resource.type() != "mail")
			return false;
		
		Resource::Reader reader(&resource);
		BinarySerializer serializer(&reader);
		Mail mail;
		while(serializer.read(mail))
			if(!mMails.contains(mail))
			{
				const Mail *p = &*mMails.insert(mail).first;
				mUnorderedMails.append(p);
			}
		
		mDigest.clear();	// so digest must be recomputed
		if(digest() != target)
			publish("/mail" + mName);
		
		notifyAll();
	}
	
	return true;
}

void Board::http(const String &prefix, Http::Request &request)
{
	Synchronize(this);
	Assert(!request.url.empty());
	
	try {
		if(request.url == "/")
		{
			if(request.get.contains("json"))
			{
				int next = 0;
				if(request.get.contains("next"))
					request.get["next"].extract(next);
				
				double timeout = 60.;
				if(request.get.contains("timeout"))
					request.get["timeout"].extract(timeout);
				
				while(next >= int(mUnorderedMails.size()))
				{
					if(!wait(timeout))
						break;
				}
				
				Http::Response response(request, 200);
				response.headers["Content-Type"] = "application/json";
				response.send();
				
				JsonSerializer json(response.stream);
				json.outputArrayBegin();
				for(int i = next; i < int(mUnorderedMails.size()); ++i)
					json.outputArrayElement(*mUnorderedMails[i]);
				json.outputArrayEnd();
			}
			
			bool isPopup = request.get.contains("popup");
			
			Http::Response response(request, 200);
			response.send();

			Html page(response.stream);
			page.header("Board " + mName, isPopup);
			
			page.open("div","topmenu");	
			if(isPopup) page.span(mName, ".button");
			//page.raw("<a class=\"button\" href=\"#\" onclick=\"createFileSelector('/"+mUser->name()+"/myself/files/?json', '#fileSelector', 'input.attachment', 'input.attachmentname','"+mUser->generateToken("directory")+"'); return false;\">Send file</a>");
			
			// TODO: should be hidden in CSS
#ifndef ANDROID
			if(!isPopup)
			{
				String popupUrl = Http::AppendGet(request.fullUrl, "popup");
				page.raw("<a class=\"button\" href=\""+popupUrl+"\" target=\"_blank\" onclick=\"return popup('"+popupUrl+"','/');\">Popup</a>");
			}
#endif
			page.close("div");
			
			page.div("", "fileSelector");	
			
			if(isPopup) page.open("div", "mail");
			else page.open("div", "mail.box");
			
			page.open("div", "messages");
			page.close("div");
			
			page.open("div", "panel");
			page.div("","attachedfile");
			page.openForm("#", "post", "mailform");
			page.textarea("input");
			page.input("hidden", "attachment");
			page.input("hidden", "attachmentname");
			page.closeForm();
			page.close("div");

			page.close("div");
			page.footer();
			return;
		}
	}
	catch(const Exception &e)
	{
		LogWarn("AddressBook::http", e.what());
		throw 500;
	}
	
	throw 404;
}

}
