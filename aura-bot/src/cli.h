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

#ifndef AURA_CLI_H_
#define AURA_CLI_H_

#include "includes.h"
#include "action.h"

#include <filesystem>
#include <CLI11/CLI11.hpp>

#define CLI_ACTION_ABOUT 1
#define CLI_ACTION_EXAMPLES 2

// CLIResult

enum class CLIResult : uint8_t
{
  kOk            = 0u,
  kError         = 1u,
  kInfoAndQuit   = 2u,
  kConfigAndQuit = 3u,
};

//
// CCLI
//

class CCLI
{
public:
  std::optional<std::filesystem::path>  m_CFGAdapterPath;
  std::optional<std::filesystem::path>  m_CFGPath;
  std::optional<std::filesystem::path>  m_HomePath;
  bool                                  m_UseStandardPaths;

  uint8_t                               m_InfoAction;

  bool                                  m_Verbose;
  std::optional<bool>                   m_LAN;
  std::optional<bool>                   m_BNET;
  std::optional<bool>                   m_IRC;
  std::optional<bool>                   m_Discord;
  std::optional<bool>                   m_ExitOnStandby;
  std::optional<bool>                   m_UseMapCFGCache;
  std::optional<std::string>            m_BindAddress;
  std::optional<uint16_t>               m_HostPort;
  std::optional<std::string>            m_LANMode;
  std::optional<std::string>            m_LogLevel;
  std::optional<bool>                   m_InitSystem;

  std::optional<uint8_t>                m_War3Version;
  std::optional<std::filesystem::path>  m_War3Path;
  std::optional<std::filesystem::path>  m_MapPath;
  std::optional<std::filesystem::path>  m_MapCFGPath;
  std::optional<std::filesystem::path>  m_MapCachePath;
  std::optional<std::filesystem::path>  m_JASSPath;
  std::optional<std::filesystem::path>  m_GameSavePath;
  std::optional<bool>                   m_ExtractJASS;

  // Host flags
  std::optional<std::string>            m_SearchTarget;
  std::optional<std::string>            m_SearchType;
  std::optional<std::string>            m_GameName;
  std::optional<bool>                   m_GameTeamsLocked;
  std::optional<bool>                   m_GameTeamsTogether;
  std::optional<bool>                   m_GameAdvancedSharedUnitControl;
  std::optional<bool>                   m_GameRandomRaces;
  std::optional<bool>                   m_GameRandomHeroes;
  std::optional<std::string>            m_GameObservers;
  std::optional<std::string>            m_GameVisibility;
  std::optional<std::string>            m_GameSpeed;
  std::optional<std::string>            m_GameOwner;
  std::optional<bool>                   m_GameOwnerLess;
  std::vector<std::string>              m_ExcludedRealms;
  std::optional<std::string>            m_MirrorSource;

  std::optional<std::string>            m_GameLobbyTimeoutMode;
  std::optional<std::string>            m_GameLobbyOwnerTimeoutMode;
  std::optional<std::string>            m_GameLoadingTimeoutMode;
  std::optional<std::string>            m_GamePlayingTimeoutMode;

  std::optional<uint32_t>               m_GameLobbyTimeout;
  std::optional<uint32_t>               m_GameLobbyOwnerTimeout;
  std::optional<uint32_t>               m_GameLoadingTimeout;
  std::optional<uint32_t>               m_GamePlayingTimeout;

  std::optional<uint8_t>                m_GamePlayingTimeoutWarningShortCountDown;
  std::optional<uint32_t>               m_GamePlayingTimeoutWarningShortInterval;
  std::optional<uint8_t>                m_GamePlayingTimeoutWarningLargeCountDown;
  std::optional<uint32_t>               m_GamePlayingTimeoutWarningLargeInterval;

  std::optional<bool>                   m_GameLobbyOwnerReleaseLANLeaver;

  std::optional<uint32_t>               m_GameLobbyCountDownInterval;
  std::optional<uint32_t>               m_GameLobbyCountDownStartValue;

  std::optional<uint8_t>                m_GameAutoStartPlayers;
  std::optional<int64_t>                m_GameAutoStartSeconds;

  std::optional<uint16_t>               m_GameLatencyAverage;
  std::optional<uint16_t>               m_GameLatencyMaxFrames;
  std::optional<uint16_t>               m_GameLatencySafeFrames;
  std::optional<bool>                   m_GameLatencyEqualizerEnabled;
  std::optional<uint8_t>                m_GameLatencyEqualizerFrames;

  std::optional<uint32_t>               m_GameMapDownloadTimeout;
  std::optional<bool>                   m_GameCheckJoinable;
  std::optional<bool>                   m_GameNotifyJoins;
  std::optional<bool>                   m_GameLobbyReplaceable;
  std::optional<bool>                   m_GameLobbyAutoRehosted;
  std::optional<bool>                   m_GameCheckReservation;
  std::optional<std::string>            m_GameHCL;
  std::optional<bool>                   m_GameFreeForAll;
  std::optional<uint8_t>                m_GameNumPlayersToStartGameOver;
  std::optional<uint8_t>                m_GamePlayersReadyMode;
  std::optional<uint32_t>               m_GameAutoKickPing;
  std::optional<uint32_t>               m_GameWarnHighPing;
  std::optional<uint32_t>               m_GameSafeHighPing;
  std::optional<bool>                   m_GameSyncNormalize;
  std::vector<std::string>              m_GameReservations;
  std::vector<uint8_t>                  m_GameCrossplayVersions;
  std::optional<bool>                   m_CheckMapVersion;
  std::optional<std::filesystem::path>  m_GameSavedPath;
  std::optional<std::string>            m_GameReconnectionMode;
  std::optional<std::string>            m_GameMapAlias;
  std::optional<std::string>            m_GameDisplayMode;
  std::optional<std::string>            m_GameIPFloodHandler;
  std::optional<std::string>            m_GameUnsafeNameHandler;
  std::optional<std::string>            m_GameBroadcastErrorHandler;
  std::optional<bool>                   m_GameHideLobbyNames;
  std::optional<std::string>            m_GameHideLoadedNames;
  std::optional<bool>                   m_GameLoadInGame;
  std::optional<bool>                   m_GameEnableJoinObserversInProgress;
  std::optional<bool>                   m_GameEnableJoinPlayersInProgress;
  std::optional<bool>                   m_GameLogCommands;
  std::optional<bool>                   m_GameAutoStartRequiresBalance;

  // UPnP
  std::optional<bool>                   m_EnableUPnP;
  std::vector<uint16_t>                 m_PortForwardTCP;
  std::vector<uint16_t>                 m_PortForwardUDP;

  // Command queue
  std::optional<std::string>            m_ExecAs;
  CommandAuth                           m_ExecAuth;
  std::string                           m_ExecGame;
  std::vector<std::string>              m_ExecCommands;
  bool                                  m_ExecBroadcast;

  CCLI();
  ~CCLI();

  // Parsing stuff
  //CLI::Validator GetIsFullyQualifiedUserValidator();
  CLIResult Parse(const int argc, char** argv);
  [[nodiscard]] uint8_t GetGameSearchType() const;
  [[nodiscard]] uint8_t GetGameLobbyTimeoutMode() const;
  [[nodiscard]] uint8_t GetGameLobbyOwnerTimeoutMode() const;
  [[nodiscard]] uint8_t GetGameLoadingTimeoutMode() const;
  [[nodiscard]] uint8_t GetGamePlayingTimeoutMode() const;
  [[nodiscard]] uint8_t GetGameReconnectionMode() const;
  [[nodiscard]] uint8_t GetGameDisplayType() const;
  [[nodiscard]] uint8_t GetGameIPFloodHandler() const;
  [[nodiscard]] uint8_t GetGameUnsafeNameHandler() const;
  [[nodiscard]] uint8_t GetGameBroadcastErrorHandler() const;
  [[nodiscard]] uint8_t GetGameHideLoadedNames() const;
  [[nodiscard]] bool CheckGameParameters() const;
  [[nodiscard]] bool CheckGameLoadParameters(std::shared_ptr<CGameSetup> nGameSetup) const;

  void RunInfoActions() const;
  void OverrideConfig(CAura* nAura) const;
  bool QueueActions(CAura* nAura) const;
  [[nodiscard]] inline std::optional<bool> GetInitSystem() const { return m_InitSystem; };
};

#endif // AURA_CLI_H_
