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

#ifndef AURA_MAP_H_
#define AURA_MAP_H_

#include "includes.h"
#include "file_util.h"
#include "game_slot.h"
#include "util.h"

#include <iterator>
#include <cctype>

#pragma once

#define ROTL(x, n) ((x) << (n)) | ((x) >> (32 - (n))) // this won't work with signed types
#define ROTR(x, n) ((x) >> (n)) | ((x) << (32 - (n))) // this won't work with signed types

//
// MapEssentials
//

struct MapEssentials
{
  bool melee;
  uint8_t numPlayers;
  uint8_t numDisabled;
  uint8_t numTeams;
  uint8_t minCompatibleGameVersion;
  uint8_t minSuggestedGameVersion;
  uint32_t editorVersion;
  uint32_t options;
  std::optional<std::array<uint8_t, 2>> width;
  std::optional<std::array<uint8_t, 2>> height;
  std::optional<std::array<uint8_t, 4>> weakHash;
  std::optional<std::array<uint8_t, 20>> sha1;
  std::optional<std::array<uint8_t, 20>> hash;
  std::vector<CGameSlot> slots;

  MapEssentials()
   : melee(false),
     numPlayers(0),
     numDisabled(0),
     numTeams(0),
     minCompatibleGameVersion(0),
     minSuggestedGameVersion(0),
     editorVersion(0),
     options(0)
  {
  }
  ~MapEssentials() = default;
};

//
// CMap
//

class CMap
{
public:
  CAura* m_Aura;

  std::optional<uint8_t>                m_NumPlayersToStartGameOver;
  std::optional<uint8_t>                m_PlayersReadyMode;
  std::optional<bool>                   m_AutoStartRequiresBalance;
  std::optional<uint32_t>               m_LatencyMaxFrames;
  std::optional<uint32_t>               m_LatencySafeFrames;
  std::optional<uint32_t>               m_AutoKickPing;
  std::optional<uint32_t>               m_WarnHighPing;
  std::optional<uint32_t>               m_SafeHighPing;

  std::optional<uint8_t>                m_LobbyTimeoutMode;
  std::optional<uint8_t>                m_LobbyOwnerTimeoutMode;
  std::optional<uint8_t>                m_LoadingTimeoutMode;
  std::optional<uint8_t>                m_PlayingTimeoutMode;

  std::optional<uint32_t>               m_LobbyTimeout;
  std::optional<uint32_t>               m_LobbyOwnerTimeout;
  std::optional<uint32_t>               m_LoadingTimeout;
  std::optional<uint32_t>               m_PlayingTimeout;

  std::optional<uint8_t>                m_PlayingTimeoutWarningShortCountDown;
  std::optional<uint32_t>               m_PlayingTimeoutWarningShortInterval;
  std::optional<uint8_t>                m_PlayingTimeoutWarningLargeCountDown;
  std::optional<uint32_t>               m_PlayingTimeoutWarningLargeInterval;

  std::optional<bool>                   m_LobbyOwnerReleaseLANLeaver;

  std::optional<uint32_t>               m_LobbyCountDownInterval;
  std::optional<uint32_t>               m_LobbyCountDownStartValue;

  std::optional<uint16_t>               m_Latency;
  std::optional<bool>                   m_LatencyEqualizerEnabled;
  std::optional<uint8_t>                m_LatencyEqualizerFrames;

  std::optional<int64_t>                m_AutoStartSeconds;
  std::optional<uint8_t>                m_AutoStartPlayers;
  std::optional<bool>                   m_HideLobbyNames;
  std::optional<uint8_t>                m_HideInGameNames;
  std::optional<bool>                   m_LoadInGame;
  std::optional<bool>                   m_EnableJoinObserversInProgress;
  std::optional<bool>                   m_EnableJoinPlayersInProgress;

  std::optional<bool>                   m_LogCommands;
  std::optional<uint8_t>                m_ReconnectionMode;
  std::optional<uint8_t>                m_IPFloodHandler;
  std::optional<uint8_t>                m_UnsafeNameHandler;
  std::optional<uint8_t>                m_BroadcastErrorHandler;
  std::optional<bool>                   m_PipeConsideredHarmful;

private:
  std::array<uint8_t, 20>   m_MapScriptsSHA1;   // config value: map sha1 (20 bytes)
  std::array<uint8_t, 20>   m_MapScriptsHash;   // config value: map sha1 (20 bytes)
  std::array<uint8_t, 4>    m_MapSize;   // config value: map size (4 bytes)
  std::array<uint8_t, 4>    m_MapCRC32;   // config value: map info (4 bytes) -> this is the real CRC
  std::array<uint8_t, 4>    m_MapScriptsWeakHash;    // config value: map crc (4 bytes) -> this is not the real CRC, it's the "xoro" value
  std::array<uint8_t, 2>    m_MapWidth;  // config value: map width (2 bytes)
  std::array<uint8_t, 2>    m_MapHeight; // config value: map height (2 bytes)
  std::vector<CGameSlot> m_Slots;
  std::string                     m_CFGName;
  std::string                     m_ClientMapPath;       // config value: map path
  std::string                     m_MapType;       // config value: map type (for stats class)
  bool                            m_MapMetaDataEnabled;
  std::string                     m_MapDefaultHCL; // config value: map default HCL to use
  std::filesystem::path           m_MapServerPath;  // config value: map local path
  std::string                     m_MapURL;
  std::string                     m_MapSiteURL;
  std::string                     m_MapShortDesc;
  SharedByteArray                 m_MapFileContents;       // the map data itself, for sending the map to players
  bool                            m_MapFileIsValid;
  bool                            m_MapLoaderIsPartial;
  uint32_t                        m_MapLocale;
  uint32_t                        m_MapOptions;
  uint32_t                        m_MapEditorVersion;
  uint8_t                         m_MapMinGameVersion;
  uint8_t                         m_MapMinSuggestedGameVersion;
  uint8_t                         m_MapNumControllers; // config value: max map number of players
  uint8_t                         m_MapNumDisabled; // config value: slots that cannot be used - not even by observers
  uint8_t                         m_MapNumTeams;   // config value: max map number of teams
  uint8_t                         m_MapVersionMaxSlots;
  uint8_t                         m_MapSpeed;
  uint8_t                         m_MapVisibility;
  uint8_t                         m_MapObservers;
  uint8_t                         m_GameFlags;
  uint8_t                         m_MapFilterMaker;
  uint8_t                         m_MapFilterType;
  uint8_t                         m_MapFilterSize;
  uint8_t                         m_MapFilterObs;
  std::array<uint8_t, 5>          m_MapContentMismatch;
  void*                           m_MapMPQ;
  std::optional<bool>             m_MapMPQResult;
  bool                            m_UseStandardPaths;
  bool                            m_Valid;
  std::string                     m_ErrorMessage;
  uint8_t                         m_HMCMode;
  uint8_t                         m_HMCTrigger1;
  uint8_t                         m_HMCTrigger2;
  uint8_t                         m_HMCSlot;
  std::string                     m_HMCPlayerName;

public:
  CMap(CAura* nAura, CConfig* CFG);
  ~CMap();

  [[nodiscard]] inline bool                       GetValid() const { return m_Valid; }
  [[nodiscard]] inline bool                       HasMismatch() const { return m_MapContentMismatch[0] != 0 || m_MapContentMismatch[1] != 0 || m_MapContentMismatch[2] != 0 || m_MapContentMismatch[3] != 0 || m_MapContentMismatch[4] != 0; }
  [[nodiscard]] inline bool                       GetMPQSucceeded() const { return m_MapMPQResult.has_value() && m_MapMPQResult.value(); }
  [[nodiscard]] inline bool                       GetMPQErrored() const { return m_MapMPQResult.has_value() && !m_MapMPQResult.value(); }
  [[nodiscard]] inline std::string                GetConfigName() const { return m_CFGName; }
  [[nodiscard]] inline std::string                GetClientPath() const { return m_ClientMapPath; }
  [[nodiscard]] inline std::array<uint8_t, 4>     GetMapSize() const { return m_MapSize; }
  [[nodiscard]] inline std::array<uint8_t, 4>     GetMapCRC32() const { return m_MapCRC32; } // <map.crc32>, but also legacy <map_hash>
  [[nodiscard]] inline std::array<uint8_t, 4>     GetMapScriptsWeakHash() const { return m_MapScriptsWeakHash; } // <map.weak_hash>, but also legacy <map_crc>
  [[nodiscard]] inline std::array<uint8_t, 20>    GetMapScriptsSHA1() const { return m_MapScriptsSHA1; } // <map.sha1>
  [[nodiscard]] inline std::array<uint8_t, 20>    GetMapScriptsHash() const { return m_MapScriptsHash; } // <map.hash>
  [[nodiscard]] std::string                       GetMapURL() const { return m_MapURL; }
  [[nodiscard]] std::string                       GetMapSiteURL() const { return m_MapSiteURL; }
  [[nodiscard]] std::string                       GetMapShortDesc() const { return m_MapShortDesc; }
  [[nodiscard]] inline uint8_t                    GetMapVisibility() const { return m_MapVisibility; }
  [[nodiscard]] inline uint8_t                    GetMapSpeed() const { return m_MapSpeed; }
  [[nodiscard]] inline uint8_t                    GetMapObservers() const { return m_MapObservers; }
  [[nodiscard]] inline uint8_t                    GetMapFlags() const { return m_GameFlags; }
  [[nodiscard]] uint32_t                          GetGameConvertedFlags() const;
  [[nodiscard]] uint32_t                          GetMapGameType() const;
  [[nodiscard]] inline uint32_t                   GetMapLocale() const { return m_MapLocale; }
  [[nodiscard]] inline uint32_t                   GetMapOptions() const { return m_MapOptions; }
  [[nodiscard]] inline uint8_t                    GetMapMinGameVersion() const { return m_MapMinGameVersion; }
  [[nodiscard]] inline uint8_t                    GetMapMinSuggestedGameVersion() const { return m_MapMinSuggestedGameVersion; }
  [[nodiscard]] uint8_t                           GetMapLayoutStyle() const;
  [[nodiscard]] inline std::array<uint8_t, 2>     GetMapWidth() const { return m_MapWidth; }
  [[nodiscard]] inline std::array<uint8_t, 2>     GetMapHeight() const { return m_MapHeight; }
  [[nodiscard]] inline std::string                GetMapType() const { return m_MapType; }
  [[nodiscard]] inline bool                       GetMapMetaDataEnabled() const { return m_MapMetaDataEnabled; }
  [[nodiscard]] inline std::string                GetMapDefaultHCL() const { return m_MapDefaultHCL; }
  [[nodiscard]] inline const std::filesystem::path&     GetServerPath() const { return m_MapServerPath; }
  [[nodiscard]] inline bool                       HasServerPath() const { return !m_MapServerPath.empty(); }
  [[nodiscard]] std::string                       GetServerFileName() const;
  [[nodiscard]] std::string                       GetClientFileName() const;
  [[nodiscard]] inline bool                       GetMapFileIsValid() const { return m_MapFileIsValid; }
  [[nodiscard]] inline const SharedByteArray&     GetMapFileContents() { return m_MapFileContents; }
  [[nodiscard]] inline bool                       HasMapFileContents() const { return m_MapFileContents != nullptr && !m_MapFileContents->empty(); }
  [[nodiscard]] bool                              GetMapFileIsFromManagedFolder() const;
  [[nodiscard]] inline uint8_t                    GetMapNumDisabled() const { return m_MapNumDisabled; }
  [[nodiscard]] inline uint8_t                    GetMapNumControllers() const { return m_MapNumControllers; }
  [[nodiscard]] inline uint8_t                    GetMapNumTeams() const { return m_MapNumTeams; }
  [[nodiscard]] inline uint8_t                    GetVersionMaxSlots() const { return m_MapVersionMaxSlots; }
  [[nodiscard]] inline std::vector<CGameSlot>     GetSlots() const { return m_Slots; }
  [[nodiscard]] bool                              GetHMCEnabled() const { return m_HMCMode != W3HMC_MODE_DISABLED; }
  [[nodiscard]] bool                              GetHMCRequired() const { return m_HMCMode == W3HMC_MODE_REQUIRED; }
  [[nodiscard]] uint8_t                           GetHMCMode() const { return m_HMCMode; }
  [[nodiscard]] uint8_t                           GetHMCTrigger1() const { return m_HMCTrigger1; }
  [[nodiscard]] uint8_t                           GetHMCTrigger2() const { return m_HMCTrigger2; }
  [[nodiscard]] uint8_t                           GetHMCSlot() const { return m_HMCSlot; }
  [[nodiscard]] std::string                       GetHMCPlayerName() const { return m_HMCPlayerName; }
  [[nodiscard]] uint8_t                           GetLobbyRace(const CGameSlot* slot) const;
  [[nodiscard]] bool                              GetUseStandardPaths() const { return m_UseStandardPaths; }
  void                                            ClearMapFileContents() { m_MapFileContents.reset(); }
  bool                                            SetTeamsLocked(const bool nEnable);
  bool                                            SetTeamsTogether(const bool nEnable);
  bool                                            SetAdvancedSharedUnitControl(const bool nEnable);
  bool                                            SetRandomRaces(const bool nEnable);
  bool                                            SetRandomHeroes(const bool nEnable);
  bool                                            SetMapVisibility(const uint8_t nMapVisibility);
  bool                                            SetMapSpeed(const uint8_t nMapSpeed);
  bool                                            SetMapObservers(const uint8_t nMapObservers);
  void                                            SetUseStandardPaths(const bool nValue) { m_UseStandardPaths = nValue; }
  [[nodiscard]] bool                              IsObserverSlot(const CGameSlot* slot) const;
  bool                                            NormalizeSlots();
  [[nodiscard]] inline std::string                GetErrorString() { return m_ErrorMessage; }

  std::optional<std::array<uint8_t, 4>>           CalculateCRC() const;
  void                                            ReadFileFromArchive(std::vector<uint8_t>& container, const std::string& fileSubPath) const;
  void                                            ReadFileFromArchive(std::string& container, const std::string& fileSubPath) const;
  std::optional<MapEssentials>                    ParseMPQFromPath(const std::filesystem::path& filePath);
  std::optional<MapEssentials>                    ParseMPQ() const;
  void Load(CConfig* CFG);
  void LoadGameConfigOverrides(CConfig& CFG);
  void LoadMapSpecificConfig(CConfig& CFG);

  bool                                            TryLoadMapFile();
  bool                                            TryReloadMapFile();
  FileChunkTransient                              GetMapFileChunk(size_t start);
  bool                                            UnlinkFile();
  [[nodiscard]] std::string                       CheckProblems();
  [[nodiscard]] uint32_t                          ChunkedChecksum(uint8_t* data, int32_t length, uint32_t checksum);
};

[[nodiscard]] inline uint32_t XORRotateLeft(const uint8_t* data, const uint32_t length)
{
  // a big thank you to Strilanc for figuring this out

  uint32_t i   = 0;
  uint32_t Val = 0;

  if (length > 3) {
    while (i < length - 3) {
      Val = ROTL(Val ^ ((uint32_t)data[i] + (uint32_t)(data[i + 1] << 8) + (uint32_t)(data[i + 2] << 16) + (uint32_t)(data[i + 3] << 24)), 3);
      i += 4;
    }
  }

  while (i < length) {
    Val = ROTL(Val ^ data[i], 3);
    ++i;
  }

  return Val;
}

#undef ROTL
#undef ROTR

#endif // AURA_MAP_H_
