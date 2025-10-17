#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <iostream>
#include <string>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define EXIT_SUCCESS 0
#define EXIT_CANCELLED 1
#define EXIT_NETWORK_FAILURE 2
#define EXIT_DISK_FULL 3
#define EXIT_REBOOT_REQUIRED 1641
#define EXIT_INSTALL_IN_PROGRESS 1618

bool silentMode = true;

bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

bool RequestAdminRights() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.lpVerb = "runas";
    sei.lpFile = path;
    sei.lpParameters = silentMode ? "/s" : "";
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteExA(&sei)) {
        return false;
    }

    return true;
}

void Log(const std::string& msg) {
    if (!silentMode) {
        std::cout << msg << std::endl;
    }
}

bool CheckDiskSpace(const std::string& path, ULONGLONG requiredBytes) {
    ULARGE_INTEGER freeBytesAvailable;
    if (GetDiskFreeSpaceExA(path.c_str(), &freeBytesAvailable, NULL, NULL)) {
        return freeBytesAvailable.QuadPart >= requiredBytes;
    }
    return true;
}

bool IsInstallationInProgress() {
    HANDLE hMutex = CreateMutexA(NULL, TRUE, "FreshArchInstallerMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return true;
    }
    return false;
}

bool DownloadFile(const std::string& url, const std::string& outputPath) {
    DeleteFileA(outputPath.c_str());

    HINTERNET hInternet = InternetOpenA("FreshArch", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    HANDLE hFile = CreateFileA(outputPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    BYTE buffer[8192];
    DWORD bytesRead, bytesWritten;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL);
    }

    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return true;
}

void RegisterProgram(const std::string& installPath, const std::string& exePath) {
    HKEY hKey;
    std::string uninstallKey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FreshArch";

    RegCreateKeyExA(HKEY_LOCAL_MACHINE, uninstallKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    std::string displayName = "FreshArch";
    std::string publisher = "KakaoStudio";
    std::string version = "1.0.0";
    std::string uninstallString = "\"" + exePath + "\" /uninstall";
    DWORD estimatedSize = 200 * 1024;
    DWORD noModify = 1;
    DWORD noRepair = 1;

    RegSetValueExA(hKey, "DisplayName", 0, REG_SZ, (BYTE*)displayName.c_str(), displayName.length() + 1);
    RegSetValueExA(hKey, "DisplayIcon", 0, REG_SZ, (BYTE*)exePath.c_str(), exePath.length() + 1);
    RegSetValueExA(hKey, "DisplayVersion", 0, REG_SZ, (BYTE*)version.c_str(), version.length() + 1);
    RegSetValueExA(hKey, "Publisher", 0, REG_SZ, (BYTE*)publisher.c_str(), publisher.length() + 1);
    RegSetValueExA(hKey, "InstallLocation", 0, REG_SZ, (BYTE*)installPath.c_str(), installPath.length() + 1);
    RegSetValueExA(hKey, "UninstallString", 0, REG_SZ, (BYTE*)uninstallString.c_str(), uninstallString.length() + 1);
    RegSetValueExA(hKey, "EstimatedSize", 0, REG_DWORD, (BYTE*)&estimatedSize, sizeof(DWORD));
    RegSetValueExA(hKey, "NoModify", 0, REG_DWORD, (BYTE*)&noModify, sizeof(DWORD));
    RegSetValueExA(hKey, "NoRepair", 0, REG_DWORD, (BYTE*)&noRepair, sizeof(DWORD));

    RegCloseKey(hKey);
}

void CreateStartMenuShortcut(const std::string& exePath) {
    CoInitialize(NULL);

    char startMenuPath[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, startMenuPath);

    std::string shortcutPath = std::string(startMenuPath) + "\\FreshArch.lnk";

    IShellLinkA* pShellLink;
    IPersistFile* pPersistFile;

    CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&pShellLink);

    pShellLink->SetPath(exePath.c_str());
    pShellLink->SetDescription("FreshArch Application");
    pShellLink->SetIconLocation(exePath.c_str(), 0);

    char workDir[MAX_PATH];
    strcpy_s(workDir, exePath.c_str());
    char* lastSlash = strrchr(workDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    pShellLink->SetWorkingDirectory(workDir);

    pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);

    wchar_t wShortcutPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, shortcutPath.c_str(), -1, wShortcutPath, MAX_PATH);
    pPersistFile->Save(wShortcutPath, TRUE);

    pPersistFile->Release();
    pShellLink->Release();

    CoUninitialize();
}

void CreateDesktopShortcut(const std::string& exePath) {
    CoInitialize(NULL);

    char desktopPath[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, desktopPath);

    std::string shortcutPath = std::string(desktopPath) + "\\FreshArch.lnk";

    IShellLinkA* pShellLink;
    IPersistFile* pPersistFile;

    CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&pShellLink);

    pShellLink->SetPath(exePath.c_str());
    pShellLink->SetDescription("FreshArch Application");
    pShellLink->SetIconLocation(exePath.c_str(), 0);

    char workDir[MAX_PATH];
    strcpy_s(workDir, exePath.c_str());
    char* lastSlash = strrchr(workDir, '\\');
    if (lastSlash) *lastSlash = '\0';
    pShellLink->SetWorkingDirectory(workDir);

    pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);

    wchar_t wShortcutPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, shortcutPath.c_str(), -1, wShortcutPath, MAX_PATH);
    pPersistFile->Save(wShortcutPath, TRUE);

    pPersistFile->Release();
    pShellLink->Release();

    CoUninitialize();
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/s") == 0 || strcmp(argv[i], "/S") == 0) {
            silentMode = true;
        }
    }

    if (IsInstallationInProgress()) {
        return EXIT_INSTALL_IN_PROGRESS;
    }

    if (!IsRunAsAdmin()) {
        if (RequestAdminRights()) {
            return EXIT_SUCCESS;
        }
        else {
            Log("Install cancelled");
            return EXIT_CANCELLED;
        }
    }

    char programFiles[MAX_PATH];
    GetEnvironmentVariableA("ProgramFiles", programFiles, MAX_PATH);
    std::string installPath = std::string(programFiles) + "\\FreshArch";

    if (!CheckDiskSpace(programFiles, 200 * 1024 * 1024)) {
        Log("Disk space is full");
        return EXIT_DISK_FULL;
    }

    CreateDirectoryA(installPath.c_str(), NULL);

    std::string tarUrl = "https://github.com/IDK-kakao/FreshArch/releases/download/Fresh0/FreshArch.tar.gz";
    std::string exeUrl = "https://github.com/IDK-kakao/FreshArch/releases/download/Fresh0/launcher.exe";

    std::string tarPath = installPath + "\\FreshArch.tar.gz";
    std::string exePath = installPath + "\\launcher.exe";

    Log("Downloading FreshArch.tar.gz...");
    if (!DownloadFile(tarUrl, tarPath)) {
        Log("Net error");
        return EXIT_NETWORK_FAILURE;
    }

    Log("Downloading launcher.exe...");
    if (!DownloadFile(exeUrl, exePath)) {
        Log("Net error - launcher.exe");
        return EXIT_NETWORK_FAILURE;
    }

    RegisterProgram(installPath, exePath);
    CreateStartMenuShortcut(exePath);
    CreateDesktopShortcut(exePath);

    Log("Install - done");
  //  ShellExecuteA(NULL, "open", exePath.c_str(), NULL, installPath.c_str(), SW_SHOW);

    return EXIT_SUCCESS;
}
