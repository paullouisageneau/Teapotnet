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
#include "tpn/html.h"

#include "pla/binaryserializer.h"

namespace tpn
{

Board::Board(const String &name) :
	mName(name),
	mHasNew(false)
{
	Assert(!mName.empty() && mName[0] == '/');	// TODO
  
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
	
	mMails.insert(mail);
	mDigest.clear();
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
			add(mail);
		
		if(digest() != target)
			publish("/mail" + mName);
	}
	
	return true;
}

void Board::http(const String &prefix, Http::Request &request)
{
	throw 500;
}

}
