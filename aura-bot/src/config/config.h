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

#ifndef AURA_CONFIG_H_
#define AURA_CONFIG_H_

#include "../file_util.h"
#include "../socket.h"
#include "../includes.h"

#define SUCCESS(T) \
    do { \
        m_ErrorLast = false; \
        return T; \
    } while(0);


#define CONFIG_ERROR(key, T) \
    do { \
        m_ErrorLast = true; \
        if (m_StrictMode) m_CriticalError = true; \
        Print(std::string("[CONFIG] Error - Invalid value provided for <") + key + std::string(">.")); \
        return T; \
    } while(0);


#define END(T) \
    do { \
        if (errored) Print(std::string("[CONFIG] Error - Invalid value provided for <") + key + std::string(">.")); \
        m_ErrorLast = errored; \
        if (errored && m_StrictMode) m_CriticalError = true; \
        return T; \
    } while(0);

//
// CConfig
//

class CConfig
{
private:
  bool                               m_ErrorLast;
  bool                               m_CriticalError;
  bool                               m_StrictMode;
  std::map<std::string, std::string> m_CFG;
  std::filesystem::path              m_File;
  std::set<std::string>              m_ValidKeys;
  std::filesystem::path              m_HomeDir;
  bool                               m_IsModified;

  uint8_t CheckRealmKey(const std::string& key) const;

public:
  CConfig();
  ~CConfig();

  [[nodiscard]] bool Read(const std::filesystem::path& file, CConfig* adapterConfig = nullptr);
  [[nodiscard]] bool Exists(const std::string& key);
  void Accept(const std::string& key);
  void Delete(const std::string& key);
  [[nodiscard]] std::vector<std::string> GetInvalidKeys(const std::bitset<120> definedRealms) const;
  [[nodiscard]] inline std::filesystem::path GetFile() const { return m_File; };
  [[nodiscard]] inline bool GetErrorLast() const { return m_ErrorLast; };
  [[nodiscard]] inline bool GetSuccess() const { return !m_CriticalError; };
  [[nodiscard]] inline bool GetStrictMode() const { return m_StrictMode; };
  inline void FailIfErrorLast() {
    if (m_ErrorLast) m_CriticalError = true;
  };
  inline void SetFailed() { m_CriticalError = true; };
  inline void SetStrictMode(const bool nStrictMode) { m_StrictMode = nStrictMode; }
  inline void SetHomeDir(const std::filesystem::path& nHomeDir) { m_HomeDir = nHomeDir; };
  inline void SetIsModified() { m_IsModified = true; };
  [[nodiscard]] const std::filesystem::path& GetHomeDir() const { return m_HomeDir; }
  [[nodiscard]] inline bool GetIsModified() const { return m_IsModified; };
  [[nodiscard]] inline const std::map<std::string, std::string>& GetEntries() const { return m_CFG; }

  [[nodiscard]] std::string GetString(const std::string& key);
  [[nodiscard]] std::string GetString(const std::string& key, const std::string& x);
  [[nodiscard]] std::string GetString(const std::string& key, const uint32_t minLength, const uint32_t maxLength, const std::string& x);

  [[nodiscard]] bool GetBool(const std::string& key, bool x);

  [[nodiscard]] int32_t GetInt(const std::string& key, int32_t x);
  [[nodiscard]] int32_t GetInt32(const std::string& key, int32_t x);
  [[nodiscard]] int64_t GetInt64(const std::string& key, int64_t x);
  [[nodiscard]] uint32_t GetUint32(const std::string& key, uint32_t x);
  [[nodiscard]] uint16_t GetUint16(const std::string& key, uint16_t x);
  [[nodiscard]] uint8_t GetUint8(const std::string& key, uint8_t x);

  [[nodiscard]] float GetFloat(const std::string& key, float x);
  [[nodiscard]] uint8_t GetStringIndex(const std::string& key, const std::vector<std::string>& fromList, const uint8_t x);

  template <typename EnumType, size_t N>
  [[nodiscard]] EnumType GetEnum(const std::string& key, const std::array<std::string, N>& fromList, EnumType x)
  {
    static_assert(std::is_enum<EnumType>::value, "EnumType must be an enum type");

    constexpr uint8_t enumCount = static_cast<uint8_t>(EnumType::LAST);
    static_assert(enumCount == static_cast<uint8_t>(N), "fromList size must match the number of enum values");

    m_ValidKeys.insert(key);

    auto it = m_CFG.find(key);
    if (it == m_CFG.end()) {
      SUCCESS(x)
    }

    for (uint8_t i = 0; i < N; ++i) {
      if (it->second == fromList[i]) {
        SUCCESS(static_cast<EnumType>(i))
      }
    }

    CONFIG_ERROR(key, x)
  }

  [[nodiscard]] std::vector<std::string> GetList(const std::string& key, char separator, const std::vector<std::string> x);
  [[nodiscard]] std::set<std::string> GetSet(const std::string& key, char separator, const std::set<std::string> x);
  [[nodiscard]] std::set<std::string> GetSetInsensitive(const std::string& key, char separator, const std::set<std::string> x);
  [[nodiscard]] std::set<uint64_t> GetUint64Set(const std::string& key, char separator, const std::set<uint64_t> x);

  [[nodiscard]] std::vector<uint8_t> GetUint8Vector(const std::string& key, const uint32_t count);
  [[nodiscard]] std::set<uint8_t> GetUint8Set(const std::string& key, char separator);
  [[nodiscard]] std::vector<uint8_t> GetIPv4(const std::string& key, const std::array<uint8_t, 4>& x);
  [[nodiscard]] std::set<std::string> GetIPStringSet(const std::string& key, char separator, const std::set<std::string> x);
  [[nodiscard]] std::vector<sockaddr_storage> GetHostListWithImplicitPort(const std::string& key, const uint16_t defaultPort, char separator);

  [[nodiscard]] std::filesystem::path GetPath(const std::string &key, const std::filesystem::path &x);
  [[nodiscard]] std::filesystem::path GetDirectory(const std::string &key, const std::filesystem::path &x);
  [[nodiscard]] sockaddr_storage GetAddressOfType(const std::string &key, const uint8_t acceptMode, const std::string& x);
  [[nodiscard]] sockaddr_storage GetAddressIPv4(const std::string &key, const std::string& x);
  [[nodiscard]] sockaddr_storage GetAddressIPv6(const std::string &key, const std::string& x);
  [[nodiscard]] sockaddr_storage GetAddress(const std::string &key, const std::string& x);
  [[nodiscard]] std::vector<sockaddr_storage> GetAddressList(const std::string& key, char separator, const std::vector<std::string> x);

  [[nodiscard]] std::optional<bool> GetMaybeBool(const std::string& key);
  [[nodiscard]] std::optional<uint8_t> GetMaybeUint8(const std::string& key);
  [[nodiscard]] std::optional<uint16_t> GetMaybeUint16(const std::string& key);
  [[nodiscard]] std::optional<uint32_t> GetMaybeUint32(const std::string& key);
  [[nodiscard]] std::optional<int64_t> GetMaybeInt64(const std::string& key);
  [[nodiscard]] std::optional<uint64_t> GetMaybeUint64(const std::string& key);
  [[nodiscard]] std::optional<sockaddr_storage> GetMaybeAddressOfType(const std::string& key, const uint8_t acceptMode);
  [[nodiscard]] std::optional<sockaddr_storage> GetMaybeAddressIPv4(const std::string& key);
  [[nodiscard]] std::optional<sockaddr_storage> GetMaybeAddressIPv6(const std::string& key);
  [[nodiscard]] std::optional<sockaddr_storage> GetMaybeAddress(const std::string& key);
  [[nodiscard]] std::optional<std::vector<uint8_t>> GetMaybeUint8Vector(const std::string& key, const uint32_t count);
  [[nodiscard]] std::optional<std::vector<uint8_t>> GetMaybeIPv4(const std::string& key);
  [[nodiscard]] std::optional<std::filesystem::path> GetMaybePath(const std::string &key);
  [[nodiscard]] std::optional<std::filesystem::path> GetMaybeDirectory(const std::string &key);

  void Set(const std::string& key, const std::string& x);
  void SetString(const std::string& key, const std::string& x);
  void SetString(const std::string& key, const std::vector<uint8_t>& x);
  void SetBool(const std::string& key, const bool& x);
  void SetInt32(const std::string& key, const int32_t& x);
  void SetInt64(const std::string& key, const int64_t& x);
  void SetUint32(const std::string& key, const uint32_t& x);
  void SetUint16(const std::string& key, const uint16_t& x);
  void SetUint8(const std::string& key, const uint8_t& x);
  void SetFloat(const std::string& key, const float& x);
  void SetUint8Vector(const std::string& key, const std::vector<uint8_t>& x);
  void SetUint8Array(const std::string& key, const uint8_t* start, const size_t end);
  [[nodiscard]] std::vector<uint8_t> Export() const;

  [[nodiscard]] static std::string ReadString(const std::filesystem::path& file, const std::string& key);
};

#undef SUCCESS
#undef CONFIG_ERROR
#undef END

#endif // AURA_CONFIG_H_
