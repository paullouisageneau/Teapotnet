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

#include "main.h"
#include "sha512.h"
#include "store.h"
#include "tracker.h"
#include "http.h"
#include "config.h"
#include "addressbook.h"

using namespace arc;

int main(int argc, char** argv)
{
	srand(time(NULL));
  
	Config::Put("tracker", "127.0.0.1");
	Config::Put("port", "8000");
	
	String test = "The quick brown fox jumps over the lazy dog";
	ByteString result;
	Sha512::Hash(test, result);

	std::cout<<"Data: "<<test<<std::endl;
	std::cout<<"Hash: "<<result.toString()<<std::endl;

	Store::Instance->addDirectory("images","/home/paulo/images");
	Store::Instance->refresh();

	// TEST
/*
	Httpd *httpd = new Httpd(8080);
	httpd->start();
	httpd->join();
*/
	
/*
	Tracker tracker(8080);
	tracker.start();

	String url("http://127.0.0.1:8080/"+result.toString());
	StringMap post;
	post["port"] = "6666";
	Http::Post(url, post);

	String output;
	Http::Get(url, &output);
	std::cout<<output<<std::endl;
*/

	AddressBook book("test");
	book.join();

	return 0;
}

