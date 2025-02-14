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

#ifndef AURA_AURA_H_
#define AURA_AURA_H_

#include "includes.h"
#include "config/config.h"
#include "config/config_bot.h"
#include "config/config_realm.h"
#include "config/config_game.h"
#include "cli.h"
#include "irc.h"
#include "discord.h"
#include "command.h"
#include "net.h"
#include "game_setup.h"

#include <sha1/sha1.h>
#include <random>
#include <filesystem>

#ifdef _WIN32
#pragma once
#include <windows.h>
#endif

#define AURA_VERSION "3.0.0.dev"
#define AURA_APP_NAME "Aura 3.0.0.dev"
#define AURA_REPOSITORY_URL "https://gitlab.com/ivojulca/aura-bot"
#define AURA_ISSUES_URL "https://gitlab.com/ivojulca/aura-bot/-/issues"

//
// CAura
//

class CAura
{
public:
  bool                                               m_ScriptsExtracted;           // indicates if there's lacking configuration info so we can quit
  bool                                               m_Exiting;                    // set to true to force aura to shutdown next update (used by SignalCatcher)
  bool                                               m_ExitingSoon;                // set to true to let aura gracefully stop all services and network traffic, and shutdown once done
  bool                                               m_Ready;                      // indicates if there's lacking configuration info so we can quit
  bool                                               m_AutoReHosted;               // whether our autorehost game setup has been used for one of the active lobbies

  uint8_t                                            m_LogLevel;
  uint8_t                                            m_GameVersion;
  uint8_t                                            m_MaxSlots;

  uint32_t                                           m_LastServerID;
  uint32_t                                           m_HostCounter;                // the current host counter (a unique number to identify a game, incremented each time a game is created)
  uint32_t                                           m_ReplacingLobbiesCounter;
  uint64_t                                           m_HistoryGameID;
  size_t                                             m_MaxGameNameSize;

  CRealmConfig*                                      m_RealmDefaultConfig;
  CGameConfig*                                       m_GameDefaultConfig;
  CCommandConfig*                                    m_CommandDefaultConfig;

  CAuraDB*                                           m_DB;                         // database
  std::shared_ptr<CGameSetup>                        m_GameSetup;                  // the currently loaded map
  std::shared_ptr<CGameSetup>                        m_AutoRehostGameSetup;        // game setup to be rehosted whenever free

  std::shared_ptr<CCommandContext>                   m_ReloadContext;
  std::shared_ptr<CCommandContext>                   m_SudoContext;

  std::optional<int64_t>                             m_LastGameHostedTicks;
  std::optional<int64_t>                             m_LastGameAutoHostedTicks;

  std::string                                        m_SudoAuthPayload;
  std::string                                        m_SudoExecCommand;

  std::string                                        m_Version;                    // Aura version string
  std::string                                        m_RepositoryURL;              // Aura repository URL
  std::string                                        m_IssuesURL;                  // Aura issues URL

  std::vector<std::weak_ptr<CCommandContext>>        m_ActiveContexts;             // declare before command sources, to ensure m_ActiveContexts is destroyed after them

  CSHA1                                              m_SHA;                        // for calculating SHA1's
  CDiscord                                           m_Discord;                    // Discord client
  CIRC                                               m_IRC;                        // IRC client
  CNet                                               m_Net;                        // network manager
  CBotConfig                                         m_Config;
  std::filesystem::path                              m_ConfigPath;
  std::filesystem::path                              m_GameInstallPath;

  std::queue<GenericAppAction>                       m_PendingActions;
  std::vector<CRealm*>                               m_Realms;                     // all our battle.net clients (there can be more than one)
  std::vector<CGame*>                                m_StartedGames;               // all games after they have started
  std::vector<CGame*>                                m_Lobbies;                    // all games before they are started
  std::vector<CGame*>                                m_LobbiesPending;             // vector for just-created lobbies before they get into m_Lobbies
  std::vector<CGame*>                                m_JoinInProgressGames;        // started games that can be joined in-progress (either as observer or player)

  std::map<std::filesystem::path, std::string>       m_CFGCacheNamesByMapNames;
  std::map<std::filesystem::path, TimedUint16>       m_MapFilesTimedBusyLocks;
  std::map<std::filesystem::path, FileChunkCached>   m_CachedFileContents;
  std::map<std::string, std::string>                 m_LastMapIdentifiersFromSuggestions;

  std::vector<std::string>                           m_RealmsIdentifiers;
  std::map<uint8_t, CRealm*>                         m_RealmsByHostCounter;
  std::map<std::string, CRealm*>                     m_RealmsByInputID;

  explicit CAura(CConfig& CFG, const CCLI& nCLI);
  ~CAura();
  CAura(CAura&) = delete;

  [[nodiscard]] CGame* GetMostRecentLobby(bool allowPending = false) const;
  [[nodiscard]] CGame* GetMostRecentLobbyFromCreator(const std::string& fromName) const;
  [[nodiscard]] CGame* GetLobbyByHostCounter(uint32_t hostCounter) const;
  [[nodiscard]] CGame* GetLobbyByHostCounterExact(uint32_t hostCounter) const;
  [[nodiscard]] CGame* GetGameByIdentifier(const uint64_t gameIdentifier) const;
  [[nodiscard]] CGame* GetGameByString(const std::string& targetGame) const;

  [[nodiscard]] CRealm* GetRealmByInputId(const std::string& inputId) const;
  [[nodiscard]] CRealm* GetRealmByHostCounter(const uint8_t hostCounter) const;
  [[nodiscard]] CRealm* GetRealmByHostName(const std::string& hostName) const;
  [[nodiscard]] uint8_t FindServiceFromHostName(const std::string& hostName, void*& location) const;

  [[nodiscard]] bool MergePendingLobbies();
  void TrackGameJoinInProgress(CGame* game);
  void UntrackGameJoinInProgress(CGame* game);

  bool QueueConfigReload(std::shared_ptr<CCommandContext> nCtx);

  // identifier generators

  uint32_t NextHostCounter();
  uint64_t NextHistoryGameID();
  uint32_t NextServerID();

  std::string GetSudoAuthPayload(const std::string& payload);

  // processing functions

  uint8_t HandleAction(const AppAction& action);
  uint8_t HandleDeferredCommandContext(const LazyCommandContext& lazyCtx);
  uint8_t HandleGenericAction(const GenericAppAction& genAction);
  bool Update();
  void AwaitSettled();
  inline bool GetReady() const { return m_Ready; }

  bool GetNewGameIsInQuota() const;
  bool GetNewGameIsInQuotaReplace() const;
  bool GetNewGameIsInQuotaConservative() const;
  bool GetNewGameIsInQuotaAutoReHost() const;
  bool CreateGame(std::shared_ptr<CGameSetup> gameSetup);
  bool GetIsAutoHostThrottled() const;

  inline bool GetIsAdvertisingGames() { return !m_Lobbies.empty() || !m_JoinInProgressGames.empty(); }
  inline bool GetHasGames() { return !m_StartedGames.empty() || !m_Lobbies.empty(); }

  // events

  void EventBNETGameRefreshSuccess(CRealm* realm);
  void EventBNETGameRefreshError(CRealm* realm);
  void EventGameDeleted(CGame* game);
  void EventGameRemake(CGame* game);
  void EventGameStarted(CGame* game);

  // other functions

  [[nodiscard]] bool ReloadConfigs();
  void TryReloadConfigs();
  bool LoadDefaultConfigs(CConfig& CFG, CNetConfig* netConfig);
  bool LoadAllConfigs(CConfig& CFG);
  void OnLoadConfigs();
  bool LoadBNETs(CConfig& CFG, std::bitset<120>& definedConfigs);

  uint8_t ExtractScripts();
  bool CopyScripts();
  void ClearAutoRehost();

  void LoadMapAliases();
  void LoadIPToCountryData(const CConfig& CFG);
  void InitContextMenu();
  void InitPathVariable();
  void InitSystem();
  void UpdateWindowTitle();
  void UpdateMetaData();

  [[nodiscard]] FileChunkTransient ReadFileChunkCacheable(const std::filesystem::path& filePath, const size_t start, const size_t end)/* noexcept*/;
  [[nodiscard]] SharedByteArray ReadFileCacheable(const std::filesystem::path& filePath, const size_t maxSize)/* noexcept*/;
  void UpdateCFGCacheEntries();

  void ClearStaleContexts();
  void ClearStaleFileChunks();
  
  inline bool MatchLogLevel(const uint8_t logLevel) { return logLevel <= m_LogLevel; } // 1: emergency ... 9: trace
  void LogPersistent(const std::string& logText);
  void GracefulExit();
  bool CheckGracefulExit();
};

#endif // AURA_AURA_H_
