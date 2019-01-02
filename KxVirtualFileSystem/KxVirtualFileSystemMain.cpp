/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

#include "KxVirtualFileSystem.h"
#include "Service.h"
#include "Convergence/ConvergenceFS.h"
#include "Mirror/MirrorFS.h"
#include "Utility.h"
#include <iostream>
#include <thread>
#include <tchar.h>

////////////////////////////////////////////////////////////////////////
//// TEST FILE //// COMPILED ONLY IN DEBUG VERSION /////////////////////
////////////////////////////////////////////////////////////////////////

int _tmain()
{
	using namespace KxVFS;

	Service* service = new Service(L"KortexVFS");
	service->Install(L"C:\\Users\\Kerber\\Documents\\Visual Studio 2017\\Projects\\Kortex Mod Manager\\Kortex\\Bin\\Data\\VFS\\Drivers\\Win7 x64\\dokan2.sys");
	service->Start();

	//MirrorFS* mirror = new MirrorFS(service, L"C:\\Users\\Kerber\\Desktop\\Test", L"D:\\Game Files\\The Elder Scrolls\\Skyrim", 0, ULONG_MAX);
	
	MirrorFS* mirrorGC = new MirrorFS(service, L"C:\\Users\\Kerber\\Documents\\My Games\\Skyrim", L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\VirtualGameConfig", 0, ULONG_MAX);
	MirrorFS* mirrorPL = new MirrorFS(service, L"C:\\Users\\Kerber\\AppData\\Local\\Skyrim", L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\PluginsOrder", 0, ULONG_MAX);

	ConvergenceFS* mainVFS = new ConvergenceFS(service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite", 0, ULONG_MAX);
	mainVFS->SetCanDeleteInVirtualFolder(true);
	mainVFS->AddVirtualFolder(L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	//mainVFS->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\TestData");
	mainVFS->AddVirtualFolder(L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\Mods\\979ba932bb6d9e9b8049c2fda6fc255d\\ModFiles");
	mainVFS->AddVirtualFolder(L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\Mods\\7cacfd93a9caea44cacfd86e614cd348\\ModFiles");
	//mirror->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\VF\\1");
	//mirror->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\VF\\2");
	//mirror->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\VF\\3");

	char c = '\0';
	while (c != 'x')
	{
		std::cin >> c;
		switch (c)
		{
			case 'm':
			{
				int error = mainVFS->Mount();
				//mirrorGC->Mount();
				//mirrorPL->Mount();
				fwprintf(stdout, L"%d: %s\r\n", error, MirrorFS::GetErrorCodeMessage(error).data());
				break;
			}
			case 'c':
			{
				mainVFS->BuildDispatcherIndex();
				break;
			}
			case 'r':
			{
				auto RunProcess = []()
				{
					STARTUPINFO startupInfo = {0};
					startupInfo.cb = sizeof(startupInfo);

					PROCESS_INFORMATION processInfo = {0};
					processInfo.hProcess = INVALID_HANDLE_VALUE;
					processInfo.hThread = INVALID_HANDLE_VALUE;

					WCHAR s1[1024] = L"\"C:\\Users\\Kerber\\Desktop\\Test\\WolfTime x64.exe\"";
					WCHAR s2[1024] = L"C:\\Users\\Kerber\\Desktop\\Test";

					//MessageBoxW(nullptr, s1, s2, 0);
					bool isOK = CreateProcessW(nullptr,
											   s1,
											   nullptr,
											   nullptr,
											   FALSE,
											   0,//CREATE_DEFAULT_ERROR_MODE|CREATE_NO_WINDOW|DETACHED_PROCESS,
											   nullptr,
											   s2,
											   &startupInfo,
											   &processInfo
					);
					DWORD error = GetLastError();
					//WaitForSingleObject(processInfo.hProcess, INFINITE);
					CloseHandle(processInfo.hThread);
					CloseHandle(processInfo.hProcess);

					fprintf(stdout, "%s: %u\r\n", isOK ? "true" : "false", error);
				};
				std::thread t(RunProcess);
				t.detach();
				//RunProcess();

				break;
			}
			case 's':
			{
				KxDynamicStringW s(L"qwerty\r\n");
				s = L"TestTestTest";
				s.append(L"XYZCVT");
				
				s.erase(2, 6);

				KxDynamicStringW spf = KxDynamicStringW::Format(L"%s, %d, 0x%p", s.data(), 45, mainVFS);

				KxDynamicStringW s2 = L"\\Desktop\\f6c0ac810be991f2.kmpproj\\";
				//s2.erase(0, 1);

				s2.erase(s2.size() - 1, 1);
				

				break;
			}

			case 'u':
			{
				fprintf(stderr, "%s\r\n", mainVFS->UnMount() ? "true" : "false");
				mirrorGC->UnMount();
				mirrorPL->UnMount();

				break;
			}
			case 'x':
			{
				fprintf(stderr, "%s\r\n", mainVFS->UnMount() ? "true" : "false");
				mirrorGC->UnMount();
				mirrorPL->UnMount();

				delete mainVFS;
				delete mirrorGC;
				delete mirrorPL;
				delete service;
				break;
			}
		};
	}
	return 0;
}
