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

#include "file_util.h"
#include "util.h"

#include <algorithm>
#include <iostream>
#include <bitset>
#include <set>
#include <optional>
#include <locale>
#include <system_error>

using namespace std;

//
// FileChunkCached
//

FileChunkCached::FileChunkCached()
 : fileSize(0),
   start(0),
   end(0)
{
};
FileChunkCached::FileChunkCached(size_t nFileSize, size_t nStart, size_t nEnd, SharedByteArray nBytes)
 : fileSize(nFileSize),
   start(nStart),
   end(nEnd),
   bytes(nBytes)
{
};

//
// FileChunkTransient
//

FileChunkTransient::FileChunkTransient()
 : start(0)
{
};

FileChunkTransient::FileChunkTransient(size_t nStart, SharedByteArray nBytes)
 : start(nStart),
   bytes(nBytes)
{
};

FileChunkTransient::FileChunkTransient(const FileChunkCached& cached)
 : start(cached.start),
   bytes(cached.bytes.lock())
{
};

bool FileExists(const filesystem::path& file)
{
  error_code ec;
  return filesystem::exists(file, ec);
}

#ifdef _WIN32
wstring GetFileName(const wstring& inputPath) {
  filesystem::path filePath = inputPath;
  return filePath.filename().wstring();
}

wstring GetFileExtension(const wstring& inputPath) {
  wstring fileName = GetFileName(inputPath);
  size_t extIndex = fileName.find_last_of(L".");
  if (extIndex == wstring::npos) return wstring();
  wstring extension = fileName.substr(extIndex);
  std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });
  return extension;
}
#else
string GetFileName(const string& inputPath) {
  filesystem::path filePath = inputPath;
  return filePath.filename().string();
}

string GetFileExtension(const string& inputPath) {
  string fileName = GetFileName(inputPath);
  size_t extIndex = fileName.find_last_of(".");
  if (extIndex == string::npos) return string();
  string extension = fileName.substr(extIndex);
  std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}
#endif

string PathToString(const filesystem::path& inputPath) {
#ifdef _WIN32
  if (inputPath.empty()) return string();
  wstring fromWide = inputPath.wstring();
  int size = WideCharToMultiByte(CP_UTF8, 0, &fromWide[0], (int)fromWide.size(), nullptr, 0, nullptr, nullptr);
  string output(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, &fromWide[0], (int)fromWide.size(), &output[0], size, nullptr, nullptr);
  return output;
#else
  return inputPath.string();
#endif
}

string PathToAbsoluteString(const filesystem::path& inputPath) {
  filesystem::path displayPath = inputPath;
  if (displayPath.empty()) {
    displayPath = filesystem::path(".");
  }
  try {
    displayPath = filesystem::absolute(displayPath);
  } catch (...) {
  }
  return PathToString(displayPath);
}

vector<filesystem::path> FilesMatch(const filesystem::path& path, const vector<PLATFORM_STRING_TYPE>& extensionList)
{
  set<PLATFORM_STRING_TYPE> extensions(extensionList.begin(), extensionList.end());
  vector<filesystem::path> Files;

#ifdef _WIN32
  WIN32_FIND_DATAW data;
  filesystem::path pattern = path / filesystem::path("*");
  HANDLE           handle = FindFirstFileW(pattern.wstring().c_str(), &data);
  memset(&data, 0, sizeof(WIN32_FIND_DATAW));

  while (handle != INVALID_HANDLE_VALUE) {
    wstring fileName(data.cFileName);
    transform(begin(fileName), end(fileName), begin(fileName), [](wchar_t c) { return static_cast<wchar_t>(std::tolower(c)); });
    if (fileName != L"..") {
      if (extensions.empty() || extensions.find(GetFileExtension(fileName)) != extensions.end()) {
        Files.emplace_back(fileName);
      }
    }
    if (FindNextFileW(handle, &data) == FALSE)
      break;
  }

  FindClose(handle);
#else
  DIR* dir = opendir(path.c_str());

  if (dir == nullptr)
    return Files;

  struct dirent* dp = nullptr;

  while ((dp = readdir(dir)) != nullptr) {
    string fileName = string(dp->d_name);
    transform(begin(fileName), end(fileName), begin(fileName), [](char c) { return static_cast<char>(std::tolower(c)); });
    if (fileName != "." && fileName != "..") {
      if (extensions.empty() || extensions.find(GetFileExtension(fileName)) != extensions.end()) {
        Files.emplace_back(dp->d_name);
      }
    }
  }

  closedir(dir);
#endif

  return Files;
}

bool FileWrite(const filesystem::path& file, const uint8_t* data, size_t length)
{
  ofstream OS;
  OS.open(file.native().c_str(), ios::binary);

  if (OS.fail())
  {
    Print("[UTIL] warning - unable to write file [" + PathToString(file) + "]");
    return false;
  }

  // write data

  OS.write(reinterpret_cast<const char*>(data), length);
  OS.close();
  return true;
}

bool FileAppend(const filesystem::path& file, const uint8_t* data, size_t length)
{
  ofstream OS;
  OS.open(file.native().c_str(), ios::binary | ios::app);

  if (OS.fail())
  {
    Print("[UTIL] warning - unable to write file [" + PathToString(file) + "]");
    return false;
  }

  // write data

  OS.write(reinterpret_cast<const char*>(data), length);
  OS.close();
  return true;
}

bool FileDelete(const filesystem::path& file)
{
  error_code e;
  bool result = filesystem::remove(file, e);
  
  if (e) {
    Print("[AURA] Cannot delete " + PathToString(file) + ". " + e.message());
  }

  return result;
}

optional<int64_t> GetMaybeModifiedTime(const filesystem::path& file)
{
  optional<int64_t> result;
  
  try {
    filesystem::file_time_type lastModifiedTime = filesystem::last_write_time(file);
    result = chrono::duration_cast<chrono::seconds>(lastModifiedTime.time_since_epoch()).count();
  } catch (...) {
  }
  return result;
}

filesystem::path CaseInsensitiveFileExists(const filesystem::path& path, const string& file)
{
  std::string mutated_file = file;
  const size_t NumberOfCombinations = static_cast<size_t>(1) << mutated_file.size();

  for (size_t perm = 0; perm < NumberOfCombinations; ++perm) {
    std::bitset<64> bs(perm);
    std::transform(mutated_file.begin(), mutated_file.end(), mutated_file.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

    for (size_t index = 0; index < bs.size() && index < mutated_file.size(); ++index) {
      if (bs[index]) {
        mutated_file[index] = static_cast<char>(::toupper(mutated_file[index]));
      }
    }

    filesystem::path testPath = path / filesystem::path(mutated_file);
    if (FileExists(testPath))
      return testPath;
  }

  return "";
}

vector<pair<string, int>> FuzzySearchFiles(const filesystem::path& directory, const vector<PLATFORM_STRING_TYPE>& baseExtensions, const string& rawPattern)
{
  // If the pattern has a valid extension, restrict the results to files with that extension.
#ifdef _WIN32
  wstring rawExtension = filesystem::path(rawPattern).wstring();
#else
  string rawExtension = filesystem::path(rawPattern).string();
#endif
  vector<PLATFORM_STRING_TYPE> extensions;
  if (!rawExtension.empty() && find(baseExtensions.begin(), baseExtensions.end(), rawExtension) != baseExtensions.end()) {
    extensions = vector<PLATFORM_STRING_TYPE>(1, rawExtension);
  } else {
    extensions = baseExtensions;
  }
  string fuzzyPattern = PrepareMapPatternForFuzzySearch(rawPattern);
  vector<filesystem::path> folderContents = FilesMatch(directory, extensions);
  set<filesystem::path> filesSet(folderContents.begin(), folderContents.end());

  // 1. Try files that start with the given pattern (up to digits and parens).
  //    These have triple weight.
  vector<pair<string, int>> inclusionMatches;
  for (const auto& mapName : filesSet) {
    string mapString = PathToString(mapName);
    if (mapName.empty()) continue;
    string cmpName = PrepareMapPatternForFuzzySearch(mapString);
    size_t fuzzyPatternIndex = cmpName.find(fuzzyPattern);
    if (fuzzyPatternIndex == string::npos) {
      continue;
    }
    bool startsWithPattern = true;
    for (uint8_t i = 0; i < fuzzyPatternIndex; ++i) {
      if (!isdigit(mapString[i]) && mapString[i] != '(' && mapString[i] != ')') {
        startsWithPattern = false;
        break;
      }
    }
    if (startsWithPattern) {
      inclusionMatches.push_back(make_pair(mapString, static_cast<int>(mapString.length() - fuzzyPattern.length())));
      if (inclusionMatches.size() >= FUZZY_SEARCH_MAX_RESULTS) {
        break;
      }
    }
  }
  if (inclusionMatches.size() > 0) {
    sort(inclusionMatches.begin(), inclusionMatches.end());
    return inclusionMatches;
  }

  // 2. Try fuzzy searching
  string::size_type maxDistance = FUZZY_SEARCH_MAX_DISTANCE;
  if (fuzzyPattern.size() < 3 * (maxDistance - 4)) {
    // This formula approximates how PS !esdata fuzzy-matches
    // It's my hope that it works here as well.
    // 3->0, 4->1, 8->2
    // 3->5, 4->5, 8->6
    //
    // I add approx +4 because PrepareMapPatternForFuzzySearch removes .w3m/.w3x extension
    // In the case of 3->0, I effectively add +5, because 3->0 has always been too strict.
    maxDistance = fuzzyPattern.size() / 3 + 4;
  }

  vector<pair<string, int>> distances;
  for (const auto& mapName : filesSet) {
    string mapString = PathToString(mapName);
    if (mapString.empty()) continue;
    string cmpName = RemoveNonAlphanumeric(mapString);
    transform(begin(cmpName), end(cmpName), begin(cmpName), [](char c) { return static_cast<char>(std::tolower(c)); });
    if ((fuzzyPattern.size() <= cmpName.size() + maxDistance) && (cmpName.size() <= maxDistance + fuzzyPattern.size())) {
      string::size_type distance = GetLevenshteinDistance(fuzzyPattern, cmpName); // source to target
      if (distance <= maxDistance) {
        distances.emplace_back(mapString, static_cast<int>(distance * 3));
      }
    }
  }

  size_t resultCount = min(FUZZY_SEARCH_MAX_RESULTS, static_cast<int>(distances.size()));
  partial_sort(
    distances.begin(),
    distances.begin() + resultCount,
    distances.end(),
    [](const pair<string, int>& a, const pair<string, int>& b) {
        return a.second < b.second;
    }
  );

  vector<pair<string, int>> fuzzyMatches(distances.begin(), distances.begin() + resultCount);
  return fuzzyMatches;
}

bool OpenMPQArchive(void** MPQ, const filesystem::path& filePath)
{
  uint32_t locale = SFileGetLocale();
  if (locale == 0) {
    Print("[AURA] loading MPQ archive [" + PathToString(filePath) + "]");
  } else {
    Print("[AURA] loading MPQ archive [" + PathToString(filePath) + "] using locale " + to_string(locale));
  }
  return SFileOpenArchive(filePath.native().c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, MPQ);
}

void CloseMPQArchive(void* MPQ)
{
  SFileCloseArchive(MPQ);
}

bool ExtractMPQFile(void* MPQ, const char* packedFileName, const filesystem::path& outPath, const uint32_t locale)
{
  vector<uint8_t> container;
  ReadMPQFile(MPQ, packedFileName, container, locale);
  if (container.empty()) {
    Print("[AURA] warning - unable to extract " + string(packedFileName) + " from MPQ archive");
    return false;
  } else if (FileWrite(outPath, container.data(), container.size())) {
    Print("[AURA] extracted " + string(packedFileName) + " to [" + PathToString(outPath) + "]");
    return true;
  } else {
    Print("[AURA] warning - unable to save extracted " + string(packedFileName) + " to [" + PathToString(outPath) + "]");
    return false;
  }
}
