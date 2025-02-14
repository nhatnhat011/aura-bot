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

#ifndef AURA_FILEUTIL_H_
#define AURA_FILEUTIL_H_

#define FUZZY_SEARCH_MAX_RESULTS 5
#define FUZZY_SEARCH_MAX_DISTANCE 10

#include "includes.h"
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cstdio>

#define __STORMLIB_SELF__
#include <StormLib.h>

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

struct FileChunkCached
{
  size_t fileSize;
  size_t start;
  size_t end;
  WeakByteArray bytes;

  FileChunkCached();
  FileChunkCached(size_t nFileSize, size_t nStart, size_t nEnd, SharedByteArray nBytes);
  ~FileChunkCached() = default;
};

struct FileChunkTransient
{
  size_t start;
  SharedByteArray bytes;

  FileChunkTransient();
  FileChunkTransient(size_t nStart, SharedByteArray nBytes);
  FileChunkTransient(const FileChunkCached& cached);
  ~FileChunkTransient() = default;
};

[[nodiscard]] bool FileExists(const std::filesystem::path& file);
[[nodiscard]] PLATFORM_STRING_TYPE GetFileName(const PLATFORM_STRING_TYPE& inputPath);
[[nodiscard]] PLATFORM_STRING_TYPE GetFileExtension(const PLATFORM_STRING_TYPE& inputPath);
[[nodiscard]] std::string PathToString(const std::filesystem::path& file);
[[nodiscard]] std::string PathToAbsoluteString(const std::filesystem::path& file);
[[nodiscard]] std::vector<std::filesystem::path> FilesMatch(const std::filesystem::path& path, const std::vector<PLATFORM_STRING_TYPE>& extensionList);

template <typename Container>
[[nodiscard]] bool FileRead(const std::filesystem::path& filePath, Container& container, const size_t maxSize) noexcept
{
  std::ifstream IS;
  container.clear();
  IS.open(filePath.native().c_str(), std::ios::binary | std::ios::in);

  if (IS.fail())  {
    Print("[FILE] warning - unable to read file [" + PathToString(filePath) + "]");
    return false;
  }

  // get length of file
  IS.seekg(0, std::ios::end);
  size_t fileSize = static_cast<long unsigned int>(IS.tellg());
  if (fileSize > maxSize) {
    Print("[FILE] error - refusing to load huge file [" + PathToString(filePath) + "]");
    return false;
  }

  // read data
  IS.seekg(0, std::ios::beg);
  try {
    container.reserve(fileSize);
    container.resize(fileSize);
  } catch (...) {
    container.clear();
    try {
      container.shrink_to_fit();
    } catch (...) {}
    Print("[FILE] error - insufficient memory for loading file [" + PathToString(filePath) + "]");
    return false;
  }
  IS.read(reinterpret_cast<char*>(container.data()), fileSize);
  if (IS.gcount() < fileSize) {
    container.clear();
    try {
      container.shrink_to_fit();
    } catch (...) {}
    Print("[FILE] error - stream failed to read all data from file [" + PathToString(filePath) + "]");
    return false;
  }
  return true;
}

template <typename Container>
[[nodiscard]] bool FileReadPartial(const std::filesystem::path& filePath, Container& container, const size_t start, size_t maxReadSize, size_t* fileSize, size_t* actualReadSize) noexcept
{
  std::ifstream IS;
  container.clear();
  IS.open(filePath.native().c_str(), std::ios::binary | std::ios::in);

  if (IS.fail())  {
    Print("[FILE] warning - unable to read file [" + PathToString(filePath) + "]");
    return false;
  }

  // get length of file
  IS.seekg(0, std::ios::end);
  *fileSize = static_cast<long unsigned int>(IS.tellg());
  if (start >= *fileSize) {
    Print("[FILE] error - cannot read pos (" + std::to_string(start) + " >= " + std::to_string(*fileSize) + ") from file [" + PathToString(filePath) + "]");
    return false;
  }
  if (maxReadSize > *fileSize - start) {
    maxReadSize = *fileSize - start;
  }

  // read data
  IS.seekg(start, std::ios::beg);
  try {
    container.reserve(maxReadSize);
    container.resize(maxReadSize);
  } catch (...) {
    container.clear();
    try {
      container.shrink_to_fit();
    } catch (...) {}
    Print("[FILE] error - insufficient memory for loading " + std::to_string(maxReadSize / 1024) + " KB chunk from file [" + PathToString(filePath) + "]");
    return false;
  }
  IS.read(reinterpret_cast<char*>(container.data()), maxReadSize);
  *actualReadSize = IS.gcount();
  if (*actualReadSize < maxReadSize) {
    container.clear();
    try {
      container.shrink_to_fit();
    } catch (...) {}
    Print("[FILE] error - stream failed to read all data (" + std::to_string(maxReadSize / 1024) + " KB) from file [" + PathToString(filePath) + "]");
    return false;
  }
  return true;
}

bool FileWrite(const std::filesystem::path& file, const uint8_t* data, size_t length);
bool FileAppend(const std::filesystem::path& file, const uint8_t* data, size_t length);
bool FileDelete(const std::filesystem::path& file);
[[nodiscard]] std::optional<int64_t> GetMaybeModifiedTime(const std::filesystem::path& file);
[[nodiscard]] std::filesystem::path CaseInsensitiveFileExists(const std::filesystem::path& path, const std::string& file);
[[nodiscard]] std::vector<std::pair<std::string, int>> FuzzySearchFiles(const std::filesystem::path& directory, const std::vector<PLATFORM_STRING_TYPE>& baseExtensions, const std::string& rawPattern);

[[nodiscard]] bool OpenMPQArchive(void** MPQ, const std::filesystem::path& filePath);
void CloseMPQArchive(void* MPQ);

template <typename Container>
bool ReadMPQFile(void* MPQ, const char* packedFileName, Container& container, const uint32_t locale)
{
  container.clear();
  SFileSetLocale(locale);

  void* subFile = nullptr;
  // override common.j
  if (SFileOpenFileEx(MPQ, packedFileName, 0, &subFile)) {
    const uint32_t fileLength = SFileGetFileSize(subFile, nullptr);

    if (fileLength > 0 && fileLength < MAX_READ_FILE_SIZE) {
      try {
        container.reserve(fileLength);
        container.resize(fileLength);
      } catch (...) {
        container.clear();
        try {
          container.shrink_to_fit();
        } catch (...) {}
        SFileCloseFile(subFile);
        Print("[FILE] error - insufficient memory for loading from archive [" + std::string(packedFileName) + "]");
        return false;
      }
#ifdef _WIN32
      unsigned long bytesRead = 0;
#else
      uint32_t bytesRead = 0;
#endif

      if (SFileReadFile(subFile, container.data(), fileLength, &bytesRead, nullptr)) {
        if (bytesRead < fileLength) {
          Print("[FILE] error reading " + std::string(packedFileName) + " - bytes read is " + std::to_string(bytesRead) + "; file length is " + std::to_string(fileLength));
          container.clear();
          try {
            container.shrink_to_fit();
          } catch (...) {}
          SFileCloseFile(subFile);
          return false;
        }
      }
    }

    SFileCloseFile(subFile);
  }
  return true;
}

bool ExtractMPQFile(void* MPQ, const char* packedFileName, const std::filesystem::path& outPath, const uint32_t locale = 0);

#endif // AURA_FILEUTIL_H_
