#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <string>
#include <fstream>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

int dl(const char* url, const char* path) {
    HINTERNET net = InternetOpenA("FA", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!net) return 1;

    HINTERNET con = InternetOpenUrlA(net, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!con) {
        InternetCloseHandle(net);
        return 1;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(con);
        InternetCloseHandle(net);
        return 1;
    }

    char buf[8192];
    DWORD read, wrote;
    while (InternetReadFile(con, buf, sizeof(buf), &read) && read > 0) {
        WriteFile(file, buf, read, &wrote, NULL);
    }

    CloseHandle(file);
    InternetCloseHandle(con);
    InternetCloseHandle(net);
    return 0;
}

int main() {
    char pf[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, 0, pf))) return 3;

    std::string dir = std::string(pf) + "\\FreshArch";
    std::string f1 = dir + "\\1";
    std::string f0 = dir + "\\0";
    std::string tar = dir + "\\FreshArch.tar.gz";
    std::string exe = dir + "\\launcher.exe";

    if (GetFileAttributesA(f1.c_str()) != INVALID_FILE_ATTRIBUTES) {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(exe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return 0;
        }
        return 3;
    }

    if (!CreateDirectoryA(dir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 3;

    std::ofstream(f0).close();

    if (dl("https://github.com/IDK-kakao/FreshArch/releases/download/untagged-d89a6d64b2d9f593ba05/FreshArch.tar.gz", tar.c_str()) != 0) return 2;
    if (dl("https://github.com/IDK-kakao/FreshArch/releases/download/untagged-d89a6d64b2d9f593ba05/launcher.exe", exe.c_str()) != 0) return 2;

    std::string cmd = "tar -xzf \"" + tar + "\" -C \"" + dir + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return 3;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit;
    GetExitCodeProcess(pi.hProcess, &exit);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit != 0) return 3;

    DeleteFileA(tar.c_str());
    MoveFileA(f0.c_str(), f1.c_str());

    return 0;
}