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

/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT

 */

#include "config.h"
#include "../includes.h"
#include "../util.h"
#include "../net.h"

#include <cstdlib>
#include <fstream>

#include <utf8/utf8.h>

using namespace std;

#define SUCCESS(T) \
    do { \
        m_ErrorLast = false; \
        return T; \
    } while(0);


#define CONFIG_ERROR(key, T) \
    do { \
        m_ErrorLast = true; \
        if (m_StrictMode) m_CriticalError = true; \
        Print(string("[CONFIG] Error - Invalid value provided for <") + key + string(">.")); \
        return T; \
    } while(0);


#define END(T) \
    do { \
        if (errored) Print(string("[CONFIG] Error - Invalid value provided for <") + key + string(">.")); \
        m_ErrorLast = errored; \
        if (errored && m_StrictMode) m_CriticalError = true; \
        return T; \
    } while(0);


//
// CConfig
//

CConfig::CConfig()
 : m_ErrorLast(false),
   m_CriticalError(false),
   m_StrictMode(false),
   m_IsModified(false)
{
}

CConfig::~CConfig() = default;

bool CConfig::Read(const filesystem::path& file, CConfig* adapterConfig)
{
  m_File = file;

  ifstream in;
  in.open(file.native().c_str(), ios::in);

  if (in.fail()) {
#ifdef _WIN32
    uint32_t errorCode = (uint32_t)GetLastOSError();
    string errorCodeString = (
      errorCode == 2 ? "file not found" : (
      (errorCode == 32 || errorCode == 33) ? "file is currently opened by another process." : (
      "error code " + to_string(errorCode)
      ))
    );
    Print("[CONFIG] warning - unable to read file [" + PathToString(file) + "] - " + errorCodeString);
#else
    Print("[CONFIG] warning - unable to read file [" + PathToString(file) + "] - " + string(strerror(errno)));
#endif
    return false;
  }

  Print("[CONFIG] loading file [" + PathToString(file) + "]");

  string RawLine;
  int lineCount = 0;

  while (!in.eof()) {
    lineCount++;
    getline(in, RawLine);

    // Strip UTF-8 BOM
    if (lineCount == 1 && RawLine.length() >= 3 && RawLine[0] == '\xEF' && RawLine[1] == '\xBB' && RawLine[2] == '\xBF') {
      RawLine = RawLine.substr(3);
    }

    // ignore blank lines and comments
    if (RawLine.empty() || RawLine[0] == '#' || RawLine[0] == ';' || RawLine == "\n") {
      continue;
    }

    // remove CR
    RawLine.erase(remove(begin(RawLine), end(RawLine), '\r'), end(RawLine));

    string Line = RawLine;
    string::size_type Split = Line.find('=');

    if (Split == string::npos || Split == 0) {
      continue;
    }

    string::size_type KeyStart   = Line.find_first_not_of(' ');
    string::size_type KeyEnd     = Line.find_last_not_of(' ', Split - 1) + 1;
    string::size_type ValueStart = Line.find_first_not_of(' ', Split + 1);
    string::size_type ValueEnd   = Line.find_last_not_of(' ') + 1;

    if (ValueStart == string::npos) {
      continue;
    }

    string Key = Line.substr(KeyStart, KeyEnd - KeyStart);
    if (adapterConfig) {
      Key = adapterConfig->GetString(Key, Key);
    }
    m_CFG[Key] = Line.substr(ValueStart, ValueEnd - ValueStart);
  }

  in.close();
  return true;
}

bool CConfig::Exists(const string& key)
{
  m_ValidKeys.insert(key);
  return m_CFG.find(key) != end(m_CFG);
}

void CConfig::Accept(const string& key)
{
  m_ValidKeys.insert(key);
}

void CConfig::Delete(const string& key)
{
  m_ValidKeys.insert(key);
  m_CFG.erase(key);
}

uint8_t CConfig::CheckRealmKey(const string& key) const
{
  if (key.length() < 9 || key.substr(0, 6) != "realm_") {
    return 0xFF;
  }
  size_t dotIndex = key.find('.');
  if (dotIndex == string::npos) {
    return 0xFF;
  }
  string realmNum = key.substr(6, dotIndex - 6);
  if (realmNum.empty() || realmNum.length() > 3) {
    return 0xFF;
  }
  int32_t value = 0;
  try {
    value = stoi(realmNum);
  } catch (...) {
  }
  if (1 <= value && value <= 120)
    return static_cast<uint8_t>(value - 1);

  return 0xFF;
}

vector<string> CConfig::GetInvalidKeys(const bitset<120> definedRealms) const
{
  vector<string> invalidKeys;
  for (const auto& entry : m_CFG) {
    if (m_ValidKeys.find(entry.first) == m_ValidKeys.end()) {
      uint8_t realmKey = CheckRealmKey(entry.first);
      if (realmKey == 0xFF || definedRealms.test(realmKey)) {
        invalidKeys.push_back("<" + entry.first + ">");
      }
    }
  }
  return invalidKeys;
}

string CConfig::GetString(const string& key)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(string())
  }

  SUCCESS(it->second)
}

string CConfig::GetString(const string& key, const string& x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  SUCCESS(it->second)
}

string CConfig::GetString(const string& key, const uint32_t minLength, const uint32_t maxLength, const string& x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  if (it->second.length() < minLength) {
    CONFIG_ERROR(key, x)
  }

  if (it->second.length() > maxLength) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(it->second)
}

uint8_t CConfig::GetStringIndex(const string& key, const vector<string>& fromList, const uint8_t x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  uint8_t maxIndex = static_cast<uint8_t>(fromList.size());
  for (uint8_t i = 0; i < maxIndex; ++i) {
    if (it->second == fromList[i]) {
      SUCCESS(i)
    }
  }

  CONFIG_ERROR(key, x)
}

bool CConfig::GetBool(const string& key, bool x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  if (it->second == "0" || it->second == "no" || it->second == "false" || it->second == "off" || it->second == "never" || it->second == "none") {
    SUCCESS(false);
  }
  if (it->second == "1" || it->second == "yes" || it->second == "true" || it->second == "on" || it->second == "always") {
    SUCCESS(true);
  }

  CONFIG_ERROR(key, x)
}

int32_t CConfig::GetInt32(const string& key, int32_t x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  int32_t Result = x;
  try {
    long Value = stol(it->second);
    if (Value > 0xFFFFFF) {
      CONFIG_ERROR(key, x)
    }
    Result = static_cast<int32_t>(Value);
  } catch (...) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(Result)
}

int64_t CConfig::GetInt64(const string& key, int64_t x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  int64_t Value = x;
  try {
    Value = stol(it->second);
  } catch (...) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(Value)
}

uint32_t CConfig::GetUint32(const string& key, uint32_t x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  int64_t Value = x;
  try {
    Value = stol(it->second);
    if (Value < 0 || 0xFFFFFFFF < Value) {
      CONFIG_ERROR(key, x)
    }
  } catch (...) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(static_cast<uint32_t>(Value))
}

uint16_t CConfig::GetUint16(const string& key, uint16_t x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  uint16_t Result = x;
  try {
    long Value = stol(it->second);
    if (Value < 0 || 0xFFFF < Value) {
      CONFIG_ERROR(key, x)
    }
    Result = static_cast<uint16_t>(Value);
  } catch (...) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(Result)
}

uint8_t CConfig::GetUint8(const string& key, uint8_t x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  uint8_t Result = x;
  try {
    long Value = stol(it->second);
    if (Value < 0 || 0xFF < Value) {
      CONFIG_ERROR(key, x)
    }
    Result = static_cast<uint8_t>(Value);
  } catch (...) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(Result)
}

float CConfig::GetFloat(const string& key, float x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  float Value = x;
  try {
    Value = stof(it->second.c_str());
  } catch (...) {
    CONFIG_ERROR(key, x)
  }

  SUCCESS(Value)
}

int32_t CConfig::GetInt(const string& key, int32_t x)
{
  return GetInt32(key, x);
}

vector<string> CConfig::GetList(const string& key, char separator, const vector<string> x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  vector<string> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.length() > 0) {
      Output.push_back(element);
    }
  }
  SUCCESS(Output)
}

set<string> CConfig::GetSet(const string& key, char separator, const set<string> x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  bool errored = false;
  set<string> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.empty())
      continue;
    if (!Output.insert(element).second)
      errored = true;
  }

  END(Output)
}

set<string> CConfig::GetSetInsensitive(const string& key, char separator, const set<string> x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  bool errored = false;
  set<string> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.empty())
      continue;
    transform(begin(element), end(element), begin(element), [](char c) { return static_cast<char>(std::tolower(c)); });
    if (!Output.insert(element).second)
      errored = true;
  }

  END(Output)
}

set<uint64_t> CConfig::GetUint64Set(const string& key, char separator, const set<uint64_t> x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  bool errored = false;
  set<uint64_t> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.empty())
      continue;

    uint64_t result = 0;
    try {
      long long value = stoll(element);
      result = static_cast<uint64_t>(value);
    } catch (const exception& e) {
      Print("Invalid value: " + element);
      Print("Error parsing uint64 - " + string(e.what()));
      CONFIG_ERROR(key, Output)
    }
    if (!Output.insert(result).second)
      errored = true;
  }

  END(Output)
}

vector<uint8_t> CConfig::GetUint8Vector(const string& key, const uint32_t count)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(vector<uint8_t>())
  }

  vector<uint8_t> Output = ExtractNumbers(it->second, count);
  if (Output.size() != count) {
    CONFIG_ERROR(key, vector<uint8_t>())
  }

  SUCCESS(Output)
}

set<uint8_t> CConfig::GetUint8Set(const string& key, char separator)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(set<uint8_t>())
  }

  bool errored = false;
  set<uint8_t> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.empty())
      continue;

    try {
      long Value = stol(element);
      if (Value < 0 || Value > 0xFF) {
        CONFIG_ERROR(key, set<uint8_t>())
      }
      if (!Output.insert(static_cast<uint8_t>(Value)).second) {
        errored = true;
      }
    } catch (...) {
      CONFIG_ERROR(key, set<uint8_t>())
    }
  }

  END(Output)
}

vector<uint8_t> CConfig::GetIPv4(const string& key, const array<uint8_t, 4> &x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(vector<uint8_t>(x.begin(), x.end()))
  }

  vector<uint8_t> Output = ExtractIPv4(it->second);
  if (Output.empty()) {
    CONFIG_ERROR(key, vector<uint8_t>(x.begin(), x.end()))
  }

  SUCCESS(Output)
}

set<string> CConfig::GetIPStringSet(const string& key, char separator, const set<string> x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

  bool errored = false;
  set<string> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.empty())
      continue;

    optional<sockaddr_storage> result = CNet::ParseAddress(element, ACCEPT_ANY);
    if (!result.has_value()) {
      errored = true;
      continue;
    }
    string normalIp = AddressToString(result.value());
    if (!Output.insert(normalIp).second) {
      errored = true;
    }
  }
  END(Output)
}

vector<sockaddr_storage> CConfig::GetHostListWithImplicitPort(const string& key, const uint16_t defaultPort, char separator)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS({})
  }

  bool errored = false;
  vector<sockaddr_storage> Output;
  stringstream ss(it->second);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.empty())
      continue;

    string ip;
    uint16_t port;
    if (!SplitIPAddressAndPortOrDefault(element, defaultPort, ip, port) || port == 0) {
      errored = true;
      continue;
    }
    optional<sockaddr_storage> result = CNet::ParseAddress(ip, ACCEPT_ANY);
    if (!result.has_value()) {
      errored = true;
      continue;
    }
    SetAddressPort(&(result.value()), port);
    Output.push_back(std::move(result.value()));
    result.reset();
  }
  END(Output)
}


filesystem::path CConfig::GetPath(const string &key, const filesystem::path &x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(x)
  }

#ifdef _WIN32
  if (!utf8::is_valid(it->second.begin(), it->second.end())) {
    CONFIG_ERROR(key, x)
  }

  wstring widePath;
  utf8::utf8to16(it->second.begin(), it->second.end(), back_inserter(widePath));

  filesystem::path value = widePath;
#else
  filesystem::path value = it->second;
#endif
  if (value.is_absolute()) {
    SUCCESS(value)
  }

  SUCCESS(filesystem::path(GetHomeDir() / value).lexically_normal())
}

filesystem::path CConfig::GetDirectory(const string &key, const filesystem::path &x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    filesystem::path defaultDirectory = x;
    NormalizeDirectory(defaultDirectory);
    SUCCESS(defaultDirectory)
  }

#ifdef _WIN32
  if (!utf8::is_valid(it->second.begin(), it->second.end())) {
    filesystem::path defaultDirectory = x;
    NormalizeDirectory(defaultDirectory);
    CONFIG_ERROR(key, defaultDirectory)
  }

  wstring widePath;
  utf8::utf8to16(it->second.begin(), it->second.end(), back_inserter(widePath));

  filesystem::path value = widePath;
#else
  filesystem::path value = it->second;
#endif
  if (value.is_absolute()) {
    NormalizeDirectory(value);
    SUCCESS(value)
  }

  value = GetHomeDir() / value;
  NormalizeDirectory(value);
  SUCCESS(value)
}

sockaddr_storage CConfig::GetAddressOfType(const string& key, const uint8_t acceptMode, const string& x)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  vector<string> tryAddresses;
  if (it != end(m_CFG)) tryAddresses.push_back(it->second);
  tryAddresses.push_back(x);

  for (uint8_t i = 0; i < 2; ++i) {
    optional<sockaddr_storage> result = CNet::ParseAddress(tryAddresses[i], acceptMode);
    if (result.has_value()) {
      if (i == 0) {
        SUCCESS(result.value())
      } else {
        CONFIG_ERROR(key, result.value());
      }
    }
  }

  struct sockaddr_storage fallback;
  memset(&fallback, 0, sizeof(fallback));
  CONFIG_ERROR(key, fallback)
}

sockaddr_storage CConfig::GetAddressIPv4(const string& key, const string& x)
{
  return GetAddressOfType(key, ACCEPT_IPV4, x);
}

sockaddr_storage CConfig::GetAddressIPv6(const string& key, const string& x)
{
  return GetAddressOfType(key, ACCEPT_IPV6, x);
}

sockaddr_storage CConfig::GetAddress(const string& key, const string& x)
{
  return GetAddressOfType(key, ACCEPT_ANY, x);
}

optional<bool> CConfig::GetMaybeBool(const string& key)
{
  m_ValidKeys.insert(key);
  optional<bool> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  if (it->second == "0" || it->second == "no" || it->second == "false" || it->second == "off" || it->second == "never" || it->second == "none") {
    result = false;
    SUCCESS(result)
  }
  if (it->second == "1" || it->second == "yes" || it->second == "true" || it->second == "on" || it->second == "always") {
    result = true;
    SUCCESS(result)
  }

  CONFIG_ERROR(key, result)
}

optional<uint8_t> CConfig::GetMaybeUint8(const string& key)
{
  m_ValidKeys.insert(key);
  optional<uint8_t> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  try {
    int64_t Value = stol(it->second);
    if (Value < 0 || 0xFF < Value) {
      CONFIG_ERROR(key, result)
    }
    result = static_cast<uint8_t>(Value);
  } catch (...) {
    CONFIG_ERROR(key, result)
  }

  SUCCESS(result)
}

optional<uint16_t> CConfig::GetMaybeUint16(const string& key)
{
  m_ValidKeys.insert(key);
  optional<uint16_t> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  try {
    int64_t Value = stol(it->second);
    if (Value < 0 || 0xFFFF < Value) {
      CONFIG_ERROR(key, result)
    }
    result = static_cast<uint16_t>(Value);
  } catch (...) {
    CONFIG_ERROR(key, result)
  }

  SUCCESS(result)
}

optional<uint32_t> CConfig::GetMaybeUint32(const string& key)
{
  m_ValidKeys.insert(key);
  optional<uint32_t> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  try {
    int64_t Value = stol(it->second);
    if (Value < 0 || 0xFFFFFFFF < Value) {
      CONFIG_ERROR(key, result)
    }
    result = static_cast<uint32_t>(Value);
  } catch (...) {
    CONFIG_ERROR(key, result)
  }

  SUCCESS(result)
}

optional<int64_t> CConfig::GetMaybeInt64(const string& key)
{
  m_ValidKeys.insert(key);
  optional<int64_t> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  try {
    long long value = stoll(it->second);
    result = static_cast<int64_t>(value);
  } catch (const exception& e) {
    Print("Invalid value: " + it->second);
    Print("Error parsing int64 - " + string(e.what()));
    CONFIG_ERROR(key, result)
  }

  SUCCESS(result)
}

optional<uint64_t> CConfig::GetMaybeUint64(const string& key)
{
  m_ValidKeys.insert(key);
  optional<uint64_t> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  try {
    long long value = stoll(it->second);
    result = static_cast<uint64_t>(value);
  } catch (const exception& e) {
    Print("Invalid value: " + it->second);
    Print("Error parsing int64 - " + string(e.what()));
    CONFIG_ERROR(key, result)
  }

  SUCCESS(result)
}

optional<vector<uint8_t>> CConfig::GetMaybeUint8Vector(const string &key, const uint32_t count)
{
  m_ValidKeys.insert(key);
  optional<vector<uint8_t>> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  vector<uint8_t> Output = ExtractNumbers(it->second, count);
  if (Output.size() != count) {
    CONFIG_ERROR(key, result)
  }

  result = Output;
  SUCCESS(result)
}

optional<vector<uint8_t>> CConfig::GetMaybeIPv4(const string &key)
{
  m_ValidKeys.insert(key);
  optional<vector<uint8_t>> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

  vector<uint8_t> Output = ExtractIPv4(it->second);
  if (Output.empty()) {
    CONFIG_ERROR(key, result)
  }

  result = Output;
  SUCCESS(result)
}

optional<filesystem::path> CConfig::GetMaybePath(const string &key)
{
  m_ValidKeys.insert(key);
  optional<filesystem::path> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

#ifdef _WIN32
  if (!utf8::is_valid(it->second.begin(), it->second.end())) {
    CONFIG_ERROR(key, result)
  }

  wstring widePath;
  utf8::utf8to16(it->second.begin(), it->second.end(), back_inserter(widePath));

  result = filesystem::path(widePath);
#else
  result = filesystem::path(it->second);
#endif
  if (result.value().is_absolute()) {
    SUCCESS(result)
  }
  result = (GetHomeDir() / result.value()).lexically_normal();
  SUCCESS(result)
}

optional<filesystem::path> CConfig::GetMaybeDirectory(const string &key)
{
  m_ValidKeys.insert(key);
  optional<filesystem::path> result;

  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    SUCCESS(result)
  }

#ifdef _WIN32
  if (!utf8::is_valid(it->second.begin(), it->second.end())) {
    CONFIG_ERROR(key, result)
  }

  wstring widePath;
  utf8::utf8to16(it->second.begin(), it->second.end(), back_inserter(widePath));

  result = filesystem::path(widePath);
#else
  result = filesystem::path(it->second);
#endif

  if (result.value().is_absolute()) {
    NormalizeDirectory(result.value());
    SUCCESS(result)
  }
  result = GetHomeDir() / result.value();
  NormalizeDirectory(result.value());
  SUCCESS(result)
}

optional<sockaddr_storage> CConfig::GetMaybeAddressOfType(const string& key, const uint8_t acceptMode)
{
  m_ValidKeys.insert(key);
  auto it = m_CFG.find(key);
  if (it == end(m_CFG)) {
    optional<sockaddr_storage> empty;
    SUCCESS(empty);
  }

  optional<sockaddr_storage> result = CNet::ParseAddress(it->second, acceptMode);
  if (result.has_value()) {
    SUCCESS(result);
  }

  CONFIG_ERROR(key, result)
}

optional<sockaddr_storage> CConfig::GetMaybeAddressIPv4(const string& key)
{
  return GetMaybeAddressOfType(key, ACCEPT_IPV4);
}

optional<sockaddr_storage> CConfig::GetMaybeAddressIPv6(const string& key)
{
  return GetMaybeAddressOfType(key, ACCEPT_IPV6);
}

optional<sockaddr_storage> CConfig::GetMaybeAddress(const string& key)
{
  return GetMaybeAddressOfType(key, ACCEPT_ANY);
}

void CConfig::Set(const string& key, const string& x)
{
  m_CFG[key] = x;
}

void CConfig::SetString(const string& key, const string& x)
{
  m_CFG[key] = x;
}

void CConfig::SetString(const string& key, const vector<uint8_t>& x)
{
  string xWrapped = string(begin(x), end(x));
  m_CFG[key] = xWrapped;
}

void CConfig::SetBool(const string& key, const bool& x)
{
  m_CFG[key] = x ? "1" : "0";
}

void CConfig::SetInt32(const string& key, const int32_t& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetInt64(const string& key, const int64_t& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetUint32(const string& key, const uint32_t& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetUint16(const string& key, const uint16_t& x)
{
  m_CFG[key] = to_string(x);
}
void CConfig::SetUint8(const string& key, const uint8_t& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetFloat(const string& key, const float& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetUint8Vector(const string& key, const vector<uint8_t> &x)
{
  m_CFG[key] = ByteArrayToDecString(x);
}

void CConfig::SetUint8Array(const string& key, const uint8_t* start, const size_t size)
{
  m_CFG[key] = ByteArrayToDecString(vector<uint8_t>(start, start + size));
}

std::vector<uint8_t> CConfig::Export() const
{
  std::ostringstream SS;
  for (auto it = m_CFG.begin(); it != m_CFG.end(); ++it) {
    SS << (it->first + " = " + it->second + "\n");
  }

  std::string str = SS.str();
  std::vector<uint8_t> bytes(str.begin(), str.end());
  return bytes;
}

std::string CConfig::ReadString(const std::filesystem::path& file, const std::string& key)
{
  std::string Output;
  ifstream in;
  in.open(file.native().c_str(), ios::in);

  if (in.fail())
    return Output;

  string RawLine;

  bool isFirstLine = true;
  while (!in.eof()) {
    getline(in, RawLine);

    if (isFirstLine) {
      if (RawLine.length() >= 3 && RawLine[0] == '\xEF' && RawLine[1] == '\xBB' && RawLine[2] == '\xBF')
        RawLine = RawLine.substr(3);
      isFirstLine = false;
    }

    // ignore blank lines and comments
    if (RawLine.empty() || RawLine[0] == '#' || RawLine[0] == ';' || RawLine == "\n") {
      continue;
    }

    // remove CR
    RawLine.erase(remove(begin(RawLine), end(RawLine), '\r'), end(RawLine));

    string Line = RawLine;
    string::size_type Split = Line.find('=');

    if (Split == string::npos || Split == 0)
      continue;

    string::size_type KeyStart   = Line.find_first_not_of(' ');
    string::size_type KeyEnd     = Line.find_last_not_of(' ', Split - 1) + 1;
    string::size_type ValueStart = Line.find_first_not_of(' ', Split + 1);
    string::size_type ValueEnd   = Line.find_last_not_of(' ') + 1;

    if (ValueStart == string::npos)
      continue;

    if (Line.substr(KeyStart, KeyEnd - KeyStart) == key) {
      Output = Line.substr(ValueStart, ValueEnd - ValueStart);
      break;
    }
  }

  in.close();
  return Output;
}

#undef SUCCESS
#undef CONFIG_ERROR
#undef END
