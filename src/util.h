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

#ifndef AURA_UTIL_H_
#define AURA_UTIL_H_

#include "includes.h"

#include <fstream>
#include <regex>
#include <filesystem>
#include <functional>

#include <utf8/utf8.h>
#include "hash.h"

#pragma once

#define TO_ARRAY(...) StringArray({__VA_ARGS__})

[[nodiscard]] inline std::string ToDecString(const uint8_t byte)
{
  return std::to_string(static_cast<uint16_t>(byte));
}

[[nodiscard]] inline std::string ToHexString(uint32_t i)
{
  std::string       result;
  std::stringstream SS;
  SS << std::hex << i;
  SS >> result;
  return result;
}

#ifdef _WIN32
[[nodiscard]] inline PLATFORM_STRING_TYPE ToDecStringCPlatform(const size_t value)
{
  PLATFORM_STRING_TYPE platformNumeral;
  std::string numeral = std::to_string(value);
  utf8::utf8to16(numeral.begin(), numeral.end(), std::back_inserter(platformNumeral));
  return platformNumeral;
}
#else
[[nodiscard]] inline PLATFORM_STRING_TYPE ToDecStringCPlatform(const size_t value)
{
  return std::to_string(value);
}
#endif

[[nodiscard]] inline std::optional<uint32_t> ParseUint32Hex(const std::string& hexString)
{
  if (hexString.empty() || hexString.size() > 8) {
    return std::nullopt;
  }

  // Initialize a uint32_t value
  uint32_t result;

  std::istringstream stream(hexString);
  stream >> std::hex >> result;

  if (stream.fail() || !stream.eof()) {
    return std::nullopt;
  }

  return result;
}

[[nodiscard]] inline std::string ToFormattedString(const double d, const uint8_t precision = 2)
{
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << d;
  return out.str();
}

[[nodiscard]] inline std::string ToFormattedRealm()
{
  return "@@LAN/VPN";
}

[[nodiscard]] inline std::string ToFormattedRealm(const std::string& hostName)
{
  if (hostName.empty()) return "@@LAN/VPN";
  return hostName;
}

inline void WriteUint16(std::vector<uint8_t>& buffer, const uint16_t value, const uint32_t offset, bool bigEndian = false)
{
  if (!bigEndian) {
    buffer[offset] = static_cast<uint8_t>(value);
    buffer[offset + 1] = static_cast<uint8_t>(value >> 8);
  } else {
    buffer[offset] = static_cast<uint8_t>(value >> 8);
    buffer[offset + 1] = static_cast<uint8_t>(value);
  }
}

inline void WriteUint32(std::vector<uint8_t>& buffer, const uint32_t value, const uint32_t offset, bool bigEndian = false)
{
  if (!bigEndian) {
    buffer[offset] = static_cast<uint8_t>(value);
    buffer[offset + 1] = static_cast<uint8_t>(value >> 8);
    buffer[offset + 2] = static_cast<uint8_t>(value >> 16);
    buffer[offset + 3] = static_cast<uint8_t>(value >> 24);
  } else {
    buffer[offset] = static_cast<uint8_t>(value >> 24);
    buffer[offset + 1] = static_cast<uint8_t>(value >> 16);
    buffer[offset + 2] = static_cast<uint8_t>(value >> 8);
    buffer[offset + 3] = static_cast<uint8_t>(value);
  }
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint8_t* a, const size_t size)
{
  return std::vector<uint8_t>(a, a + size);
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint8_t c)
{
  return std::vector<uint8_t>{c};
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint16_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint32_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const int64_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::vector<uint8_t>{
      static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)/*,
      static_cast<uint8_t>(i >> 32), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 56)*/
    };
  else
    return std::vector<uint8_t>{
      /*static_cast<uint8_t>(i >> 56), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(32),*/
      static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)
    };
}

[[nodiscard]] inline std::array<uint8_t, 1> CreateFixedByteArray(const uint8_t c)
{
  return std::array<uint8_t, 1>{c};
}

[[nodiscard]] inline std::array<uint8_t, 2> CreateFixedByteArray(const uint16_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 2>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8)};
  else
    return std::array<uint8_t, 2>{static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::array<uint8_t, 4> CreateFixedByteArray(const uint32_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 4>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)};
  else
    return std::array<uint8_t, 4>{static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::array<uint8_t, 4> CreateFixedByteArray(const int64_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 4>{
      static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)/*,
      static_cast<uint8_t>(i >> 32), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 56)*/
    };
  else
    return std::array<uint8_t, 4>{
      /*static_cast<uint8_t>(i >> 56), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(32),*/
      static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)
    };
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 1>>& optArray, const uint8_t c)
{
  std::array<uint8_t, 1> val = CreateFixedByteArray(c);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 2>>& optArray, const uint16_t i, bool bigEndian)
{
  std::array<uint8_t, 2> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 4>>& optArray, const uint32_t i, bool bigEndian)
{
  std::array<uint8_t, 4> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 4>>& optArray, const int64_t i, bool bigEndian)
{
  std::array<uint8_t, 4> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

[[nodiscard]] inline uint16_t ByteArrayToUInt16(const std::vector<uint8_t>& b, bool bigEndian, const uint32_t start = 0)
{
  if (b.size() < start + 2)
    return 0;

  if (!bigEndian)
    return static_cast<uint16_t>(b[start + 1] << 8 | b[start]);
  else
    return static_cast<uint16_t>(b[start] << 8 | b[start + 1]);
}

[[nodiscard]] inline uint32_t ByteArrayToUInt32(const std::vector<uint8_t>& b, bool bigEndian, const uint32_t start = 0)
{
  if (b.size() < start + 4)
    return 0;

  if (!bigEndian)
    return static_cast<uint32_t>(b[start + 3] << 24 | b[start + 2] << 16 | b[start + 1] << 8 | b[start]);
  else
    return static_cast<uint32_t>(b[start] << 24 | b[start + 1] << 16 | b[start + 2] << 8 | b[start + 3]);
}

[[nodiscard]] inline uint16_t ByteArrayToUInt16(const std::array<uint8_t, 2>& b, bool bigEndian)
{
  if (!bigEndian)
    return static_cast<uint16_t>(b[1] << 8 | b[0]);
  else
    return static_cast<uint16_t>(b[2] << 8 | b[3]);
}

[[nodiscard]] inline uint32_t ByteArrayToUInt32(const std::array<uint8_t, 4>& b, bool bigEndian)
{
  if (!bigEndian)
    return static_cast<uint32_t>(b[3] << 24 | b[2] << 16 | b[1] << 8 | b[0]);
  else
    return static_cast<uint32_t>(b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]);
}

[[nodiscard]] inline std::string ByteArrayToDecString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = std::to_string(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
    result += " " + std::to_string(*i);

  return result;
}

template <size_t SIZE>
[[nodiscard]] inline std::string ByteArrayToDecString(const std::array<uint8_t, SIZE>& b)
{
  std::string result = std::to_string(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
    result += " " + std::to_string(*i);

  return result;
}

[[nodiscard]] inline std::string ByteArrayToHexString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = ToHexString(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
  {
    if (*i < 0x10)
      result += " 0" + ToHexString(*i);
    else
      result += " " + ToHexString(*i);
  }

  return result;
}

template <size_t SIZE>
[[nodiscard]] inline std::string ByteArrayToHexString(const std::array<uint8_t, SIZE>& b)
{
  std::string result = ToHexString(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
  {
    if (*i < 0x10)
      result += " 0" + ToHexString(*i);
    else
      result += " " + ToHexString(*i);
  }

  return result;
}

inline void AppendByteArray(std::vector<uint8_t>& b, const std::vector<uint8_t>& append)
{
  b.insert(end(b), begin(append), end(append));
}

template <size_t SIZE>
inline void AppendByteArray(std::vector<uint8_t>& b, const std::array<uint8_t, SIZE>& append)
{
  b.insert(end(b), begin(append), end(append));
}

inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::vector<uint8_t>& append)
{
  b.insert(end(b), begin(append), end(append));
}

template <size_t SIZE>
inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::array<uint8_t, SIZE>& append)
{
  b.insert(end(b), begin(append), end(append));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint8_t* a, const int32_t size)
{
  AppendByteArray(b, CreateByteArray(a, size));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const std::string& append, bool terminator = true)
{
  // append the std::string plus a null terminator

  b.insert(end(b), begin(append), end(append));

  if (terminator)
    b.push_back(0);
}

inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::string& append, bool terminator = true)
{
  // append the std::string plus a null terminator

  b.insert(end(b), begin(append), end(append));

  if (terminator)
    b.push_back(0);
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint16_t i, bool bigEndian)
{
  AppendByteArray(b, CreateByteArray(i, bigEndian));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint32_t i, bool bigEndian)
{
  AppendByteArray(b, CreateByteArray(i, bigEndian));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const int64_t i, bool bigEndian)
{
  AppendByteArray(b, CreateByteArray(i, bigEndian));
}

[[nodiscard]] inline size_t FindNullDelimiterOrStart(const std::vector<uint8_t>& b, const size_t start)
{
  // start searching the byte array at position 'start' for the first null value
  // if found, return the subarray from 'start' to the null value but not including the null value

  size_t end = b.size();
  if (start >= end) return start;
  for (size_t i = start; i < end; ++i) {
    if (b[i] == 0) {
      return i;
    }
  }
  return start;
}

[[nodiscard]] inline size_t FindNullDelimiterOrEnd(const std::vector<uint8_t>& b, const size_t start)
{
  // start searching the byte array at position 'start' for the first null value
  // if found, return the subarray from 'start' to the null value but not including the null value

  size_t end = b.size();
  if (start >= end) return end;
  for (size_t i = start; i < end; ++i) {
    if (b[i] == 0) {
      return i;
    }
  }
  return end;
}

[[nodiscard]] inline std::string GetStringAddressRange(const uint8_t* start, const uint8_t* end)
{
  if (end == start) return std::string();
  return std::string(reinterpret_cast<const char*>(start), end - start);
}

[[nodiscard]] inline std::vector<uint8_t> ExtractCString(const std::vector<uint8_t>& b, const size_t start)
{
  // start searching the byte array at position 'start' for the first null value
  // if found, return the subarray from 'start' to the null value but not including the null value

  if (start < b.size())
  {
    for (size_t i = start; i < b.size(); ++i)
    {
      if (b[i] == 0)
        return std::vector<uint8_t>(begin(b) + start, begin(b) + i);
    }

    // no null value found, return the rest of the byte array

    return std::vector<uint8_t>(begin(b) + start, end(b));
  }

  return std::vector<uint8_t>();
}

[[nodiscard]] inline uint8_t ExtractHex(const std::vector<uint8_t>& b, const uint32_t start, bool bigEndian)
{
  // consider the byte array to contain a 2 character ASCII encoded hex value at b[start] and b[start + 1] e.g. "FF"
  // extract it as a single decoded byte

  if (start + 1 < b.size())
  {
    uint8_t c = 0;
    std::string temp = std::string(begin(b) + start, begin(b) + start + 2);

    if (bigEndian)
      temp = std::string(temp.rend(), temp.rbegin());

    std::stringstream SS;
    SS << temp;
    SS >> std::hex >> c;
    return c;
  }

  return 0;
}

[[nodiscard]] inline std::vector<uint8_t> ExtractNumbers(const std::string& s, const uint32_t maxCount)
{
  // consider the std::string to contain a bytearray in dec-text form, e.g. "52 99 128 1"

  std::vector<uint8_t> result;
  uint32_t             c;
  std::stringstream    SS;
  SS << s;

  for (uint32_t i = 0; i < maxCount; ++i)
  {
    if (SS.eof())
      break;

    SS >> c;

    if (SS.fail() || c > 0xFF)
      break;

    result.push_back(static_cast<uint8_t>(c));
  }

  return result;
}

[[nodiscard]] inline std::vector<uint8_t> ExtractHexNumbers(const std::string& s)
{
  // consider the std::string to contain a bytearray in hex-text form, e.g. "4e 17 b7 e6"

  std::vector<uint8_t> result;
  uint32_t             c;
  std::stringstream    SS;
  SS << s;

  while (!SS.eof())
  {
    SS >> std::hex >> c;
    if (c > 0xFF)
      break;

    result.push_back(static_cast<uint8_t>(c));
  }

  return result;
}

[[nodiscard]] inline std::vector<uint8_t> ExtractIPv4(const std::string& s)
{
  std::vector<uint8_t> Output;
  std::stringstream ss(s);
  while (ss.good()) {
    std::string element;
    std::getline(ss, element, '.');
    if (element.empty())
      break;

    int32_t parsedElement = 0;
    try {
      parsedElement = std::stoi(element);
    } catch (...) {
      break;
    }
    if (parsedElement < 0 || parsedElement > 0xFF)
      break;

    Output.push_back(static_cast<uint8_t>(parsedElement));
  }

  if (Output.size() != 4)
    Output.clear();

  return Output;
}

[[nodiscard]] inline std::string TrimString(const std::string& str) {
  size_t firstNonSpace = str.find_first_not_of(" ");
  size_t lastNonSpace = str.find_last_not_of(" ");

  if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
    return str.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
  } else {
    return std::string();
  }
}

[[nodiscard]] inline std::vector<std::string> SplitArgs(const std::string& s, const uint8_t expectedCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  std::string NextItem;
  std::vector<std::string> Output;
  do {
    std::getline(SS, NextItem, ',');
    if (SS.fail()) {
      break;
    }
    Output.push_back(TrimString(NextItem));
    ++parsedCount;
  } while (!SS.eof() && parsedCount < expectedCount);

  if (parsedCount != expectedCount)
    Output.clear();

  return Output;
}

[[nodiscard]] inline std::vector<std::string> SplitArgs(const std::string& s, const uint8_t minCount, const uint8_t maxCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  std::string NextItem;
  std::vector<std::string> Output;
  do {
    std::getline(SS, NextItem, ',');
    if (SS.fail()) {
      break;
    }
    Output.push_back(TrimString(NextItem));
    ++parsedCount;
  } while (!SS.eof() && parsedCount < maxCount);

  if (!(minCount <= parsedCount && parsedCount <= maxCount))
    Output.clear();

  return Output;
}

[[nodiscard]] inline std::vector<uint32_t> SplitNumericArgs(const std::string& s, const uint8_t expectedCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  uint32_t NextItem;
  std::string NextString;
  std::vector<uint32_t> Output;
  do {
    bool elemOkay = true;
    std::getline(SS, NextString, ',');
    if (SS.fail()) {
      break;
    }
    try {
      NextItem = std::stol(TrimString(NextString));
    } catch (...) {
      elemOkay = false;
    }
    if (!elemOkay) {
      Output.clear();
      break;
    }
    Output.push_back(NextItem);
    ++parsedCount;
  } while (!SS.eof() && parsedCount < expectedCount);

  if (parsedCount != expectedCount)
    Output.clear();

  return Output;
}

[[nodiscard]] inline std::vector<uint32_t> SplitNumericArgs(const std::string& s, const uint8_t minCount, const uint8_t maxCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  uint32_t NextItem;
  std::string NextString;
  std::vector<uint32_t> Output;
  do {
    bool elemOkay = true;
    std::getline(SS, NextString, ',');
    if (SS.fail()) {
      break;
    }
    try {
      NextItem = std::stol(TrimString(NextString));
    } catch (...) {
      elemOkay = false;
    }
    if (!elemOkay) {
      Output.clear();
      break;
    }
    Output.push_back(NextItem);
    ++parsedCount;
  } while (!SS.eof() && parsedCount < maxCount);

  if (!(minCount <= parsedCount && parsedCount <= maxCount))
    Output.clear();

  return Output;
}

inline void AssignLength(std::vector<uint8_t>& content)
{
  // insert the actual length of the content array into bytes 3 and 4 (indices 2 and 3)

  const uint16_t Size = static_cast<uint16_t>(content.size());
  content[2] = static_cast<uint8_t>(Size);
  content[3] = static_cast<uint8_t>(Size >> 8);
}

[[nodiscard]] inline bool ValidateLength(const std::vector<uint8_t>& content)
{
  // verify that bytes 3 and 4 (indices 2 and 3) of the content array describe the length

  size_t size = content.size();
  if (size >= 4 && size <= 0xFFFF) {
    return ByteArrayToUInt16(content, false, 2) == static_cast<uint16_t>(size);
  }
  return false;
}

[[nodiscard]] inline std::string AddPathSeparator(const std::string& path)
{
  if (path.empty())
    return std::string();

#ifdef _WIN32
  const char Separator = '\\';
#else
  const char Separator = '/';
#endif

  if (*(end(path) - 1) == Separator)
    return path;
  else
    return path + std::string(1, Separator);
}

[[nodiscard]] inline std::vector<uint8_t> EncodeStatString(std::vector<uint8_t>& data)
{
  std::vector<uint8_t> Result;
  uint8_t              Mask = 1;

  for (uint32_t i = 0; i < data.size(); ++i)
  {
    if ((data[i] % 2) == 0)
      Result.push_back(data[i] + 1);
    else
    {
      Result.push_back(data[i]);
      Mask |= 1 << ((i % 7) + 1);
    }

    if (i % 7 == 6 || i == data.size() - 1)
    {
      Result.insert(end(Result) - 1 - (i % 7), Mask);
      Mask = 1;
    }
  }

  return Result;
}

[[nodiscard]] inline std::vector<uint8_t> DecodeStatString(const std::vector<uint8_t>& data)
{
  uint8_t              Mask = 1;
  std::vector<uint8_t> Result;

  for (uint32_t i = 0; i < data.size(); ++i)
  {
    if ((i % 8) == 0)
      Mask = data[i];
    else
    {
      if ((Mask & (1 << (i % 8))) == 0)
        Result.push_back(data[i] - 1);
      else
        Result.push_back(data[i]);
    }
  }

  return Result;
}

[[nodiscard]] inline std::vector<std::string> Tokenize(const std::string& s, const char delim)
{
  std::vector<std::string> Tokens;
  std::string              Token;

  for (auto i = begin(s); i != end(s); ++i)
  {
    if (*i == delim)
    {
      if (Token.empty())
        continue;

      Tokens.push_back(Token);
      Token.clear();
    }
    else
      Token += *i;
  }

  if (!Token.empty())
    Tokens.push_back(Token);

  return Tokens;
}

[[nodiscard]] inline std::string::size_type GetLevenshteinDistance(const std::string& s1, const std::string& s2) {
  std::string::size_type m = s1.length();
  std::string::size_type n = s2.length();

  // Create a 2D vector to store the distances
  std::vector<std::vector<std::string::size_type>> dp(m + 1, std::vector<std::string::size_type>(n + 1, 0));

  for (std::string::size_type i = 0; i <= m; ++i) {
    for (std::string::size_type j = 0; j <= n; ++j) {
      if (i == 0) {
          dp[i][j] = j;
      } else if (j == 0) {
          dp[i][j] = i;
      } else if (s1[i - 1] == s2[j - 1]) {
          dp[i][j] = dp[i - 1][j - 1];
      } else {
        std::string::size_type cost = (s1[i - 1] == s2[j - 1]) ? 0 : (isdigit(s1[i - 1]) || isdigit(s2[j - 1])) ? 3 : 1;
        dp[i][j] = std::min({ dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost });
      }
    }
  }

  return dp[m][n];
}

[[nodiscard]] inline std::string::size_type GetLevenshteinDistanceForSearch(const std::string& s1, const std::string& s2, const std::string::size_type bestDistance) {
  std::string::size_type m = s1.length();
  std::string::size_type n = s2.length();

  if (m > n + bestDistance) {
    return m - n;
  }

  if (n > m + bestDistance) {
    return n - m;
  }

  // Create a 2D vector to store the distances
  std::vector<std::vector<std::string::size_type>> dp(m + 1, std::vector<std::string::size_type>(n + 1, 0));

  for (std::string::size_type i = 0; i <= m; ++i) {
    for (std::string::size_type j = 0; j <= n; ++j) {
      if (i == 0) {
          dp[i][j] = j;
      } else if (j == 0) {
          dp[i][j] = i;
      } else if (s1[i - 1] == s2[j - 1]) {
          dp[i][j] = dp[i - 1][j - 1];
      } else {
        std::string::size_type cost = (s1[i - 1] == s2[j - 1]) ? 0 : (isdigit(s1[i - 1]) || isdigit(s2[j - 1])) ? 3 : 1;
        dp[i][j] = std::min({ dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost });
      }
    }
  }

  return dp[m][n];
}

[[nodiscard]] inline std::string CheckIsValidHCL(const std::string& s) {
  std::string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";
  if (s.find_first_not_of(HCLChars) != std::string::npos) {
    return "[" + s + "] is not a valid HCL string.";
  }
  return std::string();
}

[[nodiscard]] inline std::string DurationLeftToString(int64_t remainingSeconds) {
  if (remainingSeconds < 0)
    remainingSeconds = 0;
  int64_t remainingMinutes = remainingSeconds / 60;
  remainingSeconds = remainingSeconds % 60;
  if (remainingMinutes == 0) {
    return std::to_string(remainingSeconds) + " seconds";
  } else if (remainingSeconds == 0) {
    return std::to_string(remainingMinutes) + " minutes";
  } else {
    return std::to_string(remainingMinutes) + " min " + std::to_string(remainingSeconds) + "s";
  }
}

[[nodiscard]] inline std::string RemoveNonAlphanumeric(const std::string& s) {
    std::regex nonAlphanumeric("[^a-zA-Z0-9]");
    return std::regex_replace(s, nonAlphanumeric, "");
}

[[nodiscard]] inline std::string RemoveNonAlphanumericNorHyphen(const std::string& s) {
    std::regex nonAlphanumericNorHyphen("[^a-zA-Z0-9-]");
    return std::regex_replace(s, nonAlphanumericNorHyphen, "");
}

[[nodiscard]] inline bool IsValidMapName(const std::string& s) {
  if (!s.length()) return false;
  if (s[0] == '.') return false;
  std::regex invalidChars("[^a-zA-Z0-9_ ().~-]");
  std::regex validExtensions("\\.w3(m|x)$");
  return !std::regex_search(s, invalidChars) && std::regex_search(s, validExtensions);
}

[[nodiscard]] inline bool IsValidCFGName(const std::string& s) {
  if (!s.length()) return false;
  if (s[0] == '.') return false;
  std::regex invalidChars("[^a-zA-Z0-9_ ().~-]");
  std::regex validExtensions("\\.ini$");
  return !std::regex_search(s, invalidChars) && std::regex_search(s, validExtensions);
}

[[nodiscard]] inline std::string TrimTrailingSlash(const std::string s) {
  if (s.empty()) return s;
  if (s[s.length() - 1] == '/') return s.substr(0, s.length() - 1);
  return s;
}

[[nodiscard]] inline bool IsBase10Number(const std::string& s) {
  if (s.empty()) return false;
  if (s[0] == '0') return s.length() == 1;

  for (char ch : s) {
    if (!isdigit(ch)) return false;
  }
  return true;
}

[[nodiscard]] inline std::string MaybeBase10(const std::string s) {
  return IsBase10Number(s) ? s : std::string();
}

[[nodiscard]] inline std::string JoinVector(const std::vector<std::string>& list, const std::string connector, const bool trailingConnector) {
  std::string Results;
  for (const auto& element : list)
    Results += element + connector;
  if (!trailingConnector) Results = Results.substr(0, Results.length() - 2);
  return Results;
}

[[nodiscard]] inline std::string JoinVector(const std::vector<uint16_t>& list, const std::string connector, const bool trailingConnector) {
  std::string Results;
  for (const auto& element : list)
    Results += std::to_string(element) + connector;
  if (!trailingConnector) Results = Results.substr(0, Results.length() - 2);
  return Results;
}

[[nodiscard]] inline std::string JoinVector(const std::vector<std::string>& list, const bool trailingComma) {
  return JoinVector(list, ", ", trailingComma);
}

[[nodiscard]] inline std::string JoinVector(const std::vector<uint16_t>& list, const bool trailingComma) {
  return JoinVector(list, ", ", trailingComma);
}

[[nodiscard]] inline std::string JoinSet(const std::set<std::string>& list, const std::string connector, const bool trailingConnector) {
  std::string Results;
  for (const auto& element : list)
    Results += element + connector;
  if (!trailingConnector) Results = Results.substr(0, Results.length() - 2);
  return Results;
}

[[nodiscard]] inline std::string JoinSet(const std::set<uint16_t>& list, const std::string connector, const bool trailingConnector) {
  std::string Results;
  for (const auto& element : list)
    Results += std::to_string(element) + connector;
  if (!trailingConnector) Results = Results.substr(0, Results.length() - 2);
  return Results;
}

[[nodiscard]] inline std::string JoinSet(const std::set<std::string>& list, const bool trailingComma) {
  return JoinSet(list, ", ", trailingComma);
}

[[nodiscard]] inline std::string JoinSet(const std::set<uint16_t>& list, const bool trailingComma) {
  return JoinSet(list, ", ", trailingComma);
}

[[nodiscard]] inline std::string IPv4ToString(const std::array<uint8_t, 4> ip) {
  return ToDecString(ip[0]) + "." + ToDecString(ip[1]) + "." + ToDecString(ip[2]) + "." + ToDecString(ip[3]);
}

[[nodiscard]] inline bool SplitIPAddressAndPortOrDefault(const std::string& input, const uint16_t defaultPort, std::string& ip, uint16_t& port)
{
  size_t colonPos = input.rfind(':');
  
  if (colonPos == std::string::npos) {
    ip = input;
    port = defaultPort;
    return true;
  }

  if (input.find(']') == std::string::npos) {
    ip = input.substr(0, colonPos);
    int32_t parsedPort = 0;
    try {
      parsedPort = std::stoi(input.substr(colonPos + 1));
    } catch (...) {
      return false;
    }
    if (parsedPort < 0 || 0xFFFF < parsedPort) {
      return false;
    }
    port = static_cast<uint16_t>(parsedPort);
    return true;
  }

  // This is an IPv6 literal, expect format: [IPv6]:PORT
  size_t startBracket = input.find('[');
  size_t endBracket = input.find(']');
  if (startBracket == std::string::npos || endBracket == std::string::npos || endBracket < startBracket) {
    return false;
  }

  ip = input.substr(startBracket + 1, endBracket - startBracket - 1);
  if (colonPos > endBracket) {
    int32_t parsedPort = 0;
    try {
      parsedPort = std::stoi(input.substr(colonPos + 1));
    } catch (...) {
      return false;
    }
    if (parsedPort < 0 || 0xFFFF < parsedPort) {
      return false;
    }
    port = static_cast<uint16_t>(parsedPort);
  } else {
    port = defaultPort;
  }

  return true;
}

template <size_t N>
constexpr std::array<std::string, N> StringArray(const char* const (&strings)[N]) {
  std::array<std::string, N> arr;
  for (size_t i = 0; i < N; ++i) {
    arr[i] = strings[i];
  }
  return arr;
}

[[nodiscard]] inline std::string EncodeURIComponent(const std::string& s) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        escaped << c;
    } else if (c == ' ') {
        escaped << '+';
    } else {
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
    }
  }

  return escaped.str();
}

[[nodiscard]] inline std::string DecodeURIComponent(const std::string& encoded) {
  std::ostringstream decoded;

  for (std::size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.size() &&
      std::isxdigit(encoded[i + 1]) && std::isxdigit(encoded[i + 2])) {
      int hexValue;
      std::istringstream hexStream(encoded.substr(i + 1, 2));
      hexStream >> std::hex >> hexValue;
      decoded << static_cast<char>(hexValue);
      i += 2;
    } else if (encoded[i] == '+') {
      decoded << ' ';
    } else {
      decoded << encoded[i];
    }
  }

  return decoded.str();
}

[[nodiscard]] inline std::string ParseFileName(const std::string& inputPath) {
  std::filesystem::path filePath = inputPath;
  return filePath.filename().string();
}

[[nodiscard]] inline std::string ParseFileExtension(const std::string& inputPath) {
  std::string fileName = ParseFileName(inputPath);
  size_t extIndex = fileName.find_last_of(".");
  if (extIndex == std::string::npos) return std::string();
  std::string extension = fileName.substr(extIndex);
  std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

[[nodiscard]] inline bool CaseInsensitiveEquals(const std::string& nameOne, const std::string& nameTwo) {
  std::string lowerOne = nameOne;
  std::string lowerTwo = nameTwo;
  std::transform(std::begin(lowerOne), std::end(lowerOne), std::begin(lowerOne), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::transform(std::begin(lowerTwo), std::end(lowerTwo), std::begin(lowerTwo), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lowerOne == lowerTwo;
}

[[nodiscard]] inline bool FileNameEquals(const std::string& nameOne, const std::string& nameTwo) {
#ifndef _WIN32
  return nameOne == nameTwo;
#else
  return CaseInsensitiveEquals(nameOne, nameTwo);
#endif
}

[[nodiscard]] inline bool HasNullOrBreak(const std::string& unsafeInput) {
  for (const auto& c : unsafeInput) {
    if (c == '\0' || c == '\n' || c == '\r' || c == '\f') {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline bool PathHasNullBytes(const std::filesystem::path& filePath) {
  for (const auto& c : filePath.native()) {
    if (c == '\0') {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline std::string PreparePatternForFuzzySearch(const std::string& rawPattern)
{
  std::string pattern = rawPattern;
  std::transform(std::begin(pattern), std::end(pattern), std::begin(pattern), [](unsigned char c) {
    if (c == static_cast<char>(0x20)) return static_cast<char>(0x2d);
    return static_cast<char>(std::tolower(c));
  });
  return RemoveNonAlphanumericNorHyphen(pattern);
}

[[nodiscard]] inline std::string PrepareMapPatternForFuzzySearch(const std::string& rawPattern)
{
  std::string pattern = rawPattern;
  std::transform(std::begin(pattern), std::end(pattern), std::begin(pattern), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::string extension = ParseFileExtension(pattern);
  if (extension == ".w3x" || extension == ".w3m" || extension == ".ini") {
    pattern = pattern.substr(0, pattern.length() - extension.length());
  }
  return RemoveNonAlphanumeric(pattern);
}

[[nodiscard]] inline std::vector<std::string> ReadChatTemplate(const std::filesystem::path& filePath) {
  std::ifstream in;
  in.open(filePath.native().c_str(), std::ios::in);
  std::vector<std::string> fileContents;
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (line.empty()) {
        if (!in.eof())
          fileContents.push_back(" ");
      } else {
        fileContents.push_back(line);
      }
    }
    in.close();
  }
  return fileContents;
}

[[nodiscard]] inline std::string GetNormalizedAlias(const std::string& alias)
{
  if (alias.empty()) return alias;

  std::string result;
  if (!utf8::is_valid(alias.begin(), alias.end())) {
    return result;
  }

  std::vector<unsigned short> utf16line;
  utf8::utf8to16(alias.begin(), alias.end(), std::back_inserter(utf16line));
  // Note that MSVC 2019 doesn't fully support unicode string literals.
  for (const auto& c : utf16line) {
    switch (c) {
      case 32: case 39: case 45: case 95: // whitespace ( ), single quote ('), hyphen (-), underscore (_)
        break;
      case 224: case 225: case 226: case 227: case 228: case 229: // à á â ã ä å
        result += 'a'; break;
      case 231: // ç
        result += 'c'; break;
      case 232: case 233: case 234: case 235: // è é ê ë
        result += 'e'; break;
      case 236: case 237: case 238: case 239: // ì í î ï
        result += 'i'; break;
      case 241: // ñ
        result += 'n'; break;
      case 242: case 243: case 244: case 245: case 246: case 248: // ò ó ô õ ö ø
        result += 'o'; break;
      case 249: case 250: case 251: case 252: // ù ú û ü
        result += 'u'; break;
      case 253: case 255: // ý ÿ
        result += 'y'; break;
      default:
        if (c <= 0x7f) {
          // Single-byte UTF-8 encoding for ASCII characters
          result += static_cast<char>(c & 0x7F);
          continue;
        } else if (c <= 0x7FF) {
          // Two-byte UTF-8 encoding
          result += static_cast<char>(0xC0 | ((c >> 6) & 0x1F));
          result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
          // Three-byte UTF-8 encoding
          result += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
          result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
          result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
  }
  return result;
}

inline void NormalizeDirectory(std::filesystem::path& filePath)
{
  if (filePath.empty()) return;
  filePath += filePath.preferred_separator;
  filePath = filePath.lexically_normal();
}

[[nodiscard]] inline bool FindNextMissingElementBack(uint8_t& element, std::vector<uint8_t> counters)
{
  if (element == 0) return false;
  do {
    --element;
  } while (counters[element] != 0 && element > 0);
  return counters[element] == 0;
}

[[nodiscard]] inline std::string ToLowerCase(const std::string& input)
{
  std::string output = input;
  std::transform(std::begin(output), std::end(output), std::begin(output), [](char c) { return static_cast<char>(std::tolower(c)); });
  return output;
}

[[nodiscard]] inline std::string ToUpperCase(const std::string& input)
{
  std::string output = input;
  std::transform(std::begin(output), std::end(output), std::begin(output), [](char c) { return static_cast<char>(std::toupper(c)); });
  return output;
}

[[nodiscard]] inline std::optional<uint32_t> ToUint32(const std::string& input)
{
  std::optional<uint32_t> container = std::nullopt;

  try {
    int64_t Value = std::stol(input);
    if (Value < 0 || 0xFFFFFFFF < Value) {
      return container;
    }
    container = static_cast<uint32_t>(Value);
  } catch (...) {}

  return container;
}

[[nodiscard]] inline std::optional<int32_t> ToInt32(const std::string& input)
{
  std::optional<int32_t> container = std::nullopt;

  try {
    long Value = std::stol(input);
    if (Value > 0xFFFFFF) {
      return container;
    }
    container = static_cast<int32_t>(Value);
  } catch (...) {}

  return container;
}

[[nodiscard]] inline std::optional<double> ToDouble(const std::string& input)
{
  std::optional<double> container = std::nullopt;

  try {
    double Value = std::stod(input);
    container = static_cast<double>(Value);
  } catch (...) {}

  return container;
}

[[nodiscard]] inline std::pair<std::string, std::string> SplitAddress(const std::string& fqName)
{
  std::string::size_type atSignPos = fqName.find('@');
  if (atSignPos == std::string::npos) {
    return make_pair(fqName, std::string());
  }
  return make_pair(
    TrimString(fqName.substr(0, atSignPos)),
    TrimString(fqName.substr(atSignPos + 1))
  );
}

[[nodiscard]] inline bool CheckTargetGameSyntax(const std::string& rawInput)
{
  if (rawInput.empty()) {
    return false;
  }
  std::string inputGame = ToLowerCase(rawInput);
  if (inputGame == "lobby" || inputGame == "game#lobby") {
    return true;
  }
  if (inputGame == "oldest" || inputGame == "game#oldest") {
    return true;
  }
  if (inputGame == "newest" || inputGame == "latest" || inputGame == "game#newest" || inputGame == "game#latest") {
    return true;
  }
  if (inputGame == "lobby#oldest") {
    return true;
  }
  if (inputGame == "lobby#newest") {
    return true;
  }
  if (inputGame.substr(0, 5) == "game#") {
    inputGame = inputGame.substr(5);
  } else if (inputGame.substr(0, 6) == "lobby#") {
    inputGame = inputGame.substr(6);
  }

  try {
    long long value = std::stoll(inputGame);
    return value >= 0;
  } catch (...) {
    return false;
  }
}

inline bool ReplaceText(std::string& input, const std::string& fragment, const std::string& replacement)
{
  std::string::size_type matchIndex = input.find(fragment);
  if (matchIndex == std::string::npos) return false;
  input.replace(matchIndex, fragment.size(), replacement);
  return true;
}

[[nodiscard]] inline std::string ReplaceTemplate(const std::string& input, const std::map<int64_t, std::function<std::string()>>& funcMap) {
  std::string result;
  size_t pos = 0;
  size_t start = 0;

  while ((start = input.find('{', pos)) != std::string::npos) {
    // Append the part before the token
    result.append(input, pos, start - pos);

    // Find the closing brace
    size_t end = input.find('}', start);
    if (end == std::string::npos) {
      // Unmatched opening brace
      result.append(input, start, input.size() - start);
      return result;
    }

    // Extract the token inside the braces
    std::string token = input.substr(start + 1, end - start - 1);

    // Check if the hash exists in the funcMap
    auto it = funcMap.find(HashCode(token));
    if (it != funcMap.end()) {
      // Replace the token with the output of the corresponding function
      result.append(it->second());
    } else {
      // If no match is found, keep the original token (or you can choose to return false)
      result.append("{").append(token).append("}");
    }

    // Move the position forward
    pos = end + 1;
  }

  // Append the rest of the string after the last token
  result.append(input, pos, std::string::npos);
  return result;
}

[[nodiscard]] inline float LinearInterpolation(const float x, const float x1, const float x2, const float y1, const float y2)
{
  float y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
  return y;
}

/*
[[nodiscard]] inline float HyperbolicInterpolation(const float x, const float x1, const float x2, const float y1, const float y2)
{
  float y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
  return y;
}

[[nodiscard]] inline float ExponentialInterpolation(const float x, const float x1, const float x2, const float y1, const float y2)
{
  float y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
  return y;
}
*/

#endif // AURA_UTIL_H_
