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

#ifndef AURA_GAMESETUP_H_
#define AURA_GAMESETUP_H_

#include "includes.h"
#include "socket.h"

#include <atomic>
#include <filesystem>
#include <regex>

#ifndef DISABLE_CPR
#include <cpr/cpr.h>
#endif

inline std::vector<std::pair<std::string, int>> ExtractEpicWarMaps(const std::string &s, const int maxCount) {
  std::regex pattern(R"(<a href="/maps/(\d+)/"><b>([^<\n]+)</b></a>)");
  std::vector<std::pair<std::string, int>> output;

  std::sregex_iterator iter(s.begin(), s.end(), pattern);
  std::sregex_iterator end;

  int count = 0;
  while (iter != end && count < maxCount) {
    std::smatch match = *iter;
    output.emplace_back(std::make_pair(match[2], std::stoi(match[1])));
    ++iter;
    ++count;
  }

  return output;
}

//
// CGameExtraOptions
//

class CGameExtraOptions
{
public:
  CGameExtraOptions();
  CGameExtraOptions(const std::optional<bool>& nRandomRaces, const std::optional<bool>& nRandomHeroes, const std::optional<uint8_t>& nVisibility, const std::optional<uint8_t>& nSpeed, const std::optional<uint8_t>& nObservers);
  ~CGameExtraOptions();

  std::optional<bool>         m_TeamsLocked;
  std::optional<bool>         m_TeamsTogether;
  std::optional<bool>         m_AdvancedSharedUnitControl;
  std::optional<bool>         m_RandomRaces;
  std::optional<bool>         m_RandomHeroes;
  std::optional<uint8_t>      m_Visibility;
  std::optional<uint8_t>      m_Speed;
  std::optional<uint8_t>      m_Observers;

  bool ParseMapObservers(const std::string& s);
  bool ParseMapVisibility(const std::string& s);
  bool ParseMapSpeed(const std::string& s);
  bool ParseMapRandomRaces(const std::string& s);
  bool ParseMapRandomHeroes(const std::string& s);

  void AcquireCLI(const CCLI* nCLI);
};

//
// CGameSetup
//

class CGameSetup : public std::enable_shared_from_this<CGameSetup>
{
public:
  CAura*                                          m_Aura;
  CSaveGame*                                      m_RestoredGame;
  std::shared_ptr<CMap>                           m_Map;
  std::shared_ptr<CCommandContext>                m_Ctx;

  std::string                                     m_Attribution;
  std::string                                     m_SearchRawTarget;
  uint8_t                                         m_SearchType;
  bool                                            m_AllowPaths;
  bool                                            m_StandardPaths;
  bool                                            m_LuckyMode;
  bool                                            m_Verbose;
  std::pair<std::string, std::string>             m_SearchTarget;

  bool                                            m_FoundSuggestions;
  bool                                            m_IsDownloadable;
  bool                                            m_IsStepDownloading;
  bool                                            m_IsStepDownloaded;
  std::string                                     m_BaseDownloadFileName;
  std::string                                     m_MapDownloadUri;
  uint32_t                                        m_MapDownloadSize;
  std::string                                     m_MapSiteUri;
  std::filesystem::path                           m_DownloadFilePath;
  std::ofstream*                                  m_DownloadFileStream;
#ifndef DISABLE_CPR
  std::future<uint32_t>                           m_DownloadFuture;
#endif
  int32_t                                         m_DownloadTimeout;
  int32_t                                         m_SuggestionsTimeout;
  std::optional<int64_t>                          m_ActiveTicks;
  std::string                                     m_ErrorMessage;
  uint8_t                                         m_AsyncStep;

  bool                                            m_IsMapDownloaded;

  std::filesystem::path                           m_SaveFile;

  std::string                                     m_Name;
  std::string                                     m_BaseName;
  bool                                            m_OwnerLess;
  std::pair<std::string, std::string>             m_Owner;
  std::optional<uint32_t>                         m_Identifier;
  std::optional<uint32_t>                         m_ChannelKey;
  std::optional<bool>                             m_ChecksReservation;
  std::vector<std::string>                        m_Reservations;
  bool                                            m_IsMirror;
  uint8_t                                         m_RealmsDisplayMode;
  sockaddr_storage                                m_RealmsAddress;
  std::set<std::string>                           m_RealmsExcluded;
  std::vector<uint8_t>                            m_SupportedGameVersions;

  bool                                            m_LobbyReplaceable;
  bool                                            m_LobbyAutoRehosted;
  uint16_t                                        m_CreationCounter;

  std::optional<uint8_t>                          m_LobbyTimeoutMode;
  std::optional<uint8_t>                          m_LobbyOwnerTimeoutMode;
  std::optional<uint8_t>                          m_LoadingTimeoutMode;
  std::optional<uint8_t>                          m_PlayingTimeoutMode;

  std::optional<uint32_t>                         m_LobbyTimeout;
  std::optional<uint32_t>                         m_LobbyOwnerTimeout;
  std::optional<uint32_t>                         m_LoadingTimeout;
  std::optional<uint32_t>                         m_PlayingTimeout;

  std::optional<uint8_t>                          m_PlayingTimeoutWarningShortCountDown;
  std::optional<uint32_t>                         m_PlayingTimeoutWarningShortInterval;
  std::optional<uint8_t>                          m_PlayingTimeoutWarningLargeCountDown;
  std::optional<uint32_t>                         m_PlayingTimeoutWarningLargeInterval;

  std::optional<bool>                             m_LobbyOwnerReleaseLANLeaver;

  std::optional<uint32_t>                         m_LobbyCountDownInterval;
  std::optional<uint32_t>                         m_LobbyCountDownStartValue;

  std::optional<uint8_t>                          m_AutoStartPlayers;
  std::optional<int64_t>                          m_AutoStartSeconds;
  std::optional<uint8_t>                          m_ReconnectionMode;
  std::optional<uint8_t>                          m_IPFloodHandler;
  std::optional<uint8_t>                          m_UnsafeNameHandler;
  std::optional<uint8_t>                          m_BroadcastErrorHandler;
  std::optional<uint16_t>                         m_LatencyAverage;
  std::optional<uint16_t>                         m_LatencyMaxFrames;
  std::optional<uint16_t>                         m_LatencySafeFrames;
  std::optional<bool>                             m_LatencyEqualizerEnabled;
  std::optional<uint8_t>                          m_LatencyEqualizerFrames;
  std::optional<std::string>                      m_HCL;
  std::optional<uint8_t>                          m_CustomLayout;
  std::optional<bool>                             m_CheckJoinable;
  std::optional<bool>                             m_NotifyJoins;
  std::optional<bool>                             m_HideLobbyNames;
  std::optional<uint8_t>                          m_HideInGameNames;
  std::optional<bool>                             m_LoadInGame;
  std::optional<bool>                             m_EnableJoinObserversInProgress;
  std::optional<bool>                             m_EnableJoinPlayersInProgress;
  std::optional<bool>                             m_LogCommands;
  std::optional<uint8_t>                          m_NumPlayersToStartGameOver;
  std::optional<uint8_t>                          m_PlayersReadyMode;
  std::optional<bool>                             m_AutoStartRequiresBalance;
  std::optional<uint32_t>                         m_AutoKickPing;
  std::optional<uint32_t>                         m_WarnHighPing;
  std::optional<uint32_t>                         m_SafeHighPing;
  std::optional<bool>                             m_SyncNormalize;

  std::string                                     m_CreatedBy;
  void*                                           m_CreatedFrom;
  uint8_t                                         m_CreatedFromType;

  CGameExtraOptions*                              m_MapExtraOptions;
  uint8_t                                         m_MapReadyCallbackAction;
  std::string                                     m_MapReadyCallbackData;

  std::atomic<bool>                               m_ExitingSoon;
  bool                                            m_DeleteMe;

  CGameSetup(CAura* nAura, std::shared_ptr<CCommandContext> nCtx, CConfig* mapCFG);
  CGameSetup(CAura* nAura, std::shared_ptr<CCommandContext> nCtx, const std::string nSearchRawTarget, const uint8_t nSearchType, const bool nAllowPaths, const bool nUseStandardPaths, const bool nUseLuckyMode);
  ~CGameSetup();

  [[nodiscard]] std::string GetInspectName() const;
  [[nodiscard]] bool GetDeleteMe() const { return m_DeleteMe; }
  [[nodiscard]] bool GetIsStale() const;

  void ParseInputLocal();
  void ParseInput();
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInputStandard();
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInputAlias();
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInputLocalExact();
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInputLocalTryExtensions();
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInputLocalFuzzy(std::vector<std::string>& fuzzyMatches);
#ifndef DISABLE_CPR
  void SearchInputRemoteFuzzy(std::vector<std::string>& fuzzyMatches);
#endif
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInputLocal(std::vector<std::string>& fuzzyMatches);
  [[nodiscard]] std::pair<uint8_t, std::filesystem::path> SearchInput();
  inline std::shared_ptr<CMap> GetMap() const { return m_Map; }
  [[nodiscard]] std::shared_ptr<CMap> GetBaseMapFromConfig(CConfig* mapCFG, const bool silent);
  [[nodiscard]] std::shared_ptr<CMap> GetBaseMapFromConfigFile(const std::filesystem::path& filePath, const bool isCache, const bool silent);
  [[nodiscard]] std::shared_ptr<CMap> GetBaseMapFromMapFile(const std::filesystem::path& filePath, const bool silent);
  [[nodiscard]] std::shared_ptr<CMap> GetBaseMapFromMapFileOrCache(const std::filesystem::path& mapPath, const bool silent);
  bool ApplyMapModifiers(CGameExtraOptions* extraOptions);
#ifndef DISABLE_CPR
  [[nodiscard]] uint32_t ResolveMapRepositoryTask();
  void RunResolveMapRepository();
  [[nodiscard]] uint32_t RunResolveMapRepositorySync();
  void SetDownloadFilePath(std::filesystem::path&& filePath);
  [[nodiscard]] bool PrepareDownloadMap();
  void RunDownloadMap();
  [[nodiscard]] uint32_t DownloadMapTask();
  [[nodiscard]] uint32_t RunDownloadMapSync();
  void OnResolveMapSuccess();
  void OnDownloadMapSuccess();
  void OnFetchSuggestionsEnd();
  [[nodiscard]] std::vector<std::pair<std::string, std::string>> GetMapRepositorySuggestions(const std::string & pattern, const uint8_t maxCount);


#endif
  [[nodiscard]] bool GetMapLoaded() const;
  void LoadMap();
  [[nodiscard]] bool LoadMapSync();
  void OnLoadMapSuccess();
  void OnLoadMapError();
  void SetActive();
  [[nodiscard]] bool RestoreFromSaveFile();
  bool RunHost();

  inline bool GetIsMirror() const { return m_IsMirror; }
  inline bool GetIsDownloading() const { return m_IsStepDownloading; }
  inline bool GetHasBeenHosted() const { return m_CreationCounter > 0; }

  [[nodiscard]] bool SetMirrorSource(const sockaddr_storage& nSourceAddress, const uint32_t nGameIdentifier);
  [[nodiscard]] bool SetMirrorSource(const std::string& nInput);
  void AddIgnoredRealm(const CRealm* nRealm);
  void RemoveIgnoredRealm(const CRealm* nRealm);
  void SetDisplayMode(const uint8_t nDisplayMode);
  void SetOwner(const std::string& nOwner, const CRealm* nRealm);
  void SetOwnerLess(const bool nValue) { m_OwnerLess = nValue; }
  void SetCreator(const std::string& nCreator);
  void SetCreator(const std::string& nCreator, CGame* nGame);
  void SetCreator(const std::string& nCreator, CRealm* nRealm);
  void SetCreator(const std::string& nCreator, CIRC* nIRC);
  void SetCreator(const std::string& nCreator, CDiscord* nDiscord);
  void RemoveCreator();
  [[nodiscard]] bool MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const;
  void SetName(const std::string& nName) { m_Name = nName; }
  void SetBaseName(const std::string& nName) {
    m_Name = nName;
    m_BaseName = nName;
  }
  void SetOwner(const std::string& ownerName, const std::string& ownerRealm) {
    m_Owner = std::make_pair(ownerName, ownerRealm);
  }

  void SetLobbyTimeoutMode(const uint8_t nMode) { m_LobbyTimeoutMode = nMode; }
  void SetLobbyOwnerTimeoutMode(const uint8_t nMode) { m_LobbyOwnerTimeoutMode = nMode; }
  void SetLoadingTimeoutMode(const uint8_t nMode) { m_LoadingTimeoutMode = nMode; }
  void SetPlayingTimeoutMode(const uint8_t nMode) { m_PlayingTimeoutMode = nMode; }

  void SetLobbyTimeout(const uint32_t nTimeout) { m_LobbyTimeout = nTimeout; }
  void SetLobbyOwnerTimeout(const uint32_t nTimeout) { m_LobbyOwnerTimeout = nTimeout; }
  void SetLoadingTimeout(const uint32_t nTimeout) { m_LoadingTimeout = nTimeout; }
  void SetPlayingTimeout(const uint32_t nTimeout) { m_PlayingTimeout = nTimeout; }

  void SetPlayingTimeoutWarningShortCountDown(const uint8_t nMode) { m_PlayingTimeoutWarningShortCountDown = nMode; }
  void SetPlayingTimeoutWarningShortInterval(const uint32_t nTimeout) { m_PlayingTimeoutWarningShortInterval = nTimeout; }
  void SetPlayingTimeoutWarningLargeCountDown(const uint8_t nMode) { m_PlayingTimeoutWarningLargeCountDown = nMode; }
  void SetPlayingTimeoutWarningLargeInterval(const uint32_t nTimeout) { m_PlayingTimeoutWarningLargeInterval = nTimeout; }

  void SetLobbyOwnerReleaseLANLeaver(const bool nRelease) { m_LobbyOwnerReleaseLANLeaver = nRelease; }

  void SetLobbyCountDownInterval(const uint32_t nValue) { m_LobbyCountDownInterval = nValue; }
  void SetLobbyCountDownStartValue(const uint32_t nValue) { m_LobbyCountDownStartValue = nValue; }

  void SetLobbyReplaceable(const bool nReplaceable) { m_LobbyReplaceable = nReplaceable; }
  void SetLobbyAutoRehosted(const bool nRehosted) { m_LobbyAutoRehosted = nRehosted; }
  void SetDownloadTimeout(const uint32_t nTimeout) { m_DownloadTimeout = nTimeout; }
  void SetIsCheckJoinable(const bool nCheckJoinable) { m_CheckJoinable = nCheckJoinable; }
  void SetNotifyJoins(const bool nNotifyJoins) { m_NotifyJoins = nNotifyJoins; }
  void SetVerbose(const bool nVerbose) { m_Verbose = nVerbose; }
  void SetContext(std::shared_ptr<CCommandContext> nCtx) { m_Ctx = nCtx; }
  void SetMapReadyCallback(const uint8_t action, const std::string& data) {
    m_MapReadyCallbackAction = action;
    m_MapReadyCallbackData = data;
  }
  void SetMapExtraOptions(CGameExtraOptions* opts) { m_MapExtraOptions = opts; }
  void SetGameSavedFile(const std::filesystem::path& filePath);
  void SetCheckReservation(const bool nChecksReservation) { m_ChecksReservation = nChecksReservation; }
  void SetReservations(const std::vector<std::string>& nReservations) { m_Reservations = nReservations; }
  void SetSupportedGameVersions(const std::vector<uint8_t>& nVersions) { m_SupportedGameVersions = nVersions; }
  void SetAutoStartPlayers(const uint8_t nValue) { m_AutoStartPlayers = nValue; }
  void SetAutoStartSeconds(const int64_t nValue) { m_AutoStartSeconds = nValue; }
  void SetReconnectionMode(const uint8_t nValue) { m_ReconnectionMode = nValue;}
  void SetIPFloodHandler(const uint8_t nValue) { m_IPFloodHandler = nValue;}
  void SetUnsafeNameHandler(const uint8_t nValue) { m_UnsafeNameHandler = nValue;}
  void SetBroadcastErrorHandler(const uint8_t nValue) { m_BroadcastErrorHandler = nValue;}
  void SetLatencyAverage(const uint16_t nValue) { m_LatencyAverage = nValue; }
  void SetLatencyMaxFrames(const uint16_t nValue) { m_LatencyMaxFrames = nValue; }
  void SetLatencySafeFrames(const uint16_t nValue) { m_LatencySafeFrames = nValue; }
  void SetLatencyEqualizerEnabled(const bool nValue) { m_LatencyEqualizerEnabled = nValue; }
  void SetLatencyEqualizerFrames(const uint8_t nValue) { m_LatencyEqualizerFrames = nValue; }
  void SetHCL(const std::string& nHCL) { m_HCL = nHCL; }
  void SetCustomLayout(const uint8_t nLayout) { m_CustomLayout = nLayout; }

  void SetNumPlayersToStartGameOver(const uint8_t nNumPlayersToStartGameOver) { m_NumPlayersToStartGameOver = nNumPlayersToStartGameOver; }
  void SetAutoKickPing(const uint8_t nAutoKickPing) { m_AutoKickPing = nAutoKickPing; }
  void SetWarnKickPing(const uint32_t nWarnHighPing) { m_WarnHighPing = nWarnHighPing; }
  void SetSafeKickPing(const uint32_t nSafeHighPing) { m_SafeHighPing = nSafeHighPing; }
  void SetSyncNormalize(const bool nSyncNormalize) { m_SyncNormalize = nSyncNormalize; }
  void SetHideLobbyNames(const bool nHideLobbyNames) { m_HideLobbyNames = nHideLobbyNames; }
  void SetHideInGameNames(const uint8_t nHideInGameNames) { m_HideInGameNames = nHideInGameNames; }
  void SetLoadInGame(const bool nGameLoadInGame) { m_LoadInGame = nGameLoadInGame; }
  void SetEnableJoinObserversInProgress(const bool nGameEnableJoinObserversInProgress) { m_EnableJoinObserversInProgress = nGameEnableJoinObserversInProgress; }
  void SetEnableJoinPlayersInProgress(const bool nGameEnableJoinPlayersInProgress) { m_EnableJoinPlayersInProgress = nGameEnableJoinPlayersInProgress; }

  void SetLogCommands(const bool nLogCommands) { m_LogCommands = nLogCommands; }
  void SetAutoStartRequiresBalance(const bool nRequiresBalance) { m_AutoStartRequiresBalance = nRequiresBalance; }

  void AcquireCLISimple(const CCLI* nCLI);
  void ClearExtraOptions();

  void OnGameCreate();
  [[nodiscard]] bool Update();
  void AwaitSettled();
};

#endif // AURA_GAMESETUP_H_
