/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/Convergence/ConvergenceFS.h"
#include "KxVirtualFileSystem/Mirror/MirrorFS.h"
#include "KxVirtualFileSystem/Utility.h"
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

	#if 1
	ConvergenceFS* mainVFS = new ConvergenceFS(service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite");
	mainVFS->AddVirtualFolder(L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	mainVFS->AddVirtualFolder(L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\Mods\\979ba932bb6d9e9b8049c2fda6fc255d\\ModFiles");
	mainVFS->AddVirtualFolder(L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\Mods\\7cacfd93a9caea44cacfd86e614cd348\\ModFiles");
	#endif

	//MirrorFS* mainVFS = new MirrorFS(service, L"C:\\Users\\Kerber\\Desktop\\Test", L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	//MirrorFS* mainVFS = new MirrorFS(service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite");

	wchar_t fileName[1024] = L"\"C:\\Users\\Kerber\\Desktop\\Test\\WolfTime x64.exe\"";
	wchar_t workingDir[1024] = L"C:\\Users\\Kerber\\Desktop\\Test";

	auto RunProcess = [&]()
	{
		STARTUPINFO startupInfo = {0};
		startupInfo.cb = sizeof(startupInfo);

		PROCESS_INFORMATION processInfo = {0};
		processInfo.hProcess = INVALID_HANDLE_VALUE;
		processInfo.hThread = INVALID_HANDLE_VALUE;

		const DWORD options = 0;
		const bool isOK = CreateProcessW(nullptr, fileName, nullptr, nullptr, FALSE, options, nullptr, workingDir, &startupInfo, &processInfo);
		const DWORD error = GetLastError();

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);

		fprintf(stdout, "CreateProcessW: %s with error code %u\r\n", isOK ? "true" : "false", error);
	};
	auto OpenFileDialog = [&]()
	{
		OPENFILENAMEW options = {0};
		options.lStructSize = sizeof(options);

		wchar_t buffer[1024] = {0};
		options.lpstrFile = buffer;
		options.nMaxFile = static_cast<DWORD>(std::size(buffer));

		options.lpstrInitialDir = workingDir;
		options.Flags = OFN_DONTADDTORECENT|OFN_ENABLESIZING|OFN_NOCHANGEDIR;

		GetOpenFileNameW(&options);
		const DWORD error = GetLastError();
		const DWORD error2 = CommDlgExtendedError();
		return error;
	};

	char c = '\0';
	while (c != 'x')
	{
		std::cin >> c;
		switch (c)
		{
			case 'm':
			{
				FSError error = mainVFS->Mount();
				fwprintf(stdout, L"%d: %s\r\n", error.GetCode(), error.GetMessage().data());
				break;
			}
			case 'r':
			{
				RunProcess();
				break;
			}
			case 'o':
			{
				OpenFileDialog();
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
			case 'k':
			{
				auto f1 = []()
				{
					puts("Inside f1, begin");

					auto ax1 = KxCallAtScopeExit([]()
					{
						puts("ax1");
					});

					puts("Inside f1, end");
					return ax1;
				};

				puts("Before f1");
				auto ax1 = f1();
				puts("After f1");

				break;
			}

			case 'u':
			{
				mainVFS->UnMount();

				fprintf(stderr, "%s\r\n", mainVFS->UnMount() ? "true" : "false");
				break;
			}
			case 'x':
			{
				delete mainVFS;
				delete service;
				break;
			}
		};
	}
	return 0;
}
