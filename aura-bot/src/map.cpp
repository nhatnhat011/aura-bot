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

#include "map.h"
#include "aura.h"
#include "util.h"
#include "file_util.h"
#include <crc32/crc32.h>
#include <sha1/sha1.h>
#include "config/config.h"
#include "config/config_bot.h"
#include "config/config_game.h"
#include "game_slot.h"

#define __STORMLIB_SELF__
#include <StormLib.h>

#define ROTL(x, n) ((x) << (n)) | ((x) >> (32 - (n))) // this won't work with signed types
#define ROTR(x, n) ((x) >> (n)) | ((x) << (32 - (n))) // this won't work with signed types

using namespace std;

//
// CMap
//

CMap::CMap(CAura* nAura, CConfig* CFG)
  : m_Aura(nAura),
    m_MapServerPath(CFG->GetPath("map.local_path", filesystem::path())),
    m_MapFileIsValid(false),
    m_MapLoaderIsPartial(CFG->GetBool("map.cfg.partial", false)),
    m_MapLocale(CFG->GetUint32("map.locale", 0)),
    m_MapOptions(0),
    m_MapEditorVersion(0),
    m_MapMinGameVersion(0),
    m_MapMinSuggestedGameVersion(0),
    m_MapNumControllers(0),
    m_MapNumDisabled(0),
    m_MapNumTeams(0),
    m_MapObservers(MAPOBS_NONE),
    m_GameFlags(MAPFLAG_TEAMSTOGETHER | MAPFLAG_FIXEDTEAMS),
    m_MapFilterType(MAPFILTER_TYPE_SCENARIO),
    m_MapFilterObs(MAPFILTER_OBS_NONE),
    m_MapMPQ(nullptr),
    m_UseStandardPaths(CFG->GetBool("map.standard_path", false)),
    m_HMCMode(W3HMC_MODE_DISABLED)
{
  m_MapScriptsSHA1.fill(0);
  m_MapSize.fill(0);
  m_MapCRC32.fill(0);
  m_MapScriptsWeakHash.fill(0);
  m_MapWidth.fill(0);
  m_MapHeight.fill(0);
  m_MapContentMismatch.fill(0);

  Load(CFG);
}

CMap::~CMap() = default;

uint32_t CMap::GetGameConvertedFlags() const
{
  uint32_t gameFlags = 0;

  // speed

  if (m_MapSpeed == MAPSPEED_SLOW)
    gameFlags = 0x00000000;
  else if (m_MapSpeed == MAPSPEED_NORMAL)
    gameFlags = 0x00000001;
  else
    gameFlags = 0x00000002;

  // visibility

  if (m_MapVisibility == MAPVIS_HIDETERRAIN)
    gameFlags |= 0x00000100;
  else if (m_MapVisibility == MAPVIS_EXPLORED)
    gameFlags |= 0x00000200;
  else if (m_MapVisibility == MAPVIS_ALWAYSVISIBLE)
    gameFlags |= 0x00000400;
  else
    gameFlags |= 0x00000800;

  // observers

  if (m_MapObservers == MAPOBS_ONDEFEAT)
    gameFlags |= 0x00002000;
  else if (m_MapObservers == MAPOBS_ALLOWED)
    gameFlags |= 0x00003000;
  else if (m_MapObservers == MAPOBS_REFEREES)
    gameFlags |= 0x40000000;

  // teams/units/hero/race

  if (m_GameFlags & MAPFLAG_TEAMSTOGETHER) {
    gameFlags |= 0x00004000;
  }

  if (m_GameFlags & MAPFLAG_FIXEDTEAMS)
    gameFlags |= 0x00060000;

  if (m_GameFlags & MAPFLAG_UNITSHARE)
    gameFlags |= 0x01000000;

  if (m_GameFlags & MAPFLAG_RANDOMHERO)
    gameFlags |= 0x02000000;

  if (!(m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS)) {
    // WC3 GUI is misleading in displaying the Random Races tickbox when creating LAN games.
    // It even shows Random Races: Yes in the game lobby.
    // However, this flag is totally ignored when Fixed Player Settings is enabled.
    if (m_GameFlags & MAPFLAG_RANDOMRACES)
      gameFlags |= 0x04000000;
  }

  return gameFlags;
}

uint32_t CMap::GetMapGameType() const
{
  /* spec by Strilanc as follows:

    Public Enum GameTypes As UInteger
        None = 0
        Unknown0 = 1 << 0 '[always seems to be set?]

        '''<summary>Setting this bit causes wc3 to check the map and disc if it is not signed by Blizzard</summary>
        AuthenticatedMakerBlizzard = 1 << 3
        OfficialMeleeGame = 1 << 5

    SavedGame = 1 << 9
        PrivateGame = 1 << 11

        MakerUser = 1 << 13
        MakerBlizzard = 1 << 14
        TypeMelee = 1 << 15
        TypeScenario = 1 << 16
        SizeSmall = 1 << 17
        SizeMedium = 1 << 18
        SizeLarge = 1 << 19
        ObsFull = 1 << 20
        ObsOnDeath = 1 << 21
        ObsNone = 1 << 22

        MaskObs = ObsFull Or ObsOnDeath Or ObsNone
        MaskMaker = MakerBlizzard Or MakerUser
        MaskType = TypeMelee Or TypeScenario
        MaskSize = SizeLarge Or SizeMedium Or SizeSmall
        MaskFilterable = MaskObs Or MaskMaker Or MaskType Or MaskSize
    End Enum

   */

  // note: we allow "conflicting" flags to be set at the same time (who knows if this is a good idea)
  // we also don't set any flags this class is unaware of such as Unknown0, SavedGame, and PrivateGame

  uint32_t GameType = 0;

  // maker

  if (m_MapFilterMaker & MAPFILTER_MAKER_USER)
    GameType |= MAPGAMETYPE_MAKERUSER;

  if (m_MapFilterMaker & MAPFILTER_MAKER_BLIZZARD)
    GameType |= MAPGAMETYPE_MAKERBLIZZARD;

  // type

  if (m_MapFilterType & MAPFILTER_TYPE_MELEE)
    GameType |= MAPGAMETYPE_TYPEMELEE;

  if (m_MapFilterType & MAPFILTER_TYPE_SCENARIO)
    GameType |= MAPGAMETYPE_TYPESCENARIO;

  // size

  if (m_MapFilterSize & MAPFILTER_SIZE_SMALL)
    GameType |= MAPGAMETYPE_SIZESMALL;

  if (m_MapFilterSize & MAPFILTER_SIZE_MEDIUM)
    GameType |= MAPGAMETYPE_SIZEMEDIUM;

  if (m_MapFilterSize & MAPFILTER_SIZE_LARGE)
    GameType |= MAPGAMETYPE_SIZELARGE;

  // obs

  if (m_MapFilterObs & MAPFILTER_OBS_FULL)
    GameType |= MAPGAMETYPE_OBSFULL;

  if (m_MapFilterObs & MAPFILTER_OBS_ONDEATH)
    GameType |= MAPGAMETYPE_OBSONDEATH;

  if (m_MapFilterObs & MAPFILTER_OBS_NONE)
    GameType |= MAPGAMETYPE_OBSNONE;

  return GameType;
}

uint8_t CMap::GetMapLayoutStyle() const
{
  // 0 = melee
  // 1 = custom forces
  // 2 = fixed player settings (not possible with the Warcraft III design)
  // 3 = custom forces + fixed player settings

  if (!(m_MapOptions & MAPOPT_CUSTOMFORCES))
    return MAPLAYOUT_ANY;

  if (!(m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS))
    return MAPLAYOUT_CUSTOM_FORCES;

  return MAPLAYOUT_FIXED_PLAYERS;
}

string CMap::GetServerFileName() const
{
  return PathToString(m_MapServerPath.filename());
}

string CMap::GetClientFileName() const
{
  size_t LastSlash = m_ClientMapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_ClientMapPath;
  }
  return m_ClientMapPath.substr(LastSlash + 1);
}

bool CMap::GetMapFileIsFromManagedFolder() const
{
  if (m_UseStandardPaths) return false;
  if (m_MapServerPath.empty()) return false;
  return m_MapServerPath == m_MapServerPath.filename();
}

bool CMap::IsObserverSlot(const CGameSlot* slot) const
{
  if (slot->GetUID() != 0 || slot->GetDownloadStatus() != 255) {
    return false;
  }
  if (slot->GetSlotStatus() != SLOTSTATUS_OPEN || !slot->GetIsSelectable()) {
    return false;
  }
  return slot->GetTeam() >= m_MapNumControllers && slot->GetColor() >= m_MapNumControllers;
}

bool CMap::NormalizeSlots()
{
  uint8_t i = static_cast<uint8_t>(m_Slots.size());

  bool updated = false;
  bool anyNonObserver = false;
  while (i--) {
    const CGameSlot slot = m_Slots[i];
    if (!IsObserverSlot(&slot)) {
      anyNonObserver = true;
      break;
    }
  }

  i = static_cast<uint8_t>(m_Slots.size());
  while (i--) {
    CGameSlot slot = m_Slots[i];
    if (anyNonObserver && IsObserverSlot(&slot)) {
      m_Slots.erase(m_Slots.begin() + i);
      updated = true;
      continue;
    }
    uint8_t race = GetLobbyRace(&slot);
    if (race != slot.GetRace()) {
      slot.SetRace(race);
      updated = true;
    }
  }

  return updated;
}

bool CMap::SetMapObservers(const uint8_t nMapObservers)
{
  switch (nMapObservers) {
    case MAPOBS_ALLOWED:
    case MAPOBS_REFEREES:
      m_MapObservers = nMapObservers;
      m_MapFilterObs = MAPFILTER_OBS_FULL;
      break;
    case MAPOBS_NONE:
      m_MapObservers = nMapObservers;
      m_MapFilterObs = MAPFILTER_OBS_NONE;
      break;
    case MAPOBS_ONDEFEAT:
      m_MapObservers = nMapObservers;
      m_MapFilterObs = MAPFILTER_OBS_ONDEATH;
      break;
    default:
      m_MapObservers = nMapObservers;
      return false;
  }
  return true;
}

bool CMap::SetMapVisibility(const uint8_t nMapVisibility)
{
  m_MapVisibility = nMapVisibility;
  return true;
}

bool CMap::SetMapSpeed(const uint8_t nMapSpeed)
{
  m_MapSpeed = nMapSpeed;
  return true;
}

bool CMap::SetTeamsLocked(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_FIXEDTEAMS;
  } else {
    m_GameFlags &= ~MAPFLAG_FIXEDTEAMS;
  }
  return true;
}

bool CMap::SetTeamsTogether(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_TEAMSTOGETHER;
  } else {
    m_GameFlags &= ~MAPFLAG_TEAMSTOGETHER;
  }
  return true;
}

bool CMap::SetAdvancedSharedUnitControl(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_UNITSHARE;
  } else {
    m_GameFlags &= ~MAPFLAG_UNITSHARE;
  }
  return true;
}

bool CMap::SetRandomHeroes(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_RANDOMHERO;
  } else {
    m_GameFlags &= ~MAPFLAG_RANDOMHERO;
  }
  return true;
}

bool CMap::SetRandomRaces(const bool nEnable)
{
  if (m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) {
    return false;
  }
  if (nEnable) {
    m_GameFlags |= MAPFLAG_RANDOMRACES;
  } else {
    m_GameFlags &= ~MAPFLAG_RANDOMRACES;
  }
  return true;
}

optional<array<uint8_t, 4>> CMap::CalculateCRC() const
{
  optional<array<uint8_t, 4>> result;
  if (!HasMapFileContents()) return result;
  const uint32_t crc32 = CRC32::CalculateCRC((uint8_t*)m_MapFileContents->data(), m_MapFileContents->size());
  EnsureFixedByteArray(result, crc32, false);
  DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.crc32 = " + ByteArrayToDecString(result.value()) + ">")
  return result;
}

optional<MapEssentials> CMap::ParseMPQFromPath(const filesystem::path& filePath)
{
  m_MapMPQResult = OpenMPQArchive(&m_MapMPQ, filePath);
  if (GetMPQSucceeded()) {
    optional<MapEssentials> mapEssentials = ParseMPQ();
    SFileCloseArchive(m_MapMPQ);
    m_MapMPQ = nullptr;
    return mapEssentials;
  }

  m_MapMPQ = nullptr;
#ifdef _WIN32
  uint32_t errorCode = (uint32_t)GetLastOSError();
  string errorCodeString = (
    errorCode == 2 ? "Map not found" : (
    errorCode == 11 ? "File is corrupted." : (
    (errorCode == 3 || errorCode == 15) ? "Config error: <bot.maps_path> is not a valid directory" : (
    (errorCode == 32 || errorCode == 33) ? "File is currently opened by another process." : (
    "Error code " + to_string(errorCode)
    ))))
  );
#else
  int32_t errorCode = static_cast<int32_t>(GetLastOSError());
  string errorCodeString = "Error code " + to_string(errorCode);
#endif
  Print("[MAP] warning - unable to load MPQ archive [" + PathToString(filePath) + "] - " + errorCodeString);

  return nullopt;
}

void CMap::ReadFileFromArchive(vector<uint8_t>& container, const string& fileSubPath) const
{
  const char* path = fileSubPath.c_str();
  ReadMPQFile(m_MapMPQ, path, container, m_MapLocale);
}

void CMap::ReadFileFromArchive(string& container, const string& fileSubPath) const
{
  const char* path = fileSubPath.c_str();
  ReadMPQFile(m_MapMPQ, path, container, m_MapLocale);
}

optional<MapEssentials> CMap::ParseMPQ() const
{
  optional<MapEssentials> mapEssentials;
  if (!m_MapMPQ) return mapEssentials;

  mapEssentials.emplace();

  // calculate <map.weak_hash>, and <map.sha1>
  // a big thank you to Strilanc for figuring the <map.weak_hash> algorithm out

  bool hashError = false;
  uint32_t weakHashVal = 0;
  m_Aura->m_SHA.Reset();

  string fileContents;
  ReadFileFromArchive(fileContents, R"(Scripts\common.j)");

  if (fileContents.empty()) {
    filesystem::path commonPath = m_Aura->m_Config.m_JASSPath / filesystem::path("common-" + to_string(m_Aura->m_GameVersion) +".j");
    if (!FileRead(commonPath, fileContents, MAX_READ_FILE_SIZE) || fileContents.empty()) {
      Print("[MAP] unable to calculate <map.weak_hash>, and <map.sha1> - unable to read file [" + PathToString(commonPath) + "]");
    } else {
      weakHashVal = weakHashVal ^ XORRotateLeft((uint8_t*)fileContents.data(), static_cast<uint32_t>(fileContents.size()));
      m_Aura->m_SHA.Update((uint8_t*)fileContents.data(), static_cast<uint32_t>(fileContents.size()));
    }
    hashError = hashError || fileContents.empty();
  } else {
    Print("[MAP] overriding default common.j with map copy while calculating <map.weak_hash>, and <map.sha1>");
    weakHashVal = weakHashVal ^ XORRotateLeft(reinterpret_cast<uint8_t*>(fileContents.data()), fileContents.size());
    m_Aura->m_SHA.Update(reinterpret_cast<uint8_t*>(fileContents.data()), fileContents.size());
  }

  ReadFileFromArchive(fileContents, R"(Scripts\blizzard.j)");

  if (fileContents.empty()) {
    filesystem::path blizzardPath = m_Aura->m_Config.m_JASSPath / filesystem::path("blizzard-" + to_string(m_Aura->m_GameVersion) +".j");
    if (!FileRead(blizzardPath, fileContents, MAX_READ_FILE_SIZE) || fileContents.empty()) {
      Print("[MAP] unable to calculate <map.weak_hash>, and <map.sha1> - unable to read file [" + PathToString(blizzardPath) + "]");
    } else {
      weakHashVal = weakHashVal ^ XORRotateLeft((uint8_t*)fileContents.data(), static_cast<uint32_t>(fileContents.size()));
      m_Aura->m_SHA.Update((uint8_t*)fileContents.data(), static_cast<uint32_t>(fileContents.size()));
    }
    hashError = hashError || fileContents.empty();
  } else {
    Print("[MAP] overriding default blizzard.j with map copy while calculating <map.weak_hash>, and <map.sha1>");
    weakHashVal = weakHashVal ^ XORRotateLeft(reinterpret_cast<uint8_t*>(fileContents.data()), fileContents.size());
    m_Aura->m_SHA.Update(reinterpret_cast<uint8_t*>(fileContents.data()), fileContents.size());
  }

  weakHashVal = ROTL(weakHashVal, 3);
  weakHashVal = ROTL(weakHashVal ^ 0x03F1379E, 3);
  m_Aura->m_SHA.Update((uint8_t*)"\x9E\x37\xF1\x03", 4);

  if (!hashError) {
    bool foundScript = false;
    vector<string> fileList;
    fileList.emplace_back("war3map.j");
    fileList.emplace_back(R"(scripts\war3map.j)");
    fileList.emplace_back("war3map.w3e");
    fileList.emplace_back("war3map.wpm");
    fileList.emplace_back("war3map.doo");
    fileList.emplace_back("war3map.w3u");
    fileList.emplace_back("war3map.w3b");
    fileList.emplace_back("war3map.w3d");
    fileList.emplace_back("war3map.w3a");
    fileList.emplace_back("war3map.w3q");

    for (const auto& fileName : fileList) {
      // don't use scripts\war3map.j if we've already used war3map.j (yes, some maps have both but only war3map.j is used)

      if (foundScript && fileName == R"(scripts\war3map.j)")
        continue;

      ReadFileFromArchive(fileContents, fileName);
      if (fileContents.empty()) {
        continue;
      }
      if (fileName == "war3map.j" || fileName == R"(scripts\war3map.j)") {
        foundScript = true;
      }

      weakHashVal = ROTL(weakHashVal ^ XORRotateLeft(reinterpret_cast<uint8_t*>(fileContents.data()), fileContents.size()), 3);
      m_Aura->m_SHA.Update(reinterpret_cast<uint8_t*>(fileContents.data()), fileContents.size());
    }

    if (!foundScript) {
      Print(R"([MAP] couldn't find war3map.j or scripts\war3map.j in MPQ archive, calculated <map.weak_hash>, and <map.sha1> is probably wrong)");
    }

    EnsureFixedByteArray(mapEssentials->weakHash, weakHashVal, false);
    DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.weak_hash = " + ByteArrayToDecString(mapEssentials->weakHash.value()) + ">")

    m_Aura->m_SHA.Final();
    mapEssentials->sha1.emplace();
    mapEssentials->sha1->fill(0);
    m_Aura->m_SHA.GetHash(mapEssentials->sha1->data());
    DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.sha1 = " + ByteArrayToDecString(mapEssentials->sha1.value()) + ">")
  }

  // try to calculate <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams>, <map.filter_type>

  if (m_MapLoaderIsPartial) {
    ReadFileFromArchive(fileContents, "war3map.w3i");
    if (fileContents.empty()) {
      Print("[MAP] unable to calculate <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams> - unable to extract war3map.w3i from map file");
    } else {
      istringstream ISS(fileContents);

      // war3map.w3i format found at http://www.wc3campaigns.net/tools/specs/index.html by Zepir/PitzerMike

      string   GarbageString;
      uint32_t FileFormat;
      uint32_t RawEditorVersion;
      uint32_t RawMapFlags;
      uint32_t RawMapWidth, RawMapHeight;
      uint32_t RawMapNumPlayers, RawMapNumTeams;

      ISS.read(reinterpret_cast<char*>(&FileFormat), 4); // file format (18 = ROC, 25 = TFT)

      if (FileFormat == 18 || FileFormat == 25)
      {
        ISS.seekg(4, ios::cur);            // number of saves
        ISS.read(reinterpret_cast<char*>(&RawEditorVersion), 4); // editor version
        getline(ISS, GarbageString, '\0'); // map name
        getline(ISS, GarbageString, '\0'); // map author
        getline(ISS, GarbageString, '\0'); // map description
        getline(ISS, GarbageString, '\0'); // players recommended
        ISS.seekg(32, ios::cur);           // camera bounds
        ISS.seekg(16, ios::cur);           // camera bounds complements
        ISS.read(reinterpret_cast<char*>(&RawMapWidth), 4);  // map width
        ISS.read(reinterpret_cast<char*>(&RawMapHeight), 4); // map height
        ISS.read(reinterpret_cast<char*>(&RawMapFlags), 4);  // flags
        ISS.seekg(1, ios::cur);            // map main ground type

        if (FileFormat == 18)
          ISS.seekg(4, ios::cur); // campaign background number
        else if (FileFormat == 25)
        {
          ISS.seekg(4, ios::cur);            // loading screen background number
          getline(ISS, GarbageString, '\0'); // path of custom loading screen model
        }

        getline(ISS, GarbageString, '\0'); // map loading screen text
        getline(ISS, GarbageString, '\0'); // map loading screen title
        getline(ISS, GarbageString, '\0'); // map loading screen subtitle

        if (FileFormat == 18)
          ISS.seekg(4, ios::cur); // map loading screen number
        else if (FileFormat == 25)
        {
          ISS.seekg(4, ios::cur);            // used game data set
          getline(ISS, GarbageString, '\0'); // prologue screen path
        }

        getline(ISS, GarbageString, '\0'); // prologue screen text
        getline(ISS, GarbageString, '\0'); // prologue screen title
        getline(ISS, GarbageString, '\0'); // prologue screen subtitle

        if (FileFormat == 25)
        {
          ISS.seekg(4, ios::cur);            // uses terrain fog
          ISS.seekg(4, ios::cur);            // fog start z height
          ISS.seekg(4, ios::cur);            // fog end z height
          ISS.seekg(4, ios::cur);            // fog density
          ISS.seekg(1, ios::cur);            // fog red value
          ISS.seekg(1, ios::cur);            // fog green value
          ISS.seekg(1, ios::cur);            // fog blue value
          ISS.seekg(1, ios::cur);            // fog alpha value
          ISS.seekg(4, ios::cur);            // global weather id
          getline(ISS, GarbageString, '\0'); // custom sound environment
          ISS.seekg(1, ios::cur);            // tileset id of the used custom light environment
          ISS.seekg(1, ios::cur);            // custom water tinting red value
          ISS.seekg(1, ios::cur);            // custom water tinting green value
          ISS.seekg(1, ios::cur);            // custom water tinting blue value
          ISS.seekg(1, ios::cur);            // custom water tinting alpha value
        }

        mapEssentials->editorVersion = RawEditorVersion;

        ISS.read(reinterpret_cast<char*>(&RawMapNumPlayers), 4); // number of players
        if (RawMapNumPlayers > MAX_SLOTS_MODERN) RawMapNumPlayers = 0;
        uint8_t closedSlots = 0;
        uint8_t disabledSlots = 0;

        for (uint32_t i = 0; i < RawMapNumPlayers; ++i)
        {
          CGameSlot Slot(SLOTTYPE_AUTO, 0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, 0, 1, SLOTRACE_RANDOM);
          uint32_t  Color, Type, Race;
          ISS.read(reinterpret_cast<char*>(&Color), 4); // colour
          Slot.SetColor(static_cast<uint8_t>(Color));
          ISS.read(reinterpret_cast<char*>(&Type), 4); // type

          if (Type == SLOTTYPE_NONE) {
            Slot.SetType(Type);
            Slot.SetSlotStatus(SLOTSTATUS_CLOSED);
            ++closedSlots;
          } else {
            if (!(RawMapFlags & MAPOPT_FIXEDPLAYERSETTINGS)) {
              // WC3 ignores slots defined in WorldEdit if Fixed Player Settings is disabled.
              Type = SLOTTYPE_USER;
            }
            if (Type <= SLOTTYPE_RESCUEABLE) {
              Slot.SetType(Type);
            }
            if (Type == SLOTTYPE_USER) {
              Slot.SetSlotStatus(SLOTSTATUS_OPEN);
            } else if (Type == SLOTTYPE_COMP) {
              Slot.SetSlotStatus(SLOTSTATUS_OCCUPIED);
              Slot.SetComputer(SLOTCOMP_YES);
              Slot.SetComputerType(SLOTCOMP_NORMAL);
            } else {
              Slot.SetSlotStatus(SLOTSTATUS_CLOSED);
              ++closedSlots;
              ++disabledSlots;
            }
          }

          ISS.read(reinterpret_cast<char*>(&Race), 4); // race

          if (Race == 1)
            Slot.SetRace(SLOTRACE_HUMAN);
          else if (Race == 2)
            Slot.SetRace(SLOTRACE_ORC);
          else if (Race == 3)
            Slot.SetRace(SLOTRACE_UNDEAD);
          else if (Race == 4)
            Slot.SetRace(SLOTRACE_NIGHTELF);
          else
            Slot.SetRace(SLOTRACE_RANDOM);

          ISS.seekg(4, ios::cur);            // fixed start position
          getline(ISS, GarbageString, '\0'); // player name
          ISS.seekg(4, ios::cur);            // start position x
          ISS.seekg(4, ios::cur);            // start position y
          ISS.seekg(4, ios::cur);            // ally low priorities
          ISS.seekg(4, ios::cur);            // ally high priorities

          if (Slot.GetSlotStatus() != SLOTSTATUS_CLOSED)
            mapEssentials->slots.push_back(Slot);
        }

        ISS.read(reinterpret_cast<char*>(&RawMapNumTeams), 4); // number of teams
        if (RawMapNumTeams > MAX_SLOTS_MODERN) RawMapNumTeams = 0;

        if (RawMapNumPlayers > 0 && RawMapNumTeams > 0) {
          // the bot only cares about the following options: melee, fixed player settings, custom forces
          // let's not confuse the user by displaying erroneous map options so zero them out now
          mapEssentials->options = RawMapFlags & (MAPOPT_MELEE | MAPOPT_FIXEDPLAYERSETTINGS | MAPOPT_CUSTOMFORCES);
          if (mapEssentials->options & MAPOPT_FIXEDPLAYERSETTINGS) mapEssentials->options |= MAPOPT_CUSTOMFORCES;

          DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.options = " + to_string(mapEssentials->options) + ">")

          if (!(mapEssentials->options & MAPOPT_CUSTOMFORCES)) {
            mapEssentials->numTeams = static_cast<uint8_t>(RawMapNumPlayers);
          } else {
            mapEssentials->numTeams = static_cast<uint8_t>(RawMapNumTeams);
          }

          for (uint32_t i = 0; i < mapEssentials->numTeams; ++i) {
            uint32_t PlayerMask = 0;
            if (i < RawMapNumTeams) {
              ISS.seekg(4, ios::cur);                            // flags
              ISS.read(reinterpret_cast<char*>(&PlayerMask), 4); // player mask
            }
            if (!(mapEssentials->options & MAPOPT_CUSTOMFORCES)) {
              PlayerMask = 1 << i;
            }

            for (auto& Slot : mapEssentials->slots) {
              if (0 != (PlayerMask & (1 << static_cast<uint32_t>((Slot).GetColor())))) {
                Slot.SetTeam(static_cast<uint8_t>(i));
              }
            }

            if (i < RawMapNumTeams) {
              getline(ISS, GarbageString, '\0'); // team name
            }
          }

          EnsureFixedByteArray(mapEssentials->width, static_cast<uint16_t>(RawMapWidth), false);
          EnsureFixedByteArray(mapEssentials->height, static_cast<uint16_t>(RawMapHeight), false);
          mapEssentials->numPlayers = static_cast<uint8_t>(RawMapNumPlayers) - closedSlots;
          mapEssentials->numDisabled = disabledSlots;
          mapEssentials->melee = (mapEssentials->options & MAPOPT_MELEE) != 0;

          if (!(mapEssentials->options & MAPOPT_FIXEDPLAYERSETTINGS)) {
            // make races selectable

            for (auto& slot : mapEssentials->slots)
              slot.SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
          }

#ifdef DEBUG
          uint32_t SlotNum = 1;
          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
            Print("[MAP] calculated <map.width = " + ByteArrayToDecString(mapEssentials->width.value()) + ">");
            Print("[MAP] calculated <map.height = " + ByteArrayToDecString(mapEssentials->height.value()) + ">");
            Print("[MAP] calculated <map.num_players = " + ToDecString(mapEssentials->numPlayers) + ">");
            Print("[MAP] calculated <map.num_disabled = " + ToDecString(mapEssentials->numDisabled) + ">");
            Print("[MAP] calculated <map.num_teams = " + ToDecString(mapEssentials->numTeams) + ">");
          }

          for (const auto& slot : mapEssentials->slots) {
            DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.slot_" + to_string(SlotNum) + " = " + ByteArrayToDecString(slot.GetProtocolArray()) + ">")
            ++SlotNum;
          }
#endif
        }
      }
    }
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] using mapcfg for <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams>")
  }

  fileContents.clear();

  if (mapEssentials->slots.size() > 12 || mapEssentials->numPlayers > 12 || mapEssentials->numTeams > 12) {
    mapEssentials->minCompatibleGameVersion = 29;
  }
  
  if (mapEssentials->editorVersion > 0) {
    if (6060 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 29;
    } else if (6059 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 24;
    } else if (6058 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 23;
    } else if (6057 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 22;
    } else if (6053 <= mapEssentials->editorVersion && mapEssentials->editorVersion <= 6056) {
      mapEssentials->minSuggestedGameVersion = 22; // not released
    } else if (6050 <= mapEssentials->editorVersion && mapEssentials->editorVersion <= 6052) {
      mapEssentials->minSuggestedGameVersion = 17 + static_cast<uint8_t>(mapEssentials->editorVersion - 6050);
    } else if (6046 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 16;
    } else if (6043 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 15;
    } else if (6039 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 14;
    } else if (6038 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 14; // not released
    } else if (6034 <= mapEssentials->editorVersion && mapEssentials->editorVersion <= 6037) {
      mapEssentials->minSuggestedGameVersion = 10 + static_cast<uint8_t>(mapEssentials->editorVersion - 6034);
    } else if (6031 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = 7;
    }
  }

  if (mapEssentials->minSuggestedGameVersion < mapEssentials->minCompatibleGameVersion) {
    mapEssentials->minSuggestedGameVersion = mapEssentials->minCompatibleGameVersion;
  }
  return mapEssentials;
}

void CMap::Load(CConfig* CFG)
{
  m_Valid   = true;
  m_CFGName = PathToString(CFG->GetFile().filename());

  bool ignoreMPQ = !HasServerPath() || (!m_MapLoaderIsPartial && m_Aura->m_Config.m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_NEVER);

  optional<uint32_t> mapFileSize;
  if (m_MapLoaderIsPartial || m_Aura->m_Net.m_Config.m_AllowTransfers != MAP_TRANSFERS_NEVER) {
    if (TryLoadMapFile()) {
      mapFileSize = static_cast<uint32_t>(m_MapFileContents->size());
#ifdef DEBUG
      array<uint8_t, 4> mapFileSizeBytes = CreateFixedByteArray(mapFileSize.value(), false);
      DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.size = " + ByteArrayToDecString(mapFileSizeBytes) + ">")
#endif
    } else if (m_MapLoaderIsPartial) {
      return;
    } else {
      ignoreMPQ = true;
    }
  }

  filesystem::path resolvedFilePath(m_MapServerPath);

  {
    optional<int64_t> cachedModifiedTime = CFG->GetMaybeInt64("map.local_mod_time");
    optional<int64_t> fileModifiedTime;

    if (!ignoreMPQ) {
      if (resolvedFilePath.filename() == resolvedFilePath && !m_UseStandardPaths) {
        resolvedFilePath = m_Aura->m_Config.m_MapPath / resolvedFilePath;
      }    
      fileModifiedTime = GetMaybeModifiedTime(resolvedFilePath);
      ignoreMPQ = (
        !m_MapLoaderIsPartial && m_Aura->m_Config.m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_MODIFIED && (
          !fileModifiedTime.has_value() || (
            cachedModifiedTime.has_value() && fileModifiedTime.has_value() &&
            fileModifiedTime.value() <= cachedModifiedTime.value()
          )
        )
      );
    }
    if (fileModifiedTime.has_value()) {
      if (!cachedModifiedTime.has_value() || fileModifiedTime.value() != cachedModifiedTime.value()) {
        CFG->SetInt64("map.local_mod_time", fileModifiedTime.value());
        CFG->SetIsModified();
      }
    }
  }

  // calculate <map.crc32>
  optional<array<uint8_t, 4>> crc32 = CalculateCRC();

  optional<MapEssentials> mapEssentials;
  if (!ignoreMPQ) {
    optional<MapEssentials> mapEssentialsParsed = ParseMPQFromPath(resolvedFilePath);
    mapEssentials.swap(mapEssentialsParsed);
    if (!mapEssentials.has_value()) {
      if (m_MapLoaderIsPartial) {
        Print("[MAP] failed to parse map");
        return;
      }
      Print("[MAP] failed to parse map, using config file for <map.weak_hash>, <map.sha1>");
    }
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE2, "[MAP] MPQ archive ignored");
  }

  if (mapEssentials.has_value()) {
    // If map has Melee flag, group it with other Melee maps in Battle.net game search filter
    m_MapFilterType = mapEssentials->melee ? MAPFILTER_TYPE_MELEE : MAPFILTER_TYPE_SCENARIO;
    if (m_MapFilterType == MAPFILTER_TYPE_MELEE) {
      DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] found melee map")
    }

    m_MapNumControllers = mapEssentials->numPlayers;
    m_MapNumDisabled = mapEssentials->numDisabled;
    m_MapNumTeams = mapEssentials->numTeams;
    m_MapMinGameVersion = mapEssentials->minCompatibleGameVersion;
    m_MapMinSuggestedGameVersion = mapEssentials->minSuggestedGameVersion;
    m_MapEditorVersion = mapEssentials->editorVersion;
    m_MapOptions = mapEssentials->options;

    if (mapEssentials->width.has_value()) {
      copy_n(mapEssentials->width->begin(), 2, m_MapWidth.begin());
    }
    if (mapEssentials->height.has_value()) {
      copy_n(mapEssentials->height->begin(), 2, m_MapHeight.begin());
    }

    m_Slots = mapEssentials->slots;
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE2, "[MAP] MPQ archive ignored/missing/errored");
  }

  array<uint8_t, 4> mapContentMismatch = {0, 0, 0, 0};

  vector<uint8_t> cfgFileSize = CFG->GetUint8Vector("map.size", 4);
  if (cfgFileSize.empty() == !mapFileSize.has_value()) {
    if (cfgFileSize.empty()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.size")) {
          m_ErrorMessage = "invalid <map.size> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.size>";
        }
      }
    } else {
      mapContentMismatch[0] = ByteArrayToUInt32(cfgFileSize, 0, false) != mapFileSize.value();
      copy_n(cfgFileSize.begin(), 4, m_MapSize.begin());
    }
  } else if (mapFileSize.has_value()) {
    vector<uint8_t> mapFileSizeVector = CreateByteArray(static_cast<uint32_t>(mapFileSize.value()), false);
    CFG->SetUint8Vector("map.size", mapFileSizeVector);
    cfgFileSize.swap(mapFileSizeVector);
    copy_n(cfgFileSize.begin(), 4, m_MapSize.begin());
  } else {
    copy_n(cfgFileSize.begin(), 4, m_MapSize.begin());
  }

  vector<uint8_t> cfgCRC32 = CFG->GetUint8Vector("map.crc32", 4);
  if (cfgCRC32.empty() == !crc32.has_value()) {
    if (cfgCRC32.empty()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.crc32")) {
          m_ErrorMessage = "invalid <map.crc32> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.crc32>";
        }
      }
    } else {
      mapContentMismatch[1] = ByteArrayToUInt32(cfgCRC32, 0, false) != ByteArrayToUInt32(crc32.value(), false);
      copy_n(cfgCRC32.begin(), 4, m_MapCRC32.begin());
    }
  } else if (crc32.has_value()) {
    CFG->SetUint8Array("map.crc32", crc32->data(), 4);
    copy_n(crc32->begin(), 4, m_MapCRC32.begin());
  } else {
    copy_n(cfgCRC32.begin(), 4, m_MapCRC32.begin());
  }

  vector<uint8_t> cfgWeakHash = CFG->GetUint8Vector("map.weak_hash", 4);
  if (cfgWeakHash.empty() == !(mapEssentials.has_value() && mapEssentials->weakHash.has_value())) {
    if (cfgWeakHash.empty()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.weak_hash")) {
          m_ErrorMessage = "invalid <map.weak_hash> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.weak_hash>";
        }
      }
    } else {
      mapContentMismatch[2] = ByteArrayToUInt32(cfgWeakHash, 0, false) != ByteArrayToUInt32(mapEssentials->weakHash.value(), false);
      copy_n(cfgWeakHash.begin(), 4, m_MapScriptsWeakHash.begin());
    }
  } else if (mapEssentials.has_value() && mapEssentials->weakHash.has_value()) {
    CFG->SetUint8Array("map.weak_hash", mapEssentials->weakHash->data(), 4);
    copy_n(mapEssentials->weakHash->begin(), 4, m_MapScriptsWeakHash.begin());
  } else {
    copy_n(cfgWeakHash.begin(), 4, m_MapScriptsWeakHash.begin());
  }

  vector<uint8_t> cfgSHA1 = CFG->GetUint8Vector("map.sha1", 20);
  if (cfgSHA1.empty() == !(mapEssentials.has_value() && mapEssentials->sha1.has_value())) {
    if (cfgSHA1.empty()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.sha1")) {
          m_ErrorMessage = "invalid <map.sha1> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.sha1>";
        }
      }
    } else {
      mapContentMismatch[3] = memcmp(cfgSHA1.data(), mapEssentials->sha1->data(), 20) != 0;
      copy_n(cfgSHA1.begin(), 20, m_MapScriptsSHA1.begin());
    }
  } else if (mapEssentials.has_value() && mapEssentials->sha1.has_value()) {
    CFG->SetUint8Array("map.sha1", mapEssentials->sha1->data(), 20);
    copy_n(mapEssentials->sha1->begin(), 20, m_MapScriptsSHA1.begin());
  } else {
    copy_n(cfgSHA1.begin(), 20, m_MapScriptsSHA1.begin());
  }

  if (HasMismatch()) {
    m_MapContentMismatch.swap(mapContentMismatch);
    PRINT_IF(LOG_LEVEL_WARNING, "[CACHE] error - map content mismatch");
  } else if (crc32.has_value()) {
    m_MapFileIsValid = true;
  }

  if (CFG->Exists("map.filter_type")) {
    // If map has Melee flag, group it with other Melee maps in Battle.net game search filter
    m_MapFilterType = CFG->GetUint8("map.filter_type", m_MapFilterType);
  } else {
    CFG->SetUint8("map.filter_type", m_MapFilterType);
  }  

  if (CFG->Exists("map.options")) {
    // Note: maps with any given layout style defined from WorldEdit
    // may have their layout further constrained arbitrarily when hosting games
    m_MapOptions = CFG->GetUint32("map.options", m_MapOptions);
    if (m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) m_MapOptions |= MAPOPT_CUSTOMFORCES;
  } else {
    CFG->SetUint32("map.options", m_MapOptions);
  }

  if (CFG->Exists("map.flags")) {
    m_GameFlags = CFG->GetUint8("map.flags", m_GameFlags);
  } else {
    CFG->SetUint8("map.flags", m_GameFlags);
  }

  vector<uint8_t> cfgWidth = CFG->GetUint8Vector("map.width", 2);
  if (cfgWidth.size() == 2) {
    copy_n(cfgWidth.begin(), 2, m_MapWidth.begin());
  } else {
    CFG->SetUint8Array("map.width", m_MapWidth.data(), 2);
    // already copied to m_MapWidth
  }
  if (ByteArrayToUInt16(m_MapWidth, false) == 0) {
    // Default invalid <map.width> values to 1
    m_MapWidth = {1, 0};
  }

  vector<uint8_t> cfgHeight = CFG->GetUint8Vector("map.height", 2);
  if (cfgHeight.size() == 2) {
    copy_n(cfgHeight.begin(), 2, m_MapHeight.begin());
  } else {
    CFG->SetUint8Array("map.height", m_MapHeight.data(), 2);
  }
  if (ByteArrayToUInt16(m_MapHeight, false) == 0) {
    // Default invalid <map.height> values to 1
    m_MapHeight = {1, 0};
  }

  if (CFG->Exists("map.editor_version")) {
    m_MapEditorVersion = CFG->GetUint32("map.editor_version", m_MapEditorVersion);
  } else {
    CFG->SetUint32("map.editor_version", m_MapEditorVersion);
  }

  if (CFG->Exists("map.num_disabled")) {
    m_MapNumDisabled = CFG->GetUint8("map.num_disabled", m_MapNumDisabled);
  } else {
    CFG->SetUint8("map.num_disabled", m_MapNumDisabled);
  }

  if (CFG->Exists("map.num_players")) {
    m_MapNumControllers = CFG->GetUint8("map.num_players", m_MapNumControllers);
  } else {
    CFG->SetUint8("map.num_players", m_MapNumControllers);
  }

  if (CFG->Exists("map.num_teams")) {
    m_MapNumTeams = CFG->GetUint8("map.num_teams", m_MapNumTeams);
  } else {
    CFG->SetUint8("map.num_teams", m_MapNumTeams);
  }

  // Game version compatibility and suggestions
  if (CFG->Exists("map.game_version.min")) {
    m_MapMinGameVersion = CFG->GetUint8("map.game_version.min", m_MapMinGameVersion);
  }

  if (CFG->Exists("map.game_version.suggested.min")) {
    m_MapMinSuggestedGameVersion = CFG->GetUint8("map.game_version.suggested.min", m_MapMinSuggestedGameVersion);
  }

  if (m_MapMinSuggestedGameVersion < m_MapMinGameVersion) {
    m_MapMinSuggestedGameVersion = m_MapMinGameVersion;
  }

  if (!CFG->Exists("map.game_version.min")) {
    CFG->SetUint8("map.game_version.min", m_MapMinGameVersion);
  }

  if (!CFG->Exists("map.game_version.suggested.min")) {
    CFG->SetUint8("map.game_version.suggested.min", m_MapMinSuggestedGameVersion);
  }

  if (m_MapMinGameVersion >= 29) {
    m_MapVersionMaxSlots = static_cast<uint8_t>(MAX_SLOTS_MODERN);
  } else {
    m_MapVersionMaxSlots = static_cast<uint8_t>(MAX_SLOTS_LEGACY);
  }

  if (m_Aura->m_MaxSlots < m_MapVersionMaxSlots) {
    Print("[MAP] " + ToDecString(m_Aura->m_MaxSlots) + " player limit enforced in modern map (editor version " + to_string(m_MapEditorVersion) + ")");
    m_MapVersionMaxSlots = m_Aura->m_MaxSlots;
  }

  if (CFG->Exists("map.slot_1")) {
    vector<CGameSlot> cfgSlots;

    for (uint32_t slotNum = 1; slotNum <= m_MapVersionMaxSlots; ++slotNum) {
      string encodedSlot = CFG->GetString("map.slot_" + to_string(slotNum));
      if (encodedSlot.empty()) {
        break;
      }
      vector<uint8_t> slotData = ExtractNumbers(encodedSlot, 10);
      if (slotData.size() < 9) {
        // Last (10th) element is optional for backwards-compatibility
        // it's the type of slot (SLOTTYPE_USER by default)
        break;
      }
      cfgSlots.emplace_back(slotData);
    }
    if (!cfgSlots.empty()) {
      if (m_Slots.empty() || cfgSlots.size() == m_MapVersionMaxSlots) {
        // No slot data from MPQ - or config supports observers
        m_Slots.swap(cfgSlots);
      } else if (m_Slots.size() == cfgSlots.size()) {
        // Override MPQ slot data with slots from config
        m_Slots.swap(cfgSlots);
      } else {
        // Slots from config are not compatible with slots parsed from MPQ
        CFG->SetFailed();
        if (m_ErrorMessage.empty()) {
          m_ErrorMessage = "<map.slots> do not match the map";
        }
      }
    }
  } else {
    uint32_t slotNum = 0;
    for (const auto& slot : m_Slots) {
      CFG->SetUint8Vector("map.slot_" + to_string(++slotNum), slot.GetByteArray());
    }
  }

  // Maps supporting observer slots enable them by default.
  if (m_Slots.size() + m_MapNumDisabled < m_MapVersionMaxSlots) {
    SetMapObservers(MAPOBS_ALLOWED);
  }

  LoadGameConfigOverrides(*CFG);
  LoadMapSpecificConfig(*CFG);

  // Out of the box support for auto-starting maps using the Host Force + Others Force pattern.
  if (m_MapNumTeams == 2 && m_MapNumControllers > 2 && !m_AutoStartRequiresBalance.has_value()) {
    uint8_t refTeam = 0xFF;
    uint8_t playersRefTeam = 0;
    uint8_t i = static_cast<uint8_t>(m_Slots.size());
    while (i--) {
      if (refTeam == 0xFF) {
        refTeam = m_Slots[i].GetTeam();
        ++playersRefTeam;
      } else if (refTeam == m_Slots[i].GetTeam()) {
        ++playersRefTeam;
      }
    }
    if (playersRefTeam == 1 || playersRefTeam + 1u == static_cast<uint8_t>(m_Slots.size())) {
      m_AutoStartRequiresBalance = false;
      CFG->SetBool("map.hosting.autostart.requires_balance", false);
    }
  }

  if (!CFG->GetSuccess()) {
    m_Valid = false;
    if (m_ErrorMessage.empty()) m_ErrorMessage = "invalid map config file";
    Print("[MAP] " + m_ErrorMessage);
  } else {
    string ErrorMessage = CheckProblems();
    if (!ErrorMessage.empty()) {
      Print("[MAP] " + ErrorMessage);
    } else if (m_MapLoaderIsPartial) {
      CFG->Delete("map.cfg.partial");
      m_MapLoaderIsPartial = false;
    }
  }

  ClearMapFileContents();
}

bool CMap::TryLoadMapFile()
{
  if (m_MapServerPath.empty()) {
    DPRINT_IF(LOG_LEVEL_TRACE2, "m_MapServerPath missing - map data not loaded")
    return false;
  }
  filesystem::path resolvedPath(m_MapServerPath);
  if (m_MapServerPath.filename() == m_MapServerPath && !m_UseStandardPaths) {
    resolvedPath = m_Aura->m_Config.m_MapPath / m_MapServerPath;
  }
  m_MapFileContents = m_Aura->ReadFileCacheable(resolvedPath, MAX_READ_FILE_SIZE);
  if (!HasMapFileContents()) {
    PRINT_IF(LOG_LEVEL_INFO, "[MAP] Failed to read [" + PathToString(resolvedPath) + "]")
    return false;
  }
  return true;
}

bool CMap::TryReloadMapFile()
{
  if (HasMapFileContents()) {
    return false;
  }
  if (!TryLoadMapFile()) {
    return false;
  }
  
  optional<array<uint8_t, 4>> reloadedCRC = CalculateCRC();
  if (!reloadedCRC.has_value() || ByteArrayToUInt32(reloadedCRC.value(), false) != ByteArrayToUInt32(m_MapCRC32, false)) {
    ClearMapFileContents();
    PRINT_IF(LOG_LEVEL_WARNING, "Map file [" + PathToString(m_MapServerPath) + "] has been modified - reload rejected")
    return false;
  }

  return true;
}

FileChunkTransient CMap::GetMapFileChunk(size_t start)
{
  if (HasMapFileContents()) {
    return FileChunkTransient(0, GetMapFileContents());
  } else if (m_MapServerPath.empty()) {
    return FileChunkTransient(0, SharedByteArray());
  } else {
    filesystem::path resolvedPath(m_MapServerPath);
    if (m_MapServerPath.filename() == m_MapServerPath && !m_UseStandardPaths) {
      resolvedPath = m_Aura->m_Config.m_MapPath / m_MapServerPath;
    }
    // Load up to 8 MB at a time
    return m_Aura->ReadFileChunkCacheable(resolvedPath, start, start + 0x800000);
  }
}

bool CMap::UnlinkFile()
{
  if (m_MapServerPath.empty()) return false;
  bool result = false;
  filesystem::path mapLocalPath = m_MapServerPath;
  if (mapLocalPath.is_absolute()) {
    result = FileDelete(mapLocalPath);
  } else {
    filesystem::path resolvedPath =  m_Aura->m_Config.m_MapPath / mapLocalPath;
    result = FileDelete(resolvedPath.lexically_normal());
  }
  if (result) {
    PRINT_IF(LOG_LEVEL_NOTICE, "[MAP] Deleted [" + PathToString(m_MapServerPath) + "]");
  }
  return result;
}

string CMap::CheckProblems()
{
  if (!m_Valid) {
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.empty())
  {
    m_Valid = false;
    m_ErrorMessage = "<map.path> not found";
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.length() > 53)
  {
    m_Valid = false;
    m_ErrorMessage = "<map.path> too long";
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.find('/') != string::npos)
    Print(R"(warning - map.path contains forward slashes '/' but it must use Windows style back slashes '\')");

  if (m_MapSize.size() != 4)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.size> detected";
    return m_ErrorMessage;
  }
  else if (HasMapFileContents() && m_MapFileContents->size() != ByteArrayToUInt32(m_MapSize, false))
  {
    m_Valid = false;
    m_ErrorMessage = "nonmatching <map.size> detected";
    return m_ErrorMessage;
  }

  if (m_MapCRC32.size() != 4)
  {
    m_Valid = false;
    if (m_MapCRC32.empty() && !HasMapFileContents()) {
      m_ErrorMessage = "map file not found";
    } else {
      m_ErrorMessage = "invalid <map.crc32> detected";
    }
    return m_ErrorMessage;
  }

  if (m_MapScriptsWeakHash.size() != 4)
  {
    m_Valid = false;
    if (m_MapScriptsWeakHash.empty() && GetMPQErrored()) {
      m_ErrorMessage = "cannot load map file as MPQ archive";
    } else {
      m_ErrorMessage = "invalid <map.weak_hash> detected";
    }
    return m_ErrorMessage;
  }

  if (m_MapScriptsSHA1.size() != 20)
  {
    m_Valid = false;
    if (m_MapScriptsSHA1.empty() && GetMPQErrored()) {
      m_ErrorMessage = "cannot load map file as MPQ archive";
    } else {
      m_ErrorMessage = "invalid <map.sha1> detected";
    }
    return m_ErrorMessage;
  }

  if (m_MapSpeed != MAPSPEED_SLOW && m_MapSpeed != MAPSPEED_NORMAL && m_MapSpeed != MAPSPEED_FAST)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.speed> detected";
    return m_ErrorMessage;
  }

  if (m_MapVisibility != MAPVIS_HIDETERRAIN && m_MapVisibility != MAPVIS_EXPLORED && m_MapVisibility != MAPVIS_ALWAYSVISIBLE && m_MapVisibility != MAPVIS_DEFAULT)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.visibility> detected";
    return m_ErrorMessage;
  }

  if (m_MapObservers != MAPOBS_NONE && m_MapObservers != MAPOBS_ONDEFEAT && m_MapObservers != MAPOBS_ALLOWED && m_MapObservers != MAPOBS_REFEREES)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.observers> detected";
    return m_ErrorMessage;
  }

  if (m_MapNumDisabled > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.num_disabled> detected";
    return m_ErrorMessage;
  }

  if (m_MapNumControllers < 2 || m_MapNumControllers > MAX_SLOTS_MODERN || m_MapNumControllers + m_MapNumDisabled > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.num_players> detected";
    return m_ErrorMessage;
  }

  if (m_MapNumTeams < 2 || m_MapNumTeams > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.num_teams> detected";
    return m_ErrorMessage;
  }

  if (m_Slots.size() < 2 || m_Slots.size() > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.slot_N> detected";
    return m_ErrorMessage;
  }

  if (
    m_MapNumControllers + m_MapNumDisabled > m_MapVersionMaxSlots ||
    m_MapNumTeams > m_MapVersionMaxSlots ||
    m_Slots.size() > m_MapVersionMaxSlots
  ) {
    m_Valid = false;
    if (m_MapVersionMaxSlots == MAX_SLOTS_LEGACY) {
      m_ErrorMessage = "map uses too many slots - v1.29+ required";
    } else {
      m_ErrorMessage = "map uses an invalid amount of slots";
    }
    return m_ErrorMessage;
  }

  bitset<MAX_SLOTS_MODERN> usedTeams;
  uint8_t controllerSlotCount = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() > m_MapVersionMaxSlots || slot.GetColor() > m_MapVersionMaxSlots) {
      m_Valid = false;
      if (m_MapVersionMaxSlots == MAX_SLOTS_LEGACY) {
        m_ErrorMessage = "map uses too many players - v1.29+ required";
      } else {
        m_ErrorMessage = "map uses an invalid amount of players";
      }
      return m_ErrorMessage;
    }
    if (slot.GetTeam() == m_MapVersionMaxSlots) {
      continue;
    }
    if (slot.GetTeam() > m_MapNumTeams) {
      m_Valid = false;
      m_ErrorMessage = "invalid <map.slot_N> detected";
      return m_ErrorMessage;
    }
    usedTeams.set(slot.GetTeam());
    ++controllerSlotCount;
  }
  if (controllerSlotCount != m_MapNumControllers) {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.slot_N> detected"; 
    return m_ErrorMessage;
  }
  if ((m_MapOptions & MAPOPT_CUSTOMFORCES) && usedTeams.count() <= 1) {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.slot_N> detected";
    return m_ErrorMessage;
  }

  if (m_Aura->m_GameVersion < m_MapMinGameVersion) {
    m_Valid = false;
    m_ErrorMessage = "map requires v1." + to_string(m_MapMinGameVersion) + " (using v1." + to_string(m_Aura->m_GameVersion) + ")";
    return m_ErrorMessage;
  }

  if (!m_Valid) {
    return m_ErrorMessage;
  }

  return string();
}

void CMap::LoadGameConfigOverrides(CConfig& CFG)
{
  const bool wasStrict = CFG.GetStrictMode();
  CFG.SetStrictMode(true);

  if (CFG.Exists("map.hosting.game_over.player_count")) {
    m_NumPlayersToStartGameOver = CFG.GetUint8("map.hosting.game_over.player_count", 1);
  }
  if (CFG.Exists("map.hosting.game_ready.mode")) {
    m_PlayersReadyMode = CFG.GetStringIndex("map.hosting.game_ready.mode", {"fast", "race", "explicit"}, READY_MODE_EXPECT_RACE);
  }
  if (CFG.Exists("map.hosting.autostart.requires_balance")) {
    m_AutoStartRequiresBalance = CFG.GetBool("map.hosting.autostart.requires_balance", false);
  }

  if (CFG.Exists("map.net.start_lag.sync_limit")) {
    m_LatencyMaxFrames = CFG.GetUint32("map.net.start_lag.sync_limit", 32);
  }
  if (CFG.Exists("map.net.stop_lag.sync_limit")) {
    m_LatencySafeFrames = CFG.GetUint32("map.net.stop_lag.sync_limit", 8);
  }

  if (CFG.Exists("map.hosting.high_ping.kick_ms")) {
    m_AutoKickPing = CFG.GetUint32("map.hosting.high_ping.kick_ms", 300);
  }
  if (CFG.Exists("map.hosting.high_ping.warn_ms")) {
    m_WarnHighPing = CFG.GetUint32("map.hosting.high_ping.warn_ms", 200);
  }
  if (CFG.Exists("map.hosting.high_ping.safe_ms")) {
    m_SafeHighPing = CFG.GetUint32("map.hosting.high_ping.safe_ms", 150);
  }

  if (CFG.Exists("map.hosting.expiry.lobby.mode")) {
    m_LobbyTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.lobby.mode", {"never", "empty", "ownerless", "strict"}, LOBBY_TIMEOUT_OWNERLESS);
  }
  if (CFG.Exists("map.hosting.expiry.owner.mode")) {
    m_LobbyOwnerTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.owner.mode", {"never", "absent", "strict"}, LOBBY_OWNER_TIMEOUT_ABSENT);
  }
  if (CFG.Exists("map.hosting.expiry.loading.mode")) {
    m_LoadingTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.loading.mode", {"never", "strict"}, GAME_LOADING_TIMEOUT_STRICT);
  }
  if (CFG.Exists("map.hosting.expiry.playing.mode")) {
    m_PlayingTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.playing.mode", {"never", "dry", "strict"}, GAME_PLAYING_TIMEOUT_STRICT);
  }

  if (CFG.Exists("map.hosting.expiry.lobby.timeout")) {
    m_LobbyTimeout = CFG.GetUint32("map.hosting.expiry.lobby.timeout", 600);
  }
  if (CFG.Exists("map.hosting.expiry.owner.timeout")) {
    m_LobbyOwnerTimeout = CFG.GetUint32("map.hosting.expiry.owner.timeout", 120);
  }
  if (CFG.Exists("map.hosting.expiry.loading.timeout")) {
    m_LoadingTimeout = CFG.GetUint32("map.hosting.expiry.loading.timeout", 900);
  }
  if (CFG.Exists("map.hosting.expiry.playing.timeout")) {
    m_PlayingTimeout = CFG.GetUint32("map.hosting.expiry.playing.timeout", 18000);
  }

  if (CFG.Exists("hosting.expiry.playing.timeout.warnings")) {
    m_PlayingTimeoutWarningShortCountDown = CFG.GetUint8("hosting.expiry.playing.timeout.soon_warnings", 10);
  }
  if (CFG.Exists("hosting.expiry.playing.timeout.interval")) {
    m_PlayingTimeoutWarningShortInterval = CFG.GetUint32("hosting.expiry.playing.timeout.soon_interval", 60);
  }
  if (CFG.Exists("hosting.expiry.playing.timeout.warnings")) {
    m_PlayingTimeoutWarningLargeCountDown = CFG.GetUint8("hosting.expiry.playing.timeout.eager_warnings", 5);
  }
  if (CFG.Exists("hosting.expiry.playing.timeout.interval")) {
    m_PlayingTimeoutWarningLargeInterval = CFG.GetUint32("hosting.expiry.playing.timeout.eager_interval", 900);
  }

  if (CFG.Exists("hosting.expiry.owner.lan")) {
    m_LobbyOwnerReleaseLANLeaver = CFG.GetBool("hosting.expiry.owner.lan", true);
  }

  if (CFG.Exists("map.hosting.game_start.count_down_interval")) {
    m_LobbyCountDownInterval = CFG.GetUint32("map.hosting.game_start.count_down_interval", 500);
  }
  if (CFG.Exists("map.hosting.game_start.count_down_ticks")) {
    m_LobbyCountDownStartValue = CFG.GetUint32("map.hosting.game_start.count_down_ticks", 5);
  }

  if (CFG.Exists("map.bot.latency")) {
    m_Latency = CFG.GetUint16("map.bot.latency", 100);
  }
  if (CFG.Exists("map.bot.latency.equalizer.enabled")) {
    m_LatencyEqualizerEnabled = CFG.GetBool("map.bot.latency.equalizer.enabled", false);
  }
  if (CFG.Exists("map.bot.latency.equalizer.frames")) {
    m_LatencyEqualizerFrames = CFG.GetUint8("map.bot.latency.equalizer.frames", PING_EQUALIZER_DEFAULT_FRAMES);
  }

  if (CFG.Exists("map.reconnection.mode")) {
    m_ReconnectionMode = CFG.GetStringIndex("map.reconnection.mode", {"disabled", "basic", "extended"}, RECONNECT_DISABLED);
    if (m_ReconnectionMode.value() == RECONNECT_ENABLED_GPROXY_EXTENDED) m_ReconnectionMode = m_ReconnectionMode.value() | RECONNECT_ENABLED_GPROXY_BASIC;
  }
  if (CFG.Exists("map.hosting.ip_filter.flood_handler")) {
    m_IPFloodHandler = CFG.GetStringIndex("map.hosting.ip_filter.flood_handler", {"none", "notify", "deny"}, ON_IPFLOOD_DENY);
  }
  if (CFG.Exists("map.hosting.name_filter.unsafe_handler")) {
    m_UnsafeNameHandler = CFG.GetStringIndex("map.hosting.name_filter.unsafe_handler", {"none", "censor", "deny"}, ON_UNSAFE_NAME_DENY);
  }
  if (CFG.Exists("map.hosting.realm_broadcast.error_handler")) {
    m_BroadcastErrorHandler = CFG.GetStringIndex("map.hosting.realm_broadcast.error_handler", {"ignore", "exit_main_error", "exit_empty_main_error", "exit_any_error", "exit_empty_any_error", "exit_max_errors"}, ON_ADV_ERROR_EXIT_ON_MAX_ERRORS);
  }
  if (CFG.Exists("map.hosting.name_filter.is_pipe_harmful")) {
    m_PipeConsideredHarmful = CFG.GetBool("map.hosting.name_filter.is_pipe_harmful", false);
  }
  if (CFG.Exists("map.auto_start.seconds")) {
    m_AutoStartSeconds = CFG.GetInt64("map.auto_start.seconds", 180);
  }
  if (CFG.Exists("map.auto_start.players")) {
    m_AutoStartPlayers = CFG.GetUint8("map.auto_start.players", 2);
  }
  if (CFG.Exists("map.hosting.nicknames.hide_lobby")) {
    m_HideLobbyNames = CFG.GetBool("map.hosting.nicknames.hide_lobby", false);
  }
  if (CFG.Exists("map.hosting.nicknames.hide_in_game")) {
    m_HideInGameNames = CFG.GetStringIndex("map.hosting.nicknames.hide_in_game", {"never", "host", "always", "auto"}, HIDE_IGN_AUTO);
  }
  if (CFG.Exists("map.hosting.nicknames.hide_lobby")) {
    m_HideLobbyNames = CFG.GetBool("map.hosting.nicknames.hide_lobby", false);
  }
  if (CFG.Exists("hosting.load_in_game.enabled")) {
    m_HideLobbyNames = CFG.GetBool("hosting.load_in_game.enabled", false);
  }
  if (CFG.Exists("hosting.join_in_progress.observers")) {
    m_HideLobbyNames = CFG.GetBool("hosting.join_in_progress.observers", false);
  }
  if (CFG.Exists("hosting.join_in_progress.players")) {
    m_LogCommands = CFG.GetBool("hosting.join_in_progress.players", false);
  }

  CFG.SetStrictMode(wasStrict);
}

void CMap::LoadMapSpecificConfig(CConfig& CFG)
{
  const bool wasStrict = CFG.GetStrictMode();
  CFG.SetStrictMode(true);

  // Note: m_ClientMapPath can be computed from m_MapServerPath - this is a cache
  m_ClientMapPath = CFG.GetString("map.path");

  // These aren't necessarily passed verbatim to CGameConfig
  // (CGameSetup members may be used instead)
  m_MapSpeed = CFG.GetUint8("map.speed", MAPSPEED_FAST);
  m_MapVisibility = CFG.GetUint8("map.visibility", MAPVIS_DEFAULT);
  if (CFG.Exists("map.observers")) {
    SetMapObservers(CFG.GetUint8("map.observers", m_MapObservers));
    CFG.FailIfErrorLast();
  }
  if (CFG.Exists("map.filter_obs")) {
    m_MapFilterObs = CFG.GetUint8("map.filter_obs", m_MapFilterObs);
    CFG.FailIfErrorLast();
  }
  m_MapFilterMaker = CFG.GetUint8("map.filter_maker", MAPFILTER_MAKER_USER);
  m_MapFilterSize = CFG.GetUint8("map.filter_size", MAPFILTER_SIZE_LARGE);

  m_MapSiteURL = CFG.GetString("map.site");
  m_MapShortDesc = CFG.GetString("map.short_desc");
  m_MapURL = CFG.GetString("map.url");

  m_MapType = CFG.GetString("map.type");
  m_MapMetaDataEnabled = CFG.GetBool("map.meta_data.enabled", m_MapType == "dota" || m_MapType == "evergreen");
  m_MapDefaultHCL = CFG.GetString("map.default_hcl");
  if (!CheckIsValidHCL(m_MapDefaultHCL).empty()) {
    Print("[MAP] HCL string [" + m_MapDefaultHCL + "] is not valid.");
    CFG.SetFailed();
  }

  // Host to bot map communication (W3HMC)
  m_HMCMode = CFG.GetStringIndex("map.w3hmc.mode", {"disabled", "optional", "required"}, W3HMC_MODE_DISABLED);
  m_HMCTrigger1 = CFG.GetUint8("map.w3hmc.trigger_1", 0);
  m_HMCTrigger2 = CFG.GetUint8("map.w3hmc.trigger_2", 0);
  m_HMCSlot = CFG.GetUint8("map.w3hmc.slot", 1);
  m_HMCPlayerName = CFG.GetString("map.w3hmc.player_name", 1, 15, "[HMC]Aura");

  CFG.SetStrictMode(wasStrict);
}

uint8_t CMap::GetLobbyRace(const CGameSlot* slot) const
{
  bool isFixedRace = GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS;
  bool isRandomRace = GetMapFlags() & MAPFLAG_RANDOMRACES;
  if (isFixedRace) return slot->GetRaceFixed();
  // If the map has fixed player settings, races cannot be randomized.
  if (isRandomRace) return SLOTRACE_RANDOM;
  // Note: If the slot was never selectable, it isn't promoted to selectable.
  return slot->GetRaceSelectable();
}
