/*************************************************************************
 *   Copyright (C) 2011-2013 by Paul-Louis Ageneau                       *
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

#include "tpn/main.h"
#include "tpn/store.h"
#include "tpn/tracker.h"
#include "tpn/cache.h"
#include "tpn/store.h"
#include "tpn/core.h"
#include "tpn/user.h"
#include "tpn/config.h"
#include "tpn/portmapping.h"
#include "tpn/fountain.h"

#include "pla/map.h"
#include "pla/time.h"
#include "pla/http.h"
#include "pla/directory.h"
#include "pla/thread.h"
#include "pla/scheduler.h"
#include "pla/random.h"
#include "pla/securetransport.h"
#include "pla/proxy.h"

#include <signal.h>

#ifdef WINDOWS
#include <shellapi.h>
#include <wininet.h>
#define SHARED __attribute__((section(".shared"), shared))
#endif

#ifdef MACOSX
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFUserNotification.h>
#endif

#include "pla/jsonserializer.h"

using namespace tpn;

#ifdef WINDOWS
int InterfacePort SHARED = 0;

void openUserInterface(void)
{
	if(!InterfacePort) return;
	String url;
	url << "http://localhost:" << InterfacePort << "/";
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOW);
}
#endif

#ifdef MACOSX
int InterfacePort = 0;	// TODO: should be in some kind of shared section

void openUserInterface(void)
{
	if(!InterfacePort) return;
	String url;
	url << "http://localhost:" << InterfacePort << "/";
	String command = "open " + url;
	system(command.c_str());
}
#endif

#ifdef ANDROID
void mainWrapper(void)
{
	char tmp[] = {"teapotnet"};
	char *argv[1];
	argv[0] = tmp;
	main(1, argv);
	
	/*char command[] = {"teapotnet"};
	char param[] = {"--verbose"};
	char *argv[2];
	argv[0] = command;
	argv[1] = param;
	main(2, argv);*/
}

Thread *MainThread = NULL;

extern "C" 
{
	JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setWorkingDirectory(JNIEnv *env, jobject obj, jstring dir);
	JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setTempDirectory(JNIEnv *env, jobject obj, jstring dir);
	JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setSharedDirectory(JNIEnv *env, jobject obj, jstring dir);
	JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setCacheDirectory(JNIEnv *env, jobject obj, jstring dir);
	JNIEXPORT void JNICALL Java_org_ageneau_teapotnet_MainActivity_start(JNIEnv *env, jobject obj);
	JNIEXPORT void JNICALL Java_org_ageneau_teapotnet_MainActivity_updateAll(JNIEnv *env, jobject obj);
};

JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setWorkingDirectory(JNIEnv *env, jobject obj, jstring dir)
{
        String str = env->GetStringUTFChars(dir, NULL);
	
	try {
		if(!Directory::Exist(str)) Directory::Create(str);
		Directory::ChangeCurrent(str);
		return JNI_TRUE;
	}
	catch(const Exception &e)
	{
		LogError("main", e.what());
		return JNI_FALSE;
	}
}

String TempDirectory;
String SharedDirectory;
String CacheDirectory;

JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setTempDirectory(JNIEnv *env, jobject obj, jstring dir)
{
        String str = env->GetStringUTFChars(dir, NULL);
	
	try {
		if(!Directory::Exist(str)) Directory::Create(str);
		TempDirectory = str;
		return JNI_TRUE;
	}
	catch(const Exception &e)
	{
		LogError("main", e.what());
		return JNI_FALSE;
	}
}

JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setSharedDirectory(JNIEnv *env, jobject obj, jstring dir)
{
        String str = env->GetStringUTFChars(dir, NULL);
	
	try {
		if(!Directory::Exist(str)) Directory::Create(str);
		SharedDirectory = str;
		return JNI_TRUE;
	}
	catch(const Exception &e)
	{
		LogError("main", e.what());
		return JNI_FALSE;
	}
}

JNIEXPORT jboolean JNICALL Java_org_ageneau_teapotnet_MainActivity_setCacheDirectory(JNIEnv *env, jobject obj, jstring dir)
{
        String str = env->GetStringUTFChars(dir, NULL);
	
	try {
		if(!Directory::Exist(str)) Directory::Create(str);
		CacheDirectory = str;
		return JNI_TRUE;
	}
	catch(const Exception &e)
	{
		LogError("main", e.what());
		return JNI_FALSE;
	}
}

JNIEXPORT void JNICALL Java_org_ageneau_teapotnet_MainActivity_start(JNIEnv *env, jobject obj)
{
	Log("main", "Starting main thread...");
	MainThread = new Thread(mainWrapper);
}

JNIEXPORT void JNICALL Java_org_ageneau_teapotnet_MainActivity_updateAll(JNIEnv *env, jobject obj)
{
	Log("main", "Updating all users...");
	User::UpdateAll();
}
#endif

int main(int argc, char** argv)
{
	int exitCode = 0;
	
	struct timeval tv;
  	Assert(gettimeofday(&tv, 0) == 0);
  	unsigned seed = unsigned(tv.tv_sec) ^ unsigned(tv.tv_usec); 

#ifdef WINDOWS
	srand(seed);
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2,2), &WSAData);
#else
	seed^= getpid();
	srand(seed);
	srandom(seed);
	
	//signal(SIGPIPE, SIG_IGN);
	
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
#endif
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_process_attach_np();
#endif

	SecureTransport::Init();	// This is necessary for Random
	Fountain::Init();
	
	// ---------- AES auto-test ----------
	try {
		BinaryString key, salt, iv;
		Random(Random::Key).readBinary(key, 32);
		Random(Random::Nonce).readBinary(salt, 32);
		Random(Random::Nonce).readBinary(iv, 16);
		
		String message = "Hello world ! Hello world ! Hello world !";
		
		BinaryString tmp;
		Aes encryptor(&tmp);
		encryptor.setEncryptionKey(key);
		encryptor.setInitializationVector(iv);
		encryptor.write(message);
		encryptor.close();
		
		Aes decryptor(&tmp);
		decryptor.setDecryptionKey(key);
		decryptor.setInitializationVector(iv);
		String result;
		decryptor.readLine(result);
		decryptor.close();
		
		Assert(message == result);
	}
	catch(const Exception &e)
	{
		LogError("main", String("AES auto-test failed: ") + e.what());
		exit(1);
	}
	// ----------
	
	StringMap args;
	try {
		Assert(argc >= 1);

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
			std::cout<<" --port port\t\tSet the TPN port"<<std::endl;
			std::cout<<" --ifport port\t\tSet the HTTP interface port"<<std::endl;
			std::cout<<" --tracker [port]\tEnable the local tracker"<<std::endl;
			std::cout<<" --directory path\t\tSet the working directory"<<std::endl;
			std::cout<<" --verbose\tVerbose output"<<std::endl;
			std::cout<<" --trace\tProtocol tracing output"<<std::endl;
#if defined(WINDOWS) || defined(MACOSX)
			std::cout<<" --nointerface\t\tPrevent lauching the web browser"<<std::endl;
#endif
#if defined(WINDOWS)
			std::cout<<" --noupdate\t\tPrevent trying to update the program"<<std::endl;
#endif
			return 0;
		}
		
		if(args.contains("verbose"))	LogLevel = LEVEL_DEBUG;
		if(args.contains("trace"))	LogLevel = LEVEL_TRACE;
		
		if(args.contains("benchmark") || args.contains("b"))
		{
			exitCode = benchmark(args);
		}
		else {
			exitCode = run(args);
		}
	}
	catch(const std::exception &e)
	{
		LogError("main", e.what());
		
#ifdef WINDOWS
		UINT uType = MB_OK|MB_ICONERROR|MB_SETFOREGROUND|MB_SYSTEMMODAL;
		if(args.contains("daemon") || args.contains("boot")) uType|= MB_SERVICE_NOTIFICATION;
		MessageBox(NULL, e.what(), "Teapotnet - Error", uType);
#endif
		
#ifdef MACOSX
		const char *header = "Teapotnet - Error";
		const char *message = e.what();
		CFStringRef headerRef  = CFStringCreateWithCString(NULL, header, strlen(header));
		CFStringRef messageRef = CFStringCreateWithCString(NULL, message, strlen(message));
		CFOptionFlags result;

		CFUserNotificationDisplayAlert(0,		// no timeout
						kCFUserNotificationStopAlertLevel,
						NULL,		// default icon
						NULL,		// unused
						NULL,		// localization of strings
						headerRef,	// header text 
						messageRef,	// message text
						NULL,		// default OK button
						NULL,		// no alternate button
						NULL,		// no other button
						&result);
		
		CFRelease(headerRef);
		CFRelease(messageRef);
#endif
		exitCode = 1;	  
	}
	
	// Cleanup
	SecureTransport::Cleanup();
	Fountain::Cleanup();
	
#ifdef PTW32_STATIC_LIB
	pthread_win32_process_detach_np();
#endif
	
#ifdef WINDOWS
	WSACleanup();
#endif
	
	return exitCode;
}

int run(StringMap &args)
{
#ifndef WINDOWS
#ifndef ANDROID
	// Main config file name
	// Should tell us where the static dir is located
	const String mainConfigFileName = "/etc/teapotnet/config.conf";
	if(File::Exist(mainConfigFileName)) Config::Load(mainConfigFileName);
#endif
#endif

	const String configFileName = "config.txt";	// TODO: also defined in core.cpp
	if(File::Exist(configFileName)) Config::Load(configFileName);

	Config::Default("tracker", "teapotnet.org");
	Config::Default("port", "8080");
	Config::Default("tracker_port", "80");
	Config::Default("interface_port", "8480");
	Config::Default("profiles_dir", "profiles");
	Config::Default("static_dir", "static");
	Config::Default("shared_dir", "shared");
	Config::Default("temp_dir", "temp");
	Config::Default("external_address", "auto");
	Config::Default("external_port", "auto");
	Config::Default("port_mapping_enabled", "true");
	Config::Default("http_timeout", "5000");
	Config::Default("request_timeout", "5000");
	Config::Default("tpot_timeout", "5000");
	Config::Default("user_global_shares", "true");
	Config::Default("http_proxy", "auto");
	Config::Default("prefetch_delay", "300000");
	Config::Default("max_connections", "1024");
	
#ifdef ANDROID
	Config::Default("force_http_tunnel", "false");
	Config::Default("cache_max_size", "100");		// MiB
	Config::Default("cache_max_file_size", "10");		// MiB
	Config::Default("prefetch_max_file_size", "0");		// MiB (0 means disabled)
	
	if(!TempDirectory.empty()) Config::Put("temp_dir", TempDirectory);
	if(!SharedDirectory.empty()) Config::Put("shared_dir", SharedDirectory);
	if(!CacheDirectory.empty()) Config::Put("cache_dir", CacheDirectory);
#else
	Config::Default("force_http_tunnel", "false");
	Config::Default("cache_max_size", "10000");		// MiB
	Config::Default("cache_max_file_size", "2000");		// MiB
	Config::Default("prefetch_max_file_size", "10");	// MiB
#endif

#if defined(WINDOWS) || defined(MACOSX)
	bool isBoot = args.contains("boot");
	bool isSilent = args.contains("nointerface");
	if(isBoot) ForceLogToFile = true;
	
	if(!InterfacePort) try {
		int port = Config::Get("interface_port").toInt();
		Socket sock(Address("127.0.0.1", port), 0.2);
		InterfacePort = port;
	}
	catch(...) {}

	if(InterfacePort)
	{
		if(!isSilent && !isBoot)
			openUserInterface();
		return 0;
	}
#endif
	
	String workingDirectory;

#ifdef MACOSX
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	if(mainBundle != NULL && !File::Exist("static/common.js"))
	{
		char path[PATH_MAX];
		CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
		if(resourcesURL != NULL && CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8*)path, PATH_MAX))
		{
			// It's a bundle, and we have the resources path
			String resourcesPath(path);
			
			CFURLRef executableURL = CFBundleCopyExecutableURL(mainBundle);
			if(executableURL == NULL || !CFURLGetFileSystemRepresentation(executableURL, TRUE, (UInt8*)path, PATH_MAX))
				throw Exception("Unable to find application executable path");
			
			String executablePath(path);
			
			// Set directories
			Config::Put("static_dir", resourcesPath + "/static");
			workingDirectory = Directory::GetHomeDirectory() + "/Teapotnet";
			ForceLogToFile = true;
			
			// TODO: backward compatibility, should be removed
			system("if [ -d ~/TeapotNet ]; then mv ~/TeapotNet ~/Teapotnet; fi");
			
			if(!isBoot)	// If it's not the service process
			{
				String plist = "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\
<plist version=\"1.0\">\n\
<dict>\n\
<key>Label</key>\n\
<string>org.ageneau.teapotnet</string>\n\
<key>ProgramArguments</key>\n\
<array>\n\
	<string>"+executablePath+"</string>\n\
	<string>--boot</string>\n\
</array>\n\
<key>RunAtLoad</key>\n\
<true/>\n\
<key>KeepAlive</key>\n\
<false/>\n\
<key>AbandonProcessGroup</key>\n\
<true/>\n\
</dict>\n\
</plist>\n";

				File plistFile("/tmp/Teapotnet.plist", File::Truncate);
				plistFile.write(plist);
				plistFile.close();
				
				// Clean
				system("launchctl remove org.ageneau.teapotnet");
			
				// TODO: backward compatibility, should be removed
				system("if [ -f ~/Library/LaunchAgents/TeapotNet.plist ]; then rm ~/Library/LaunchAgents/TeapotNet.plist; fi");

				// Launch now
				system("launchctl load /tmp/Teapotnet.plist");
				
				// Launch at startup
				system("mkdir -p ~/Library/LaunchAgents");
				system("mv /tmp/Teapotnet.plist ~/Library/LaunchAgents");
				
				// Let some time for the service process to launch
				Thread::Sleep(1.);
				
				// Try interface
				if(!InterfacePort) try {
					int port = Config::Get("interface_port").toInt();
					Socket sock(Address("127.0.0.1", port), 0.2);
					InterfacePort = port;
				}
				catch(...) {}

				if(InterfacePort)
				{
					if(!isSilent) openUserInterface();
					return 0;
				}
			}
			
			CFRelease(resourcesURL);
			CFRelease(executableURL);
		}
	}
#endif
	
	args.get("directory", workingDirectory);

	if(!workingDirectory.empty())
	{
		if(!Directory::Exist(workingDirectory))
			Directory::Create(workingDirectory);
		
		Directory::ChangeCurrent(workingDirectory);
	}
	
	// Remove old log file
	File::Remove("log.txt");


#if defined(WINDOWS)
	ForceLogToFile = true;

	if(isBoot)
	{
		Config::Save(configFileName);
		Sleep(1000);
	}
	else {
		try {
			Config::Save(configFileName);
		}
		catch(...)
		{
			LogInfo("main", "Trying to run as administrator..."); 
			Sleep(100);
			if(int(ShellExecute(NULL, "runas", argv[0], commandLine.c_str(), NULL, SW_SHOW)) <= 32)
				throw Exception("Unable to run as administrator");
			return 0;
		}
	}
	
	if(File::Exist("winupdater.new.exe"))
		File::Rename("winupdater.new.exe", "winupdater.exe");
	
	if(!args.contains("noupdate"))
	{
		unsigned currentDay = Time::Now().toDays();
		unsigned updateDay = 0;
		if(!Config::Get("last_update_day").empty())
			Config::Get("last_update_day").extract(updateDay);
		if(updateDay > currentDay) updateDay = 0;		

		if(updateDay + 1 < currentDay)
		{
			LogInfo("main", "Looking for updates...");
			String release = "win32";
			String url = String(DOWNLOADURL) + "?version&release=" + release + "&current=" + APPVERSION;
			
			try {
				String content;
				int result = 0;
				
				if(isBoot)
				{
					int attempts = 4;
					while(true)
					{
						try {
							result = Http::Get(url, &content);
						}
						catch(const NetException &e)
						{
							if(--attempts == 0) break;
							content.clear();
							LogInfo("main", "Waiting for network availability...");
							Sleep(5000);
							continue;
						}
						catch(...)
						{
							// Timeouts are catched here
						}
						
						break;
					}
				}
				else result = Http::Get(url, &content);
				
				if(result != 200) 
					throw Exception("HTTP error code " + String::number(result));
				
				Config::Put("last_update_day", String::number(currentDay));
				Config::Save(configFileName);
					
				unsigned lastVersion = content.trimmed().dottedToInt();
				unsigned appVersion = String(APPVERSION).dottedToInt();
				
				Assert(appVersion != 0);
				if(lastVersion > appVersion)
					if(Config::LaunchUpdater(&commandLine))
						return 0;
			}
			catch(const Exception &e)
			{
				LogWarn("main", String("Unable to look for updates: ") + e.what());
			}
		}
	}

#else	// ifdef WINDOWS

	Config::Save(configFileName);
#endif

	LogInfo("main", "Starting...");
	
	File::CleanTemp();
	Http::UserAgent = String(APPNAME) + '/' + APPVERSION;
	Http::RequestTimeout = milliseconds(Config::Get("http_timeout").toInt());
	Proxy::HttpProxy = Config::Get("http_proxy").trimmed();
	
	Tracker *tracker = NULL;
	if(args.contains("tracker"))
	{
		if(args["tracker"].empty()) 
			args["tracker"] = Config::Get("tracker_port");
		
		int port;
		try {
			args["tracker"] >> port;
		}
		catch(...)
		{
			throw Exception("Invalid tracker port specified");
		}

		LogInfo("main", "Launching the tracker...");
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
	
	// Init Cache and Store
	Cache::Instance = new Cache;
	Store::Instance = new Store;
	
	// Starting interface
	Interface::Instance = new Interface(ifport);
	Interface::Instance->start();
	
	// Starting core
	bool portChanged = false;
	int attempts = 20;
	while(true)
	{
		try {
			Core::Instance = new Core(port);
		}
		catch(const NetException &e)
		{
			if(--attempts == 0) throw NetException("Unable to listen for incoming network connections");
			
			int newPort = Random().uniform(1024, 49151);
			LogInfo("main", "Unable to listen on port " + String::number(port) + ", trying port " + String::number(newPort));
			port = newPort;
			portChanged = true;
			continue;
		}
		
		break;
	}
	
	Core::Instance->start();
	
	if(portChanged || args.contains("port") || args.contains("ifport"))
	{
		Config::Put("port", String::number(port));
		Config::Put("interface_port", String::number(ifport));
		Config::Save(configFileName);
	}
	
	// Starting port mapping
	PortMapping::Instance = new PortMapping;
	PortMapping::Instance->add(PortMapping::TCP, port, port);
	
	if(Config::Get("port_mapping_enabled").toBool())
	{
		LogInfo("main", "NAT port mapping is enabled");
		PortMapping::Instance->enable();
	}
	else {
		LogInfo("main", "NAT port mapping is disabled");
	}
	
	try {
		if(!Directory::Exist(Config::Get("profiles_dir")))
			Directory::Create(Config::Get("profiles_dir"));
		
		Directory profilesDir(Config::Get("profiles_dir"));
		while(profilesDir.nextFile())
		{
			if(profilesDir.fileIsDir())
			{
				String name = profilesDir.fileName();
				User *user;
				
				LogInfo("main", String("Loading user ") + name + "...");
				
				try {
					user = new User(name);
				}
				catch(const std::exception &e)
				{
					LogError("main", "Unable to load user " + name + ": " + e.what());
					continue;
				}
				
				Thread::Sleep(0.1);
			}
		}
		
		String usersFileName = "users.txt";
		if(File::Exist(usersFileName))
		{
			File usersFile(usersFileName, File::Read);
			String line;
			while(usersFile.readLine(line))
			{
				String &name = line;
				name.trim();
				String password = name.cut(' ');
				password.trim();
				if(name.empty()) continue;
				LogInfo("main", String("Creating user ") + name + "...");
				User *user = new User(name, password);
				Thread::Sleep(0.1);
				line.clear();
			}
			usersFile.close();
			File::Remove(usersFileName);
		}

		LogInfo("main", String("Ready. You can access the interface on http://localhost:") + String::number(ifport) + "/");
		
#if defined(WINDOWS) || defined(MACOSX)
		InterfacePort = ifport;
		if(!isSilent && !isBoot)
			openUserInterface();

		class CheckUpdateTask : public Task
		{
		public:
			void run(void)
			{
				Config::CheckUpdate();
			}
		};
		
		CheckUpdateTask checkUpdateTask;
		Scheduler::Global->schedule(&checkUpdateTask, 300.);	// 5 min
		Scheduler::Global->repeat(&checkUpdateTask, 86400.);	// 1 day
#endif

		Core::Instance->join();
		
#if defined(WINDOWS) || defined(MACOSX)
		Scheduler::Global->remove(&checkUpdateTask);
#endif		
	}
	catch(...)
	{
		PortMapping::Instance->disable();
		throw;
	}
	
	PortMapping::Instance->disable();
	LogInfo("main", "Finished");
	return 0;
}

int benchmark(StringMap &args)
{
	std::cout << "Benchmarking fountain..." << std::endl;
	
	TempFile *file = new TempFile;
	file->writeZero(1024*1024);
	
	unsigned n = 1024 + 1;
	
	Array<BinaryString> tmp;
	tmp.resize(n);
	
	Fountain::Source source(file, 0, 1024*1024);
	
	Time t1;
	unsigned tokens = 1024;
	for(unsigned i=0; i<n; ++i)
		source.generate(tmp[i], &tokens);
	
	Time t2;
	Fountain::Sink sink;
	for(unsigned i=0; i<n; ++i)
		if(sink.solve(tmp[i]))
			break;
	
	Time t3;
	Assert(sink.isComplete());
	
	std::cout << "Coding:   " << 1./(t2-t1) << " MB/s" << std::endl;
	std::cout << "Decoding: " << 1./(t3-t2) << " MB/s" << std::endl;
}

