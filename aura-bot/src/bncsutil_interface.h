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

#ifndef AURA_BNCSUTILINTERFACE_H_
#define AURA_BNCSUTILINTERFACE_H_

#include "file_util.h"
#include "config/config_realm.h"

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

//
// CBNCSUtilInterface
//

class CBNCSUtilInterface
{
private:
  std::array<uint8_t, 32>       m_ClientKey;                // set in HELP_SID_AUTH_ACCOUNTLOGON
  std::array<uint8_t, 20>       m_M1;                       // set in HELP_SID_AUTH_ACCOUNTLOGONPROOF
  std::array<uint8_t, 20>       m_PvPGNPasswordHash;        // set in HELP_PvPGNPasswordHash
  std::array<uint8_t, 4>        m_EXEVersion;               // set in HELP_SID_AUTH_CHECK
  std::array<uint8_t, 4>        m_EXEVersionHash;           // set in HELP_SID_AUTH_CHECK
  std::array<uint8_t, 4>        m_DefaultEXEVersion;        // set in initialization
  std::array<uint8_t, 4>        m_DefaultEXEVersionHash;    // set in initialization
  void*                         m_NLS;
  std::vector<uint8_t>          m_KeyInfoROC;               // set in HELP_SID_AUTH_CHECK
  std::vector<uint8_t>          m_KeyInfoTFT;               // set in HELP_SID_AUTH_CHECK
  std::string                   m_EXEInfo;                  // set in HELP_SID_AUTH_CHECK
  std::string                   m_DefaultEXEInfo;           // set in initialization

public:
  CBNCSUtilInterface(const std::string& userName, const std::string& userPassword);
  ~CBNCSUtilInterface();
  CBNCSUtilInterface(CBNCSUtilInterface&) = delete;

  inline const std::array<uint8_t, 4>&    GetEXEVersion() const { return m_EXEVersion; }
  inline const std::array<uint8_t, 4>&    GetEXEVersionHash() const { return m_EXEVersionHash; }
  inline const std::string&               GetEXEInfo() const { return m_EXEInfo; }
  inline const std::array<uint8_t, 4>&    GetDefaultEXEVersion() const { return m_DefaultEXEVersion; }
  inline const std::array<uint8_t, 4>&    GetDefaultEXEVersionHash() const { return m_DefaultEXEVersionHash; }
  inline const std::string&               GetDefaultEXEInfo() const { return m_DefaultEXEInfo; }
  inline const std::vector<uint8_t>&      GetKeyInfoROC() const { return m_KeyInfoROC; }
  inline const std::vector<uint8_t>&      GetKeyInfoTFT() const { return m_KeyInfoTFT; }
  inline const std::array<uint8_t, 32>&   GetClientKey() const { return m_ClientKey; }
  inline const std::array<uint8_t, 20>&   GetM1() const { return m_M1; }
  inline const std::array<uint8_t, 20>&   GetPvPGNPasswordHash() const { return m_PvPGNPasswordHash; }
  inline bool                             CheckValidEXEInfo() const { return !m_EXEInfo.empty(); }

  inline void                             SetEXEInfo(const std::string& nEXEInfo) { m_EXEInfo = nEXEInfo; }

  void                                    Reset(const std::string& userName, const std::string& userPassword);

  bool                                    HELP_SID_AUTH_CHECK(const std::filesystem::path& war3Path, const CRealmConfig* realmConfig, const std::string& valueStringFormula, const std::string& mpqFileName, const std::array<uint8_t, 4>& clientToken, const std::array<uint8_t, 4>& serverToken, const uint8_t war3Version);
  bool                                    HELP_SID_AUTH_ACCOUNTLOGON();
  bool                                    HELP_SID_AUTH_ACCOUNTLOGONPROOF(const std::array<uint8_t, 32>& salt, const std::array<uint8_t, 32>& serverKey);
  bool                                    HELP_PvPGNPasswordHash(const std::string& userPassword);

  static                                  std::optional<uint8_t> GetGameVersion(const std::filesystem::path& war3Path);

private:
  std::vector<uint8_t>                    CreateKeyInfo(const std::string& key, uint32_t clientToken, uint32_t serverToken);
};

#endif // AURA_BNCSUTILINTERFACE_H_
