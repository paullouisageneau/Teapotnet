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

#include "main.h"
#include "map.h"
#include "sha512.h"
#include "time.h"
#include "store.h"
#include "tracker.h"
#include "http.h"
#include "config.h"
#include "core.h"
#include "user.h"
#include "directory.h"
#include "portmapping.h"

#include <signal.h>

using namespace tpot;

Mutex tpot::LogMutex;


#ifdef WINDOWS

#include <shellapi.h>

#define SHARED __attribute__((section(".shared"), shared))
int InterfacePort SHARED = 0;

void openUserInterface(void)
{
	if(!InterfacePort) return;

	String url;
	url << "http://localhost:" << InterfacePort << "/";
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOW);
}

#endif

int main(int argc, char** argv)
{
	srand(time(NULL));
  
#ifdef WINDOWS
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2,2), &WSAData);
#else
	signal(SIGPIPE, SIG_IGN);
#endif
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_process_attach_np();
#endif

	try {
	  
/*	
	Assert(Address("127.0.0.1:80").isLocal());
	Assert(Address("::1:80").isLocal());
	Assert(Address("::FFFF:127.0.0.1:80").isLocal());
	Assert(Address("192.168.0.1:80").isPrivate());
	Assert(Address("::FFFF:192.168.0.1:80").isPrivate());
*/

	unsigned appVersion = String(APPVERSION).dottedToInt();
	Assert(appVersion != 0);
	Assert(argc >= 1);
	
#ifndef WINDOWS
		// Main config file name
		// Should tell us where the static dir is located
		const String mainConfigFileName = "/etc/teapotnet/config.conf";
		if(File::Exist(mainConfigFileName)) Config::Load(mainConfigFileName);
#endif

		const String configFileName = "config.txt";
		if(File::Exist(configFileName)) Config::Load(configFileName);

		Config::Default("tracker", "teapotnet.org");
		Config::Default("port", "8480");
		Config::Default("tracker_port", "8488");
		Config::Default("interface_port", "8080");
		Config::Default("profiles_dir", "profiles");
		Config::Default("static_dir", "static");
		Config::Default("shared_dir", "shared");
		Config::Default("temp_dir", "temp");
		Config::Default("external_address", "auto");
		Config::Default("http_timeout", "10000");
		Config::Default("request_timeout", "10000");
		Config::Default("meeting_timeout", "15000");
		Config::Default("tpot_timeout", "15000");
		Config::Default("tpot_read_timeout", "60000");
		Config::Default("user_global_shares", "true");
		
		if(Config::Get("request_timeout").toInt() < 10000)
			Config::Put("request_timeout", "10000");	
		
		if(Config::Get("tpot_timeout").toInt() < 15000)
			Config::Put("tpot_timeout", "15000");
		
		StringMap args;
		String last;
		String commandLine;
		for(int i=1; i<argc; ++i)
		{
			String str(argv[i]);
			if(str.empty()) continue;
			
			if(!commandLine.empty()) commandLine+= ' ';
			commandLine+= str;

			if(str[0] == '-')
			{
				if(!last.empty()) args[last] = "";
			  
				if(str[0] == '-') str.ignore();
				if(!str.empty() && str[0] == '-') str.ignore();
				if(str.empty())
				{
					std::cerr<<"Invalid option: "<<argv[i]<<std::endl;
					std::cerr<<"Try \""<<argv[0]<<" --help\" for help"<<std::endl;
					return 1;
				}
				last = str;
			}
			else {
				if(last.empty()) args[str] = "";
				else {
					args[last] = str;
					last.clear();
				}
			}
		}
		if(!last.empty()) args[last] = "";
		
		if(args.contains("help") || args.contains("h"))
		{
			std::cout<<APPNAME<<" "<<APPVERSION<<std::endl;
			std::cout<<APPAUTHOR<<std::endl;
			std::cout<<"For information, please visit "<<APPLINK<<std::endl;
			std::cout<<"Usage: ";
			if(argc) std::cout<<argv[0];
			else std::cout<<"teapotnet";
			std::cout<<" [options]"<<std::endl;
			std::cout<<std::endl;
			std::cout<<"Available options include:"<<std::endl;
			std::cout<<" --port port\t\tSet the TPOT port"<<std::endl;
			std::cout<<" --ifport port\t\tSet the HTTP interface port"<<std::endl;
			std::cout<<" --tracker [port]\tEnable the local tracker"<<std::endl;
#ifdef WINDOWS
			std::cout<<" --nointerface\t\tPrevent lauching the web browser"<<std::endl;
			std::cout<<" --noupdate\t\tPrevent trying to update the program"<<std::endl;
			std::cout<<" --debug\t\tKeep the console window on screen"<<std::endl;
#endif
			return 0;
		}
		
#ifdef WINDOWS
		if(InterfacePort)
		{
			if(!args.contains("nointerface"))
				openUserInterface();
			return 0;
		}

		try {
			Config::Save(configFileName);
		}
		catch(...)
		{
			Log("main", "Unable to save the configuration file, trying to restart as administrator..."); 
			Sleep(100);
			if(int(ShellExecute(NULL, "runas", argv[0], commandLine.c_str(), NULL, SW_SHOW)) > 32)
				return 0;
		}
		
		if(!args.contains("noupdate"))
		{
			unsigned currentDay = Time::Now().toDays();
			unsigned updateDay = 0;
			if(!Config::Get("last_update_day").empty())
				Config::Get("last_update_day").extract(updateDay);
			
			if(updateDay != currentDay)
			{
				Log("main", "Looking for updates...");

				String url = String(DOWNLOADURL) + "?version&release=win32" + "&current=" + APPVERSION;
				
				try {
					String content;
					if(Http::Get(url, &content) == 200)
					{
						content.trim();

						unsigned lastVersion = content.dottedToInt();
						if(lastVersion && appVersion <= lastVersion)
						{
							Config::Put("last_update_day", String::number(currentDay));
							if(int(ShellExecute(NULL, NULL, "winupdater.exe", commandLine.c_str(), NULL, SW_SHOW)) > 32)
								return 0;
							else Log("main", "Warning: Unable to run the updater, skipping program update.");
						}
					}
					else Log("main", "Warning: Failed to query the last available version");
				}
				catch(const Exception &e)
				{
					Log("main", String("Warning: Unable to look for updates: ") + e.what());
				}
			}
		}
#endif
		
		Log("main", "Starting...");
		File::CleanTemp();
		
		Tracker *tracker = NULL;
		if(args.contains("tracker"))
		{
			if(args["tracker"].empty()) 
				args["tracker"] = Config::Get("tracker_port");
			int port;
			args["tracker"] >> port;
			
			Log("main", "Launching the tracker");
			tracker = new Tracker(port);
			tracker->start();
		}
		
		String sport = Config::Get("port");
		if(args.contains("port")) sport = args["port"];
		int port;
		sport >> port;
		
		String sifport = Config::Get("interface_port");
		if(args.contains("ifport")) sifport = args["ifport"];
		int ifport;
		sifport >> ifport;

		// Creating global store
		Store::GlobalInstance = new Store(NULL);
		
		// Starting interface and core
		Interface::Instance = new Interface(ifport);
		Interface::Instance->start();
		
		// Starting core
		Core::Instance = new Core(port);
		Core::Instance->start();
		
		if(args.contains("port") || args.contains("ifport"))
		{
			Config::Put("port", String::number(port));
			Config::Put("interface_port", String::number(ifport));
			Config::Save(configFileName);
		}
		
		// Starting port mapping
		PortMapping::Instance = new PortMapping;
		if(Config::Get("external_address").empty()
			|| Config::Get("external_address") == "auto")
		{
			Log("main", "NAT port mapping is enabled");
			PortMapping::Instance->init();
			PortMapping::Instance->addTcp(port, port);
			PortMapping::Instance->start();
		}
		else Log("main", "NAT port mapping is disabled");
		
		if(!Directory::Exist(Config::Get("profiles_dir")))
			Directory::Create(Config::Get("profiles_dir"));
		
		Directory profilesDir(Config::Get("profiles_dir"));
		while(profilesDir.nextFile())
		{
			if(profilesDir.fileIsDir())
			{
				String name = profilesDir.fileName();
				User *user;
				
				Log("main", String("Loading user ") + name + "...");
				
				try {
					user = new User(name);	
				}
				catch(const std::exception &e)
				{
					Log("main", "ERROR: Unable to load user \"" + name + "\": " + e.what());
					continue;
				}
				
				user->start();
				msleep(100);
			}
		}
		
		String usersFileName = "users.txt";
		File usersFile;
		
		if(File::Exist(usersFileName))
		{
			usersFile.open(usersFileName, File::Read);
			String line;
			while(usersFile.readLine(line))
			{
				String &name = line;
				name.trim();
				String password = name.cut(' ');
				password.trim();
				if(name.empty()) continue;
				Log("main", String("Creating user ") + name + "...");
				User *user = new User(name, password);
				user->start();
				msleep(100);
				line.clear();
			}
			usersFile.close();
		}
		usersFile.open(usersFileName, File::Truncate);
		usersFile.close();

#ifdef WINDOWS
		InterfacePort = ifport;
		if(!args.contains("nointerface"))
			openUserInterface();
		
		if(!args.contains("debug") && !Config::Get("debug").toBool())
		{
			HWND hWnd = GetConsoleWindow();
			ShowWindow(hWnd, SW_HIDE);
		}
#endif
		
		Log("main", String("Ready. You can access the interface on http://localhost:") + String::number(ifport) + "/");		
		Core::Instance->join();
		Log("main", "Finished");
	}
	catch(const std::exception &e)
	{
		Log("main", String("ERROR: ") + e.what());
#ifdef WINDOWS
		HWND hWnd = GetConsoleWindow();
                ShowWindow(hWnd, SW_SHOW);
		std::cin.get();
#endif
		return 1;	  
	}
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_process_detach_np();
#endif
	
#ifdef WINDOWS
	WSACleanup();
#endif
	
	exit(0);
}

