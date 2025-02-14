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

#include "aura.h"
#include "util.h"
#include "file_util.h"
#include "os_util.h"
#include "bncsutil_interface.h"
#include <crc32/crc32.h>
#include <sha1/sha1.h>
#include "auradb.h"
#include <csvparser/csvparser.h>
#include "config/config.h"
#include "config/config_bot.h"
#include "config/config_realm.h"
#include "config/config_game.h"
#include "config/config_irc.h"
#include "socket.h"
#include "connection.h"
#include "realm.h"
#include "map.h"
#include "game_seeker.h"
#include "game_user.h"
#include "protocol/game_protocol.h"
#include "protocol/gps_protocol.h"
#include "game.h"
#include "cli.h"
#include "irc.h"
#include "protocol/vlan_protocol.h"
#include <utf8/utf8.h>

#include <csignal>
#include <cstdlib>
#include <thread>
#include <cassert>
#include <fstream>
#include <algorithm>
#include <string>
#include <bitset>
#include <iterator>
#include <exception>
#include <system_error>
#include <locale>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <ws2tcpip.h>
#include <winsock2.h>
#include <process.h>
#endif

using namespace std;

#undef FD_SETSIZE
#define FD_SETSIZE 512

bool                  gRestart     = false;
volatile sig_atomic_t gGracefulExit = 0;

#ifdef _WIN32
static wchar_t* auraHome = nullptr;
static wchar_t* war3Home = nullptr;
#endif

inline void GetAuraHome(const CCLI& cliApp, filesystem::path& homeDir)
{
  if (cliApp.m_HomePath.has_value()) {
    homeDir = cliApp.m_HomePath.value();
    return;
  }
#ifdef _WIN32
  size_t valueSize;
  errno_t err = _wdupenv_s(&auraHome, &valueSize, L"AURA_HOME");
  if (!err && auraHome != nullptr) {
    wstring homeDirString = auraHome;
#else
  const char* envValue = getenv("AURA_HOME");
  if (envValue != nullptr) {
    string homeDirString = envValue;
#endif
    homeDir = filesystem::path(homeDirString);
    NormalizeDirectory(homeDir);
    return;
  }
  if (cliApp.m_CFGPath.has_value()) {
    homeDir = cliApp.m_CFGPath.value().parent_path();
    NormalizeDirectory(homeDir);
    return;
  }

  homeDir = GetExeDirectory();
}

inline filesystem::path GetConfigPath(const CCLI& cliApp, const filesystem::path& homeDir)
{
  if (!cliApp.m_CFGPath.has_value()) return homeDir / "config.ini";
  if (!cliApp.m_UseStandardPaths && (cliApp.m_CFGPath.value() == cliApp.m_CFGPath.value().filename())) {
    return homeDir / cliApp.m_CFGPath.value();
  } else {
    return cliApp.m_CFGPath.value();
  }
}

inline filesystem::path GetConfigAdapterPath(const CCLI& cliApp, const filesystem::path& homeDir)
{
  if (!cliApp.m_CFGAdapterPath.has_value()) return homeDir / "legacy-config-adapter.ini";
  if (!cliApp.m_UseStandardPaths && (cliApp.m_CFGAdapterPath.value() == cliApp.m_CFGAdapterPath.value().filename())) {
    return homeDir / cliApp.m_CFGAdapterPath.value();
  } else {
    return cliApp.m_CFGAdapterPath.value();
  }
}

inline bool LoadConfig(CConfig& CFG, CCLI& cliApp, const filesystem::path& homeDir)
{
  const filesystem::path configPath = GetConfigPath(cliApp, homeDir);
  const bool isCustomConfigFile = cliApp.m_CFGPath.has_value();
  bool isDirectSuccess = false;

  // Config adapter is a back-compat mechanism
  if (cliApp.m_CFGAdapterPath.has_value()) {
    CConfig configAdapter;
    const filesystem::path configAdapterPath = GetConfigAdapterPath(cliApp, homeDir);
    const bool adapterSuccess = configAdapter.Read(configAdapterPath);
    if (!adapterSuccess) {
      filesystem::path cwd;
      try {
        cwd = filesystem::current_path();
      } catch (...) {}
      NormalizeDirectory(cwd);

      Print("[AURA] required config adapter file not found [" + PathToString(configAdapterPath) + "]");
      if (!cliApp.m_UseStandardPaths && configAdapterPath.parent_path() == homeDir.parent_path() && (cwd.empty() || homeDir.parent_path() != cwd.parent_path())) {
        Print("[HINT] --config-adapter was resolved relative to [" + PathToAbsoluteString(homeDir) + "]");
        Print("[HINT] use --stdpaths to read [" + PathToString(cwd / configAdapterPath.filename()) + "]");
      }
      return false;
    }
    isDirectSuccess = CFG.Read(configPath, &configAdapter);
  } else {
    isDirectSuccess = CFG.Read(configPath, nullptr);
  }

  if (!isDirectSuccess && isCustomConfigFile) {
    filesystem::path cwd;
    try {
      cwd = filesystem::current_path();
    } catch (...) {}
    NormalizeDirectory(cwd);
    Print("[AURA] required config file not found [" + PathToString(configPath) + "]");
    if (!cliApp.m_UseStandardPaths && configPath.parent_path() == homeDir.parent_path() && (cwd.empty() || homeDir.parent_path() != cwd.parent_path())) {
      Print("[HINT] --config was resolved relative to [" + PathToAbsoluteString(homeDir) + "]");
      Print("[HINT] use --stdpaths to read [" + PathToString(cwd / configPath.filename()) + "]");
    }
#ifdef _WIN32
    Print("[HINT] using --config=<FILE> is not recommended, prefer --homedir=<DIR>, or setting %AURA_HOME% instead");
#else
    Print("[HINT] using --config=<FILE> is not recommended, prefer --homedir=<DIR>, or setting $AURA_HOME instead");
#endif
    Print("[HINT] both alternatives auto-initialize \"config.ini\" from \"config-example.ini\" in the same folder");
    return false;
  }
  const bool homePathMatchRequired = CFG.GetBool("bot.home_path.allow_mismatch", false);
  if (isCustomConfigFile) {
    bool pathsMatch = configPath.parent_path() == homeDir.parent_path();
    if (!pathsMatch) {
      try {
        pathsMatch = filesystem::absolute(configPath.parent_path()) == filesystem::absolute(homeDir.parent_path());
      } catch (...) {}
    }
    if (homePathMatchRequired && !pathsMatch) {
      Print("[AURA] error - config file is not located within home dir [" + PathToString(homeDir) + "] - this is not recommended");
      Print("[HINT] to skip this check and execute Aura nevertheless, set <bot.home_path.allow_mismatch = yes> in your config file");
      Print("[HINT] paths in your config file [" + PathToString(configPath) + "] will be resolved relative to the home dir");
      return false;
    } else if (cliApp.m_HomePath.has_value()) {
      Print("[AURA] using --homedir=" + PathToString(homeDir));
    } else {
#ifdef _WIN32
      Print("[AURA] using %AURA_HOME%=" + PathToString(homeDir));
#else
      Print("[AURA] using $AURA_HOME=" + PathToString(homeDir));
#endif
    }
  }

  if (isDirectSuccess) {
    CFG.SetHomeDir(move(homeDir));

    if (cliApp.m_CFGAdapterPath.has_value()) {
      const string baseMigratedFileName = "config-migrated";
      const vector<uint8_t> migratedBytes = CFG.Export();
      filesystem::path migratedPath;
      uint32_t loopCounter = 0;
      do {
        if ((0 < loopCounter) && (loopCounter % 100 == 0)) {
          // Just so it doesn't look like Aura hung up.
          Print("[AURA] destination file [" + PathToString(migratedPath) + "] already exists");
        }
        string migratedFileName;
        if (loopCounter > 0) {
          migratedFileName = migratedFileName + "." + to_string(loopCounter) + ".ini";
        } else {
          migratedFileName = baseMigratedFileName + ".ini";
        }
        migratedPath = configPath.parent_path() / filesystem::path(migratedFileName);
        ++loopCounter;
      } while (migratedPath == configPath || FileExists(migratedPath));

      Print("[AURA] exporting updated configuration to [" + PathToString(migratedPath) + "]...");
      if (!FileWrite(migratedPath, migratedBytes.data(), migratedBytes.size())) {
        Print("[AURA] error exporting configuration file");
        return false;
      }
      Print("[AURA] configuration exported OK");
      Print("[AURA] before starting Aura again, please check the contents of the exported file, and rename it");
      Print("[AURA] see the CONFIG.md file for up-to-date documentation on supported config keys, and their accepted values");
      return true;
    }

    return true;
  }

  const filesystem::path configExamplePath = homeDir / filesystem::path("config-example.ini");

  vector<uint8_t> exampleContents;
  if (!FileRead(configExamplePath, exampleContents, MAX_READ_FILE_SIZE) || exampleContents.empty()) {
    // But Aura can actually work without a config file ;)
    Print("[AURA] config.ini, config-example.ini not found within home dir [" + PathToString(homeDir) + "].");
    Print("[AURA] using automatic configuration");
  } else {
    Print("[AURA] copying config-example.ini to config.ini...");
    FileWrite(configPath, reinterpret_cast<const uint8_t*>(exampleContents.data()), exampleContents.size());
    if (!CFG.Read(configPath)) {
      Print("[AURA] error initializing config.ini");
      return false;
    }
  }

  CFG.SetHomeDir(move(homeDir));
  return true;
}

inline PLATFORM_STRING_TYPE GetAuraTitle(CGame* detailsGame, size_t lobbyCount, size_t gameCount, bool hasRehost)
{
  const static PLATFORM_STRING_TYPE HyphenConnector = PLATFORM_STRING(" - ");
  const static PLATFORM_STRING_TYPE DetailsLobbyPrefix = PLATFORM_STRING(" - Lobby: ");
  const static PLATFORM_STRING_TYPE DetailsGamePrefix = PLATFORM_STRING(" - Playing: ");
  const static PLATFORM_STRING_TYPE SingleLobbySuffix = PLATFORM_STRING(" hosted lobby");
  const static PLATFORM_STRING_TYPE PluralLobbySuffix = PLATFORM_STRING(" hosted lobbies");
  const static PLATFORM_STRING_TYPE SingleGameSuffix = PLATFORM_STRING(" hosted game");
  const static PLATFORM_STRING_TYPE PluralGameSuffix = PLATFORM_STRING(" hosted games");
  const static PLATFORM_STRING_TYPE IdleSuffix = PLATFORM_STRING(" - Idle");
  const static PLATFORM_STRING_TYPE RehostingSuffix = PLATFORM_STRING(" | Auto-rehosting");
  const static PLATFORM_STRING_TYPE EmptyString = PLATFORM_STRING("");
  const bool showDetails = detailsGame != nullptr;

  PLATFORM_STRING_TYPE titleText = PLATFORM_STRING(AURA_APP_NAME);

  if (showDetails) {
    string detailsText = detailsGame->GetStatusDescription();
#ifdef _WIN32
    PLATFORM_STRING_TYPE detailsTextPlatform;
    if (utf8::is_valid(detailsText.begin(), detailsText.end())) {
      utf8::utf8to16(detailsText.begin(), detailsText.end(), back_inserter(detailsTextPlatform));
    }
#else
    PLATFORM_STRING_TYPE& detailsTextPlatform = detailsText;
#endif
    titleText += (lobbyCount == 1 ? DetailsLobbyPrefix : DetailsGamePrefix) + detailsTextPlatform;
  } else if (lobbyCount == 0 && gameCount == 0) {
    titleText += IdleSuffix;
  } else if (lobbyCount > 0 && gameCount > 0) {
    titleText += (
      HyphenConnector +
      ToDecStringCPlatform(lobbyCount) + (lobbyCount > 1 ? PluralLobbySuffix : SingleLobbySuffix) +
      HyphenConnector +
      ToDecStringCPlatform(gameCount) + (gameCount > 1 ? PluralGameSuffix : SingleGameSuffix)
    );
  } else if (lobbyCount > 0) {
    titleText += HyphenConnector + ToDecStringCPlatform(lobbyCount) + (lobbyCount > 1 ? PluralLobbySuffix : SingleLobbySuffix);
  } else {
    titleText += HyphenConnector + ToDecStringCPlatform(gameCount) + (gameCount > 1 ? PluralGameSuffix : SingleGameSuffix);
  }
  
  return titleText + (hasRehost ? RehostingSuffix : EmptyString);
}

//
// main
//

int main(const int argc, char** argv)
{
  int exitCode = 0;

  // seed the PRNG
  srand(static_cast<uint32_t>(time(nullptr)));

  // disable sync since we don't use cstdio anyway
  ios_base::sync_with_stdio(false);

#ifdef _WIN32
  // print UTF-8 to the console
  SetConsoleOutputCP(CP_UTF8);
#endif

  signal(SIGINT, [](int32_t) -> void {
    if (gGracefulExit == 1) {
      Print("[!!!] caught signal SIGINT, exiting NOW");
      exit(1);
    } else {
      Print("[!!!] caught signal SIGINT, exiting gracefully...");
      gGracefulExit = 1;
    }
  });

#ifndef _WIN32
  // disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

  signal(SIGPIPE, SIG_IGN);
#endif

#ifdef _WIN32
  // initialize winsock

  WSADATA wsadata;

  if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
    Print("[AURA] error starting winsock");
    return 1;
  }

  // increase process priority
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

  if (htons(0xe017) == 0xe017) {
    Print("[AURA] warning - big endian system support is experimental");
  }

  // extra scopes for tracking lifetimes
  {
    optional<CAura> gAura;
    {
      CCLI cliApp;
      CLIResult cliResult = cliApp.Parse(argc, argv);
      switch (cliResult) {
        case CLIResult::kInfoAndQuit:
          cliApp.RunInfoActions();
          exitCode = 0;
          break;
        case CLIResult::kError:
          Print("[AURA] invalid CLI usage - please see CLI.md");
          exitCode = 1;
          break;
        case CLIResult::kOk:
        case CLIResult::kConfigAndQuit: {
          CConfig CFG;
          filesystem::path homeDir;
          GetAuraHome(cliApp, homeDir);
          if (!LoadConfig(CFG, cliApp, homeDir)) {
            Print("[AURA] error loading configuration");
            exitCode = 1;
            break;
          }
          if (cliResult == CLIResult::kConfigAndQuit) {
            exitCode = 0;
            break;
          }
          // initialize aura
          gAura.emplace(CFG, cliApp);
          if (!gAura->GetReady()) {
            exitCode = 1;
            Print("[AURA] initialization failure");
          }
        }
      }
    }

    if (gAura.has_value() && gAura->GetReady()) {
      // loop start

      while (!gAura->Update())
        ;

      gAura->AwaitSettled();

      // loop end - shut down
      Print("[AURA] shutting down");
    }
  }


#ifdef _WIN32
  // shutdown winsock

  WSACleanup();
#endif

#ifdef _WIN32
  free(auraHome);
  free(war3Home);
#endif

  // restart the program

  if (gRestart)
  {
#ifdef _WIN32
    _spawnl(_P_OVERLAY, argv[0], argv[0], nullptr);
#else
    execl(argv[0], argv[0], nullptr);
#endif
  }

  return exitCode;
}

//
// CAura
//

CAura::CAura(CConfig& CFG, const CCLI& nCLI)
  : m_ScriptsExtracted(false),
    m_Exiting(false),
    m_ExitingSoon(false),
    m_Ready(true),
    m_AutoReHosted(false),

    m_LogLevel(LOG_LEVEL_DEBUG),
    m_GameVersion(0u),
    m_MaxSlots(MAX_SLOTS_LEGACY),

    m_LastServerID(0xFu),
    m_HostCounter(0u),
    m_ReplacingLobbiesCounter(0u),
    m_HistoryGameID(0u),
    m_MaxGameNameSize(31u),

    m_RealmDefaultConfig(nullptr),
    m_GameDefaultConfig(nullptr),
    m_CommandDefaultConfig(new CCommandConfig()),

    m_DB(new CAuraDB(CFG)),
    //m_GameSetup(nullptr),
    //m_AutoRehostGameSetup(nullptr),

    m_ReloadContext(nullptr),
    m_SudoContext(nullptr),

    m_Version(AURA_VERSION),
    m_RepositoryURL(AURA_REPOSITORY_URL),
    m_IssuesURL(AURA_ISSUES_URL),

    m_Discord(CDiscord(CFG)),
    m_IRC(CIRC(CFG)),
    m_Net(CNet(CFG)),
    m_Config(CBotConfig(CFG)),
    m_ConfigPath(CFG.GetFile())
{
  m_Discord.m_Aura = this;
  m_IRC.m_Aura = this;
  m_Net.m_Aura = this;

  Print("[AURA] Aura version " + m_Version);

  if (m_DB->HasError()) {
    Print("[CONFIG] Error: Critical errors found in [" + PathToString(m_DB->GetFile()) + "]: " + m_DB->GetError());
    m_Ready = false;
    return;
  }
  m_HistoryGameID = m_DB->GetLatestHistoryGameId();

  CRC32::Initialize();

  if (!CFG.GetSuccess() || !LoadDefaultConfigs(CFG, &m_Net.m_Config)) {
    Print("[CONFIG] Error: Critical errors found in " + PathToString(m_ConfigPath.filename()));
    m_Ready = false;
    return;
  }
  nCLI.OverrideConfig(this);
  OnLoadConfigs();

  // Eagerly install as shell extension.
  if (m_DB->GetIsFirstRun()) {
    LoadMapAliases();
    LoadIPToCountryData(CFG);
    if (nCLI.GetInitSystem().value_or(true)) {
      InitSystem();
    }
  } else if (nCLI.GetInitSystem().value_or(false)) {
    InitSystem();
  }

  if (m_GameVersion == 0) {
    Print("[CONFIG] Game version and path are missing.");
    m_Ready = false;
    return;
  }
  Print("[AURA] running game version 1." + to_string(m_GameVersion));

  if (!m_Net.Init()) {
    Print("[AURA] error - close active instances of Warcraft, and/or pause LANViewer to initialize Aura.");
    m_Ready = false;
    return;
  }

  if (m_Net.m_Config.m_UDPEnableCustomPortTCP4) {
    Print("[AURA] broadcasting games port " + to_string(m_Net.m_Config.m_UDPCustomPortTCP4) + " over LAN");
  }

  m_RealmsIdentifiers.resize(16);
  if (m_Config.m_EnableBNET.has_value()) {
    if (m_Config.m_EnableBNET.value()) {
      Print("[AURA] all realms forcibly set to ENABLED <bot.toggle_every_realm = on>");
    } else {
      Print("[AURA] all realms forcibly set to DISABLED <bot.toggle_every_realm = off>");
    }
  }
  bitset<120> definedRealms;
  if (m_Config.m_EnableBNET.value_or(true)) {
    LoadBNETs(CFG, definedRealms);
  }

  try {
    filesystem::create_directory(m_Config.m_MapPath);
  } catch (...) {
    Print("[AURA] warning - <bot.maps_path> is not a valid directory");
  }

  try {
    filesystem::create_directory(m_Config.m_MapCFGPath);
  } catch (...) {
    Print("[AURA] warning - <bot.map.configs_path> is not a valid directory");
  }

  try {
    filesystem::create_directory(m_Config.m_MapCachePath);
  } catch (...) {
    Print("[AURA] warning - <bot.map.cache_path> is not a valid directory");
  }

  try {
    filesystem::create_directory(m_Config.m_JASSPath);
  } catch (...) {
    Print("[AURA] warning - <bot.jass_path> is not a valid directory");
  }

  if (m_Config.m_ExtractJASS) {
    // extract common.j and blizzard.j from War3Patch.mpq or War3.mpq (depending on version) if we can
    // these two files are necessary for calculating <map.weak_hash>, and <map.sha1> when loading maps so we make sure they are available
    // see CMap :: Load for more information
    m_ScriptsExtracted = ExtractScripts() == 2;
    if (!m_ScriptsExtracted) {
      if (!CopyScripts()) {
        m_Ready = false;
        return;
      }
    }
  }

  if (m_Config.m_EnableCFGCache) {
    UpdateCFGCacheEntries();
  }

  if (!nCLI.QueueActions(this)) {
    m_Ready = false;
    return;
  }

  vector<string> invalidKeys = CFG.GetInvalidKeys(definedRealms);
  if (!invalidKeys.empty()) {
    Print("[CONFIG] warning - some keys are misnamed: " + JoinVector(invalidKeys, false));
  }

  if (m_Realms.empty() && m_Config.m_EnableBNET.value_or(true))
    Print("[AURA] notice - no enabled battle.net connections configured");
  if (!m_IRC.GetIsEnabled())
    Print("[AURA] notice - no irc connection configured");
  if (!m_Discord.GetIsEnabled())
    Print("[AURA] notice - no discord connection configured");

  if (m_Realms.empty() && !m_IRC.GetIsEnabled() && !m_Discord.GetIsEnabled() && m_PendingActions.empty()) {
    Print("[AURA] error - no inputs connected");
    m_Ready = false;
    return;
  }

  UpdateMetaData();
}

bool CAura::LoadBNETs(CConfig& CFG, bitset<120>& definedRealms)
{
  // load the battle.net connections
  // we're just loading the config data and creating the CRealm classes here, the connections are established later (in the Update function)

  bool isInvalidConfig = false;
  map<string, uint8_t> uniqueInputIds;
  map<string, uint8_t> uniqueNames;
  vector<CRealmConfig*> realmConfigs(120, nullptr);
  const bool hasGlobalHostName = CFG.Exists("realm_global.host_name");
  for (uint8_t i = 1; i <= 120; ++i) {
    if (!hasGlobalHostName && !CFG.Exists("realm_" + to_string(i) + ".host_name")) {
      continue;
    }
    CRealmConfig* ThisConfig = new CRealmConfig(CFG, m_RealmDefaultConfig, i);
    if (m_Config.m_EnableBNET.has_value()) {
      ThisConfig->m_Enabled = m_Config.m_EnableBNET.value();
    }
    if (ThisConfig->m_UserName.empty() || ThisConfig->m_PassWord.empty()) {
      ThisConfig->m_Enabled = false;
    }
    if (!ThisConfig->m_Enabled) {
      delete ThisConfig;
    } else if (uniqueNames.find(ThisConfig->m_UniqueName) != uniqueNames.end()) {
      Print("[CONFIG] <realm_" + to_string(uniqueNames.at(ThisConfig->m_UniqueName) + 1) + ".unique_name> must be different from <realm_" + to_string(i) + ".unique_name>");
      isInvalidConfig = true;
      delete ThisConfig;
    } else if (uniqueInputIds.find(ThisConfig->m_InputID) != uniqueInputIds.end()) {
      Print("[CONFIG] <realm_" + to_string(uniqueNames.at(ThisConfig->m_UniqueName) + 1) + ".input_id> must be different from <realm_" + to_string(i) + ".input_id>");
      isInvalidConfig = true;
      delete ThisConfig;
    } else {
      uniqueNames[ThisConfig->m_UniqueName] = i - 1;
      uniqueInputIds[ThisConfig->m_InputID] = i - 1;
      realmConfigs[i - 1] = ThisConfig;
      definedRealms.set(i - 1);
    }
  }

  if (isInvalidConfig) {
    for (auto& realmConfig : realmConfigs) {
      delete realmConfig;
    }
    return false;
  }

  m_RealmsByHostCounter.clear();
  uint8_t i = static_cast<uint8_t>(m_Realms.size());
  while (i--) {
    string inputID = m_Realms[i]->GetInputID();
    if (uniqueInputIds.find(inputID) == uniqueInputIds.end()) {
      delete m_Realms[i];
      m_RealmsByInputID.erase(inputID);
      m_Realms.erase(m_Realms.begin() + i);
    }
  }

  size_t longestGamePrefixSize = 0;
  for (const auto& entry : uniqueInputIds) {
    CRealm* matchingRealm = GetRealmByInputId(entry.first);
    CRealmConfig* realmConfig = realmConfigs[entry.second];
    if (matchingRealm == nullptr) {
      matchingRealm = new CRealm(this, realmConfig);
      m_Realms.push_back(matchingRealm);
      m_RealmsByInputID[entry.first] = matchingRealm;
      m_RealmsIdentifiers.push_back(entry.first);
      // m_RealmsIdentifiers[matchingRealm->GetInternalID()] == matchingRealm->GetInputID();
      if (MatchLogLevel(LOG_LEVEL_DEBUG)) {
        Print("[AURA] server found: " + matchingRealm->GetUniqueDisplayName());
      }
    } else {
      const bool DoResetConnection = (
        matchingRealm->GetServer() != realmConfig->m_HostName ||
        matchingRealm->GetServerPort() != realmConfig->m_ServerPort ||
        matchingRealm->GetLoginName() != realmConfig->m_UserName ||
        (matchingRealm->GetEnabled() && !realmConfig->m_Enabled) ||
        !matchingRealm->GetLoggedIn()
      );
      matchingRealm->SetConfig(realmConfig);
      matchingRealm->SetHostCounter(realmConfig->m_ServerIndex + 15);
      matchingRealm->ResetLogin();
      if (DoResetConnection) matchingRealm->ResetConnection(false);
      if (MatchLogLevel(LOG_LEVEL_DEBUG)) {
        Print("[AURA] server reloaded: " + matchingRealm->GetUniqueDisplayName());
      }
    }

    if (realmConfig->m_GamePrefix.length() > longestGamePrefixSize)
      longestGamePrefixSize = realmConfig->m_GamePrefix.length();

    m_RealmsByHostCounter[matchingRealm->GetHostCounterID()] = matchingRealm;
    realmConfig->Reset();
    delete realmConfig;
  }

  m_MaxGameNameSize = 31 - longestGamePrefixSize;
  return true;
}

bool CAura::CopyScripts()
{
  // Try to use manually extracted files already available in bot.map.configs_path
  filesystem::path autoExtractedCommonPath = m_Config.m_JASSPath / filesystem::path("common-" + to_string(m_GameVersion) + ".j");
  filesystem::path autoExtractedBlizzardPath = m_Config.m_JASSPath / filesystem::path("blizzard-" + to_string(m_GameVersion) + ".j");
  bool commonExists = FileExists(autoExtractedCommonPath);
  bool blizzardExists = FileExists(autoExtractedBlizzardPath);
  if (commonExists && blizzardExists) {
    return true;
  }

  if (!commonExists) {
    filesystem::path manuallyExtractedCommonPath = m_Config.m_JASSPath / filesystem::path("common.j");
    try {
      filesystem::copy_file(manuallyExtractedCommonPath, autoExtractedCommonPath, filesystem::copy_options::skip_existing);
    } catch (const exception& e) {
      Print("[AURA] " + string(e.what()));
      return false;
    }
  }
  if (!blizzardExists) {
    filesystem::path manuallyExtractedBlizzardPath = m_Config.m_JASSPath / filesystem::path("blizzard.j");
    try {
      filesystem::copy_file(manuallyExtractedBlizzardPath, autoExtractedBlizzardPath, filesystem::copy_options::skip_existing);
    } catch (const exception& e) {
      Print("[AURA] " + string(e.what()));
      return false;
    }
  }
  return true;
}

void CAura::ClearAutoRehost()
{
  if (!m_AutoRehostGameSetup) {
    return;
  }
  m_AutoRehostGameSetup.reset();
}

CAura::~CAura()
{
  m_SudoContext.reset();
  m_ReloadContext.reset();

  delete m_RealmDefaultConfig;
  delete m_GameDefaultConfig;
  delete m_CommandDefaultConfig;

  ClearAutoRehost();

  if (m_GameSetup) {
    m_GameSetup->m_ExitingSoon = true;
  }

  for (const auto& realm : m_Realms) {
    delete realm;
  }

  for (const auto& lobby : m_LobbiesPending) {
    delete lobby;
  }
  for (const auto& lobby : m_Lobbies) {
    delete lobby;
  }
  for (const auto& game : m_StartedGames) {
    delete game;
  }

  m_JoinInProgressGames.clear();

  delete m_DB;
}

CGame* CAura::GetMostRecentLobby(bool allowPending) const
{
  if (allowPending && !m_LobbiesPending.empty()) return m_LobbiesPending.back();
  if (m_Lobbies.empty()) return nullptr;
  return m_Lobbies.back();
}

CGame* CAura::GetMostRecentLobbyFromCreator(const string& fromName) const
{
  for (auto it = rbegin(m_Lobbies); it != rend(m_Lobbies); ++it) {
    if ((*it)->GetCreatorName() == fromName) {
      return (*it);
    }
  }
  return nullptr;
}

CGame* CAura::GetLobbyByHostCounter(uint32_t hostCounter) const
{
  hostCounter = hostCounter & 0x00FFFFFF;
  for (const auto& lobby : m_Lobbies) {
    if (lobby->GetHostCounter() == hostCounter) {
      return lobby;
    }
  }
  return nullptr;
}

CGame* CAura::GetLobbyByHostCounterExact(uint32_t hostCounter) const
{
  for (const auto& lobby : m_Lobbies) {
    if (lobby->GetHostCounter() == hostCounter) {
      return lobby;
    }
  }
  return nullptr;
}

CGame* CAura::GetGameByIdentifier(const uint64_t gameIdentifier) const
{
  for (const auto& lobby : m_Lobbies) {
    if (lobby->GetGameID() == gameIdentifier) {
      return lobby;
    }
  }
  for (const auto& game : m_StartedGames) {
    if (game->GetGameID() == gameIdentifier) {
      return game;
    }
  }
  return nullptr;
}

CGame* CAura::GetGameByString(const string& rawInput) const
{
  // See also util.h:CheckTargetGameSyntax
  if (rawInput.empty()) {
    return nullptr;
  }
  string inputGame = ToLowerCase(rawInput);
  if (inputGame == "lobby" || inputGame == "game#lobby") {
    return GetMostRecentLobby();
  }
  if (inputGame == "oldest" || inputGame == "game#oldest") {
    if (m_StartedGames.empty()) return nullptr;
    return m_StartedGames[0];
  }
  if (inputGame == "newest" || inputGame == "latest" || inputGame == "game#newest" || inputGame == "game#latest") {
    if (m_StartedGames.empty()) return nullptr;
    return m_StartedGames[m_StartedGames.size() - 1];
  }
  if (inputGame == "lobby#oldest") {
    if (m_Lobbies.empty()) return nullptr;
    return m_Lobbies[0];
  }
  if (inputGame == "lobby#newest") {
    return GetMostRecentLobby();
  }
  if (inputGame.substr(0, 5) == "game#") {
    inputGame = inputGame.substr(5);
  } else if (inputGame.substr(0, 6) == "lobby#") {
    inputGame = inputGame.substr(6);
  }

  uint64_t gameID = 0;
  try {
    long long value = stoll(inputGame);
    if (value < 0) return nullptr;
    gameID = static_cast<uint64_t>(value);
  } catch (...) {
    return nullptr;
  }

  return GetGameByIdentifier(gameID);
}

CRealm* CAura::GetRealmByInputId(const string& inputId) const
{
  auto it = m_RealmsByInputID.find(inputId);
  if (it == m_RealmsByInputID.end()) return nullptr;
  return it->second;
}

CRealm* CAura::GetRealmByHostCounter(const uint8_t hostCounter) const
{
  auto it = m_RealmsByHostCounter.find(hostCounter);
  if (it == m_RealmsByHostCounter.end()) return nullptr;
  return it->second;
}

CRealm* CAura::GetRealmByHostName(const string& hostName) const
{
  for (const auto& realm : m_Realms) {
    if (!realm->GetLoggedIn()) continue;
    if (realm->GetIsMirror()) continue;
    if (realm->GetServer() == hostName) return realm;
  }
  return nullptr;
}

uint8_t CAura::FindServiceFromHostName(const string& hostName, void*& location) const
{
  if (hostName.empty()) {
    return SERVICE_TYPE_NONE;
  }
  if (m_IRC.MatchHostName(hostName)) {
    return SERVICE_TYPE_IRC;
  }
  if (m_Discord.MatchHostName(hostName)) {
    return SERVICE_TYPE_DISCORD;
  }
  for (const auto& realm : m_Realms) {
    if (realm->GetServer() == hostName) {
      return SERVICE_TYPE_REALM;
    }
  }
  return SERVICE_TYPE_INVALID;
}

uint8_t CAura::HandleAction(const AppAction& action)
{
  switch (action.type) {
#ifndef DISABLE_MINIUPNP
    case APP_ACTION_TYPE_UPNP: {
      uint16_t externalPort = static_cast<uint16_t>(action.value_1);
      uint16_t internalPort = static_cast<uint16_t>(action.value_2);
      if (action.type == APP_ACTION_MODE_TCP) {
        m_Net.RequestUPnP(NET_PROTOCOL_TCP, externalPort, internalPort, LOG_LEVEL_DEBUG);
      } else if (action.type == APP_ACTION_MODE_UDP) {
        m_Net.RequestUPnP(NET_PROTOCOL_UDP, externalPort, internalPort, LOG_LEVEL_DEBUG);
      }
      return APP_ACTION_DONE;
    }
#endif
    case APP_ACTION_TYPE_HOST: {
      bool success = m_GameSetup->RunHost();
      if (!success) {
        // Delete all other pending actions
        return APP_ACTION_ERROR;
      }
      MergePendingLobbies();
      return APP_ACTION_DONE;
    }
  }

  return APP_ACTION_ERROR;
}

uint8_t CAura::HandleDeferredCommandContext(const LazyCommandContext& lazyCtx)
{
  return CCommandContext::TryDeferred(this, lazyCtx);
}

uint8_t CAura::HandleGenericAction(const GenericAppAction& genAction)
{
  switch (genAction.index()) {
    case 0: { // AppAction
      const AppAction& action = std::get<AppAction>(genAction);
      uint8_t result = HandleAction(action);
      if (result == APP_ACTION_WAIT && GetTicks() < action.queuedTime + 20000) {
        result = APP_ACTION_TIMEOUT;
      }
      return result;
    }
    case 1: { // LazyCommandContext
      const LazyCommandContext& lazyCtx = std::get<LazyCommandContext>(genAction);
      uint8_t result = HandleDeferredCommandContext(lazyCtx);
      if (result == APP_ACTION_WAIT && GetTicks() < lazyCtx.queuedTime + 20000) {
        result = APP_ACTION_TIMEOUT;
      }
      return result;
    }
  }

  return APP_ACTION_ERROR;
}

bool CAura::Update()
{
  if (gGracefulExit == 1 || m_ExitingSoon) {
    // Intentionally execute on every loop turn after graceful exit is flagged.
    GracefulExit();
  }

  // 1. pending actions
  bool skipActions = false;
  while (!m_PendingActions.empty()) {
    if (skipActions) {
      m_PendingActions.pop();
      continue;
    }
    uint8_t actionResult = HandleGenericAction(m_PendingActions.front());
    if (actionResult == APP_ACTION_WAIT) {
      break;
    }
    if (actionResult == APP_ACTION_ERROR) {
      Print("[AURA] Queued action errored. Pending actions aborted.");
      skipActions = true;
    } else if (actionResult == APP_ACTION_TIMEOUT) {
      Print("[AURA] Queued action timed out. Pending actions aborted.");
      skipActions = true;
    }
    m_PendingActions.pop();
  }

  bool metaDataNeedsUpdate = false;

  if (m_ReloadContext) {
    TryReloadConfigs();
    assert(m_ReloadContext == nullptr && "m_ReloadContext should be reset");
  }

  if (m_AutoRehostGameSetup && !m_AutoReHosted) {
    if (!(m_GameSetup && m_GameSetup->GetIsDownloading()) &&
      (GetNewGameIsInQuotaAutoReHost() && !GetIsAutoHostThrottled())
    ) {
      m_AutoRehostGameSetup->SetActive();
      AppAction rehostAction = AppAction(APP_ACTION_TYPE_HOST);
      m_PendingActions.push(rehostAction);
    }
  }

  bool isStandby = (
    m_Lobbies.empty() && m_StartedGames.empty() &&
    !m_Net.m_HealthCheckInProgress &&
    !(m_GameSetup && m_GameSetup->GetIsDownloading()) &&
    m_PendingActions.empty() &&
    !m_AutoRehostGameSetup
  );

  if (isStandby && (m_Config.m_ExitOnStandby || (m_ExitingSoon && CheckGracefulExit()))) {
    return true;
  }

  uint32_t NumFDs = 0;

  // take every socket we own and throw it in one giant select statement so we can block on all sockets

  int32_t nfds = 0;
  fd_set  fd, send_fd;
  FD_ZERO(&fd);
  FD_ZERO(&send_fd);

  // 2. all running game servers

  for (const auto& server : m_Net.m_GameServers) {
    server.second->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
    ++NumFDs;
  }

  // 3. all unassigned incoming TCP connections

  for (const auto& serverConnections : m_Net.m_IncomingConnections) {
    // std::pair<uint16_t, vector<CConnection*>>
    for (const auto& connection : serverConnections.second) {
      if (connection->GetSocket()) {
        connection->GetSocket()->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
        ++NumFDs;
      }
    }
  }

  // 4. all managed TCP connections

  for (const auto& serverConnections : m_Net.m_ManagedConnections) {
    // std::pair<uint16_t, vector<CConnection*>>
    for (const auto& connection : serverConnections.second) {
      if (connection->GetSocket()) {
        connection->GetSocket()->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
        ++NumFDs;
      }
    }
  }

  // 5. the current lobby's player sockets

  for (const auto& lobby : m_Lobbies) {
    NumFDs += lobby->SetFD(&fd, &send_fd, &nfds);
  }

  // 6. all running games' player sockets

  for (const auto& game : m_StartedGames) {
    NumFDs += game->SetFD(&fd, &send_fd, &nfds);
  }

  // 7. all battle.net sockets

  for (const auto& realm : m_Realms) {
    NumFDs += realm->SetFD(&fd, &send_fd, &nfds);
  }

  // 8. irc socket
  if (m_IRC.GetIsEnabled()) {
    NumFDs += m_IRC.SetFD(&fd, &send_fd, &nfds);
  }

  // 9. UDP sockets, outgoing test connections
  NumFDs += m_Net.SetFD(&fd, &send_fd, &nfds);

  // before we call select we need to determine how long to block for
  // 50 ms is the hard maximum

  int64_t usecBlock = 50000;

  for (const auto& game : m_StartedGames) {
    int64_t nextGameTimedActionMicroSeconds = game->GetNextTimedActionMicroSeconds();
    if (nextGameTimedActionMicroSeconds < usecBlock) {
      usecBlock = nextGameTimedActionMicroSeconds;
    }
  }

  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = static_cast<long int>(usecBlock);

  struct timeval send_tv;
  send_tv.tv_sec  = 0;
  send_tv.tv_usec = 0;

#ifdef _WIN32
  select(1, &fd, nullptr, nullptr, &tv);
  select(1, nullptr, &send_fd, nullptr, &send_tv);
#else
  select(nfds + 1, &fd, nullptr, nullptr, &tv);
  select(nfds + 1, nullptr, &send_fd, nullptr, &send_tv);
#endif

  if (NumFDs == 0) {
    // we don't have any sockets (i.e. we aren't connected to battle.net and irc maybe due to a lost connection and there aren't any games running)
    // select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 200ms to kill some time

    this_thread::sleep_for(chrono::milliseconds(200));
  }

  // update map downloads
  if (m_GameSetup) {
    if (m_GameSetup->Update()) {
      m_GameSetup.reset();
    }
  }

  // if hosting a lobby, accept new connections to its game server

  for (const auto& server : m_Net.m_GameServers) {
    if (m_ExitingSoon) {
      server.second->Discard(static_cast<fd_set*>(&fd));
      continue;
    }
    uint16_t localPort = server.first;
    if (m_Net.m_IncomingConnections[localPort].size() >= MAX_INCOMING_CONNECTIONS) {
      server.second->Discard(static_cast<fd_set*>(&fd));
      continue;
    }
    CStreamIOSocket* socket = server.second->Accept(static_cast<fd_set*>(&fd));
    if (socket) {
      if (m_Net.m_Config.m_ProxyReconnect > 0) {
        CConnection* incomingConnection = new CConnection(this, localPort, socket);
#ifdef DEBUG
        if (MatchLogLevel(LOG_LEVEL_TRACE2)) {
          Print("[AURA] incoming connection from " + incomingConnection->GetIPString());
        }
#endif
        m_Net.m_IncomingConnections[localPort].push_back(incomingConnection);
      } else if (m_Lobbies.empty() && m_JoinInProgressGames.empty()) {
#ifdef DEBUG
        if (MatchLogLevel(LOG_LEVEL_TRACE2)) {
          Print("[AURA] connection to port " + to_string(localPort) + " rejected.");
        }
#endif
        delete socket;
      } else {
        CConnection* incomingConnection = new CConnection(this, localPort, socket);
#ifdef DEBUG
        if (MatchLogLevel(LOG_LEVEL_TRACE2)) {
          Print("[AURA] incoming connection from " + incomingConnection->GetIPString());
        }
#endif
        m_Net.m_IncomingConnections[localPort].push_back(incomingConnection);
      }
      if (m_Net.m_IncomingConnections[localPort].size() >= MAX_INCOMING_CONNECTIONS) {
        Print("[AURA] " + to_string(m_Net.m_IncomingConnections[localPort].size()) + " connections at port " + to_string(localPort) + " - rejecting further connections");
      }
    }

    if (server.second->HasError()) {
      m_Exiting = true;
    }
  }

  // update unassigned incoming connections

  for (auto& serverConnections : m_Net.m_IncomingConnections) {
    int64_t timeout = (int64_t)LinearInterpolation((float)serverConnections.second.size(), (float)1., (float)MAX_INCOMING_CONNECTIONS, (float)GAME_USER_CONNECTION_MAX_TIMEOUT, (float)GAME_USER_CONNECTION_MIN_TIMEOUT);
    for (auto i = begin(serverConnections.second); i != end(serverConnections.second);) {
      // *i is a pointer to a CConnection
      uint8_t result = (*i)->Update(&fd, &send_fd, timeout);
      if (result == INCON_UPDATE_OK) {
        ++i;
        continue;
      }

      // flush the socket (e.g. in case a rejection message is queued)
      if ((*i)->GetSocket()) {
        (*i)->GetSocket()->DoSend(static_cast<fd_set*>(&send_fd));
      }
      delete *i;
      i = serverConnections.second.erase(i);
    }
  }

  // update CGameSeeker incoming connections

  for (auto& serverConnections : m_Net.m_ManagedConnections) {
    int64_t timeout = (int64_t)LinearInterpolation((float)serverConnections.second.size(), (float)1., (float)MAX_INCOMING_CONNECTIONS, (float)GAME_USER_CONNECTION_MAX_TIMEOUT, (float)GAME_USER_CONNECTION_MIN_TIMEOUT);
    for (auto i = begin(serverConnections.second); i != end(serverConnections.second);) {
      // *i is a pointer to a CConnection
      uint8_t result = (*i)->Update(&fd, &send_fd, timeout);
      if (result == INCON_UPDATE_OK) {
        ++i;
        continue;
      }

      // flush the socket (e.g. in case a rejection message is queued)
      if ((*i)->GetSocket()) {
        (*i)->GetSocket()->DoSend(static_cast<fd_set*>(&send_fd));
      }
      delete *i;
      i = serverConnections.second.erase(i);
    }
  }

  // update games, starting from lobbies

  for (auto it = begin(m_Lobbies); it != end(m_Lobbies);) {
    if ((*it)->Update(&fd, &send_fd)) {
      if ((*it)->GetExiting()) {
        EventGameDeleted(*it);
        delete *it;
      } else {
        EventGameStarted(*it);
      }
      it = m_Lobbies.erase(it);
      metaDataNeedsUpdate = true;
    } else {
      (*it)->UpdatePost(&send_fd);
      ++it;
    }
  }

  for (auto it = begin(m_StartedGames); it != end(m_StartedGames);) {
    if ((*it)->Update(&fd, &send_fd)) {
      (*it)->FlushLogs();
      if ((*it)->GetExiting()) {
        EventGameDeleted(*it);
        delete *it;
      } else {
        EventGameRemake(*it);
      }
      it = m_StartedGames.erase(it);
      metaDataNeedsUpdate = true;
    } else {
      (*it)->UpdatePost(&send_fd);
      ++it;
    }
  }

  for (const auto& realm : m_Realms) {
    realm->Update(&fd, &send_fd);
  }

  m_IRC.Update(&fd, &send_fd);
  m_Discord.Update();

  // UDP sockets, outgoing test connections
  m_Net.Update(&fd, &send_fd);

  // move stuff from pending vectors to their intended places
  m_Net.MergeDownGradedConnections();
  if (MergePendingLobbies()) {
    metaDataNeedsUpdate = true;
  }

  if (metaDataNeedsUpdate) {
    UpdateMetaData();
  }

  // house-keeping
  ClearStaleContexts();

  return m_Exiting;
}

void CAura::AwaitSettled()
{
  if (m_GameSetup) {
    m_GameSetup->AwaitSettled();
  }
  if (m_AutoRehostGameSetup) {
    m_AutoRehostGameSetup->AwaitSettled();
  }
}

void CAura::EventBNETGameRefreshSuccess(CRealm* successRealm)
{
  successRealm->ResolveGameBroadcastStatus(true);
}

void CAura::EventBNETGameRefreshError(CRealm* errorRealm)
{
  if (errorRealm->GetIsGameBroadcastErrored()) {
    return;
  }

  errorRealm->ResolveGameBroadcastStatus(false); 

  // If the game has someone in it, advertise the fail only in the lobby (as it is probably a rehost).
  // Otherwise whisper the game creator that the (re)host failed.

  CGame* game = errorRealm->GetGameBroadcast();

  if (game->GetHasAnyUser()) {
    game->SendAllChat("Cannot register game on server [" + errorRealm->GetServer() + "]. Try another name");
  } else {
    switch (game->GetCreatedFromType()) {
      case SERVICE_TYPE_REALM:
        reinterpret_cast<CRealm*>(game->GetCreatedFrom())->QueueWhisper("Cannot register game on server [" + errorRealm->GetServer() + "]. Try another name", game->GetCreatorName());
        break;
      case SERVICE_TYPE_IRC:
        reinterpret_cast<CIRC*>(game->GetCreatedFrom())->SendUser("Cannot register game on server [" + errorRealm->GetServer() + "]. Try another name", game->GetCreatorName());
        break;
      /*
      // FIXME: CAura::EventBNETGameRefreshError SendUser() - Discord case
      case SERVICE_TYPE_DISCORD:
        reinterpret_cast<CDiscord*>(game->GetCreatedFrom())->SendUser("Unable to create game on server [" + errorRealm->GetServer() + "]. Try another name", game->GetCreatorName());
        break;*/
      default:
        break;
    }
  }

  Print("[GAME: " + game->GetGameName() + "] Cannot register game on server [" + errorRealm->GetServer() + "]. Try another name");

  bool earlyExit = false;
  switch (game->m_Config.m_BroadcastErrorHandler) {
    case ON_ADV_ERROR_EXIT_ON_MAIN_ERROR:
      if (!errorRealm->GetIsMain()) break;
      // fall through
    case ON_ADV_ERROR_EXIT_ON_ANY_ERROR:
      earlyExit = true;
      break;
    case ON_ADV_ERROR_EXIT_ON_MAIN_ERROR_IF_EMPTY:
      if (!errorRealm->GetIsMain()) break;
      // fall through
    case ON_ADV_ERROR_EXIT_ON_ANY_ERROR_IF_EMPTY:
      if (!game->GetHasAnyUser()) {
        // we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)
        earlyExit = true;
      }
      break;
  }
  if (earlyExit) {
    game->StopPlayers("failed to broadcast game");
    game->SetExiting(true);
    return;
  }

  if (game->m_Config.m_BroadcastErrorHandler == ON_ADV_ERROR_EXIT_ON_MAX_ERRORS) {
    for (auto& realm : m_Realms) {
      if (!realm->GetEnabled()) {
        continue;
      }
      if (game->GetIsMirror() && realm->GetIsMirror()) {
      // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
      // Do not display external games in those realms.
        continue;
      }
      if (realm->GetGameVersion() > 0 && !game->GetIsSupportedGameVersion(realm->GetGameVersion())) {
        continue;
      }
      if (game->GetIsRealmExcluded(realm->GetServer())) {
        continue;
      }
      if (!realm->GetIsGameBroadcastErrored()) {
        return;
      }
    }

    game->StopPlayers("failed to broadcast game");
    game->SetExiting(true);
    return;
  }
}

void CAura::EventGameDeleted(CGame* game)
{
  if (game->GetFromAutoReHost()) {
    m_AutoReHosted = false;
  }

  if (game->GetIsLobby()) {
    Print("[AURA] deleting lobby [" + game->GetGameName() + "]");
    if (game->GetUDPEnabled()) {
      game->SendGameDiscoveryDecreate();
    }
    for (auto& realm : m_Realms) {
      if (realm->GetGameBroadcast() == game) {
        realm->ResetGameBroadcastData();
      }
    }
  } else {
    Print("[AURA] deleting game [" + game->GetGameName() + "]");
    if ((game->GetGameTicks() / 1000) < 180) {
      // Do not announce game ended if game lasted less than 3 minutes.
      return;
    }
    for (auto& realm : m_Realms) {
      if (!realm->GetAnnounceHostToChat()) continue;
      if (game->GetGameLoaded()) {
        realm->QueueChatChannel("Game ended: " + game->GetEndDescription());
        if (game->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(this))) {
          realm->QueueWhisper("Game ended: " + game->GetEndDescription(), game->GetCreatorName());
        }
      }
    }
  }
}

void CAura::EventGameRemake(CGame* game)
{
  // Only called from CGame::Update() while iterating m_StartedGames
  Print("[AURA] remaking game [" + game->GetGameName() + "]");
  m_LobbiesPending.push_back(game);

  /*
  if (game->GetFromAutoReHost()) {
    m_AutoReHosted = true;
  }
  */

  for (auto& realm : m_Realms) {
    if (!realm->GetAnnounceHostToChat()) continue;
    realm->QueueChatChannel("Game remake: " + game->GetMap()->GetServerFileName());
    if (game->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(this))) {
      realm->QueueWhisper("Game remake: " + game->GetMap()->GetServerFileName(), game->GetCreatorName());
    }
  }
}

void CAura::EventGameStarted(CGame* game)
{
  // Only called from CGame::Update() while iterating m_Lobbies
  Print("[AURA] started game [" + game->GetGameName() + "]");
  m_StartedGames.push_back(game);

  if (game->GetFromAutoReHost()) {
    m_AutoReHosted = false;
  }

  /*
  for (auto& realm : m_Realms) {
    if (!realm->GetAnnounceHostToChat()) continue;
    realm->QueueChatChannel("Game started: " + game->GetMap()->GetServerFileName());
    if (game->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(this))) {
      realm->QueueWhisper("Game started: " + game->GetMap()->GetServerFileName(), game->GetCreatorName());
    }
  }
  */
}

bool CAura::ReloadConfigs()
{
  bool success = true;
  uint8_t WasVersion = m_GameVersion;
  bool WasCacheEnabled = m_Config.m_EnableCFGCache;
  filesystem::path WasMapPath = m_Config.m_MapPath;
  filesystem::path WasCFGPath = m_Config.m_MapCFGPath;
  filesystem::path WasCachePath = m_Config.m_MapCachePath;
  filesystem::path WasJASSPath = m_Config.m_JASSPath;
  CConfig CFG;
  if (!CFG.Read(m_ConfigPath)) {
    Print("[CONFIG] warning - failed to read config file");
  } else if (!LoadAllConfigs(CFG)) {
    Print("[CONFIG] error - bot configuration invalid: not reloaded");
    success = false;
  }
  OnLoadConfigs();
  bitset<120> definedRealms;
  if (!LoadBNETs(CFG, definedRealms)) {
    Print("[CONFIG] error - realms misconfigured: not reloaded");
    success = false;
  }
  vector<string> invalidKeys = CFG.GetInvalidKeys(definedRealms);
  if (!invalidKeys.empty()) {
    Print("[CONFIG] warning - the following keys are invalid/misnamed: " + JoinVector(invalidKeys, false));
  }

  if (m_GameVersion != WasVersion) {
    Print("[AURA] Running game version 1." + to_string(m_GameVersion));
  }

  if (m_Config.m_ExtractJASS) {
    if (!m_ScriptsExtracted || m_GameVersion != WasVersion) {
      m_ScriptsExtracted = ExtractScripts() == 2;
      if (!m_ScriptsExtracted) {
        CopyScripts();
      }
    }
  }

  bool reCachePresets = WasCacheEnabled != m_Config.m_EnableCFGCache;
  if (WasMapPath != m_Config.m_MapPath) {
    try {
      filesystem::create_directory(m_Config.m_MapPath);
    } catch (...) {
      Print("[AURA] warning - <bot.maps_path> is not a valid directory");
    }
    reCachePresets = true;
  }
  if (WasCachePath != m_Config.m_MapCachePath) {
    try {
      filesystem::create_directory(m_Config.m_MapCachePath);
    } catch (...) {
      Print("[AURA] warning - <bot.map.cache_path> is not a valid directory");
    }
    reCachePresets = true;
  }
  if (WasCFGPath != m_Config.m_MapCFGPath) {
    try {
      filesystem::create_directory(m_Config.m_MapCFGPath);
    } catch (...) {
      Print("[AURA] warning - <bot.map.configs_path> is not a valid directory");
    }
  }
  if (WasJASSPath != m_Config.m_JASSPath) {
    try {
      filesystem::create_directory(m_Config.m_JASSPath);
    } catch (...) {
      Print("[AURA] warning - <bot.jass_path> is not a valid directory");
    }
  }

  if (!m_Config.m_EnableCFGCache) {
    m_CFGCacheNamesByMapNames.clear();
  } else if (reCachePresets) {
    UpdateCFGCacheEntries();
  }
  m_Net.OnConfigReload();

  return success;
}

void CAura::TryReloadConfigs()
{
  const bool success = ReloadConfigs();
  if (!m_ReloadContext->GetPartiallyDestroyed()) {
    if (success) {
      m_ReloadContext->SendReply("Reloaded successfully.");
    } else {
      m_ReloadContext->ErrorReply("Reload failed. See the console output.");
    }
  }
  m_ReloadContext.reset();
}

bool CAura::LoadDefaultConfigs(CConfig& CFG, CNetConfig* netConfig)
{
  CRealmConfig* RealmDefaultConfig = new CRealmConfig(CFG, netConfig);
  CGameConfig* GameDefaultConfig = new CGameConfig(CFG);

  if (!CFG.GetSuccess()) {
    delete RealmDefaultConfig;
    delete GameDefaultConfig;
    return false;
  }
  
  delete m_RealmDefaultConfig;
  delete m_GameDefaultConfig;

  m_RealmDefaultConfig = RealmDefaultConfig;
  m_GameDefaultConfig = GameDefaultConfig;

  return true;
}

bool CAura::LoadAllConfigs(CConfig& CFG)
{
  CBotConfig BotConfig = CBotConfig(CFG);
  CNetConfig NetConfig = CNetConfig(CFG);
  CIRCConfig IRCConfig = CIRCConfig(CFG);
  CDiscordConfig DiscordConfig = CDiscordConfig(CFG);

  if (!CFG.GetSuccess()) {
    return false;
  }

  if (!LoadDefaultConfigs(CFG, &NetConfig)) {
    return false;
  }

  // Copy, but prevent double free of CCommandConfig* members
  m_Config = BotConfig;
  m_IRC.m_Config = IRCConfig;
  m_Discord.m_Config = DiscordConfig;
  m_Net.m_Config = NetConfig;

  BotConfig.Reset();
  IRCConfig.Reset();
  DiscordConfig.Reset();
  return true;
}

void CAura::OnLoadConfigs()
{
  m_LogLevel = m_Config.m_LogLevel;

  if (m_Config.m_Warcraft3Path.has_value()) {
    m_GameInstallPath = m_Config.m_Warcraft3Path.value();
  } else if (m_GameInstallPath.empty()) {
#ifdef _WIN32
    size_t valueSize;
    errno_t err = _wdupenv_s(&war3Home, &valueSize, L"WAR3_HOME"); 
    if (!err && war3Home != nullptr) {
      wstring war3Path = war3Home;
#else
    const char* envValue = getenv("WAR3_HOME");
    if (envValue != nullptr) {
      string war3Path = envValue;
#endif
      m_GameInstallPath = filesystem::path(war3Path);
    } else {
#ifdef _WIN32
      optional<filesystem::path> maybeInstallPath = MaybeReadRegistryPath(L"SOFTWARE\\Blizzard Entertainment\\Warcraft III", L"InstallPath");
      if (maybeInstallPath.has_value()) {
        m_GameInstallPath = maybeInstallPath.value();
      } else {
        vector<wchar_t*> tryPaths = {
          L"C:\\Program Files (x86)\\Warcraft III\\",
          L"C:\\Program Files\\Warcraft III\\",
          L"C:\\Games\\Warcraft III\\",
          L"C:\\Warcraft III\\",
          L"D:\\Games\\Warcraft III\\",
          L"D:\\Warcraft III\\"
        };
        error_code ec;
        for (const auto& opt : tryPaths) {
          filesystem::path testPath = opt;
          if (filesystem::is_directory(testPath, ec)) {
            m_GameInstallPath = testPath;
          }
        }
      }
#endif
    }
    if (m_GameInstallPath.empty()) {
#ifdef _WIN32
      // Make sure this error message can be looked up.
      Print("[AURA] Registry error loading key 'Warcraft III\\InstallPath'");
#endif
    } else {
      NormalizeDirectory(m_GameInstallPath);
      Print("[AURA] Using <game.install_path = " + PathToString(m_GameInstallPath) + ">");
    }
  }

  if (m_Config.m_War3Version.has_value()) {
    m_GameVersion = m_Config.m_War3Version.value();
  } else if (m_GameVersion == 0 && !m_GameInstallPath.empty() && htons(0xe017) == 0x17e0) {
    optional<uint8_t> AutoVersion = CBNCSUtilInterface::GetGameVersion(m_GameInstallPath);
    if (AutoVersion.has_value()) {
      m_GameVersion = AutoVersion.value();
    }
  }

  m_MaxSlots = m_GameVersion >= 29 ? MAX_SLOTS_MODERN : MAX_SLOTS_LEGACY;
  m_Lobbies.reserve(m_Config.m_MaxLobbies);
  m_StartedGames.reserve(m_Config.m_MaxStartedGames);
}

uint8_t CAura::ExtractScripts()
{
  if (m_GameInstallPath.empty()) {
    return 0;
  }

  uint8_t FilesExtracted = 0;
  const filesystem::path MPQFilePath = [&]() {
    if (m_GameVersion >= 28)
      return m_GameInstallPath / filesystem::path("War3.mpq");
    else
      return m_GameInstallPath / filesystem::path("War3Patch.mpq");
  }();

  void* MPQ;
  if (OpenMPQArchive(&MPQ, MPQFilePath)) {
    FilesExtracted += ExtractMPQFile(MPQ, R"(Scripts\common.j)", m_Config.m_JASSPath / filesystem::path("common-" + to_string(m_GameVersion) + ".j"));
    FilesExtracted += ExtractMPQFile(MPQ, R"(Scripts\blizzard.j)", m_Config.m_JASSPath / filesystem::path("blizzard-" + to_string(m_GameVersion) + ".j"));
    CloseMPQArchive(MPQ);
  } else {
#ifdef _WIN32
    uint32_t errorCode = (uint32_t)GetLastOSError();
    string errorCodeString = (
      errorCode == 2 ? "Config error: <game.install_path> is not the WC3 directory" : (
      errorCode == 11 ? "File is corrupted." : (
      (errorCode == 3 || errorCode == 15) ? "Config error: <game.install_path> is not a valid directory" : (
      (errorCode == 32 || errorCode == 33) ? "File is currently opened by another process." : (
      "Error code " + to_string(errorCode)
      ))))
    );
#else
    string errorCodeString = "Error code " + to_string(errno);
#endif
    Print("[AURA] warning - unable to load MPQ archive [" + PathToString(MPQFilePath) + "] - " + errorCodeString);
  }

  return FilesExtracted;
}

void CAura::LoadMapAliases()
{
  CConfig aliases;
  if (!aliases.Read(m_Config.m_AliasesPath)) {
    return;
  }

  if (!m_DB->Begin()) {
    Print("[AURA] internal database error - map aliases will not be available");
    return;
  }

  for (const auto& entry : aliases.GetEntries()) {
    string normalizedAlias = GetNormalizedAlias(entry.first);
    if (normalizedAlias.empty()) continue;
    static_cast<void>(m_DB->AliasAdd(normalizedAlias, entry.second));
  }

  if (!m_DB->Commit()) {
    Print("[AURA] internal database error - map aliases will not be available");
  }
}

void CAura::LoadIPToCountryData(const CConfig& CFG)
{
  ifstream in;
  filesystem::path GeoFilePath = CFG.GetHomeDir() / filesystem::path("ip-to-country.csv");
  in.open(GeoFilePath.native().c_str(), ios::in);

  if (in.fail()) {
    Print("[AURA] warning - unable to read file [ip-to-country.csv], geolocalization data not loaded");
    return;
  }
  // the begin and commit statements are optimizations
  // we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time

  if (!m_DB->Begin()) {
    Print("[AURA] internal database error - geolocalization will not be available");
    in.close();
    return;
  }

  string    Line, Skip, IP1, IP2, Country;
  CSVParser parser;

  in.seekg(0, ios::end);
  in.seekg(0, ios::beg);

  while (!in.eof()) {
    getline(in, Line);

    if (Line.empty())
      continue;

    parser << Line;
    parser >> Skip;
    parser >> Skip;
    parser >> IP1;
    parser >> IP2;
    parser >> Country;
    static_cast<void>(m_DB->FromAdd(stoul(IP1), stoul(IP2), Country));
  }

  if (!m_DB->Commit()) {
    Print("[AURA] internal database error - geolocalization will not be available");
  }

  in.close();
}

void CAura::InitContextMenu()
{
#ifdef _WIN32
  DeleteUserRegistryKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.w3m");
  DeleteUserRegistryKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.w3x");

  wstring scenario = L"WorldEdit.Scenario";
  wstring scenarioEx = L"WorldEdit.ScenarioEx";

  wstring openWithAuraCommand = L"\"";
  openWithAuraCommand += GetExePath().wstring();
  openWithAuraCommand += L"\" \"%1\" --stdpaths";

  SetUserRegistryKey(L"Software\\Classes\\.w3m", L"", scenario.c_str());
  SetUserRegistryKey(L"Software\\Classes\\.w3x", L"", scenarioEx.c_str());
  SetUserRegistryKey(L"Software\\Classes\\WorldEdit.Scenario\\shell\\Host with Aura\\command", L"", openWithAuraCommand.c_str());
  SetUserRegistryKey(L"Software\\Classes\\WorldEdit.ScenarioEx\\shell\\Host with Aura\\command", L"", openWithAuraCommand.c_str());
  Print("[AURA] Installed to context menu.");
#endif
}

void CAura::InitPathVariable()
{
  filesystem::path exeDirectory = GetExeDirectory();
  try {
    filesystem::path exeDirectoryAbsolute = filesystem::absolute(exeDirectory);
    EnsureDirectoryInUserPath(exeDirectoryAbsolute);
  } catch (...) {
  }
}

void CAura::InitSystem()
{
  InitContextMenu();
  InitPathVariable();
}

void CAura::UpdateWindowTitle()
{
  CGame* detailsGame = nullptr;
  if (m_Lobbies.size() == 1) {
    if (m_StartedGames.size() == 0) detailsGame = m_Lobbies.back();
  } else if (m_Lobbies.empty() && m_StartedGames.size() == 1) {
    detailsGame = m_StartedGames.back();
  }
  PLATFORM_STRING_TYPE windowTitle = GetAuraTitle(detailsGame, m_Lobbies.size(), m_StartedGames.size(), m_AutoRehostGameSetup != nullptr);
  SetWindowTitle(windowTitle);
}

void CAura::UpdateMetaData()
{
  UpdateWindowTitle();
}

void CAura::UpdateCFGCacheEntries()
{
  m_CFGCacheNamesByMapNames.clear();

  // Preload map.Localpath -> mapcache entries
  const vector<filesystem::path> cacheFiles = FilesMatch(m_Config.m_MapCachePath, FILE_EXTENSIONS_CONFIG);
  for (const auto& cfgName : cacheFiles) {
    string localPathString = CConfig::ReadString(m_Config.m_MapCachePath / cfgName, "map.local_path");
    filesystem::path localPath = localPathString;
    localPath = localPath.lexically_normal();
    try {
      if (localPath == localPath.filename() || filesystem::absolute(localPath.parent_path()) == filesystem::absolute(m_Config.m_MapPath.parent_path())) {
        string mapString = PathToString(localPath.filename());
        string cfgString = PathToString(cfgName);
        if (mapString.empty() || cfgString.empty()) continue;
        m_CFGCacheNamesByMapNames[localPath.filename()] = cfgString;
      }
    } catch (...) {
      // filesystem::absolute may throw errors
    }
  }
}

void CAura::ClearStaleContexts()
{
  auto it = m_ActiveContexts.rbegin();
  auto itEnd = m_ActiveContexts.rend();
  while (it != itEnd) {
    if (it->expired()) {
      it = vector<weak_ptr<CCommandContext>>::reverse_iterator(m_ActiveContexts.erase((++it).base()));
    } else {
      ++it;
    }
  }

  if (m_ActiveContexts.size() > 5) {
    Print("[DEBUG] weak_ptr<CCommandContext> leak detected (m_ActiveContexts size is " + to_string(m_ActiveContexts.size()) + ")");
  }
}

void CAura::ClearStaleFileChunks()
{
  vector<filesystem::path> staleCacheKeys;
  for (const auto& cacheEntries : m_CachedFileContents) {
    if (cacheEntries.second.bytes.expired()) {
      staleCacheKeys.push_back(cacheEntries.first);
    }
  }
  for (const auto& staleCacheKey : staleCacheKeys) {
    m_CachedFileContents.erase(staleCacheKey);
  }
}

void CAura::LogPersistent(const string& logText)
{
  ofstream writeStream;
  writeStream.open(m_Config.m_LogPath.native().c_str(), ios::binary | ios::app );

  if (writeStream.fail( )) {
    return;
  }
  
  LogStream(writeStream, logText);
  writeStream.close( );
}

void CAura::GracefulExit()
{
  m_ExitingSoon = true;
  m_Config.m_Enabled = false;

  ClearAutoRehost();

  if (m_GameSetup) {
    m_GameSetup->m_ExitingSoon = true;
  }

  for (auto& game : m_StartedGames) {
    game->SendEveryoneElseLeftAndDisconnect("shutdown");
  }

  for (auto& lobby : m_Lobbies) {
    lobby->StopPlayers("shutdown");
    lobby->SetExiting(true);
  }

  m_Net.GracefulExit();

  for (auto& realm : m_Realms) {
    realm->Disable();
  }

  m_IRC.Disable();
  m_Discord.Disable();
}

bool CAura::CheckGracefulExit()
{
  /* Already checked:
    (m_Lobbies.empty() && m_StartedGames.empty() &&
    !m_Net.m_HealthCheckInProgress &&
    !(m_GameSetup && m_GameSetup->GetIsDownloading()) &&
    m_PendingActions.empty())
  */

  if (m_IRC.GetIsEnabled() && m_IRC.GetSocket()->GetConnected()) {
    return false;
  }
  for (auto& realm : m_Realms) {
    if (realm->GetSocket()->GetConnected()) {
      return false;
    }
  }
  for (auto& serverConnections : m_Net.m_IncomingConnections) {
    if (!serverConnections.second.empty()) {
      return false;
    }
  }
  for (auto& serverConnections : m_Net.m_ManagedConnections) {
    if (!serverConnections.second.empty()) {
      return false;
    }
  }
  if (!m_Net.m_DownGradedConnections.empty()) {
    return false;
  }
  return true;
}

bool CAura::GetNewGameIsInQuota() const
{
  if (m_Lobbies.size() - m_ReplacingLobbiesCounter >= m_Config.m_MaxLobbies) return false;
  if (m_Lobbies.size() + m_StartedGames.size() >= m_Config.m_MaxTotalGames) return false;
  return true;
}

bool CAura::GetNewGameIsInQuotaReplace() const
{
  if (m_Lobbies.size() - m_ReplacingLobbiesCounter > m_Config.m_MaxLobbies) return false;
  if (m_Lobbies.size() + m_StartedGames.size() >= m_Config.m_MaxTotalGames) return false;
  return true;
}

bool CAura::GetNewGameIsInQuotaConservative() const
{
  if (m_Lobbies.size() >= m_Config.m_MaxLobbies) return false;
  if (m_StartedGames.size() >= m_Config.m_MaxStartedGames) return false;
  if (m_Lobbies.size() + m_StartedGames.size() >= m_Config.m_MaxTotalGames) return false;
  return true;
}

bool CAura::GetNewGameIsInQuotaAutoReHost() const
{
  if (m_Config.m_AutoRehostQuotaConservative) {
    return GetNewGameIsInQuotaConservative();
  } else {
    return GetNewGameIsInQuota();
  }
}

bool CAura::GetIsAutoHostThrottled() const
{
  if (m_Realms.empty()) return false;
  return m_LastGameAutoHostedTicks.has_value() && m_LastGameAutoHostedTicks.value() + static_cast<int64_t>(AUTO_REHOST_COOLDOWN_TICKS) >= GetTicks();
}

bool CAura::CreateGame(shared_ptr<CGameSetup> gameSetup)
{
  if (!m_Config.m_Enabled) {
    gameSetup->m_Ctx->ErrorReply("The bot is disabled", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (gameSetup->m_Name.size() > m_MaxGameNameSize) {
    gameSetup->m_Ctx->ErrorReply("The game name is too long (max " + to_string(m_MaxGameNameSize) + " characters)", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (!gameSetup->m_Map) {
    gameSetup->m_Ctx->ErrorReply("The currently loaded game setup is invalid", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }
  if (!gameSetup->m_Map || !gameSetup->m_Map->GetValid()) {
    gameSetup->m_Ctx->ErrorReply("The currently loaded map config file is invalid", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (!GetNewGameIsInQuota()) {
    if (m_Lobbies.size() == 1) {
      gameSetup->m_Ctx->ErrorReply("Another game lobby [" + GetMostRecentLobby()->GetStatusDescription() + "] is currently hosted.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    } else {
      gameSetup->m_Ctx->ErrorReply("Too many lobbies (" + to_string(m_Lobbies.size()) + ") are currently hosted.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    }
    return false;
  }

  if (gameSetup->GetIsMirror()) {
    Print("[AURA] mirroring game [" + gameSetup->m_Name + "]");
  } else if (gameSetup->m_RestoredGame) {
    Print("[AURA] creating loaded game [" + gameSetup->m_Name + "]");
  } else {
    Print("[AURA] creating game [" + gameSetup->m_Name + "]");
  }

  CGame* createdLobby = new CGame(this, gameSetup);
  m_LobbiesPending.push_back(createdLobby);
  m_LastGameHostedTicks = GetTicks();
  if (createdLobby->GetFromAutoReHost()) {
    m_AutoRehostGameSetup = gameSetup;
    m_LastGameAutoHostedTicks = m_LastGameHostedTicks;
    m_AutoReHosted = true;
  }
  gameSetup->OnGameCreate();

  if (createdLobby->GetExiting()) {
    delete createdLobby;
    createdLobby = nullptr;
    gameSetup->m_Ctx->ErrorReply("Cannot assign a TCP/IP port to game [" + gameSetup->m_Name + "].", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  UpdateMetaData();

#ifndef DISABLE_MINIUPNP
  if (m_Net.m_Config.m_EnableUPnP && createdLobby->GetIsLobbyStrict() && m_StartedGames.empty()) {
    // FIXME? This is a long synchronous network call.
    m_Net.RequestUPnP(NET_PROTOCOL_TCP, createdLobby->GetHostPortForDiscoveryInfo(AF_INET), createdLobby->GetHostPort(), LOG_LEVEL_INFO);
  }
#endif

  if (createdLobby->GetIsCheckJoinable() && !m_Net.GetIsFetchingIPAddresses()) {
    uint8_t checkMode = HEALTH_CHECK_ALL;
    if (!m_Net.m_SupportTCPOverIPv6) {
      checkMode &= ~HEALTH_CHECK_PUBLIC_IPV6;
      checkMode &= ~HEALTH_CHECK_LOOPBACK_IPV6;
    }
    if (createdLobby->GetIsVerbose()) {
      checkMode |= HEALTH_CHECK_VERBOSE;
    }
    m_Net.QueryHealthCheck(gameSetup->m_Ctx, checkMode, nullptr, createdLobby);
    createdLobby->SetIsCheckJoinable(false);
  }

  if (createdLobby->GetUDPEnabled()) {
    createdLobby->SendGameDiscoveryCreate();
  }

  for (auto& realm : m_Realms) {
    if (!createdLobby->GetIsMirror() && !createdLobby->GetIsRestored()) {
      realm->HoldFriends(createdLobby);
      realm->HoldClan(createdLobby);
    }

    if (createdLobby->GetIsMirror() && realm->GetIsMirror()) {
      // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
      // Do not display external games in those realms.
      continue;
    }
    if (gameSetup->m_RealmsExcluded.find(realm->GetServer()) != gameSetup->m_RealmsExcluded.end()) {
      continue;
    }
    if (realm->GetGameVersion() > 0 && !createdLobby->GetIsSupportedGameVersion(realm->GetGameVersion())) {
      if (MatchLogLevel(LOG_LEVEL_WARNING)) {
        Print(realm->GetLogPrefix() + "skipping announcement for v 1." + ToDecString(realm->GetGameVersion()) + "(check <hosting.crossplay.versions>)");
      }
      continue;
    }

    if (createdLobby->GetDisplayMode() == GAME_PUBLIC && realm->GetAnnounceHostToChat()) {
      realm->QueueGameChatAnnouncement(createdLobby);
    } else {
      // Send STARTADVEX3
      createdLobby->AnnounceToRealm(realm);

      // if we're creating a private game we don't need to send any further game refresh messages so we can rejoin the chat immediately
      // unfortunately, this doesn't work on PVPGN servers, because they consider an enterchat message to be a gameuncreate message when in a game
      // so don't rejoin the chat if we're using PVPGN

      if (createdLobby->GetDisplayMode() == GAME_PRIVATE && !realm->GetPvPGN()) {
        realm->SendEnterChat();
      }
    }
  }

  if (createdLobby->GetDisplayMode() != GAME_PUBLIC ||
    gameSetup->m_CreatedFromType != SERVICE_TYPE_REALM ||
    gameSetup->m_Ctx->GetIsWhisper()) {
    gameSetup->m_Ctx->SendPrivateReply(createdLobby->GetAnnounceText());
  }

  if (createdLobby->GetDisplayMode() == GAME_PUBLIC) {
    if (m_IRC.GetIsEnabled()) {
     m_IRC.SendAllChannels(createdLobby->GetAnnounceText());
    }
    if (m_Discord.GetIsEnabled()) {
      // TODO: Announce game created to all supported Discord guilds+channels
      //m_Discord.SendAnnouncementChannels(createdLobby->GetAnnounceText());
    }
  }

  uint32_t mapSize = ByteArrayToUInt32(createdLobby->GetMap()->GetMapSize(), false);
  if (m_GameVersion <= 26 && mapSize > 0x800000) {
    Print("[AURA] warning - hosting game beyond 8MB map size limit: [" + createdLobby->GetMap()->GetServerFileName() + "]");
  }
  if (m_GameVersion < createdLobby->GetMap()->GetMapMinSuggestedGameVersion()) {
    Print("[AURA] warning - hosting game that MAY require version 1." + to_string(createdLobby->GetMap()->GetMapMinSuggestedGameVersion()));
  }

  return true;
}

bool CAura::MergePendingLobbies()
{
  if (m_LobbiesPending.empty()) return false;
  m_Lobbies.reserve(m_Lobbies.size() + m_LobbiesPending.size());
  m_Lobbies.insert(m_Lobbies.end(), m_LobbiesPending.begin(), m_LobbiesPending.end());
  m_LobbiesPending.clear();
  return true;
}

void CAura::TrackGameJoinInProgress(CGame* game)
{
  m_JoinInProgressGames.push_back(game);
}

void CAura::UntrackGameJoinInProgress(CGame* game)
{
  for (auto it = begin(m_JoinInProgressGames); it != end(m_JoinInProgressGames);) {
    if (*it == game) {
      it = m_JoinInProgressGames.erase(it);
    } else {
      ++it;
    }
  }
}

bool CAura::QueueConfigReload(shared_ptr<CCommandContext> nCtx)
{
  if (m_ReloadContext) return false;
  m_ReloadContext = nCtx;
  return true;
}

uint32_t CAura::NextHostCounter()
{
  m_HostCounter = (m_HostCounter + 1) & 0x00FFFFFF;
  if (m_HostCounter < m_Config.m_MinHostCounter) {
    m_HostCounter = m_Config.m_MinHostCounter;
  }
  return m_HostCounter;
}

uint64_t CAura::NextHistoryGameID()
{
  m_HistoryGameID = (m_HistoryGameID + 1);
  return m_HistoryGameID;
}

uint32_t CAura::NextServerID()
{
  ++m_LastServerID;
  if (m_LastServerID < 0x10) {
    // Ran out of server IDs.
    m_LastServerID = 0;
  }
  return m_LastServerID;
}

FileChunkTransient CAura::ReadFileChunkCacheable(const std::filesystem::path& filePath, const size_t start, const size_t end)
{
  auto it = m_CachedFileContents.find(filePath);
  if (it != m_CachedFileContents.end()) {
    const FileChunkCached& chunk = it->second;
    if (chunk.start <= start && start < chunk.end) {
      WeakByteArray maybeCachedPtr = chunk.bytes;
      if (!maybeCachedPtr.expired()) {
        //Print("[DEBUG] Reusing cached map data for [" + PathToString(filePath) + ":" + to_string(chunk.start) + "] (" + to_string((chunk.end - chunk.start) / 1024) + " / " + to_string(chunk.fileSize / 1024) + " KB)");
        return FileChunkTransient(chunk);
      }
    }
  }

  SharedByteArray fileContentsPtr = make_shared<vector<uint8_t>>();
  size_t fileSize = 0;
  size_t actualReadSize = 0;
  if (!FileReadPartial(filePath, *(fileContentsPtr.get()), start, end - start, &fileSize, &actualReadSize) || fileContentsPtr->empty()) {
    m_CachedFileContents.erase(filePath);
    fileContentsPtr.reset();
    return FileChunkTransient();
  }

#ifdef DEBUG
  if (MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print("[AURA] Cached map file contents in-memory for [" + PathToString(filePath) + ":" + to_string(start) + "] ( " + to_string(actualReadSize / 1024) + " / " + to_string(fileSize / 1024) + " KB)");
  }
#endif
  m_CachedFileContents[filePath] = FileChunkCached(fileSize, start, start + actualReadSize, fileContentsPtr);

  // Try to dedupe across maps with different names but same content.
  for (const auto& cacheEntries : m_CachedFileContents) {
    if (cacheEntries.first == filePath) continue;
    const FileChunkCached& otherFileChunk = cacheEntries.second;
    WeakByteArray maybeCachedPtr = otherFileChunk.bytes;
    if (maybeCachedPtr.expired()) {
      continue;
    }
    SharedByteArray otherContentsPtr = maybeCachedPtr.lock();
    if (otherContentsPtr->size() != fileContentsPtr->size()) {
      continue;
    }
    if (memcmp(otherContentsPtr->data(), fileContentsPtr->data(), fileContentsPtr->size()) == 0) {
      //Print("[DEBUG] Reusing cached " + to_string((otherFileChunk.end - otherFileChunk.start) / 1024) + " KB from [" + PathToString(cacheEntries.first) + "] for [" + PathToString(filePath) + "]");
      //fileContentsPtr = otherContentsPtr;
      m_CachedFileContents[filePath] = FileChunkCached(otherFileChunk.fileSize, otherFileChunk.start, otherFileChunk.end, otherContentsPtr);
      // Iterator is invalid now
      break;
    }
  }

  // Prevent the cache from being filled with stale chunk data
  ClearStaleFileChunks();

  return FileChunkTransient(m_CachedFileContents[filePath]);
}

SharedByteArray CAura::ReadFileCacheable(const std::filesystem::path& filePath, const size_t maxSize)
{
  return ReadFileChunkCacheable(filePath, 0, 0xFFFFFFFF).bytes;
}

string CAura::GetSudoAuthPayload(const string& payload)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  // Generate random hex digits
  string result;
  result.reserve(21 + payload.length());

  for (size_t i = 0; i < 20; ++i) {
      const int randomDigit = dis(gen);
      result += (randomDigit < 10) ? (char)('0' + randomDigit) : (char)('a' + (randomDigit - 10));
  }

  result += " " + payload;
  m_SudoAuthPayload = result;
  return result;
}
