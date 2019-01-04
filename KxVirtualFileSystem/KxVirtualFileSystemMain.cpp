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

	auto service = std::make_unique<Service>(L"KortexVFS");
	service->Install(L"C:\\Users\\Kerber\\Documents\\Visual Studio 2017\\Projects\\Kortex Mod Manager\\Kortex\\Bin\\Data\\VFS\\Drivers\\Win7 x64\\dokan2.sys");
	service->Start();

	#if 1
	auto mainVFS = std::make_unique<ConvergenceFS>(*service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite");
	mainVFS->EnableSecurityFunctions(true);

	mainVFS->AddVirtualFolder(L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	mainVFS->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\Mod Organizer 2");
	mainVFS->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\Resources");
	#endif

	//auto mainVFS = std::make_unique<MirrorFS>(*service, L"C:\\Users\\Kerber\\Desktop\\Test", L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	//auto mainVFS = std::make_unique<MirrorFS>(*service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite");

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
			case 'c':
			{
				size_t count = mainVFS->BuildDispatcherIndex();
				wprintf(L"BuildDispatcherIndex: %zu entries built\r\n", count);
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
				mainVFS.reset();
				service.reset();
				break;
			}
		};
	}
	return 0;
}
