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

#include "bncsutil_interface.h"

#include <bncsutil/bncsutil.h>

#include <algorithm>
#include <cmath>

#include "util.h"

using namespace std;

//
// CBNCSUtilInterface
//

CBNCSUtilInterface::CBNCSUtilInterface(const string& userName, const string& userPassword)
 : m_ClientKey({}),
   m_M1({}),
   m_PvPGNPasswordHash({}),
   m_EXEVersion({}),
   m_EXEVersionHash({}),
   m_DefaultEXEVersion({173, 1, 27, 1}),
   m_DefaultEXEVersionHash({72, 160, 171, 170}),
   m_DefaultEXEInfo("war3.exe 15/03/16 00:00:00 515048")
{
  m_NLS = new NLS(userName, userPassword);
}

CBNCSUtilInterface::~CBNCSUtilInterface()
{
  delete static_cast<NLS*>(m_NLS);
}

void CBNCSUtilInterface::Reset(const string& userName, const string& userPassword)
{
  delete static_cast<NLS*>(m_NLS);
  m_NLS = new NLS(userName, userPassword);
}

optional<uint8_t> CBNCSUtilInterface::GetGameVersion(const filesystem::path& war3Path)
{
  optional<uint8_t> version;
  const filesystem::path FileStormDLL = CaseInsensitiveFileExists(war3Path, "storm.dll");
  const filesystem::path FileGameDLL  = CaseInsensitiveFileExists(war3Path, "game.dll");
  const filesystem::path WarcraftIIIExe = CaseInsensitiveFileExists(war3Path, "Warcraft III.exe");
  const filesystem::path War3Exe = CaseInsensitiveFileExists(war3Path, "war3.exe");
  if (WarcraftIIIExe.empty() && War3Exe.empty()) {
    Print("[CONFIG] Game path corrupted or invalid (" + PathToString(war3Path) + "). Executable file not found.");
    Print("[CONFIG] Config required: <game.version>, <realm_N.auth_*>");
    return version;
  }
  if (FileStormDLL.empty() != FileGameDLL.empty()) {
    if (FileStormDLL.empty()) {
      Print("[CONFIG] Game.dll found, but Storm.dll missing at " + PathToString(war3Path) + ".");
    } else {
      Print("[CONFIG] Storm.dll found, but Game.dll missing at " + PathToString(war3Path) + ".");
    }
    Print("[CONFIG] Config required: <game.version>, <realm_N.auth_*>");
    return version;
  }
  if (FileStormDLL.empty()) {
    if (WarcraftIIIExe.empty()) {
      Print("[CONFIG] Game path corrupted or invalid (" + PathToString(war3Path) + "). No game files found.");
      Print("[CONFIG] Config required: <game.version>, <realm_N.auth_*>");
      return version;
    }
  }
  if (!War3Exe.empty()) {
    if (FileStormDLL.empty()) {
      Print("[CONFIG] Game path corrupted or invalid (" + PathToString(war3Path)  + "). Storm.dll is missing.");
      Print("[CONFIG] Config required: <game.version>, <realm_N.auth_*>");
      return version;
    }
  }

  uint8_t versionMode;
  if (!War3Exe.empty()) {
    versionMode = 27;
  } else if (!FileStormDLL.empty()) {
    versionMode = 28;
  } else {
    versionMode = 29;
  }

  const filesystem::path CheckExe = versionMode >= 28 ? WarcraftIIIExe : War3Exe;
  char     buf[1024];
  uint32_t EXEVersion = 0;
  getExeInfo(PathToString(CheckExe).c_str(), buf, 1024, &EXEVersion, BNCSUTIL_PLATFORM_X86);
  uint8_t readVersion = static_cast<uint8_t>(EXEVersion >> 16);

  if (readVersion == 0) {
    Print("[CONFIG] Game path corrupted or invalid (" + PathToString(war3Path)  + ").");
    Print("[CONFIG] Game path has files from v1." + to_string(versionMode));
    Print("[CONFIG] " + PathToString(CheckExe) + " cannot read version");
    Print("[CONFIG] Config required: <game.version>, <realm_N.auth_*>");
    return version;
  }
  if ((versionMode == 28) != (readVersion == 28) || (versionMode < 28 && readVersion > 28) || (versionMode > 28 && readVersion < 28)) {
    Print("[CONFIG] Game path corrupted or invalid (" + PathToString(war3Path)  + ").");
    Print("[CONFIG] Game path has files from v1." + to_string(versionMode));
    Print("[CONFIG] " + PathToString(CheckExe) + " is v1." + to_string(readVersion));
    Print("[CONFIG] Config required: <game.version>, <realm_N.auth_*>");
    return version;
  }

  version = readVersion;
  return version;
}

bool CBNCSUtilInterface::HELP_SID_AUTH_CHECK(const filesystem::path& war3Path, const CRealmConfig* realmConfig, const string& valueStringFormula, const string& mpqFileName, const std::array<uint8_t, 4>& clientToken, const std::array<uint8_t, 4>& serverToken, const uint8_t war3Version)
{
  m_KeyInfoROC     = CreateKeyInfo(realmConfig->m_CDKeyROC, ByteArrayToUInt32(clientToken, false), ByteArrayToUInt32(serverToken, false));
  m_KeyInfoTFT     = CreateKeyInfo(realmConfig->m_CDKeyTFT, ByteArrayToUInt32(clientToken, false), ByteArrayToUInt32(serverToken, false));

  if (m_KeyInfoROC.size() != 36)
    Print("[BNCS] unable to create ROC key info - invalid ROC key");

  if (m_KeyInfoTFT.size() != 36)
    Print("[BNCS] unable to create TFT key info - invalid TFT key");

  if (realmConfig->m_AuthUseCustomVersion) {
    if (realmConfig->m_AuthExeVersion.has_value()) {
      copy_n(realmConfig->m_AuthExeVersion.value().begin(), 4, m_EXEVersion.begin());
    }
    if (realmConfig->m_AuthExeVersionHash.has_value()) {
      copy_n(realmConfig->m_AuthExeVersionHash.value().begin(), 4, m_EXEVersionHash.begin());
    }
    if (!realmConfig->m_AuthExeInfo.empty()) {
      SetEXEInfo(realmConfig->m_AuthExeInfo);
    }
    return true;
  }

  if (war3Path.empty()) {
    return false;
  }

  const filesystem::path FileWar3EXE = [&]() {
    if (war3Version >= 28)
      return CaseInsensitiveFileExists(war3Path, "Warcraft III.exe");
    else
      return CaseInsensitiveFileExists(war3Path, "war3.exe");
  }();
  const filesystem::path FileStormDLL = CaseInsensitiveFileExists(war3Path, "storm.dll");
  const filesystem::path FileGameDLL  = CaseInsensitiveFileExists(war3Path, "game.dll");

  if (!FileWar3EXE.empty() && (war3Version >= 29 || (!FileStormDLL.empty() && !FileGameDLL.empty())))
  {
    int bufferSize = 512;
    int requiredSize = 0;
    vector<char> buffer(bufferSize);

    uint32_t EXEVersion = 0;
    unsigned long EXEVersionHash = 0;

    do {
      bufferSize *= 2;
      buffer.resize(bufferSize);
      requiredSize = getExeInfo(PathToString(FileWar3EXE).c_str(), buffer.data(), bufferSize, &EXEVersion, BNCSUTIL_PLATFORM_X86);
    } while (0 < requiredSize && bufferSize < requiredSize);

    if (requiredSize == 0) {
      return false;
    }

    if (war3Version >= 29)
    {
      const char* filesArray[] = {PathToString(FileWar3EXE).c_str()};
      checkRevision(valueStringFormula.c_str(), filesArray, 1, extractMPQNumber(mpqFileName.c_str()), &EXEVersionHash);
    }
    else 
      checkRevisionFlat(valueStringFormula.c_str(), PathToString(FileWar3EXE).c_str(), PathToString(FileStormDLL).c_str(), PathToString(FileGameDLL).c_str(), extractMPQNumber(mpqFileName.c_str()), &EXEVersionHash);

    buffer.resize(requiredSize);
    m_EXEInfo        = buffer.data();
    m_EXEVersion     = CreateFixedByteArray(EXEVersion, false);
    m_EXEVersionHash = CreateFixedByteArray(int64_t(EXEVersionHash), false);

    return true;
  }
  else
  {
    if (FileWar3EXE.empty())
      Print("[BNCS] unable to open War3EXE [" + PathToString(FileWar3EXE) + "]");

    if (FileStormDLL.empty() && war3Version < 29)
      Print("[BNCS] unable to open StormDLL [" + PathToString(FileStormDLL) + "]");
    if (FileGameDLL.empty() && war3Version < 29)
      Print("[BNCS] unable to open GameDLL [" + PathToString(FileGameDLL) + "]");
  }

  return false;
}

bool CBNCSUtilInterface::HELP_SID_AUTH_ACCOUNTLOGON()
{
  // set m_ClientKey

  char buf[32];
  (static_cast<NLS*>(m_NLS))->getPublicKey(buf);
  copy_n(buf, 32, m_ClientKey.begin());
  return true;
}

bool CBNCSUtilInterface::HELP_SID_AUTH_ACCOUNTLOGONPROOF(const std::array<uint8_t, 32>& salt, const std::array<uint8_t, 32>& serverKey)
{
  // set m_M1

  char buf[20];
  (static_cast<NLS*>(m_NLS))->getClientSessionKey(buf, string(begin(salt), end(salt)).c_str(), string(begin(serverKey), end(serverKey)).c_str());
  copy_n(buf, 20, m_M1.begin());
  return true;
}

bool CBNCSUtilInterface::HELP_PvPGNPasswordHash(const string& userPassword)
{
  // set m_PvPGNPasswordHash

  char buf[20];
  hashPassword(userPassword.c_str(), buf);
  copy_n(buf, 20, m_PvPGNPasswordHash.begin());
  return true;
}

std::vector<uint8_t> CBNCSUtilInterface::CreateKeyInfo(const string& key, uint32_t clientToken, uint32_t serverToken)
{
  std::vector<uint8_t> KeyInfo;
  CDKeyDecoder         Decoder(key.c_str(), key.size());

  if (Decoder.isKeyValid())
  {
    const uint8_t Zeros[] = {0, 0, 0, 0};
    AppendByteArray(KeyInfo, CreateByteArray(static_cast<uint32_t>(key.size()), false));
    AppendByteArray(KeyInfo, CreateByteArray(Decoder.getProduct(), false));
    AppendByteArray(KeyInfo, CreateByteArray(Decoder.getVal1(), false));
    AppendByteArray(KeyInfo, CreateByteArray(Zeros, 4));
    size_t Length = Decoder.calculateHash(clientToken, serverToken);
    auto   buf    = new char[Length];
    Length        = Decoder.getHash(buf);
    AppendByteArray(KeyInfo, CreateByteArray(reinterpret_cast<uint8_t*>(buf), Length));
    delete[] buf;
  }

  return KeyInfo;
}
