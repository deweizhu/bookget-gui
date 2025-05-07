// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckFailure.h"
#include "Util.h"
#include <codecvt>
#include <regex>
#include <Windows.h>
#include <filesystem>
#include <string>    
#include <tlhelp32.h>
#include <shlobj.h> // For SHGetFolderPath

std::wstring Util::UnixEpochToDateTime(double value)
{
    WCHAR rawResult[32] = {};
    std::time_t rawTime = std::time_t(value / 1000);
    struct tm timeStruct = {};
    gmtime_s(&timeStruct, &rawTime);
    _wasctime_s(rawResult, 32, &timeStruct);
    std::wstring result(rawResult);
    return result;
}

//zhudw
bool Util::fileWrite(const std::wstring  filename, std::wstring data)    
{    
    std::ofstream outfile(filename, std::ios::binary | std::ios::out | std::ios::trunc);
	if (!outfile.is_open()) {
		return 0;
	}
    
    /// 将 UTF-16 字符串转换为 UTF-8 字符串
    int length = ::WideCharToMultiByte(CP_UTF8, 0, data.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length == 0) {
        return 0;
    }

    std::string utf8String(length, '\0');
    if (!::WideCharToMultiByte(CP_UTF8, 0, data.c_str(), -1, &utf8String[0], length, nullptr, nullptr)) {
        return 0;
    }
    outfile.write(utf8String.c_str(), utf8String.length());
	outfile.close();
	return true;    
}    

std::wstring Util::fileRead(const std::wstring  filename)    
{    
	std::ifstream infile(filename, std::ios::binary);
	if (!infile.is_open()) {
		return L"";
	}

    // 读取文件内容并转换为 UTF-16 字符串
    std::string utf8String;
    std::wstring result;
    while (std::getline(infile, utf8String)) {
         // 将 UTF-8 字符串转换为 UTF-16 字符串
        int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
        if (length == 0) {
            break;
        }
        std::wstring utf16String(length, L'\0');
        if (!::MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &utf16String[0], length)) {
          break;
        }
        result += utf16String;
    }

	infile.close();
    return result;
}    

bool Util::fileAppend(const std::wstring  filename, std::wstring data)    
{    
    std::ofstream outfile(filename, std::ios::binary | std::ios::out | std::ios::app);
    if (!outfile.is_open()) {
        return 0;
    }

    /// 将 UTF-16 字符串转换为 UTF-8 字符串
    int length = ::WideCharToMultiByte(CP_UTF8, 0, data.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length == 0) {
        return 0;
    }
    std::string utf8String(length, '\0');
    if (!::WideCharToMultiByte(CP_UTF8, 0, data.c_str(), -1, &utf8String[0], length, nullptr, nullptr)) {
        return 0;
    }
    outfile.write(utf8String.c_str(), utf8String.length());
	outfile.close();
	return true;    
}    


std::wstring Util::BoolToString(BOOL value)
{
    return value ? L"true" : L"false";
}

std::wstring Util::EncodeQuote(std::wstring raw)
{
    return L"\"" + std::regex_replace(raw, std::wregex(L"\""), L"\\\"") + L"\"";
}


std::wstring Util::GetCurrentExeDirectory()
{
    TCHAR buffer[MAX_PATH] = {0};
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::filesystem::path path(buffer);
    std::wstring filePath = path.parent_path().lexically_normal();
    return filePath;
}



std::wstring Util::GetUserHomeDirectory()
{
    wchar_t path[MAX_PATH];
    
    // CSIDL_PROFILE 表示用户的主目录（通常是 C:\Users\Username）
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, path)))
    {
        return path;
    }
    
    // 如果 SHGetFolderPathW 失败，尝试从环境变量获取
    wchar_t userProfile[MAX_PATH];
    size_t requiredSize = 0;
    if (_wgetenv_s(&requiredSize, userProfile, MAX_PATH, L"USERPROFILE") == 0 && requiredSize > 0)
    {
        return userProfile;
    }
    
    // 如果还是失败，尝试组合 HOMEDRIVE 和 HOMEPATH
    wchar_t homeDrive[MAX_PATH];
    wchar_t homePath[MAX_PATH];
    if (_wgetenv_s(&requiredSize, homeDrive, MAX_PATH, L"HOMEDRIVE") == 0 && requiredSize > 0 &&
        _wgetenv_s(&requiredSize, homePath, MAX_PATH, L"HOMEPATH") == 0 && requiredSize > 0)
    {
        return std::wstring(homeDrive) + homePath;
    }
    
    // 所有方法都失败，返回空字符串
    return L"";
}


std::string Util::Utf16ToUtf8(const std::wstring& utf16) {

    std::string utf8; // Result
     if (utf16.empty()) {
        return utf8;
     }

    if (utf16.length() > static_cast<size_t>((std::numeric_limits<int>::max)()))
    {
      throw std::overflow_error(
        "Input string too long: size_t-length doesn't fit into int.");
    }

    // Safely convert from size_t (STL string's length)
    // to int (for Win32 APIs)
    const int utf16Length = static_cast<int>(utf16.length());

    // Safely fails if an invalid UTF-8 character
    // is encountered in the input string
    //constexpr DWORD kFlags = MB_ERR_INVALID_CHARS;
    constexpr DWORD kFlags = 0;

    const int utf8Length = ::WideCharToMultiByte(
                              CP_UTF8,       // convert to UTF-8
                              kFlags,        // Conversion flags
                              utf16.data(),   // Source UTF-16 string pointer
                              utf16Length,    // Length of the source UTF-8 string, in chars
                              nullptr,       // Unused - no conversion done in this step
                              0,              // Request size of destination buffer, in wchar_ts
                              nullptr, nullptr
                            );

    if (utf8Length == 0)
    {
      // Conversion error: capture error code and throw
      const DWORD error = ::GetLastError();
      throw std::overflow_error(
        "Cannot get result string length when converting " \
        "from UTF-16 to UTF-8 (WideCharToMultiByte failed)." +
        error);
    }

    utf8.resize(utf8Length);

    // Convert from UTF-16 to UTF-8
    int result = ::WideCharToMultiByte(
      CP_UTF8,       // convert to UTF-8
      kFlags,        // Conversion flags
      utf16.data(),   // Source UTF-16 string pointer
      utf16Length,    // Length of source UTF-16 string, in chars
      &utf8[0],     // Pointer to destination buffer
      utf8Length,    // Size of destination buffer, in wchar_ts  
      nullptr, nullptr
    );

    if (result == 0)
    {
      // Conversion error: capture error code and throw
      const DWORD error = ::GetLastError();
      throw std::overflow_error(
        "Cannot convert from UTF-16 to UTF-8 "\
        "(WideCharToMultiByte failed)." + 
        error);
    }

    return utf8;
} // End of Utf16ToUtf8


std::wstring Util::Utf8ToUtf16(const std::string& utf8) {
     std::wstring utf16; // Result
     if (utf8.empty()) {
        return utf16;
     }

    if (utf8.length() > static_cast<size_t>((std::numeric_limits<int>::max)()))
    {
      throw std::overflow_error(
        "Input string too long: size_t-length doesn't fit into int.");
    }

    // Safely convert from size_t (STL string's length)
    // to int (for Win32 APIs)
    const int utf8Length = static_cast<int>(utf8.length());

    // Safely fails if an invalid UTF-8 character
    // is encountered in the input string
    //constexpr DWORD kFlags = MB_ERR_INVALID_CHARS;
    constexpr DWORD kFlags = 0;

    const int utf16Length = ::MultiByteToWideChar(
                              CP_UTF8,       // Source string is in UTF-8
                              kFlags,        // Conversion flags
                              utf8.data(),   // Source UTF-8 string pointer
                              utf8Length,    // Length of the source UTF-8 string, in chars
                              nullptr,       // Unused - no conversion done in this step
                              0              // Request size of destination buffer, in wchar_ts
                            );
    if (utf16Length == 0)
    {
      // Conversion error: capture error code and throw
      const DWORD error = ::GetLastError();
      throw std::overflow_error(
        "Cannot get result string length when converting " \
        "from UTF-8 to UTF-16 (MultiByteToWideChar failed)." +
        error);
    }

    utf16.resize(utf16Length);

    // Convert from UTF-8 to UTF-16
    int result = ::MultiByteToWideChar(
      CP_UTF8,       // Source string is in UTF-8
      kFlags,        // Conversion flags
      utf8.data(),   // Source UTF-8 string pointer
      utf8Length,    // Length of source UTF-8 string, in chars
      &utf16[0],     // Pointer to destination buffer
      utf16Length    // Size of destination buffer, in wchar_ts          
    );

    if (result == 0)
    {
      // Conversion error: capture error code and throw
      const DWORD error = ::GetLastError();
      throw std::overflow_error(
        "Cannot convert from UTF-8 to UTF-16 "\
        "(MultiByteToWideChar failed)." + 
        error);
    }

    return utf16;
} // End of Utf8ToUtf16

bool Util::FindProcessExist(const std::wstring processName)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe32{};
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return false;
    }

    int iCount = 0;
    do {
        if (std::wstring(pe32.szExeFile) == processName) {
            iCount++;
            break;
        }
    } while (Process32Next(hSnapshot, &pe32));
    CloseHandle(hSnapshot);
    return iCount > 0;
}