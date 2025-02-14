/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef AURA_OSUTIL_H_
#define AURA_OSUTIL_H_

#include "includes.h"
#include "file_util.h"

#ifdef _WIN32
#pragma once
#include <windows.h>
#define stat _stat
#else
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#endif

// unistd.h and limits.h

#ifdef _WIN32
#define PATH_ENVVAR_SEPARATOR L";"
#else
#define PATH_ENVVAR_SEPARATOR ":"
#endif

#ifdef _WIN32
[[nodiscard]] std::optional<std::wstring> MaybeReadRegistry(const wchar_t* mainKey, const wchar_t* subKey);
[[nodiscard]] std::optional<std::filesystem::path> MaybeReadRegistryPath(const wchar_t* mainKey, const wchar_t* subKey);
bool DeleteUserRegistryKey(const wchar_t* subKey);
bool SetUserRegistryKey(const wchar_t* subKey, const wchar_t* valueName, const wchar_t* value);
#endif
[[nodiscard]] std::optional<std::string> GetUserMultiPlayerName();

[[nodiscard]] std::filesystem::path GetExePath();
[[nodiscard]] std::filesystem::path GetExeDirectory();
[[nodiscard]] PLATFORM_STRING_TYPE ReadPersistentUserPathEnvironment();
void SetPersistentUserPathEnvironment(const PLATFORM_STRING_TYPE& nUserPath);
[[nodiscard]] bool GetIsDirectoryInUserPath(const std::filesystem::path& nDirectory, PLATFORM_STRING_TYPE& nUserPath);
void AddDirectoryToUserPath(const std::filesystem::path& nDirectory, PLATFORM_STRING_TYPE& nUserPath);
void EnsureDirectoryInUserPath(const std::filesystem::path& nDirectory);
void SetWindowTitle(PLATFORM_STRING_TYPE nWindowTitle);

#endif // AURA_FILEUTIL_H_
