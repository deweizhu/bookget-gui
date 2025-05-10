// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ctime>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>


class Util
{
public:
    static std::wstring UnixEpochToDateTime(double value);

    static bool fileWrite(std::wstring filename, std::wstring data);
    static bool fileAppend(std::wstring filename, std::wstring data);
    static std::wstring fileRead(std::wstring filename);
    static std::wstring GetCurrentExeDirectory();
    static std::wstring GetUserHomeDirectory();

    static std::wstring BoolToString(BOOL value);
    static std::wstring EncodeQuote(std::wstring raw);

    static std::string Utf16ToUtf8(const std::wstring& utf16);
    static std::wstring Utf8ToUtf16(const std::string& utf8);

    static bool FindProcessExist(const std::wstring);
    static void Trim(std::wstring& str);
    static bool CheckIfUrlsFileExists();

    static bool IsImageUrl(const std::wstring& url);

    static std::wstring GetFileNameFromUrl(const std::wstring& url);
    static bool IsImageContentType(const wchar_t* contentType);
    static std::wstring ParseJsonBool(const wchar_t* json);

};
