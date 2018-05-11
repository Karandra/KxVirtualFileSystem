#include "KxVirtualFileSystem.h"
#include "KxVFSService.h"
#include "Mirror/KxVFSMirror.h"
#include "Convergence/KxVFSConvergence.h"
#include "Utility/KxDynamicString.h"
#include "Utility/KxFileFinder.h"
#include <iostream>
#include <thread>
#include <tchar.h>

int _tmain()
{
	KxVFSService* pService = new KxVFSService(L"KortexVFS");
	pService->Install(L"C:\\Users\\Kerber\\Documents\\Visual Studio 2017\\Projects\\Kortex Mod Manager\\Kortex\\Bin\\Data\\VFS\\Drivers\\Win7 x64\\dokan2.sys");
	pService->Start();

	//KxVFSMirror* pMirror = new KxVFSMirror(pService, L"C:\\Users\\Kerber\\Desktop\\Test", L"D:\\Game Files\\The Elder Scrolls\\Skyrim", 0, ULONG_MAX);
	
	KxVFSMirror* pMirrorGC = new KxVFSMirror(pService, L"C:\\Users\\Kerber\\Documents\\My Games\\Skyrim", L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\VirtualGameConfig", 0, ULONG_MAX);
	KxVFSMirror* pMirrorPL = new KxVFSMirror(pService, L"C:\\Users\\Kerber\\AppData\\Local\\Skyrim", L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\PluginsOrder", 0, ULONG_MAX);

	KxVFSConvergence* pMainVFS = new KxVFSConvergence(pService, L"C:\\Users\\Kerber\\Desktop\\Test", L"C:\\Users\\Kerber\\Desktop\\TestWrite", 0, ULONG_MAX);
	pMainVFS->SetCanDeleteInVirtualFolder(true);
	pMainVFS->AddVirtualFolder(L"D:\\Game Files\\The Elder Scrolls\\Skyrim");
	//pMainVFS->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\TestData");
	pMainVFS->AddVirtualFolder(L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\Mods\\979ba932bb6d9e9b8049c2fda6fc255d\\ModFiles");
	pMainVFS->AddVirtualFolder(L"D:\\Games\\Kortex Mod Manager\\Skyrim\\Default\\Mods\\7cacfd93a9caea44cacfd86e614cd348\\ModFiles");
	//pMirror->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\VF\\1");
	//pMirror->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\VF\\2");
	//pMirror->AddVirtualFolder(L"C:\\Users\\Kerber\\Desktop\\VF\\3");

	char c = NULL;
	while (c != 'x')
	{
		std::cin >> c;
		switch (c)
		{
			case 'm':
			{
				int nError = pMainVFS->Mount();
				pMirrorGC->Mount();
				pMirrorPL->Mount();
				fwprintf(stdout, L"%d: %s\r\n", nError, KxVFSMirror::GetErrorCodeMessage(nError).data());
				break;
			}
			case 'c':
			{
				pMainVFS->RefreshDispatcherIndex();
				break;
			}
			case 'r':
			{
				auto ThreadEntry = []()
				{
					STARTUPINFO tStartupInfo = {0};
					tStartupInfo.cb = sizeof(tStartupInfo);

					PROCESS_INFORMATION tProcessInfo = {0};
					tProcessInfo.hProcess = INVALID_HANDLE_VALUE;
					tProcessInfo.hThread = INVALID_HANDLE_VALUE;

					WCHAR s[1024] = L"C:\\Users\\Kerber\\Desktop\\Test\\enbhost.exe";
					WCHAR s2[1024] = L"\"C:\\Users\\Kerber\\Desktop\\Test\\enbhost.exe\"";
					WCHAR s3[1024] = L"C:\\Users\\Kerber\\Desktop\\Test";
					//WCHAR s[1024] = L"M:\\enbhost.exe";
					//WCHAR s2[1024] = L"M:\\enbhost.exe";

					bool bOK = CreateProcessW
					(
						NULL,
						s2,
						NULL,
						NULL,
						FALSE,
						0,
						NULL,
						s3,
						&tStartupInfo,
						&tProcessInfo
					);
					DWORD nError = GetLastError();
					WaitForSingleObject(tProcessInfo.hProcess, INFINITE);

					fprintf(stdout, "%s: %u\r\n", bOK ? "true" : "false", nError);
				};
				std::thread t(ThreadEntry);
				t.detach();

				break;
			}
			case 's':
			{
				KxDynamicString s(L"qwerty\r\n");
				s = L"TestTestTest";
				s.append(L"XYZCVT");
				
				s.erase(2, 6);

				KxDynamicString spf = KxDynamicString::Format(L"%s, %d, 0x%p", s.data(), 45, pMainVFS);

				KxDynamicString s2 = L"\\Desktop\\f6c0ac810be991f2.kmpproj\\";
				//s2.erase(0, 1);

				s2.erase(s2.size() - 1, 1);
				

				break;
			}

			case 'u':
			{
				fprintf(stderr, "%s\r\n", pMainVFS->UnMount() ? "true" : "false");
				pMirrorGC->UnMount();
				pMirrorPL->UnMount();

				break;
			}
			case 'x':
			{
				fprintf(stderr, "%s\r\n", pMainVFS->UnMount() ? "true" : "false");
				pMirrorGC->UnMount();
				pMirrorPL->UnMount();

				delete pMainVFS;
				delete pMirrorGC;
				delete pMirrorPL;
				delete pService;
				break;
			}
		};
	}
	return 0;
}
