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

#include "os_util.h"
#include "util.h"

using namespace std;

#ifdef _WIN32
optional<wstring> MaybeReadRegistry(const wchar_t* mainKey, const wchar_t* subKey)
{
  optional<wstring> result;
  HKEY hKey;
  LPCWSTR registryPath = mainKey;
  LPCWSTR keyName = subKey;
  WCHAR buffer[2048];

  if (RegOpenKeyExW(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD bufferSize = sizeof(buffer);
    DWORD valueType;
    // Query the value of the desired registry entry
    if (RegQueryValueExW(hKey, keyName, nullptr, &valueType, reinterpret_cast<BYTE*>(buffer), &bufferSize) == ERROR_SUCCESS) {
      if (valueType == REG_SZ && 0 < bufferSize && bufferSize < 2048) {
        buffer[bufferSize / sizeof(WCHAR)] = L'\0';
        result = wstring(buffer);
      } else {
        Print("[REGISTRY] error - value too long");
      }
    }
    // Close the key
    RegCloseKey(hKey);
  }
  return result;
}

optional<filesystem::path> MaybeReadRegistryPath(const wchar_t* mainKey, const wchar_t* subKey)
{
  optional<wstring> content = MaybeReadRegistry(mainKey, subKey);
  if (!content.has_value()) return nullopt;

  optional<filesystem::path> result;
  result = filesystem::path(content.value());
  return result;
}

bool DeleteUserRegistryKey(const wchar_t* subKey)
{
  return RegDeleteTree(HKEY_CURRENT_USER, subKey) == ERROR_SUCCESS;
}

bool SetUserRegistryKey(const wchar_t* subKey, const wchar_t* valueName, const wchar_t* value)
{
  HKEY hKey;
  LONG result = RegCreateKeyEx(HKEY_CURRENT_USER, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
  if (result == ERROR_SUCCESS) {
    result = RegSetValueEx(hKey, valueName, 0, REG_SZ, (BYTE*)value, (wcslen(value) + 1) * sizeof(wchar_t));
    RegCloseKey(hKey);
    return true;
  }
  return false;
}

#endif

optional<string> GetUserMultiPlayerName()
{
#ifdef _WIN32
  optional<wstring> localName = MaybeReadRegistry(L"SOFTWARE\\Blizzard Entertainment\\Warcraft III\\String", L"userlocal");
  if (!localName.has_value()) {
    // Fallback to Battle.net name
    localName = MaybeReadRegistry(L"SOFTWARE\\Blizzard Entertainment\\Warcraft III\\String", L"userbnet");
  }
  if (!localName.has_value()) return nullopt;

  optional<string> result;
  int size = WideCharToMultiByte(CP_UTF8, 0, &localName.value()[0], (int)localName.value().size(), nullptr, 0, nullptr, nullptr);
  string multiByte(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, &localName.value()[0], (int)localName.value().size(), &multiByte[0], size, nullptr, nullptr);
  result = multiByte;
  return result;
#else
  return nullopt;
#endif
}

filesystem::path GetExePath()
{
  static filesystem::path Memoized;
  if (!Memoized.empty())
    return Memoized;

#ifdef _WIN32
  vector<wchar_t> buffer(2048);
  DWORD length = 0;
#else
  vector<char> buffer(2048);
  vector<char>::size_type length = 0;
#endif

  do {
    buffer.resize(buffer.size() * 2);
#ifdef _WIN32
    length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
#else
    length = readlink("/proc/self/exe", buffer.data(), buffer.size());
#endif
  } while ((buffer.size() <= 0xFFFF) && (buffer.size() - 1 <= static_cast<size_t>(length)));

  if (length == 0) {
    Print("[AURA] Failed to retrieve Aura's directory.");
    return Memoized;
  }
  buffer.resize(length);

  Memoized = filesystem::path(buffer.data());
  return Memoized;
}

filesystem::path GetExeDirectory()
{
  filesystem::path executablePath = GetExePath();
  filesystem::path cwd;
  try {
    cwd = filesystem::current_path();
  } catch (...) {}
  if (!cwd.empty()) {
    NormalizeDirectory(cwd);
    cwd = cwd.parent_path();
  }

  bool cwdIsAncestor = cwd.empty();
  if (!cwdIsAncestor) {
    try {
      filesystem::path exeAncestor = executablePath;
      filesystem::path exeRoot = exeAncestor.root_path();
      while (exeAncestor != exeRoot) {
        exeAncestor = exeAncestor.parent_path();
        if (exeAncestor == cwd) {
          cwdIsAncestor = true;
          break;
        }
      }
    } catch (...) {}
  }

  filesystem::path exeDirectory;
  if (cwdIsAncestor) {    
    exeDirectory = executablePath.parent_path().lexically_relative(cwd);
  } else {
    exeDirectory = executablePath.parent_path();
  }

  NormalizeDirectory(exeDirectory);
  return exeDirectory;
} 

PLATFORM_STRING_TYPE ReadPersistentUserPathEnvironment()
{
#ifdef _WIN32
  optional<PLATFORM_STRING_TYPE> userPath = MaybeReadRegistry(L"Environment", L"PATH");
  if (userPath.has_value()) {
    return userPath.value();
  }
#else
  // Maybe ~/.bash_profile, maybe ~/.bash_rc, maybe...
  // This is a mess.
#endif
  return PLATFORM_STRING_TYPE();
}

void SetPersistentUserPathEnvironment(const PLATFORM_STRING_TYPE& nUserPath)
{
#ifdef _WIN32
  SetUserRegistryKey(L"Environment", L"PATH", nUserPath.c_str());
#else
#endif
}

bool GetIsDirectoryInUserPath(const filesystem::path& nDirectory, PLATFORM_STRING_TYPE& nUserPath)
{
  nUserPath = ReadPersistentUserPathEnvironment();
  size_t startPos = 0;
  size_t endPos = nUserPath.find(PATH_ENVVAR_SEPARATOR, startPos);
  while (endPos != PLATFORM_STRING_TYPE::npos) {
    PLATFORM_STRING_TYPE currentDirectory = nUserPath.substr(startPos, endPos - startPos);
    if (currentDirectory == nDirectory.native()) {
      return true;
    }
    if (!nDirectory.empty() && !currentDirectory.empty() &&
      (currentDirectory.substr(0, currentDirectory.size() - 1) == nDirectory.native().substr(0, nDirectory.native().size() - 1))
    ) {
      return true;
    }
    startPos = endPos + 1;
    endPos = nUserPath.find(PATH_ENVVAR_SEPARATOR, startPos);
  }
  return false;
}

void AddDirectoryToUserPath(const filesystem::path& nDirectory, PLATFORM_STRING_TYPE& nUserPath)
{
  if (nDirectory.empty()) return;
  nUserPath = nDirectory.native() + PLATFORM_STRING_TYPE(PATH_ENVVAR_SEPARATOR) + nUserPath;
  SetPersistentUserPathEnvironment(nUserPath);
}

void EnsureDirectoryInUserPath(const filesystem::path& nDirectory)
{
  if (nDirectory.empty()) return;
  PLATFORM_STRING_TYPE userPath;
  if (!GetIsDirectoryInUserPath(nDirectory, userPath)) {
    AddDirectoryToUserPath(nDirectory, userPath);
    Print("[AURA] Installed to user PATH environment variable.");
  }
}

void SetWindowTitle(PLATFORM_STRING_TYPE nWindowTitle)
{
#ifdef _WIN32
  SetConsoleTitleW(nWindowTitle.c_str());
#else
  cout << "\033]0;" << nWindowTitle.c_str() << "\007";
#endif
}
