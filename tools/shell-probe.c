// 诊断探针：复刻 StoryBoard 打开 shapes 目录的三个通道，逐个报告返回值。
// build: x86_64-w64-mingw32-gcc probe.c -o probe.exe -municode -lshell32 -lole32
#define UNICODE
#define _UNICODE
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <windows.h>

int wmain(int argc, wchar_t** argv)
{
    if (argc < 3) {
        wprintf(L"usage: probe.exe <directory> <logfile>\n");
        return 2;
    }
    const wchar_t* dir = argv[1];
    FILE* log = NULL;
    _wfopen_s(&log, argv[2], L"w");
    if (!log) {
        return 3;
    }

    HRESULT initHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    fwprintf(log, L"CoInitializeEx: 0x%08lX\n", (unsigned long)initHr);

    HINSTANCE h = ShellExecuteW(NULL, L"open", dir, NULL, NULL, SW_SHOWNORMAL);
    fwprintf(log, L"ShellExecute open: %ld\n", (long)(intptr_t)h);

    PIDLIST_ABSOLUTE pidl = NULL;
    HRESULT parseHr = SHParseDisplayName(dir, NULL, &pidl, 0, NULL);
    fwprintf(log, L"SHParseDisplayName: 0x%08lX\n", (unsigned long)parseHr);
    if (SUCCEEDED(parseHr) && pidl) {
        HRESULT openHr = SHOpenFolderAndSelectItems(pidl, 0, NULL, 0);
        fwprintf(log, L"SHOpenFolderAndSelectItems: 0x%08lX\n", (unsigned long)openHr);
        ILFree(pidl);
    }
    CoUninitialize();
    fclose(log);
    return 0;
}
