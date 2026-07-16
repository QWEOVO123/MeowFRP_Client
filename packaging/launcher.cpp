#include <windows.h>

#include <string>
#include <vector>

static std::wstring quoteArg(const std::wstring &value)
{
    std::wstring escaped = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            escaped += L"\\\"";
        } else {
            escaped += ch;
        }
    }
    escaped += L"\"";
    return escaped;
}

static std::wstring parentDir(const std::wstring &path)
{
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, pos);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t launcherPath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, launcherPath, MAX_PATH) == 0) {
        MessageBoxW(nullptr, L"无法定位启动器路径。", L"frp-control-client", MB_ICONERROR);
        return 1;
    }

    const std::wstring root = parentDir(launcherPath);
    const std::wstring appDir = root + L"\\app";
    const std::wstring appPath = root + L"\\app\\frp-control-client-app.exe";
    const std::wstring libPath = root + L"\\lib";

    DWORD attrs = GetFileAttributesW(appPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(nullptr, L"找不到 app\\frp-control-client-app.exe。", L"frp-control-client", MB_ICONERROR);
        return 1;
    }

    DWORD pathLen = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    std::wstring oldPath;
    if (pathLen > 0) {
        oldPath.resize(pathLen);
        GetEnvironmentVariableW(L"PATH", oldPath.data(), pathLen);
        if (!oldPath.empty() && oldPath.back() == L'\0') {
            oldPath.pop_back();
        }
    }
    SetEnvironmentVariableW(L"PATH", (libPath + L";" + oldPath).c_str());
    SetDllDirectoryW(libPath.c_str());

    std::wstring commandLine = quoteArg(appPath);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, appDir.c_str(), &startup, &process)) {
        wchar_t message[256]{};
        wsprintfW(message, L"启动客户端失败，错误码：%lu", GetLastError());
        MessageBoxW(nullptr, message, L"frp-control-client", MB_ICONERROR);
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
