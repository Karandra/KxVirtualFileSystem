#include "stdafx.h"
#include "KxVFS/FileSystemService.h"
#include "KxVFS/ConvergenceFS.h"
#include "KxVFS/MirrorFS.h"
#include "KxVFS/Utility.h"
#include <iostream>
#include <thread>
#include <tchar.h>

////////////////////////////////////////////////////////////////////////
//// TEST FILE //// COMPILED ONLY IN DEBUG CONFIGURATION ///////////////
////////////////////////////////////////////////////////////////////////

int _tmain()
{
	using namespace KxVFS;

	auto service = std::make_unique<FileSystemService>(L"Dokan2");
	service->Start();

	#if 1
	auto mainVFS = std::make_unique<ConvergenceFS>(*service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite");

	//mainVFS->AddVirtualFolder(L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	//mainVFS->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\Mod Organizer 2\\Mods");
	mainVFS->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\Resources");
	#else
	auto mainVFS = std::make_unique<MirrorFS>(*service, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite");
	#endif

	mainVFS->GetIOManager().EnableAsyncIO(true);
	mainVFS->EnableExtendedSecurity(true);
	mainVFS->EnableImpersonateCallerUser(false);

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

		KxVFS_Log(LogLevel::Info, L"CreateProcessW: %1 with error code %2\r\n", isOK ? L"true" : L"false", error);
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
				KxVFS_Log(LogLevel::Info, L"%1: %2\r\n", error.GetCode(), error.GetMessage().data());
				break;
			}
			case 'l':
			{
				if (ILogger::IsLogEnabled())
				{
					ILogger::EnableLog(false);
					puts("Log disabled");
				}
				else
				{
					ILogger::EnableLog();
					puts("Log enabled");
				}
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
			case 'p':
			{
				#if 1
				SHELLEXECUTEINFOW info = {0};
				info.cbSize = sizeof(info);
				info.fMask = SEE_MASK_DEFAULT|SEE_MASK_INVOKEIDLIST;
				info.hwnd = nullptr;
				info.lpVerb = L"properties";
				info.lpFile = L"\"C:\\Users\\Kerber\\Desktop\\Test\\VeryHigh.ini\"";
				info.lpDirectory = nullptr;
				info.lpParameters = nullptr;
				info.nShow = SW_SHOWNORMAL;

				::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
				BOOL status = ::ShellExecuteExW(&info);
				::CoUninitialize();
				#endif

				#if 0
				FileHandle handle = ::CreateFileW(L"C:\\Users\\Kerber\\Desktop\\Test\\VeryHigh.ini",
												  GENERIC_READ,
												  FILE_SHARE_READ,
												  nullptr,
												  OPEN_EXISTING,
												  FILE_ATTRIBUTE_NORMAL,
												  nullptr);

				BY_HANDLE_FILE_INFORMATION info = {0};
				::GetFileInformationByHandle(handle, &info);
				#endif

				#if 0
				FileAttributes attributes = FromInt<FileAttributes>(::GetFileAttributesW(L"C:\\Users\\Kerber\\Desktop\\Test\\VeryHigh.ini"));
				#endif
				break;
			}
			case 'c':
			{
				//constexpr auto source = L"C:\\Users\\Kerber\\Desktop\\FreeFlyCamSKSE_v10_2_1.zip";
				//constexpr auto source = L"C:\\Users\\Kerber\\Desktop\\Test\\VeryHigh.ini";
				constexpr auto source = L"C:\\Users\\Kerber\\Desktop\\FreeFlyCamSKSE_v10_2_1.zip";

				//constexpr auto target = L"C:\\Users\\Kerber\\Desktop\\Test\\FreeFlyCamSKSE_v10_2_1.zip";
				//constexpr auto target = L"C:\\Users\\Kerber\\Desktop\\Test\\VeryHigh2.ini";
				constexpr auto target = L"C:\\Users\\Kerber\\Desktop\\Test\\123\\456\\FreeFlyCamSKSE_v10_2_1.zip";

				Utility::CreateDirectory(L"C:\\Users\\Kerber\\Desktop\\Test\\123");
				Utility::CreateDirectory(L"C:\\Users\\Kerber\\Desktop\\Test\\123\\456");
				::CopyFileW(source, target, FALSE);
				KxVFS_Log(LogLevel::Info, L"Done: %1", ::GetLastError());

				break;
			}
			case 'v':
			{
				constexpr auto path = L"C:\\Users\\Kerber\\Desktop\\Test\\Data\\Scripts\\TestFile.json";
				FileHandle handle(path, AccessRights::GenericWrite, FileShare::All, CreationDisposition::CreateAlways);
				KxVFS_Log(LogLevel::Info, L"Done: %1", ::GetLastError());
				break;
			}
			case 'b':
			{
				constexpr auto path = L"C:\\Users\\Kerber\\Desktop\\Test\\Data\\Scripts\\TestFolder";
				Utility::CreateDirectory(path, nullptr);
				KxVFS_Log(LogLevel::Info, L"Done: %1", ::GetLastError());
				break;
			}
			case 't':
			{
				uint32_t pid = 0;
				std::cin >> pid;
				KxVFS_Log(LogLevel::Info, L"IsProcessCreatedInVFS: %1", mainVFS->IsProcessCreatedInVFS(pid));
				break;
			}

			case 'u':
			{
				KxVFS_Log(LogLevel::Info, L"%1\r\n", mainVFS->UnMount() ? L"true" : L"false");
				break;
			}
			case 'x':
			{
				mainVFS->UnMount();
				mainVFS.reset();
				service.reset();
				break;
			}
			case '0':
			{
				system("cls");
				break;
			}
			case '1':
			{
				IFileSystem::UnMountDirectory(mainVFS->GetMountPoint());
				break;
			}
		};
	}
	return 0;
}
