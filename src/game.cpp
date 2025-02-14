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

#include "game.h"
#include "command.h"
#include "aura.h"
#include "util.h"
#include "config/config.h"
#include "config/config_bot.h"
#include "config/config_game.h"
#include "config/config_commands.h"
#include "irc.h"
#include "socket.h"
#include "net.h"
#include "auradb.h"
#include "realm.h"
#include "map.h"
#include "connection.h"
#include "game_user.h"
#include "game_virtual_user.h"
#include "protocol/game_protocol.h"
#include "protocol/gps_protocol.h"
#include "protocol/vlan_protocol.h"
#include "stats.h"
#include "w3mmd.h"
#include "irc.h"
#include "file_util.h"

#include <bitset>
#include <ctime>
#include <cmath>
#include <algorithm>

using namespace std;

//
// CGameLogRecord
//

#define LOG_APP_IF(T, U) \
    static_assert(T < LOG_LEVEL_TRACE, "Use DLOG_APP_IF for tracing log levels");\
    if (m_Aura->MatchLogLevel(T)) {\
        LogApp(U); \
    }

#ifdef DEBUG
#define DLOG_APP_IF(T, U) \
    static_assert(T >= LOG_LEVEL_TRACE, "Use LOG_APP_IF for regular log levels");\
    if (m_Aura->MatchLogLevel(T)) {\
        LogApp(U); \
    }
#else
#define DLOG_APP_IF(T, U)
#endif

CGameLogRecord::CGameLogRecord(int64_t gameTicks, string text)
  : m_Ticks(gameTicks),
    m_Text(move(text))
{
}

string CGameLogRecord::ToString() const
{
  int64_t gameTicks = m_Ticks;
  int64_t hours = gameTicks / 3600000;
  gameTicks -= hours * 3600000;
  int64_t mins = gameTicks / 60000;
  gameTicks -= mins * 60000;
  int64_t seconds = gameTicks / 1000;
  string hh = to_string(hours);
  string mm = to_string(mins);
  string ss = to_string(seconds);
  if (hours < 10) hh = "0" + hh;
  if (mins < 10) mm = "0" + mm;
  if (seconds < 10) ss = "0" + ss;
  return "[" + hh + ":" + mm + ":" + ss + "] " + m_Text;
}

CGameLogRecord::~CGameLogRecord() = default;

//
// CQueuedActionsFrame
//

CQueuedActionsFrame::CQueuedActionsFrame()
 : callback(ON_SEND_ACTIONS_NONE),
   pauseUID(0xFF),
   bufferSize(0),
   activeQueue(nullptr)
 {
   activeQueue = &actions.emplace_back();
 }

CQueuedActionsFrame::~CQueuedActionsFrame() = default;

void CQueuedActionsFrame::AddAction(CIncomingAction&& action)
{
  const uint16_t actionSize = action.GetLength();

  // we aren't allowed to send more than 1460 bytes in a single packet but it's possible we might have more than that many bytes waiting in the queue
  // check if adding the next action to the sub actions queue would put us over the limit
  // (1452 because the INCOMING_ACTION and INCOMING_ACTION2 packets use an extra 8 bytes)
  // we'd be over the limit if we added the next action to the sub actions queue

  /*if (actionSize > 1452) {
    if (bufferSize > 0) {
      activeQueue = &actions.emplace_back();
    }
    bufferSize = actionSize;
  } else */if (bufferSize + actionSize > 1452) {
    activeQueue = &actions.emplace_back();
    activeQueue->reserve(DEFAULT_ACTIONS_PER_FRAME);
    bufferSize = actionSize;
  } else {
    bufferSize += actionSize;
  }
  activeQueue->push_back(std::move(action));
}

vector<uint8_t> CQueuedActionsFrame::GetBytes(const uint16_t sendInterval) const
{
  vector<uint8_t> packet;

  // the W3GS_INCOMING_ACTION2 packet handles the overflow but it must be sent *before*
  // the corresponding W3GS_INCOMING_ACTION packet

  auto it = actions.begin();
  auto back = actions.end() - 1;
  while (it != back) {
    const vector<uint8_t> subPacket = GameProtocol::SEND_W3GS_INCOMING_ACTION2(*it);
    AppendByteArrayFast(packet, subPacket);
    ++it;
  }

  {
    const vector<uint8_t> subPacket = GameProtocol::SEND_W3GS_INCOMING_ACTION(*it, sendInterval);
    AppendByteArrayFast(packet, subPacket);
  }

  // Note: Must ensure Reset() is called afterwards
  return packet;
}

void CQueuedActionsFrame::Reset()
{
  actions.clear();
  callback = ON_SEND_ACTIONS_NONE;
  bufferSize = 0;
  activeQueue = &actions.emplace_back();
  activeQueue->reserve(DEFAULT_ACTIONS_PER_FRAME);
  leavers.clear();
}

bool CQueuedActionsFrame::GetIsEmpty() const
{
  if (callback != ON_SEND_ACTIONS_NONE) return false;
  if (!leavers.empty()) return false;
  if (bufferSize != 0) return false;
  if (actions.empty() && activeQueue == nullptr) return true;
  return actions.size() == 1 && activeQueue == &actions.front() && activeQueue->empty();
}

size_t CQueuedActionsFrame::GetActionCount() const
{
  if (actions.empty()) return 0;
  uint32_t count = 0;
  for (const ActionQueue& actionQueue : actions) {
    count += actionQueue.size();
  }
  return count;
}

void CQueuedActionsFrame::MergeFrame(CQueuedActionsFrame& frame)
{
  if (frame.bufferSize == 0) return;

  callback = frame.callback;

  for (auto& user : frame.leavers) {
    leavers.push_back(user);
  }

  auto it = frame.actions.begin();
  while (it != frame.actions.end()) {
    ActionQueue& subActions = (*it);
    auto it2 = subActions.begin();
    while (it2 != subActions.end()) {
      AddAction(std::move(*it2));
      ++it2;
    }
    // subActions is now in indeterminate state, so use erase() instead of clear()
    subActions.erase(subActions.begin(), subActions.end());
    ++it;
  }
  frame.Reset();
}

bool CQueuedActionsFrame::GetHasActionsBy(const uint8_t UID) const
{
  for (const ActionQueue& actionQueue : actions) {
    for (const auto& action : actionQueue) {
      if (action.GetUID() == UID) {
        return true;
      }
    }
  }
  return false;
}

//
// CGame
//

CGame::CGame(CAura* nAura, shared_ptr<CGameSetup> nGameSetup)
  : m_Aura(nAura),
    m_Config(CGameConfig(nAura->m_GameDefaultConfig, nGameSetup->m_Map, nGameSetup)),
    m_Verbose(nGameSetup->m_Verbose),
    m_Socket(nullptr),
    m_LastLeaverBannable(nullptr),
    m_CustomStats(nullptr),
    m_DotaStats(nullptr),
    m_RestoredGame(nGameSetup->m_RestoredGame),
    m_CurrentActionsFrame(nullptr),
    m_Map(nGameSetup->m_Map),
    m_GameFlags(0),
    m_PauseUser(nullptr),
    m_GameName(nGameSetup->m_Name),
    m_GameHistoryId(nAura->NextHistoryGameID()),
    m_FromAutoReHost(nGameSetup->m_LobbyAutoRehosted),
    m_OwnerLess(nGameSetup->m_OwnerLess),
    m_OwnerName(nGameSetup->m_Owner.first),
    m_OwnerRealm(nGameSetup->m_Owner.second),
    m_CreatorText(nGameSetup->m_Attribution),
    m_CreatedBy(nGameSetup->m_CreatedBy),
    m_CreatedFrom(nGameSetup->m_CreatedFrom),
    m_CreatedFromType(nGameSetup->m_CreatedFromType),
    m_RealmsExcluded(nGameSetup->m_RealmsExcluded),
    m_HCLCommandString(nGameSetup->m_HCL.has_value() ? nGameSetup->m_HCL.value() : nGameSetup->m_Map->GetMapDefaultHCL()),
    m_MapPath(nGameSetup->m_Map->GetClientPath()),
    m_MapSiteURL(nGameSetup->m_Map->GetMapSiteURL()),
    m_GameTicks(0),
    m_CreationTime(GetTime()),
    m_LastPingTime(GetTime()),
    m_LastRefreshTime(GetTime()),
    m_LastDownloadTicks(GetTime()),
    m_LastDownloadCounterResetTicks(GetTicks()),
    m_LastCountDownTicks(0),
    m_StartedLoadingTicks(0),
    m_FinishedLoadingTicks(0),
    m_LastActionSentTicks(0),
    m_LastActionLateBy(0),
    m_LastPausedTicks(0),
    m_PausedTicksDeltaSum(0),
    m_StartedLaggingTime(0),
    m_LastLagScreenTime(0),
    m_PingReportedSinceLagTimes(0),
    m_LastUserSeen(GetTicks()),
    m_LastOwnerSeen(GetTicks()),
    m_StartedKickVoteTime(0),
    m_LastCustomStatsUpdateTime(0),
    m_GameOver(GAME_ONGOING),
    m_LastLagScreenResetTime(0),
    m_RandomSeed(0),
    m_HostCounter(nGameSetup->m_Identifier.has_value() ? nGameSetup->m_Identifier.value() : nAura->NextHostCounter()),
    m_EntryKey(0),
    m_SyncCounter(0),
    m_SyncCounterChecked(0),
    m_MaxPingEqualizerDelayFrames(0),
    m_LastPingEqualizerGameTicks(0),
    m_DownloadCounter(0),
    m_CountDownCounter(0),
    m_StartPlayers(0),
    m_ControllersBalanced(false),
    m_ControllersReadyCount(0),
    m_ControllersNotReadyCount(0),
    m_ControllersWithMap(0),
    m_CustomLayout(nGameSetup->m_CustomLayout.has_value() ? nGameSetup->m_CustomLayout.value() : MAPLAYOUT_ANY),
    m_CustomLayoutData(make_pair(nAura->m_MaxSlots, nAura->m_MaxSlots)),
    m_HostPort(0),
    m_PublicHostOverride(nGameSetup->GetIsMirror()),
    m_DisplayMode(nGameSetup->m_RealmsDisplayMode),
    m_IsAutoVirtualPlayers(false),
    m_VirtualHostUID(0xFF),
    m_Exiting(false),
    m_ExitingSoon(false),
    m_SlotInfoChanged(0),
    m_JoinedVirtualHosts(0),
    m_ReconnectProtocols(0),
    m_Replaceable(nGameSetup->m_LobbyReplaceable),
    m_Replacing(false),
    m_PublicStart(false),
    m_Locked(false),
    m_ChatOnly(false),
    m_MuteAll(false),
    m_MuteLobby(false),
    m_IsMirror(nGameSetup->GetIsMirror()),
    m_CountDownStarted(false),
    m_CountDownFast(false),
    m_CountDownUserInitiated(false),
    m_GameLoading(false),
    m_GameLoaded(false),
    m_LobbyLoading(false),
    m_Lagging(false),
    m_Paused(false),
    m_Desynced(false),
    m_IsDraftMode(false),
    m_IsHiddenPlayerNames(false),
    m_HadLeaver(false),
    m_CheckReservation(nGameSetup->m_ChecksReservation.has_value() ? nGameSetup->m_ChecksReservation.value() : nGameSetup->m_RestoredGame != nullptr),
    m_UsesCustomReferees(false),
    m_SentPriorityWhois(false),
    m_Remaking(false),
    m_Remade(false),
    m_SaveOnLeave(SAVE_ON_LEAVE_AUTO),
    m_HMCEnabled(false),
    m_BufferingEnabled(BUFFERING_ENABLED_NONE),
    m_BeforePlayingEmptyActions(0),
    m_SupportedGameVersionsMin(nAura->m_GameVersion),
    m_SupportedGameVersionsMax(nAura->m_GameVersion),
    m_GameDiscoveryInfoChanged(false),
    m_GameDiscoveryInfoVersionOffset(0),
    m_GameDiscoveryInfoDynamicOffset(0)
{
  m_IsHiddenPlayerNames = m_Config.m_HideLobbyNames;
  m_SupportedGameVersionsMin = m_Aura->m_GameVersion;
  m_SupportedGameVersionsMax = m_Aura->m_GameVersion;
  m_SupportedGameVersions.set(m_Aura->m_GameVersion);
  vector<uint8_t> supportedGameVersions = !nGameSetup->m_SupportedGameVersions.empty() ? nGameSetup->m_SupportedGameVersions : m_Aura->m_GameDefaultConfig->m_SupportedGameVersions;
  for (const auto& version : supportedGameVersions) {
    if (version >= 64) continue;
    if (m_Aura->m_GameVersion >= 29) {
      if (version < 29) continue;
    } else {
      if (version >= 29) continue;
    }
    if (m_Aura->m_GameVersion >= 23) {
      if (version < 23) continue;
    } else {
      if (version >= 23) continue;
    }
    m_SupportedGameVersions.set(version);
    if (version < m_SupportedGameVersionsMin) m_SupportedGameVersionsMin = version;
    if (version > m_SupportedGameVersionsMax) m_SupportedGameVersionsMax = version;
  }

  if (m_Config.m_LoadInGame) {
    m_BufferingEnabled |= BUFFERING_ENABLED_LOADING;
  }
  if (m_Config.m_EnableJoinObserversInProgress || m_Config.m_EnableJoinPlayersInProgress) {
    m_BufferingEnabled |= BUFFERING_ENABLED_ALL;
  }

  m_GameFlags = CalcGameFlags();

  if (!nGameSetup->GetIsMirror()) {
    for (const auto& userName : nGameSetup->m_Reservations) {
      AddToReserved(userName);
    }

    InitPRNG();

    // wait time of 1 minute  = 0 empty actions required
    // wait time of 2 minutes = 1 empty action required...

    if (m_GProxyEmptyActions > 0) {
      m_GProxyEmptyActions = m_Aura->m_Net.m_Config.m_ReconnectWaitTicksLegacy / 60000 - 1;
      if (m_GProxyEmptyActions > 9) {
        m_GProxyEmptyActions = 9;
      }
    }

    // start listening for connections

    uint16_t hostPort = nAura->m_Net.NextHostPort();
    m_Socket = m_Aura->m_Net.GetOrCreateTCPServer(hostPort, "Game <<" + m_GameName + ">>");

    if (m_Socket) {
      m_HostPort = m_Socket->GetPort();
    } else {
      m_Exiting = true;
    }

    // Only maps in <bot.maps_path>
    if (m_Map->GetMapFileIsFromManagedFolder()) {
      auto it = m_Aura->m_MapFilesTimedBusyLocks.find(m_Map->GetServerPath());
      if (it == m_Aura->m_MapFilesTimedBusyLocks.end()) {
        m_Aura->m_MapFilesTimedBusyLocks[m_Map->GetServerPath()] = make_pair<int64_t, uint16_t>(GetTicks(), 0u);
      } else {
        it->second.first = GetTicks();
        it->second.second++;
      }
    }
  } else {
    SetIsCheckJoinable(false);
    m_PublicHostAddress = AddressToIPv4Array(&(nGameSetup->m_RealmsAddress));
    m_PublicHostPort = GetAddressPort(&(nGameSetup->m_RealmsAddress));
  }

  InitSlots();
  UpdateReadyCounters();

  if (!m_IsMirror) {
    if (nGameSetup->m_AutoStartSeconds.has_value() || nGameSetup->m_AutoStartPlayers.has_value()) {
      uint8_t autoStartPlayers = nGameSetup->m_AutoStartPlayers.value_or(0);
      int64_t autoStartSeconds = (int64_t)nGameSetup->m_AutoStartSeconds.value_or(0);
      if (!nGameSetup->m_AutoStartPlayers.has_value() || autoStartPlayers > m_ControllersReadyCount) {
        m_AutoStartRequirements.push_back(make_pair(
          autoStartPlayers,
          m_CreationTime + autoStartSeconds
        ));
      }
    } else if (m_Map->m_AutoStartSeconds.has_value() || m_Map->m_AutoStartPlayers.has_value()) {
      uint8_t autoStartPlayers = m_Map->m_AutoStartPlayers.value_or(0);
      int64_t autoStartSeconds = (int64_t)m_Map->m_AutoStartSeconds.value_or(0);
      if (m_Map->m_AutoStartPlayers.has_value() || autoStartPlayers > m_ControllersReadyCount) {
        m_AutoStartRequirements.push_back(make_pair(
          autoStartPlayers,
          m_CreationTime + autoStartSeconds
        ));
      }
    }
  }
}

void CGame::ClearActions()
{
  m_Actions.reset();
}

void CGame::Reset()
{
  m_PauseUser = nullptr;
  m_FakeUsers.clear();

  for (auto& entry : m_SyncPlayers) {
    entry.second.clear();
  }
  m_SyncPlayers.clear();

  ClearActions();

  if (m_GameLoaded && m_Config.m_SaveStats) {
    // store the CDBGamePlayers in the database
    // add non-dota stats
    if (!m_DBGamePlayers.empty()) {
      int64_t Ticks = GetTicks();
      LOG_APP_IF(LOG_LEVEL_DEBUG, "[STATS] saving game end player data to database")
      if (m_Aura->m_DB->Begin()) {
        for (auto& dbPlayer : m_DBGamePlayers) {
          // exclude observers
          if (dbPlayer->GetColor() == m_Map->GetVersionMaxSlots()) {
            continue;
          }
          m_Aura->m_DB->UpdateGamePlayerOnEnd(
            dbPlayer->GetName(),
            dbPlayer->GetServer(),
            dbPlayer->GetIP(),
            dbPlayer->GetLoadingTime(),
            m_GameTicks / 1000,
            dbPlayer->GetLeftTime()
          );
        }
        if (!m_Aura->m_DB->Commit()) {
          LOG_APP_IF(LOG_LEVEL_WARNING, "[STATS] failed to commit game end player data")
        } else {
          LOG_APP_IF(LOG_LEVEL_DEBUG, "[STATS] commited game end player data in " + to_string(GetTicks() - Ticks) + " ms")
        }
      } else {
        LOG_APP_IF(LOG_LEVEL_WARNING, "[STATS] failed to begin transaction game end player data")
      }
    }
    // store the stats in the database
    if (m_CustomStats) {
      m_CustomStats->FlushQueue();
      LOG_APP_IF(LOG_LEVEL_INFO, "[STATS] MMD detected winners: " + JoinVector(m_CustomStats->GetWinners(), false))
    }
    if (m_DotaStats) m_DotaStats->Save(m_Aura, m_Aura->m_DB);
  }

  for (auto& user : m_DBGamePlayers) {
    delete user;
  }
  m_DBGamePlayers.clear();

  ClearBannableUsers();

  delete m_CustomStats;
  m_CustomStats = nullptr;

  delete m_DotaStats;
  m_DotaStats = nullptr;

  for (const auto& ptr : m_Aura->m_ActiveContexts) {
    auto ctx = ptr.lock();
    if (!ctx) continue;
    if (ctx->m_SourceGame == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_SourceGame = nullptr;
    }
    if (ctx->m_TargetGame == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_TargetGame = nullptr;
    }
  }

  for (auto& realm : m_Aura->m_Realms) {
    realm->ResetGameChatAnnouncement();
  }
}

void CGame::ReleaseMapBusyTimedLock() const
{
  // check whether the map is in <bot.maps_path>
  if (!m_Map->GetMapFileIsFromManagedFolder()) {
    return;
  }

  auto it = m_Aura->m_MapFilesTimedBusyLocks.find(m_Map->GetServerPath());
  if (it == m_Aura->m_MapFilesTimedBusyLocks.end()) {
    return;
  }

  it->second.first = GetTicks();
  if (--it->second.second > 0) {
    return;
  }

  const bool deleteTooLarge = (
    m_Aura->m_Config.m_EnableDeleteOversizedMaps &&
    (ByteArrayToUInt32(m_Map->GetMapSize(), false) > m_Aura->m_Config.m_MaxSavedMapSize * 1024) &&
    // Ensure the mapcache ini file has been created before trying to delete from disk
    m_Aura->m_CFGCacheNamesByMapNames.find(m_Map->GetServerPath()) != m_Aura->m_CFGCacheNamesByMapNames.end()
  );

  if (deleteTooLarge) {
    // Release from disk
    m_Map->UnlinkFile();
  }
}

void CGame::StartGameOverTimer(bool isMMD)
{
  m_ExitingSoon = true;
  m_GameOver = isMMD ? GAME_OVER_MMD : GAME_OVER_TRUSTED;
  m_GameOverTime = GetTime();
  if (isMMD) {
    m_GameOverTolerance = 300;
  } else {
    m_GameOverTolerance = 60;
  }

  if (GetNumJoinedUsers() > 0) {
    SendAllChat("Gameover timer started (disconnecting in " + to_string(m_GameOverTolerance.value_or(60)) + " seconds...)");
  }

  if (GetIsLobby()) {
    if (GetUDPEnabled()) {
      SendGameDiscoveryDecreate();
      SetUDPEnabled(false);
    }
    if (m_DisplayMode != GAME_NONE) {
      AnnounceDecreateToRealms();
      m_DisplayMode = GAME_NONE;
    }
    m_ChatOnly = true;
    StopCountDown();
  }

  m_Aura->UntrackGameJoinInProgress(this);
}

CGame::~CGame()
{
  Reset();
  ReleaseMapBusyTimedLock();

  for (auto& user : m_Users) {
    delete user;
  }

  if (GetIsBeingReplaced()) {
    --m_Aura->m_ReplacingLobbiesCounter;
  }
  m_Aura->UntrackGameJoinInProgress(this);
}

void CGame::InitPRNG()
{
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<uint32_t> dis;
  m_RandomSeed = dis(gen);
  m_EntryKey = dis(gen);
}

void CGame::InitSlots()
{
  if (m_RestoredGame) {
    m_Slots = m_RestoredGame->GetSlots();
    // reset user slots
    for (auto& slot : m_Slots) {
      if (slot.GetIsPlayerOrFake()) {
        slot.SetUID(0);
        slot.SetDownloadStatus(100);
        slot.SetSlotStatus(SLOTSTATUS_OPEN);
      }
    }
    return;
  }

  // Done at the CGame level rather than CMap,
  // so that Aura is able to deal with outdated/bugged map configs.

  m_Slots = m_Map->GetSlots();

  const bool useObservers = m_Map->GetMapObservers() == MAPOBS_ALLOWED || m_Map->GetMapObservers() == MAPOBS_REFEREES;

  // Match actual observer slots to set map flags.
  if (!useObservers) {
    CloseObserverSlots();
  }

  const bool customForces = m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES;
  const bool fixedPlayers = m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS;
  bitset<MAX_SLOTS_MODERN> usedColors;
  for (auto& slot : m_Slots) {
    slot.SetUID(0);
    slot.SetDownloadStatus(SLOTPROG_RST);

    if (!fixedPlayers) {
      slot.SetType(SLOTTYPE_USER);
    } else switch (slot.GetType()) {
      case SLOTTYPE_USER:
        break;
      case SLOTTYPE_COMP:
        slot.SetComputer(SLOTCOMP_YES);
        break;
      default:
        // Treat every other value as SLOTTYPE_AUTO
        // CMap should never set SLOTTYPE_NONE
        // I bet that we don't need to set SLOTTYPE_NEUTRAL nor SLOTTYPE_RESCUEABLE either,
        // since we already got <map.num_disabled>
        if (slot.GetIsComputer()) {
          slot.SetType(SLOTTYPE_COMP);
        } else {
          slot.SetType(SLOTTYPE_USER);
        }
        break;
    }

    if (slot.GetComputer() > 0) {
      // The way WC3 client treats computer slots defined from WorldEdit depends on the
      // Fixed Player Settings flag:
      //  - OFF: Any computer slots are ignored, and they are treated as Open slots instead.
      //  - ON: Computer slots are enforced. They cannot be removed, or edited in any way.
      //
      // For Aura, enforcing computer slots with Fixed Player Settings ON is a must.
      // However, we can support default editable computer slots when it's OFF, through mapcfg files.
      //
      // All this also means that when Fixed Player Settings is off, there are no unselectable slots.
      slot.SetComputer(SLOTCOMP_YES);
      slot.SetSlotStatus(SLOTSTATUS_OCCUPIED);
    } else {
      //slot.SetComputer(SLOTCOMP_NO);
      slot.SetSlotStatus(slot.GetSlotStatus() & SLOTSTATUS_VALID_INITIAL_NON_COMPUTER);
    }

    if (!slot.GetIsSelectable()) {
      // There is no way to define default handicaps/difficulty using WorldEdit,
      // and unselectable cannot be changed in the game lobby.
      slot.SetHandicap(100);
      slot.SetComputerType(SLOTCOMP_NORMAL);
    } else {
      // Handicap valid engine values are 50, 60, 70, 80, 90, 100
      // The other 250 uint8 values may be set on-the-fly by Aura,
      // and are used by maps that implement HCL.
      //
      // Aura supports default handicaps through mapcfg files.
      uint8_t handicap = slot.GetHandicap() / 10;
      if (handicap < 5) handicap = 5;
      if (handicap > 10) handicap = 10;
      slot.SetHandicap(handicap * 10);
      slot.SetComputerType(slot.GetComputerType() & SLOTCOMP_VALID);
    }

    if (!customForces) {
      // default user-customizable slot is always observer
      // only when users join do we assign them a team
      // (if they leave, the slots are reset to observers)
      slot.SetTeam(m_Map->GetVersionMaxSlots());
    }

    // Ensure colors are unique for each playable slot.
    // Observers must have color 12 or 24, according to game version.
    if (slot.GetTeam() == m_Map->GetVersionMaxSlots()) {
      slot.SetColor(m_Map->GetVersionMaxSlots());
    } else {
      const uint8_t originalColor = slot.GetColor();
      if (usedColors.test(originalColor)) {
        uint8_t testColor = originalColor;
        do {
          testColor = (testColor + 1) % m_Map->GetVersionMaxSlots();
        } while (usedColors.test(testColor) && testColor != originalColor);
        slot.SetColor(testColor);
        usedColors.set(testColor);
      } else {
        usedColors.set(originalColor);
      }
    }

    // When Fixed Player Settings is enabled, MAPFLAG_RANDOMRACES cannot be turned on.
    if (!fixedPlayers && (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES)) {
      slot.SetRace(SLOTRACE_RANDOM);
    } else {
      // Ensure race is unambiguous. It's defined as a bitfield,
      // so we gotta unset contradictory bits.
      bitset<8> slotRace(slot.GetRace());
      slotRace.reset(7);
      if (fixedPlayers) {
        // disable SLOTRACE_SELECTABLE
        slotRace.reset(6);
      } else {
        // enable SLOTRACE_SELECTABLE
        slotRace.set(6);
      }
      slotRace.reset(4);
      uint8_t chosenRaceBit = 5; // SLOTRACE_RANDOM
      bool foundRace = false;
      while (chosenRaceBit--) {
        // Iterate backwards so that SLOTRACE_RANDOM is preferred
        // Why? Because if someone edited the mapcfg with an ambiguous race,
        // it's likely they don't know what they are doing.
        if (foundRace) {
          slotRace.reset(chosenRaceBit);
        } else {
          foundRace = slotRace.test(chosenRaceBit);
        }
      }
      if (!foundRace) { // Slot is missing a default race.
        chosenRaceBit = 5; // SLOTRACE_RANDOM
        slotRace.set(chosenRaceBit);
        while (chosenRaceBit--) slotRace.reset(chosenRaceBit);
      }
      slot.SetRace(static_cast<uint8_t>(slotRace.to_ulong()));
    }
  }

  if (useObservers) {
    OpenObserverSlots();
  }

  if (m_Map->GetHMCEnabled()) {
    CreateHMCPlayer();
  }
}

bool CGame::MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const
{
  if (m_CreatedFromType != fromType) return false;
  switch (fromType) {
    case SERVICE_TYPE_REALM:
      return reinterpret_cast<const CRealm*>(m_CreatedFrom) == reinterpret_cast<const CRealm*>(fromThing);
    case SERVICE_TYPE_IRC:
      return reinterpret_cast<const CIRC*>(m_CreatedFrom) == reinterpret_cast<const CIRC*>(fromThing);
    case SERVICE_TYPE_DISCORD:
      return reinterpret_cast<const CDiscord*>(m_CreatedFrom) == reinterpret_cast<const CDiscord*>(fromThing);
  }
  return false;
}

uint8_t CGame::GetLayout() const
{
  if (m_RestoredGame) return MAPLAYOUT_FIXED_PLAYERS;
  return GetMap()->GetMapLayoutStyle();
}

bool CGame::GetIsCustomForces() const
{
  if (m_RestoredGame) return true;
  return GetMap()->GetMapLayoutStyle() != MAPLAYOUT_ANY;
}

int64_t CGame::GetNextTimedActionMicroSeconds() const
{
  // return the number of ticks (ms) until the next "timed action", which for our purposes is the next game update
  // the main Aura loop will make sure the next loop update happens at or before this value
  // note: there's no reason this function couldn't take into account the game's other timers too but they're far less critical
  // warning: this function must take into account when actions are not being sent (e.g. during loading or lagging)

  if (!m_GameLoaded || m_Lagging)
    return 50000;

  const int64_t TicksSinceLastUpdate = GetTicks() - m_LastActionSentTicks;

  if (TicksSinceLastUpdate > GetLatency() - m_LastActionLateBy)
    return 0;
  else
    return (GetLatency() - m_LastActionLateBy - TicksSinceLastUpdate) * 1000;
}

uint32_t CGame::GetSlotsOccupied() const
{
  uint32_t NumSlotsOccupied = 0;

  for (const auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED)
      ++NumSlotsOccupied;
  }

  return NumSlotsOccupied;
}

uint32_t CGame::GetSlotsOpen() const
{
  uint32_t NumSlotsOpen = 0;

  for (const auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OPEN)
      ++NumSlotsOpen;
  }

  return NumSlotsOpen;
}

bool CGame::HasSlotsOpen() const
{
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OPEN)
      return true;
  }
  return false;
}

bool CGame::GetIsSinglePlayerMode() const
{
  return GetNumJoinedUsersOrFake() < 2;
}

bool CGame::GetHasAnyFullObservers() const
{
  return m_Map->GetMapObservers() == MAPOBS_ALLOWED && GetNumJoinedObservers() >= 1;
}

bool CGame::GetHasChatSendHost() const
{
  if (GetHasChatSendPermaHost()) return true;
  if (GetHasAnyFullObservers()) {
    return GetNumJoinedPlayersOrFake() >= 2;
  } else {
    return GetNumJoinedPlayersOrFakeUsers() >= 2;
  }
}

bool CGame::GetHasChatRecvHost() const
{
  if (GetHasChatRecvPermaHost()) return true;
  if (m_Map->GetMapObservers() == MAPOBS_ALLOWED && GetNumJoinedObservers() == 1) return false;
  return GetNumJoinedPlayersOrFakeUsers() >= 2;
}

bool CGame::GetHasChatSendPermaHost() const
{
  return GetNumFakePlayers() > 0 || (m_Map->GetMapObservers() == MAPOBS_REFEREES && GetNumFakeObservers() > 0);
}

bool CGame::GetHasChatRecvPermaHost() const
{
  if (GetNumFakeObservers() > 0) return true;
  return GetNumFakePlayers() > 0 && !GetHasAnyFullObservers();
}

uint32_t CGame::GetNumJoinedUsers() const
{
  uint32_t counter = 0;

  for (const auto& user : m_Users) {
    if (user->GetDeleteMe() || user->GetDisconnectedUnrecoverably())
      continue;

    ++counter;
  }

  return counter;
}

uint32_t CGame::GetNumJoinedUsersOrFake() const
{
  uint32_t counter = static_cast<uint8_t>(m_FakeUsers.size());

  for (const auto& user : m_Users) {
    if (user->GetDeleteMe() || user->GetDisconnectedUnrecoverably())
      continue;

    ++counter;
  }

  return counter;
}

uint8_t CGame::GetNumJoinedPlayers() const
{
  uint8_t counter = 0;

  for (const auto& user : m_Users) {
    if (user->GetDeleteMe() || user->GetDisconnectedUnrecoverably())
      continue;
    if (user->GetIsObserver())
      continue;

    ++counter;
  }

  return counter;
}

uint8_t CGame::GetNumJoinedObservers() const
{
  uint8_t counter = 0;

  for (const auto& user : m_Users) {
    if (user->GetDeleteMe() || user->GetDisconnectedUnrecoverably())
      continue;
    if (!user->GetIsObserver())
      continue;

    ++counter;
  }

  return counter;
}

uint8_t CGame::GetNumFakePlayers() const
{
  uint8_t counter = 0;
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    if (!fakeUser.GetIsObserver()) {
      ++counter;
    }
  }
  return counter;
}

uint8_t CGame::GetNumFakeObservers() const
{
  uint8_t counter = 0;
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    if (fakeUser.GetIsObserver()) {
      ++counter;
    }
  }
  return counter;
}

uint8_t CGame::GetNumJoinedPlayersOrFake() const
{
  return GetNumJoinedPlayers() + GetNumFakePlayers();
}

uint8_t CGame::GetNumJoinedObserversOrFake() const
{
  return GetNumJoinedObservers() + GetNumFakeObservers();
}

uint8_t CGame::GetNumJoinedPlayersOrFakeUsers() const
{
  uint8_t counter = static_cast<uint8_t>(m_FakeUsers.size());

  for (const auto& user : m_Users) {
    if (user->GetDeleteMe() || user->GetDisconnectedUnrecoverably())
      continue;
    if (user->GetIsObserver())
      continue;

    ++counter;
  }

  return counter;
}

uint8_t CGame::GetNumOccupiedSlots() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      ++count;
    }
  }
  return count;
}

uint8_t CGame::GetNumPotentialControllers() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      ++count;
    }
  }
  if (count > m_Map->GetMapNumControllers()) {
    return m_Map->GetMapNumControllers();
  }
  return count;
}

uint8_t CGame::GetNumControllers() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetTeam() != m_Map->GetVersionMaxSlots()) {
      ++count;
    }
  }
  return count;
}

uint8_t CGame::GetNumComputers() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetIsComputer()) {
      ++count;
    }
  }
  return count;
}

uint8_t CGame::GetNumTeamControllersOrOpen(const uint8_t team) const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (slot.GetTeam() == team) {
      ++count;
    }
  }
  return count;
}

string CGame::GetClientFileName() const
{
  size_t LastSlash = m_MapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_MapPath;
  }
  return m_MapPath.substr(LastSlash + 1);
}

string CGame::GetStatusDescription() const
{
  if (m_IsMirror)
     return "[" + GetClientFileName() + "] (Mirror) \"" + m_GameName + "\"";

  string Description = (
    "[" + GetClientFileName() + "] \"" + m_GameName + "\" - " + m_OwnerName + " - " +
    ToDecString(GetNumJoinedPlayersOrFake()) + "/" + ToDecString(m_GameLoading || m_GameLoaded ? m_ControllersWithMap : static_cast<uint8_t>(m_Slots.size()))
  );

  if (m_GameLoading || m_GameLoaded)
    Description += " : " + to_string((m_GameTicks / 1000) / 60) + "min";
  else
    Description += " : " + to_string((GetTime() - m_CreationTime) / 60) + "min";

  return Description;
}

string CGame::GetEndDescription() const
{
  if (m_IsMirror)
     return "[" + GetClientFileName() + "] (Mirror) \"" + m_GameName + "\"";

  string winnersFragment;
  if (m_CustomStats) {
    m_CustomStats->FlushQueue();
    vector<string> winners = m_CustomStats->GetWinners();
    if (winners.size() > 2) {
      winnersFragment = "Winners: [" + winners[0] + "], and others";
    } else if (winners.size() == 2) {
      winnersFragment = "Winners: [" + winners[0] + "] and [" + winners[1] + "]";
    } else if (winners.size() == 1) {
      winnersFragment = "Winner: [" + winners[0] + "]";
    }
  }

  string Description = (
    "[" + GetClientFileName() + "] \"" + m_GameName + "\". " + (winnersFragment.empty() ? ("Players: " + m_PlayedBy) : winnersFragment)
  );

  if (m_GameLoading || m_GameLoaded)
    Description += " : " + to_string((m_GameTicks / 1000) / 60) + "min";
  else
    Description += " : " + to_string((GetTime() - m_CreationTime) / 60) + "min";

  return Description;
}

string CGame::GetCategory() const
{
  if (m_GameLoading || m_GameLoaded)
    return "GAME";

  return "LOBBY";
}

string CGame::GetLogPrefix() const
{
  string MinString = to_string((m_GameTicks / 1000) / 60);
  string SecString = to_string((m_GameTicks / 1000) % 60);

  if (MinString.size() == 1)
    MinString.insert(0, "0");

  if (SecString.size() == 1)
    SecString.insert(0, "0");

  if (m_GameLoaded && m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    return "[" + GetCategory() + ": " + GetGameName() + " | Frame " + to_string(m_SyncCounter) + "] ";
  } else {
    return "[" + GetCategory() + ": " + GetGameName() + "] ";
  }
}

ImmutableUserList CGame::GetPlayers() const
{
  ImmutableUserList players;
  for (const auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && !user->GetIsObserver()) {
      // Check GetLeftMessageSent instead of GetDeleteMe for debugging purposes
      players.push_back(user);
    }
  }
  return players;
}

ImmutableUserList CGame::GetObservers() const
{
  ImmutableUserList observers;
  for (const auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && user->GetIsObserver()) {
      // Check GetLeftMessageSent instead of GetDeleteMe for debugging purposes
      observers.push_back(user);
    }
  }
  return observers;
}

ImmutableUserList CGame::GetUnreadyPlayers() const
{
  ImmutableUserList players;
  for (const auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && !user->GetIsObserver()) {
      if (!user->GetIsReady()) {
        players.push_back(user);
      }
    }
  }
  return players;
}

ImmutableUserList CGame::GetWaitingReconnectPlayers() const
{
  ImmutableUserList players;
  for (const auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && user->GetDisconnected() && user->GetGProxyAny()) {
      players.push_back(user);
    }
  }
  return players;
}

uint32_t CGame::SetFD(void* fd, void* send_fd, int32_t* nfds) const
{
  uint32_t NumFDs = 0;

  for (auto& user : m_Users) {
    if (user->GetDisconnected()) continue;
    user->GetSocket()->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    ++NumFDs;
  }

  return NumFDs;
}

void CGame::UpdateJoinable()
{
  const int64_t Time = GetTime(), Ticks = GetTicks();

  // refresh every 3 seconds

  if (m_LastRefreshTime + 3 <= Time) {
    // send a game refresh packet to each battle.net connection

    if (m_DisplayMode == GAME_PUBLIC && HasSlotsOpen()) {
      for (auto& realm : m_Aura->m_Realms) {
        if (!realm->GetLoggedIn()) {
          continue;
        }
        if (m_IsMirror && realm->GetIsMirror()) {
        // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
        // Do not display external games in those realms.
          continue;
        }
        if (realm->GetIsChatQueuedGameAnnouncement()) {
          // Wait til we have sent a chat message first.
          continue;
        }
        if (!GetIsSupportedGameVersion(realm->GetGameVersion())) {
          continue;
        }
        if (m_RealmsExcluded.find(realm->GetServer()) != m_RealmsExcluded.end()) {
          continue;
        }
        // Send STARTADVEX3
        AnnounceToRealm(realm);
      }
    }

    if (m_Aura->m_StartedGames.empty()) {
      // This is a lobby. Take the chance to update the detailed console title
      m_Aura->UpdateMetaData();
    }

    m_LastRefreshTime = Time;
  }

  if (m_IsMirror) {
    return;
  }

  // send more map data

  if (Ticks - m_LastDownloadCounterResetTicks >= 1000) {
    // hackhack: another timer hijack is in progress here
    // since the download counter is reset once per second it's a great place to update the slot info if necessary

    if (m_SlotInfoChanged & SLOTS_DOWNLOAD_PROGRESS_CHANGED) {
      SendAllSlotInfo();
      UpdateReadyCounters();
      m_SlotInfoChanged &= ~(SLOTS_DOWNLOAD_PROGRESS_CHANGED);
    }

    m_DownloadCounter               = 0;
    m_LastDownloadCounterResetTicks = Ticks;
  }

  if (Ticks - m_LastDownloadTicks >= 100) {
    uint32_t Downloaders = 0;
    uint32_t prevDownloadCounter = m_DownloadCounter;

    for (auto& user : m_Users) {
      if (user->GetDownloadStarted() && !user->GetDownloadFinished()) {
        ++Downloaders;

        if (m_Aura->m_Net.m_Config.m_MaxDownloaders > 0 && Downloaders > m_Aura->m_Net.m_Config.m_MaxDownloaders) {
          break;
        }

        // send up to 100 pieces of the map at once so that the download goes faster
        // if we wait for each MAPPART packet to be acknowledged by the client it'll take a long time to download
        // this is because we would have to wait the round trip time (the ping time) between sending every 1442 bytes of map data
        // doing it this way allows us to send at least 1.4 MB in each round trip interval which is much more reasonable
        // the theoretical throughput is [1.4 MB * 1000 / ping] in KB/sec so someone with 100 ping (round trip ping, not LC ping) could download at 1400 KB/sec
        // note: this creates a queue of map data which clogs up the connection when the client is on a slower connection (e.g. dialup)
        // in this case any changes to the lobby are delayed by the amount of time it takes to send the queued data (i.e. 140 KB, which could be 30 seconds or more)
        // for example, users joining and leaving, slot changes, chat messages would all appear to happen much later for the low bandwidth user
        // note: the throughput is also limited by the number of times this code is executed each second
        // e.g. if we send the maximum amount (1.4 MB) 10 times per second the theoretical throughput is 1400 KB/sec
        // therefore the maximum throughput is 14 MB/sec, and this value slowly diminishes as the user's ping increases
        // in addition to this, the throughput is limited by the configuration value bot_maxdownloadspeed
        // in summary: the actual throughput is MIN( 1.4 * 1000 / ping, 1400, bot_maxdownloadspeed ) in KB/sec assuming only one user is downloading the map

        const uint32_t MapSize = ByteArrayToUInt32(m_Map->GetMapSize(), false);

        if (user->GetLastMapPartSentOffsetEnd() == 0 && (
            user->GetLastMapPartSentOffsetEnd() < user->GetLastMapPartAcked() + 1442 * m_Aura->m_Net.m_Config.m_MaxParallelMapPackets &&
            user->GetLastMapPartSentOffsetEnd() < MapSize
          )
        ) {
          // overwrite the "started download ticks" since this is the first time we've sent any map data to the user
          // prior to this we've only determined if the user needs to download the map but it's possible we could have delayed sending any data due to download limits

          user->SetStartedDownloadingTicks(Ticks);
        }
        while (
          user->GetLastMapPartSentOffsetEnd() < user->GetLastMapPartAcked() + 1442 * m_Aura->m_Net.m_Config.m_MaxParallelMapPackets &&
          user->GetLastMapPartSentOffsetEnd() < MapSize
        ) {
          if (m_Aura->m_Net.m_Config.m_MaxUploadSpeed > 0 && m_DownloadCounter > m_Aura->m_Net.m_Config.m_MaxUploadSpeed * 1024) {
            // limit the download speed if we're sending too much data
            // the download counter is the # of map bytes downloaded in the last second (it's reset once per second)
            break;
          }

          uint32_t lastOffsetEnd = user->GetLastMapPartSentOffsetEnd();
          const FileChunkTransient cachedChunk = GetMapChunk(lastOffsetEnd);
          if (!cachedChunk.bytes) {
            user->AddKickReason(GameUser::KickReason::MAP_MISSING);
            if (!user->HasLeftReason()) {
              user->SetLeftReason("autokicked - they don't have the map, and it cannot be transferred (deleted)");
            }
            user->CloseConnection(false);
            break;
          }
          const vector<uint8_t> packet = GameProtocol::SEND_W3GS_MAPPART(GetHostUID(), user->GetUID(), lastOffsetEnd, cachedChunk);
          uint32_t chunkSendSize = static_cast<uint32_t>(packet.size() - 18);
          user->SetLastMapPartSentOffsetEnd(lastOffsetEnd + chunkSendSize);
          m_DownloadCounter += chunkSendSize;
          Send(user, packet);
        }
      }
    }

    m_LastDownloadTicks = Ticks;
  }
}

bool CGame::UpdateLobby()
{
  const int64_t Ticks = GetTicks();

  if (m_SlotInfoChanged & (SLOTS_ALIGNMENT_CHANGED)) {
    SendAllSlotInfo();
    UpdateReadyCounters();
    m_SlotInfoChanged &= ~(SLOTS_ALIGNMENT_CHANGED);
  }

  if (GetIsAutoStartDue()) {
    SendAllChat("Game automatically starting in. . .");
    StartCountDown(false, true);
  }

  if (!m_Users.empty()) {
    m_LastUserSeen = Ticks;
    if (HasOwnerInGame()) {
      m_LastOwnerSeen = Ticks;
    }
  }

  // countdown every m_LobbyCountDownInterval ms (default 500 ms)

  if (m_CountDownStarted && Ticks - m_LastCountDownTicks >= m_Config.m_LobbyCountDownInterval) {
    bool shouldStartLoading = false;
    if (m_CountDownCounter > 0) {
      // we use a countdown counter rather than a "finish countdown time" here because it might alternately round up or down the count
      // this sometimes resulted in a countdown of e.g. "6 5 3 2 1" during my testing which looks pretty dumb
      // doing it this way ensures it's always "5 4 3 2 1" but each interval might not be *exactly* the same length

      SendAllChat(to_string(m_CountDownCounter--) + ". . .");
    } else if (GetNumJoinedUsers() >= 1) { // allow observing AI vs AI matches
      shouldStartLoading = true;
    } else {
      // Some operations may remove fake users during countdown.
      // Ensure that the game doesn't start if there are neither real nor fake users.
      // (If a user leaves or joins, the countdown is stopped elsewhere.)
      LOG_APP_IF(LOG_LEVEL_DEBUG, "countdown stopped - lobby is empty.")
      StopCountDown();
    }

    m_LastCountDownTicks = Ticks;
    if (shouldStartLoading) {
      EventGameStartedLoading();
      return true;
    }
  }

  // release abandoned lobbies, so other users can take ownership
  CheckLobbyTimeouts();

  if (m_Exiting) {
    return true;
  }

  // last action of CGame::UpdateLobby
  // try to create the virtual host user, if there are slots available
  //
  // ensures that all pending users' leave messages have already been sent
  // either at CGame::EventUserDeleted or at CGame::EventRequestJoin (reserve system kicks)
  if (!m_GameLoading && GetSlotsOpen() > 0) {
    CreateVirtualHost();
  }

  return false;
}

void CGame::UpdateLoading()
{
  const int64_t Time = GetTime(), Ticks = GetTicks();

  bool finishedLoading = true;
  bool anyLoaded = false;
  for (auto& user : m_Users) {
    if (user->GetFinishedLoading()) {
      anyLoaded = true;
    } else if (!user->GetDisconnected()) {
      finishedLoading = false;
      break;
    }
  }

  if (finishedLoading) {
    if (anyLoaded) {
      if (!m_Config.m_LoadInGame && !m_LoadingVirtualBuffer.empty()) {
        // CGame::UpdateLoading: Fake users loaded
        if (m_LoadingVirtualBuffer.size() == 5 * m_FakeUsers.size()) {
          SendAll(m_LoadingVirtualBuffer);
        } else {
          // Cannot just send the whole m_LoadingVirtualBuffer, because, when load-in-game is disabled,
          // it will also contain load packets for real users who didn't actually load the game,
          // but these packets were already sent to real users
          vector<uint8_t> onlyFakeUsersLoaded = vector<uint8_t>(m_LoadingVirtualBuffer.begin(), m_LoadingVirtualBuffer.begin() + (5 * m_FakeUsers.size()));
          SendAll(onlyFakeUsersLoaded);
        }
      }

      m_LastActionSentTicks = Ticks;
      m_FinishedLoadingTicks = Ticks;
      m_GameLoading = false;
      m_GameLoaded = true;
      EventGameLoaded();
    } else {
      // Flush leaver queue to allow players and the game itself to be destroyed.
      SendAllActionsCallback();
    }
  } else {
    if (m_Config.m_LoadingTimeoutMode == GAME_LOADING_TIMEOUT_STRICT) {
      if (Ticks - m_StartedLoadingTicks > static_cast<int64_t>(m_Config.m_LoadingTimeout)) {
        StopLoadPending("was automatically dropped after " + to_string(m_Config.m_LoadingTimeout / 1000) + " seconds");
      }
    }

    // Warcraft III disconnects if it doesn't receive an action packet for more than ~65 seconds
    if (m_Config.m_LoadInGame && anyLoaded && Time - m_LastLagScreenResetTime >= 60 ) {
      ResetLagScreen();
    }
  }
}

void CGame::UpdateLoaded()
{
  const int64_t Time = GetTime(), Ticks = GetTicks();

  // check if anyone has started lagging
  // we consider a user to have started lagging if they're more than m_SyncLimit keepalives behind

  if (!m_Lagging) {
    string LaggingString;
    bool startedLagging = false;
    vector<uint32_t> framesBehind = GetPlayersFramesBehind();
    uint8_t i = static_cast<uint8_t>(m_Users.size());
    while (i--) {
      if (framesBehind[i] > GetSyncLimit() && !m_Users[i]->GetDisconnectedUnrecoverably()) {
        startedLagging = true;
        break;
      }
    }
    if (startedLagging) {
      uint8_t worstLaggerIndex = 0;
      uint8_t bestLaggerIndex = 0;
      uint32_t worstLaggerFrames = 0;
      uint32_t bestLaggerFrames = 0xFFFFFFFF;
      UserList laggingPlayers;
      i = static_cast<uint8_t>(m_Users.size());
      while (i--) {
        if (framesBehind[i] > GetSyncLimitSafe() && !m_Users[i]->GetDisconnectedUnrecoverably()) {
          m_Users[i]->SetLagging(true);
          m_Users[i]->SetStartedLaggingTicks(Ticks);
          m_Users[i]->ClearStalePings();
          laggingPlayers.push_back(m_Users[i]);
          if (framesBehind[i] > worstLaggerFrames) {
            worstLaggerIndex = i;
            worstLaggerFrames = framesBehind[i];
          }
          if (framesBehind[i] < bestLaggerFrames) {
            bestLaggerIndex = i;
            bestLaggerFrames = framesBehind[i];
          }
        }
      }
      if (laggingPlayers.size() == m_Users.size()) {
        // Avoid showing everyone as lagging
        m_Users[bestLaggerIndex]->SetLagging(false);
        m_Users[bestLaggerIndex]->SetStartedLaggingTicks(0);
        laggingPlayers.erase(laggingPlayers.begin() + (m_Users.size() - 1 - bestLaggerIndex));
      }

      if (!laggingPlayers.empty()) {
        // start the lag screen
        LOG_APP_IF(LOG_LEVEL_INFO, "global lagger update (+" + ToNameListSentence(laggingPlayers) + ")")
        SendAll(GameProtocol::SEND_W3GS_START_LAG(laggingPlayers));
        ResetDropVotes();

        m_Lagging = true;
        m_StartedLaggingTime = Time;
        m_LastLagScreenResetTime = Time;

        // print debug information
        double worstLaggerSeconds = static_cast<double>(worstLaggerFrames) * static_cast<double>(GetLatency()) / static_cast<double>(1000.);
        if (m_Aura->MatchLogLevel(LOG_LEVEL_INFO)) {
          LogApp("started lagging on " + ToNameListSentence(laggingPlayers, true) + ".");
          LogApp("worst lagger is [" + m_Users[worstLaggerIndex]->GetName() + "] (" + ToFormattedString(worstLaggerSeconds) + " seconds behind)");
        }
      }
    }
  } else if (!m_Users.empty()) { // m_Lagging == true (context: CGame::UpdateLoaded())
    pair<int64_t, int64_t> waitTicks = GetReconnectWaitTicks();
    UserList droppedUsers;
    for (auto& user : m_Users) {
      if (!user->GetLagging()) {
        continue;
      }
      bool timeExceeded = false;
      if (user->GetDisconnected() && user->GetGProxyExtended()) {
        timeExceeded = Ticks - user->GetStartedLaggingTicks() > waitTicks.second;
      } else if (user->GetDisconnected() && user->GetGProxyAny()) {
        timeExceeded = Ticks - user->GetStartedLaggingTicks() > waitTicks.first;
      } else {
        timeExceeded = Ticks - user->GetStartedLaggingTicks() > 60000;
      }
      if (timeExceeded) {
        if (user->GetDisconnected()) {
          StopLagger(user, "failed to reconnect within " + to_string((Ticks - user->GetStartedLaggingTicks()) / 1000) + " seconds");
        } else {
          StopLagger(user, "was automatically dropped after " + to_string((Ticks - user->GetStartedLaggingTicks()) / 1000) + " seconds");
        }
        droppedUsers.push_back(user);
      }
    }
    if (!droppedUsers.empty()) {
      for (const auto& user : droppedUsers) {
        if (TrySaveOnDisconnect(user, false)) {
          break;
        }
      }
      ResetDropVotes();
    }

    // Warcraft III disconnects if it doesn't receive an action packet for more than ~65 seconds
    if (Time - m_LastLagScreenResetTime >= 60) {
      ResetLagScreen();
    }

    // check if anyone has stopped lagging normally
    // we consider a user to have stopped lagging if they're less than m_SyncLimitSafe keepalives behind

    uint8_t playersLaggingCounter = 0;
    for (auto& user : m_Users) {
      if (!user->GetLagging()) {
        continue;
      }

      if (user->GetGProxyDisconnectNoticeSent()) {
        ++playersLaggingCounter;
        ReportRecoverableDisconnect(user);
        continue;
      }

      if (user->GetDisconnectedUnrecoverably()) {
        user->SetLagging(false);
        user->SetStartedLaggingTicks(0);
        LOG_APP_IF(LOG_LEVEL_INFO, "global lagger update (-" + user->GetName() + ")")
        SendAll(GameProtocol::SEND_W3GS_STOP_LAG(user));
        LOG_APP_IF(LOG_LEVEL_INFO, "lagging user disconnected [" + user->GetName() + "]")
      } else if (user->GetIsBehindFramesNormal(GetSyncLimitSafe())) {
        ++playersLaggingCounter;
      } else {
        LOG_APP_IF(LOG_LEVEL_INFO, "global lagger update (-" + user->GetName() + ")")
        SendAll(GameProtocol::SEND_W3GS_STOP_LAG(user));
        user->SetLagging(false);
        user->SetStartedLaggingTicks(0);
        LOG_APP_IF(LOG_LEVEL_INFO, "user no longer lagging [" + user->GetName() + "] (" + user->GetDelayText(true) + ")")
      }
    }

    if (playersLaggingCounter == 0) {
      m_Lagging = false;
      m_LastActionSentTicks = Ticks - GetLatency();
      m_LastActionLateBy = 0;
      m_PingReportedSinceLagTimes = 0;
      LOG_APP_IF(LOG_LEVEL_INFO, "stopped lagging after " + ToFormattedString(static_cast<double>(Time - m_StartedLaggingTime)) + " seconds")
    }
  }

  if (m_Lagging) {
    // reset m_LastActionSentTicks because we want the game to stop running while the lag screen is up
    m_LastActionSentTicks = Ticks;

    // keep track of the last lag screen time so we can avoid timing out users
    m_LastLagScreenTime = Time;

    // every 17 seconds, report most recent lag data
    if (Time - m_StartedLaggingTime >= m_PingReportedSinceLagTimes * 17) {
      ReportAllPings();
      ++m_PingReportedSinceLagTimes;
    }
    if (m_Config.m_SyncNormalize) {
      if (m_PingReportedSinceLagTimes == 2 && Ticks - m_FinishedLoadingTicks < 60000) {
        NormalizeSyncCounters();
      } else if (m_PingReportedSinceLagTimes == 3 && Ticks - m_FinishedLoadingTicks < 180000) {
        NormalizeSyncCounters();
      }
    }
  }

  switch (m_Config.m_PlayingTimeoutMode) {
    case GAME_PLAYING_TIMEOUT_NEVER:
      break;
    case GAME_PLAYING_TIMEOUT_DRY:
    case GAME_PLAYING_TIMEOUT_STRICT:
      if (Ticks - m_FinishedLoadingTicks > static_cast<int64_t>(m_Config.m_PlayingTimeout)) {
        if (m_Config.m_PlayingTimeoutMode == GAME_PLAYING_TIMEOUT_STRICT) {
          m_GameOverTolerance = 0;
          StartGameOverTimer();
        } else {
          Log("game timed out after " + to_string(m_Config.m_PlayingTimeout / 1000) + " seconds");
          m_Config.m_PlayingTimeoutMode = GAME_PLAYING_TIMEOUT_NEVER;
        }
      }
      break;
    default:
      // impossible path
      break;
  }

  // TODO: Implement game timeout warnings
  // m_PlayingTimeoutWarningShortCountDown    = CFG.GetUint8("hosting.expiry.playing.timeout.soon_warnings", 10);
  // m_PlayingTimeoutWarningShortInterval     = CFG.GetUint32("hosting.expiry.playing.timeout.soon_interval", 60);
  // m_PlayingTimeoutWarningLargeCountDown    = CFG.GetUint8("hosting.expiry.playing.timeout.eager_warnings", 3);
  // m_PlayingTimeoutWarningLargeInterval     = CFG.GetUint32("hosting.expiry.playing.timeout.eager_interval", 1200);

  // TODO: Implement game pause timeout (and also warnings)
  // On timeout:
  // Warnings are needed in order for other players to unpause if so they wish.
  /*
  // Must disconnect, because unpausing as m_PauseUser would desync them anyway
  m_PauseUser->SetLeftReason("pause time limit exceeded");
  m_PauseUser->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  m_PauseUser->DisableReconnect();
  m_PauseUser->CloseConnection();
  if (!user->GetIsEndingOrEnded()) {
    Resume(user, user->GetPingEqualizerFrame(), true);
    QueueLeftMessage(m_PauseUser);
  }
  */
}

bool CGame::Update(void* fd, void* send_fd)
{
  const int64_t Time = GetTime(), Ticks = GetTicks();

  // ping every 8 seconds
  // changed this to ping during game loading as well to hopefully fix some problems with people disconnecting during loading
  // changed this to ping during the game as well

  if (!m_LobbyLoading && (Time - m_LastPingTime >= 5)) {
    // we must send pings to users who are downloading the map because
    // Warcraft III disconnects from the lobby if it doesn't receive a ping every ~90 seconds
    // so if the user takes longer than 90 seconds to download the map they would be disconnected unless we keep sending pings

    vector<uint8_t> pingPacket = GameProtocol::SEND_W3GS_PING_FROM_HOST();
    for (auto& user : m_Users) {
      // Avoid ping-spamming GProxy-reconnected players
      if (!user->GetDisconnected()) {
        user->Send(pingPacket);
      }
    }

    // we also broadcast the game to the local network every 5 seconds so we hijack this timer for our nefarious purposes
    // however we only want to broadcast if the countdown hasn't started

    if (GetUDPEnabled() && GetIsStageAcceptingJoins()) {
      if (m_Aura->m_Net.m_UDPMainServerEnabled && m_Aura->m_Net.m_Config.m_UDPBroadcastStrictMode) {
        SendGameDiscoveryRefresh();
      } else {
        SendGameDiscoveryInfo();
      }
    }

    m_LastPingTime = Time;
  }

  // update users

  for (auto i = begin(m_Users); i != end(m_Users);) {
    if ((*i)->Update(fd, (*i)->GetGProxyAny() ? GAME_USER_TIMEOUT_RECONNECTABLE : GAME_USER_TIMEOUT_VANILLA)) {
      EventUserDeleted(*i, fd, send_fd);
      m_Aura->m_Net.OnUserKicked(*i);
      delete *i;
      i = m_Users.erase(i);
    } else {
      ++i;
    }
  }

  if (m_Remaking) {
    m_Remaking = false;
    if (m_Aura->GetNewGameIsInQuota()) {
      m_Remade = true;
    } else {
      // Cannot remake
      m_Exiting = true;
    }
    if (m_CustomStats) m_CustomStats->FlushQueue();
    return true;
  }

  if (m_LobbyLoading) {
    if (!m_Users.empty()) {
      return m_Exiting;
    }
    // This is a remake.
    // All users left the original game, and they can rejoin now.
    m_LobbyLoading = false;
    LOG_APP_IF(LOG_LEVEL_INFO, "finished loading after remake")
    CreateVirtualHost();
  }

  // keep track of the largest sync counter (the number of keepalive packets received by each user)
  // if anyone falls behind by more than m_SyncLimit keepalives we start the lag screen

  if (m_GameLoaded) {
    UpdateLoaded();
  }

  // send actions every m_Latency milliseconds
  // actions are at the heart of every Warcraft 3 game but luckily we don't need to know their contents to relay them
  // we queue user actions in EventUserAction then just resend them in batches to all users here

  if (m_GameLoaded && !m_Lagging && Ticks - m_LastActionSentTicks >= GetLatency() - m_LastActionLateBy)
    SendAllActions();

  UpdateLogs();

  // end the game if there aren't any users left
  if (m_Users.empty() && (m_GameLoading || m_GameLoaded || m_ExitingSoon)) {
    if (!m_Exiting) {
      if (m_CustomStats) m_CustomStats->FlushQueue();
      LOG_APP_IF(LOG_LEVEL_INFO, "is over (no users left)")
      m_Exiting = true;
    }
    return m_Exiting;
  }

  if (m_GameLoading) {
    UpdateLoading();
  }

  // expire the votekick
  if (!m_KickVotePlayer.empty() && Time - m_StartedKickVoteTime >= 60) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "votekick against user [" + m_KickVotePlayer + "] expired")
    SendAllChat("A votekick against user [" + m_KickVotePlayer + "] has expired");
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  // start the gameover timer if there's only a configured number of players left
  // do not count observers, but fake users are counted regardless
  uint8_t RemainingPlayers = GetNumJoinedPlayersOrFakeUsers() - m_JoinedVirtualHosts;
  if (RemainingPlayers != m_StartPlayers && !GetIsGameOverTrusted() && (m_GameLoading || m_GameLoaded)) {
    if (RemainingPlayers == 0) {
      LOG_APP_IF(LOG_LEVEL_INFO, "gameover timer started: 0 p | " + ToDecString(GetNumJoinedObservers()) + " obs | 0 fake")
      StartGameOverTimer();
    } else if (RemainingPlayers <= m_Config.m_NumPlayersToStartGameOver) {
      LOG_APP_IF(LOG_LEVEL_INFO, "gameover timer started: " + ToDecString(GetNumJoinedPlayers()) + " p | " + ToDecString(GetNumComputers()) + " comp | " + ToDecString(GetNumJoinedObservers()) + " obs | " + to_string(m_FakeUsers.size() - m_JoinedVirtualHosts) + " fake | " + ToDecString(m_JoinedVirtualHosts) + " vhost")
      StartGameOverTimer();
    }
  }

  // finish the gameover timer
  if (GetIsGameOver() && m_GameOverTime.value() + m_GameOverTolerance.value_or(60) < Time) {
    // Disconnect the user socket, destroy it, but do not send W3GS_PLAYERLEAVE
    // Sending it would force them to actually quit the game, and go to the scorescreen.
    if (m_GameLoading || m_GameLoaded) {
      SendEveryoneElseLeftAndDisconnect("was disconnected (gameover timer finished)");
    } else {
      StopPlayers("was disconnected (gameover timer finished)");
    }
  }

  if (m_CustomStats && Time - m_LastCustomStatsUpdateTime >= 30) {
    if (!m_CustomStats->UpdateQueue() && !GetIsGameOver()) {
      Log("gameover timer started (w3mmd reported game over)");
      StartGameOverTimer(true);
    }
    m_LastCustomStatsUpdateTime = Time;
  }

  if (GetIsStageAcceptingJoins()) {
    // Also updates mirror games.
    UpdateJoinable();
  }

  if (GetIsLobbyStrict()) {
    if (UpdateLobby()) {
      // EventGameStartedLoading or m_Exiting
      return true;
    }
  }

  return m_Exiting;
}

void CGame::UpdatePost(void* send_fd) const
{
  // we need to manually call DoSend on each user now because GameUser::CGameUser::Update doesn't do it
  // this is in case user 2 generates a packet for user 1 during the update but it doesn't get sent because user 1 already finished updating
  // in reality since we're queueing actions it might not make a big difference but oh well

  for (const auto& user : m_Users) {
    if (user->GetDisconnected()) continue;
    user->GetSocket()->DoSend(static_cast<fd_set*>(send_fd));
  }
}

void CGame::CheckLobbyTimeouts()
{
  if (HasOwnerSet()) {
    switch (m_Config.m_LobbyOwnerTimeoutMode) {
      case LOBBY_OWNER_TIMEOUT_NEVER:
        break;
      case LOBBY_OWNER_TIMEOUT_ABSENT:
        if (m_LastOwnerSeen + static_cast<int64_t>(m_Config.m_LobbyOwnerTimeout) < GetTicks()) {
          ReleaseOwner();
        }
        break;
      case LOBBY_OWNER_TIMEOUT_STRICT:
        if (m_LastOwnerAssigned + static_cast<int64_t>(m_Config.m_LobbyOwnerTimeout) < GetTicks()) {
          ReleaseOwner();
        }
        break;
    }
  }

  if (!m_Aura->m_Net.m_HealthCheckInProgress && (!m_IsMirror || m_Config.m_LobbyTimeoutMode == LOBBY_TIMEOUT_STRICT)) {
    bool timedOut = false;
    switch (m_Config.m_LobbyTimeoutMode) {
    case LOBBY_TIMEOUT_NEVER:
      break;
    case LOBBY_TIMEOUT_EMPTY:
      timedOut = m_LastUserSeen + static_cast<int64_t>(m_Config.m_LobbyTimeout) < GetTicks();
      break;
    case LOBBY_TIMEOUT_OWNERLESS:
      timedOut = m_LastOwnerSeen + static_cast<int64_t>(m_Config.m_LobbyTimeout) < GetTicks();
      break;
    case LOBBY_TIMEOUT_STRICT:
      timedOut = m_CreationTime + (static_cast<int64_t>(m_Config.m_LobbyTimeout) / 1000) < GetTime();
      break;
    }
    if (timedOut) {
      Log("is over (lobby time limit hit)");
      m_Exiting = true;
    }
  }
}

void CGame::RunActionsScheduler(const uint8_t maxNewEqualizerOffset, const uint8_t maxOldEqualizerOffset)
{
  const int64_t Ticks = GetTicks();
  if (m_LastActionSentTicks != 0) {
    const int64_t ActualSendInterval = Ticks - m_LastActionSentTicks;
    const int64_t ExpectedSendInterval = GetLatency() - m_LastActionLateBy;
    int64_t ThisActionLateBy = ActualSendInterval - ExpectedSendInterval;

    if (ThisActionLateBy > m_Config.m_PerfThreshold && !GetIsSinglePlayerMode()) {
      // something is going terribly wrong - Aura is probably starved of resources
      // print a message because even though this will take more resources it should provide some information to the administrator for future reference
      // other solutions - dynamically modify the latency, request higher priority, terminate other games, ???
      LOG_APP_IF(LOG_LEVEL_WARNING, "warning - action should be sent after " + to_string(ExpectedSendInterval) + "ms, but was sent after " + to_string(ActualSendInterval) + "ms [latency is " + to_string(GetLatency()) + "ms]")
    }

    if (ThisActionLateBy > GetLatency()) {
      ThisActionLateBy = GetLatency();
    }

    m_LastActionLateBy = ThisActionLateBy;
  }
  m_LastActionSentTicks = Ticks;

  if (maxNewEqualizerOffset < maxOldEqualizerOffset) {
    // No longer are that many frames needed.
    vector<QueuedActionsFrameNode*> mergeableNodes = GetFrameNodesInRangeInclusive(maxNewEqualizerOffset, maxOldEqualizerOffset);
    MergeFrameNodes(mergeableNodes);
  }

  m_CurrentActionsFrame = m_CurrentActionsFrame->next;
  for (auto& user : m_Users) {
    user->AdvanceActiveGameFrame();
  }
}

void CGame::LogApp(const string& logText) const
{
  Print(GetLogPrefix() + logText);
}

void CGame::Log(const string& logText)
{
  if (m_GameLoaded) {
    Log(logText, m_GameTicks);
  } else {
    Print(GetLogPrefix() + logText);
  }
}

void CGame::Log(const string& logText, int64_t gameTicks)
{
  m_PendingLogs.push(new CGameLogRecord(gameTicks, logText));
}

void CGame::UpdateLogs()
{
  int64_t ticks = m_GameTicks;
  while (!m_PendingLogs.empty()) {
    CGameLogRecord* record = m_PendingLogs.front();
    if (ticks + static_cast<int64_t>(m_Config.m_LogDelay) < record->GetTicks()) {
      break;
    }
    Print(GetLogPrefix() + record->ToString());
    delete record;
    m_PendingLogs.pop();
  }
}

void CGame::FlushLogs()
{
  while (!m_PendingLogs.empty()) {
    CGameLogRecord* record = m_PendingLogs.front();
    Print(GetLogPrefix() + record->ToString());
    delete record;
    m_PendingLogs.pop();
  }
}

void CGame::LogSlots()
{
  uint8_t i = 0;
  while (i < static_cast<uint8_t>(m_Slots.size())) {
    LogApp("slot_" + ToDecString(i) + " = <" + ByteArrayToHexString(m_Slots[i].GetProtocolArray()) + ">");
    ++i;
  }
}

void CGame::Send(CConnection* user, const std::vector<uint8_t>& data) const
{
  if (user)
    user->Send(data);
}

void CGame::Send(uint8_t UID, const std::vector<uint8_t>& data) const
{
  GameUser::CGameUser* user = GetUserFromUID(UID);
  Send(user, data);
}

void CGame::Send(const std::vector<uint8_t>& UIDs, const std::vector<uint8_t>& data) const
{
  for (auto& UID : UIDs) {
    Send(UID, data);
  }
}

void CGame::SendAll(const std::vector<uint8_t>& data) const
{
  for (auto& user : m_Users) {
    user->Send(data);
  }
}

void CGame::SendAsChat(CConnection* user, const std::vector<uint8_t>& data) const
{
  if (user->GetType() == INCON_TYPE_PLAYER && static_cast<const GameUser::CGameUser*>(user)->GetIsInLoadingScreen()) {
    return;
  }
  // TODO: m_BufferingEnabled & BUFFERING_ENABLED_PLAYING
  user->Send(data);
}

bool CGame::SendAllAsChat(const std::vector<uint8_t>& data) const
{
  bool success = false;
  for (auto& user : m_Users) {
    if (user->GetIsInLoadingScreen()) {
      continue;
    }
    // TODO: m_BufferingEnabled & BUFFERING_ENABLED_PLAYING
    user->Send(data);
    success = true;
  }
  return success;
}

void CGame::SendChat(uint8_t fromUID, GameUser::CGameUser* user, const string& message, const uint8_t logLevel) const
{
  // send a private message to one user - it'll be marked [Private] in Warcraft 3

  if (message.empty() || !user || user->GetIsInLoadingScreen()) {
    return;
  }

#ifdef DEBUG
  if (m_Aura->MatchLogLevel(logLevel)) {
    const GameUser::CGameUser* fromUser = GetUserFromUID(fromUID);
    if (fromUser) {
      LogApp("sent as [" + fromUser->GetName() + "] -> [" + user->GetName() + " (UID:" + ToDecString(user->GetUID()) + ")] <<" + message + ">>");
    } else if (fromUID == m_VirtualHostUID) {
      LogApp("sent as Virtual Host -> [" + user->GetName() + " (UID:" + ToDecString(user->GetUID()) + ")] <<" + message + ">>");
    } else {
      LogApp("sent as [UID:" + ToDecString(fromUID) + "] -> [" + user->GetName() + " (UID:" + ToDecString(user->GetUID()) + ")] <<" + message + ">>");
    }
  }
#else
  if (m_Aura->MatchLogLevel(logLevel)) {
    LogApp("sent to [" + user->GetName() + "] <<" + message + ">>");
  }
#endif

  if (!m_GameLoading && !m_GameLoaded) {
    if (message.size() > 254)
      SendAsChat(user, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, CreateByteArray(user->GetUID()), 16, std::vector<uint8_t>(), message.substr(0, 254)));
    else
      SendAsChat(user, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, CreateByteArray(user->GetUID()), 16, std::vector<uint8_t>(), message));
  } else {
    uint8_t extraFlags[] = {3, 0, 0, 0};

    // based on my limited testing it seems that the extra flags' first byte contains 3 plus the recipient's colour to denote a private message

    uint8_t SID = GetSIDFromUID(user->GetUID());

    if (SID < m_Slots.size())
      extraFlags[0] = 3 + m_Slots[SID].GetColor();

    if (message.size() > 127)
      SendAsChat(user, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, CreateByteArray(user->GetUID()), 32, CreateByteArray(extraFlags, 4), message.substr(0, 127)));
    else
      SendAsChat(user, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, CreateByteArray(user->GetUID()), 32, CreateByteArray(extraFlags, 4), message));
  }
}

void CGame::SendChat(uint8_t fromUID, uint8_t toUID, const string& message, const uint8_t logLevel) const
{
  SendChat(fromUID, GetUserFromUID(toUID), message, logLevel);
}

void CGame::SendChat(GameUser::CGameUser* user, const string& message, const uint8_t logLevel) const
{
  SendChat(GetHostUID(), user, message, logLevel);
}

void CGame::SendChat(uint8_t toUID, const string& message, const uint8_t logLevel) const
{
  SendChat(GetHostUID(), toUID, message, logLevel);
}

bool CGame::SendAllChat(uint8_t fromUID, const string& message) const
{
  if (m_GameLoading && !m_Config.m_LoadInGame)
    return false;

  if (message.empty())
    return false;

  vector<uint8_t> toUIDs = GetChatUIDs();
  if (toUIDs.empty()) {
    return false;
  }

  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    const GameUser::CGameUser* fromUser = GetUserFromUID(fromUID);
    if (fromUser) {
      LogApp("sent as [" + fromUser->GetName() + "] <<" + message + ">>");
    } else if (fromUID == m_VirtualHostUID) {
      LogApp("sent as Virtual Host <<" + message + ">>");
    } else {
      LogApp("sent as [UID:" + ToDecString(fromUID) + "] <<" + message + ">>");
    }
  } else {
    LOG_APP_IF(LOG_LEVEL_INFO, "sent <<" + message + ">>")
  }

  // send a public message to all users - it'll be marked [All] in Warcraft 3

  uint8_t maxSize = !m_GameLoading && !m_GameLoaded ? 254 : 127;
  bool success = false;
  if (message.size() < maxSize) {
    if (!m_GameLoading && !m_GameLoaded) {
        success = SendAllAsChat(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, toUIDs, 16, std::vector<uint8_t>(), message));
    } else {
        success = SendAllAsChat(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, toUIDs, 32, CreateByteArray(static_cast<uint32_t>(0), false), message));
    }
  } else {
    bool success = false;
    string leftMessage = message;
    while (leftMessage.size() > maxSize) {
      if (!m_GameLoading && !m_GameLoaded) {
        success = SendAllAsChat(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, toUIDs, 16, std::vector<uint8_t>(), leftMessage.substr(0, maxSize))) || success;
      } else {
        success = SendAllAsChat(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, toUIDs, 32, CreateByteArray(static_cast<uint32_t>(0), false), leftMessage.substr(0, maxSize))) || success;
      }
      leftMessage = leftMessage.substr(maxSize);
    }

    if (!leftMessage.empty()) {
      if (!m_GameLoading && !m_GameLoaded) {
        success = SendAllAsChat(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, toUIDs, 16, std::vector<uint8_t>(), leftMessage)) || success;
      } else {
        success = SendAllAsChat(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(fromUID, toUIDs, 32, CreateByteArray(static_cast<uint32_t>(0), false), leftMessage)) || success;
      }
    }
  }
  return success;
}

bool CGame::SendAllChat(const string& message) const
{
  return SendAllChat(GetHostUID(), message);
}

void CGame::UpdateReadyCounters()
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> readyControllersByTeam(numTeams, 0);
  m_ControllersWithMap = 0;
  m_ControllersBalanced = true;
  m_ControllersReadyCount = 0;
  m_ControllersNotReadyCount = 0;
  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() != SLOTSTATUS_OCCUPIED || m_Slots[i].GetTeam() == m_Map->GetVersionMaxSlots()) {
      continue;
    }
    GameUser::CGameUser* Player = GetUserFromSID(i);
    if (!Player) {
      ++m_ControllersWithMap;
      ++m_ControllersReadyCount;
      ++readyControllersByTeam[m_Slots[i].GetTeam()];
    } else if (Player->GetMapReady()) {
      ++m_ControllersWithMap;
      if (Player->UpdateReady()) {
        ++m_ControllersReadyCount;
        ++readyControllersByTeam[m_Slots[i].GetTeam()];
      } else {
        ++m_ControllersNotReadyCount;
      }
    } else {
      ++m_ControllersNotReadyCount;
    }
  }
  uint8_t refCount = 0;
  uint8_t i = static_cast<uint8_t>(numTeams);
  while (i--) {
    // allow empty teams
    if (readyControllersByTeam[i] == 0) continue;
    if (refCount == 0) {
      refCount = readyControllersByTeam[i];
    } else if (readyControllersByTeam[i] != refCount) {
      m_ControllersBalanced = false;
      break;
    }
  }
}

void CGame::SendAllSlotInfo()
{
  if (m_GameLoading || m_GameLoaded)
    return;

  if (!m_Users.empty()) {
    SendAll(GameProtocol::SEND_W3GS_SLOTINFO(m_Slots, m_RandomSeed, GetLayout(), m_Map->GetMapNumControllers()));
  }
  m_SlotInfoChanged = 0;
}

uint8_t CGame::GetNumEnabledTeamSlots(const uint8_t team) const
{
  // Only for Custom Forces
  uint8_t counter = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (slot.GetTeam() == team) {
      ++counter;
    }
  }
  return counter;
}

vector<uint8_t> CGame::GetNumFixedComputersByTeam() const
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> fixedComputers(numTeams, 0);
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Map->GetVersionMaxSlots()) continue;
    if (!slot.GetIsSelectable()) {
      ++fixedComputers[slot.GetTeam()];
    }
  }
  return fixedComputers;
}

vector<uint8_t> CGame::GetPotentialTeamSizes() const
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes(numTeams, 0);
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Map->GetVersionMaxSlots()) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    ++teamSizes[slot.GetTeam()];
  }
  return teamSizes;
}


pair<uint8_t, uint8_t> CGame::GetLargestPotentialTeam() const
{
  // Only for Custom Forces
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  pair<uint8_t, uint8_t> largestTeam = make_pair(m_Map->GetVersionMaxSlots(), 0u);
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (teamSizes[team] > largestTeam.second) {
      largestTeam = make_pair(team, teamSizes[team]);
    }
  }
  return largestTeam;
}

pair<uint8_t, uint8_t> CGame::GetSmallestPotentialTeam(const uint8_t minSize, const uint8_t exceptTeam) const
{
  // Only for Custom Forces
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  pair<uint8_t, uint8_t> smallestTeam = make_pair(m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots());
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (team == exceptTeam || teamSizes[team] < minSize) continue;
    if (teamSizes[team] < smallestTeam.second) {
      smallestTeam = make_pair(team, teamSizes[team]);
    }
  }
  return smallestTeam;
}

vector<uint8_t> CGame::GetActiveTeamSizes() const
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes(numTeams, 0);
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Map->GetVersionMaxSlots()) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      ++teamSizes[slot.GetTeam()];
    }
  }
  return teamSizes;
}

uint8_t CGame::GetSelectableTeamSlotFront(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t endSID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  for (uint8_t i = 0; i < endSID; ++i) {
    const CGameSlot& slot = m_Slots[i];
    if (slot.GetTeam() != team) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot.GetIsSelectable()) continue;
    if (slot.GetSlotStatus() != SLOTSTATUS_OPEN && i < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      forceResult = i;
      continue;
    }
    return i;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

uint8_t CGame::GetSelectableTeamSlotBack(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t SID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  while (SID--) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || slot->GetTeam() != team) continue;
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot->GetIsSelectable()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OPEN && SID < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      if (forceResult == 0xFF) forceResult = SID;
      continue;
    }
    return SID;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

uint8_t CGame::GetSelectableTeamSlotBackExceptHumanLike(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t SID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  while (SID--) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || slot->GetTeam() != team) continue;
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot->GetIsSelectable()) continue;
    if (slot->GetIsPlayerOrFake()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OPEN && SID < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      if (forceResult == 0xFF) forceResult = SID;
      continue;
    }
    return SID;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

uint8_t CGame::GetSelectableTeamSlotBackExceptComputer(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t SID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  while (SID--) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || slot->GetTeam() != team) continue;
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot->GetIsSelectable()) continue;
    if (slot->GetIsComputer()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OPEN && SID < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      if (forceResult == 0xFF) forceResult = SID;
      continue;
    }
    return SID;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

bool CGame::FindHumanVsAITeams(const uint8_t humanCount, const uint8_t computerCount, pair<uint8_t, uint8_t>& teams) const
{
  if (!GetIsCustomForces()) {
    teams.first = 0;
    teams.second = 1;
    return true;
  } else if (!(m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)) {
    // MAPOPT_CUSTOMFORCES
    pair<uint8_t, uint8_t> largestTeam = GetLargestPotentialTeam();
    pair<uint8_t, uint8_t> smallestTeam = GetSmallestPotentialTeam(humanCount < computerCount ? humanCount : computerCount, largestTeam.first);
    if (largestTeam.second == 0 || smallestTeam.second == m_Map->GetVersionMaxSlots()) {
      return false;
    }
    pair<uint8_t, uint8_t>& computerTeam = (computerCount > humanCount ? largestTeam : smallestTeam);
    pair<uint8_t, uint8_t>& humanTeam = (computerCount > humanCount ? smallestTeam : largestTeam);
    if (humanTeam.second < humanCount || computerTeam.second < computerCount) {
      return false;
    }
    teams.first = humanTeam.first;
    teams.second = computerTeam.first;
    return true;
  }

  // Fixed Player Settings

  vector<uint8_t> lockedTeams = GetNumFixedComputersByTeam();
  uint8_t fixedTeamsCounter = 0;
  uint8_t forcedComputerTeam = 0xFF;
  for (uint8_t team = 0; team < lockedTeams.size(); ++team) {
    if (lockedTeams[team] == 0) continue;
    if (++fixedTeamsCounter >= 2) {
      // Bail-out if there are fixed computers in different teams.
      // This is the case in DotA/AoS maps.
      return false;
    }
    forcedComputerTeam = team;
  }
  if (forcedComputerTeam != 0xFF) {
    if (GetNumEnabledTeamSlots(forcedComputerTeam) < computerCount) {
      return false;
    }
  }

  {
    const uint8_t numTeams = m_Map->GetMapNumTeams();
    vector<uint8_t> teamSizes = GetPotentialTeamSizes();
    pair<uint8_t, uint8_t> largestTeam = make_pair(m_Map->GetVersionMaxSlots(), 0u);
    pair<uint8_t, uint8_t> smallestTeam = make_pair(m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots());
    for (uint8_t team = 0; team < numTeams; ++team) {
      if (team == forcedComputerTeam) continue;
      if (teamSizes[team] > largestTeam.second) {
        largestTeam = make_pair(team, teamSizes[team]);
      }
      if (teamSizes[team] < smallestTeam.second) {
        smallestTeam = make_pair(team, teamSizes[team]);
      }
    }
    if (forcedComputerTeam != 0xFF) {
      if (largestTeam.second < humanCount) {
        return false;
      }
      teams.first = largestTeam.first;
      teams.second = forcedComputerTeam;
    } else {
      // Just like MAPOPT_CUSTOMFORCES
      pair<uint8_t, uint8_t>& computerTeam = (computerCount > humanCount ? largestTeam : smallestTeam);
      pair<uint8_t, uint8_t>& humanTeam = (computerCount > humanCount ? smallestTeam : largestTeam);
      if (humanTeam.second < humanCount || computerTeam.second < computerCount) {
        return false;
      }
      teams.first = humanTeam.first;
      teams.second = humanTeam.second;
    }
    return true;
  }
}

void CGame::ResetLayout(const bool quiet)
{
  if (m_CustomLayout == CUSTOM_LAYOUT_NONE) {
    return;
  }
  m_CustomLayout = CUSTOM_LAYOUT_NONE;
  if (!quiet) {
    SendAllChat("Team restrictions automatically removed.");
  }
}

void CGame::ResetLayoutIfNotMatching()
{
  switch (m_CustomLayout) {
    case CUSTOM_LAYOUT_NONE:
      break;
    case CUSTOM_LAYOUT_ONE_VS_ALL:
    case CUSTOM_LAYOUT_HUMANS_VS_AI: {
      if (
        (GetNumTeamControllersOrOpen(m_CustomLayoutData.first) == 0) ||
        (GetNumTeamControllersOrOpen(m_CustomLayoutData.second) == 0)
      ) {
        ResetLayout(false);
        break;
      }
      bool isNotMatching = false;
      if (m_CustomLayout == CUSTOM_LAYOUT_HUMANS_VS_AI) {
        for (const auto& slot : m_Slots) {
          if (slot.GetSlotStatus() != SLOTSTATUS_CLOSED) continue;
          if (slot.GetIsComputer()) {
            if (slot.GetTeam() != m_CustomLayoutData.second) {
              isNotMatching = true;
              break;
            }
          } else {
            // Open, human, or fake user
            if (slot.GetTeam() != m_CustomLayoutData.first) {
              isNotMatching = true;
              break;
            }
          }
        }
      } else {
      }
      if (isNotMatching) {
        ResetLayout(false);
      }
      break;
    }
    case CUSTOM_LAYOUT_FFA: {
      if (GetHasAnyActiveTeam()) {
        ResetLayout(false);
      }
      break;
    }
    case CUSTOM_LAYOUT_DRAFT:
    case CUSTOM_LAYOUT_COMPACT:
    case CUSTOM_LAYOUT_ISOPLAYERS:
    default:
      break;
  }
}

bool CGame::SetLayoutCompact()
{
  m_CustomLayout = CUSTOM_LAYOUT_COMPACT;

  if (GetIsCustomForces()) {
    // Unsupported, and not very useful anyway.
    // Typical maps with fixed user settings are NvN, and compacting will just not be useful.
    return false;
  }

  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetActiveTeamSizes();
  pair<uint8_t, uint8_t> largestTeam = make_pair(m_Map->GetVersionMaxSlots(), 0u);
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (largestTeam.second < teamSizes[team]) {
      largestTeam = make_pair(team, teamSizes[team]);
    }
  }
  if (largestTeam.second <= 1) {
    return false;
  }

  const uint8_t controllerCount = GetNumControllers();
  if (controllerCount < 2) {
    return false;
  }
  //const uint8_t extraPlayers = controllerCount % largestTeam.second;
  const uint8_t expectedFullTeams = controllerCount / largestTeam.second;
  if (expectedFullTeams < 2) {
    // Compacting is used for NvNvN...
    return false;
  }
  //const uint8_t expectedMaxTeam = expectedFullTeams - (extraPlayers == 0);
  vector<uint8_t> premadeMappings(numTeams, m_Map->GetVersionMaxSlots());
  bitset<MAX_SLOTS_MODERN> fullTeams;
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (teamSizes[team] == largestTeam.second) {
      if (!fullTeams.test(team)) {
        premadeMappings[team] = static_cast<uint8_t>(fullTeams.count());
        fullTeams.set(team);
      }
    }
  }

  const uint8_t autoTeamOffset = static_cast<uint8_t>(fullTeams.count());

  for (auto& slot : m_Slots) {
    uint8_t team = slot.GetTeam();
    if (fullTeams.test(team)) {
      slot.SetTeam(premadeMappings[team]);
    } else {
      slot.SetTeam(autoTeamOffset);
    }
  }

  uint8_t i = numTeams;
  while (i--) {
    if (i < autoTeamOffset) {
      teamSizes[i] = largestTeam.second;
    } else if (i == autoTeamOffset) {
      teamSizes[i] = controllerCount - (largestTeam.second * autoTeamOffset);
    } else {
      teamSizes[i] = 0;
    }
  }

  uint8_t fillingTeamNum = autoTeamOffset;
  for (auto& slot : m_Slots) {
    uint8_t team = slot.GetTeam();
    if (team < autoTeamOffset) continue;
    if (teamSizes[team] > largestTeam.second) {
      if (teamSizes[fillingTeamNum] >= largestTeam.second) {
        ++fillingTeamNum;
      }
      slot.SetTeam(fillingTeamNum);
      --teamSizes[team];
      ++teamSizes[fillingTeamNum];
    }
  }

  return true;
}

bool CGame::SetLayoutTwoTeams()
{
  m_CustomLayout = CUSTOM_LAYOUT_ISOPLAYERS;

  // TODO(IceSandslash): SetLayoutTwoTeams
  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    if (m_Map->GetMapNumTeams() != 2) {
      return false;
    }
  }
  //m_CustomLayout = CUSTOM_LAYOUT_ISOPLAYERS;
  return false;
}

bool CGame::SetLayoutHumansVsAI(const uint8_t humanTeam, const uint8_t computerTeam)
{
  m_CustomLayout = CUSTOM_LAYOUT_HUMANS_VS_AI;
  const bool isSwap = GetIsCustomForces();
  if (isSwap) {
    uint8_t SID = static_cast<uint8_t>(m_Slots.size()) - 1;
    uint8_t endHumanSID = SID;
    uint8_t endComputerSID = SID;
    while (SID != 0xFF) {
      CGameSlot* slot = GetSlot(SID);
      if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        --SID;
        continue;
      }
      const bool isComputer = slot->GetIsComputer();
      const uint8_t currentTeam = slot->GetTeam();
      const uint8_t targetTeam = isComputer ? computerTeam : humanTeam;
      if (currentTeam == targetTeam) {
        --SID;
        continue;
      }
      uint8_t& selfEndSID = isComputer ? endComputerSID : endHumanSID;
      uint8_t& otherEndSID = isComputer ? endHumanSID : endComputerSID;
      uint8_t swapSID = 0xFF;
      if (isComputer) {
        swapSID = GetSelectableTeamSlotBackExceptComputer(targetTeam, SID, selfEndSID, true);
      } else {
        swapSID = GetSelectableTeamSlotBackExceptHumanLike(targetTeam, SID, selfEndSID, true);
      }
      if (swapSID == 0xFF) {
        return false;
      }
      const bool isTwoWays = InspectSlot(swapSID)->GetSlotStatus() == SLOTSTATUS_OCCUPIED;
      if (!SwapSlots(SID, swapSID)) {
        Print(ByteArrayToDecString(InspectSlot(SID)->GetByteArray()));
        Print(ByteArrayToDecString(InspectSlot(swapSID)->GetByteArray()));
      } else {
        // slot still points to the same SID
        selfEndSID = swapSID;
        if (isTwoWays && SID > otherEndSID) {
          otherEndSID = SID;
        }
      }
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (!isTwoWays) --SID;
    }
    CloseAllTeamSlots(computerTeam);
  } else {
    uint8_t remainingSlots = m_Map->GetMapNumControllers() - GetNumControllers();
    if (remainingSlots > 0) {
      for (auto& slot : m_Slots) {
        if (slot.GetSlotStatus() != SLOTSTATUS_OCCUPIED) continue;
        uint8_t targetTeam = slot.GetIsComputer() ? computerTeam : humanTeam;
        uint8_t wasTeam = slot.GetTeam();
        if (wasTeam != targetTeam) {
          slot.SetTeam(targetTeam);
          m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
          if (wasTeam == m_Map->GetVersionMaxSlots()) {
            if (--remainingSlots == 0) break;
          }
        }
      }
    }
  }
  m_CustomLayoutData = make_pair(humanTeam, computerTeam);
  return true;
}

bool CGame::SetLayoutFFA()
{
  m_CustomLayout = CUSTOM_LAYOUT_FFA;

  uint8_t nextTeam = GetNumControllers(); // Only arrange non-observers
  const bool isSwap = GetIsCustomForces();
  if (isSwap && nextTeam > m_Map->GetMapNumTeams()) {
    return false;
  }

  vector<uint8_t> lockedTeams = GetNumFixedComputersByTeam();
  for (const auto& count : lockedTeams) {
    if (count > 1) {
      return false;
    }
  }

  if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
    return true; // every team got 1 fixed computer slot
  }
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  bitset<MAX_SLOTS_MODERN> occupiedTeams;
  while (SID--) {
    CGameSlot* slot = GetSlot(SID);
    if (slot->GetTeam() == m_Map->GetVersionMaxSlots()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) continue;
    if (slot->GetTeam() == nextTeam) {
      // Slot already has the right team. Skip both team and slot.
      occupiedTeams.set(nextTeam);
      if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
        break;
      }
      continue;
    }
    if (isSwap) {
      uint8_t swapSID = GetSelectableTeamSlotBack(nextTeam, SID, static_cast<uint8_t>(m_Slots.size()), true);
      if (swapSID == 0xFF) {
        return false;
      }
      if (!SwapSlots(SID, swapSID)) {
        return false;
      }
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
        break;
      }
      occupiedTeams.set(nextTeam);
    } else {
      slot->SetTeam(nextTeam);
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
        break;
      }
      occupiedTeams.set(nextTeam);
    }
  }
  if (isSwap) {
    CloseAllTeamSlots(occupiedTeams);
  }
  return true;
}

uint8_t CGame::GetOneVsAllTeamAll() const
{
  if (!GetIsCustomForces()) {
    return 1;
  }

  const uint8_t mapNumTeams = m_Map->GetMapNumTeams();
  const uint8_t expectedTeamSize = GetNumPotentialControllers() - 1;
  vector<uint8_t> lockedTeams = GetNumFixedComputersByTeam();

  // Make sure GetOneVsAllTeamAll() yields the team with the fixed computer slots.
  // Fixed computer slots in different forces are not allowed in OneVsAll mode.
  uint8_t resultTeam = 0xFF;
  uint8_t fixedTeamsCounter = 0;
  for (uint8_t team = 0; team < lockedTeams.size(); ++team) {
    if (lockedTeams[team] == 0) continue;
    if (++fixedTeamsCounter >= 2) {
      return 0xFF;
    }
    resultTeam = team;
  }

  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  if (resultTeam == 0xFF) {
    pair<uint8_t, uint8_t> largestTeam = make_pair(m_Map->GetVersionMaxSlots(), 0u);
    for (uint8_t team = 0; team < mapNumTeams; ++team) {
      if (teamSizes[team] > largestTeam.second) {
        largestTeam = make_pair(team, teamSizes[team]);
      }
    }
    resultTeam = largestTeam.first;
  }
  if (expectedTeamSize > teamSizes[resultTeam]) {
    return 0xFF;
  } else {
    return resultTeam;
  }
}

uint8_t CGame::GetOneVsAllTeamOne(const uint8_t teamAll) const
{
  if (!GetIsCustomForces()) {
    return 0;
  }

  const uint8_t mapNumTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  pair<uint8_t, uint8_t> smallestTeam = make_pair(m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots());
  for (uint8_t team = 0; team < mapNumTeams; ++team) {
    if (team == teamAll) continue;
    if (teamSizes[team] < smallestTeam.second) {
      smallestTeam = make_pair(team, teamSizes[team]);
    }
  }
  // We can be sure that smallestTeam does not contain fixed computer slots, because:
  // - TeamAll contains the only allowed computer slot.
  // - CMap validator ensures that there are at least two well-defined teams in the game.
  return smallestTeam.first;
}

bool CGame::SetLayoutOneVsAll(const GameUser::CGameUser* targetPlayer)
{
  m_CustomLayout = CUSTOM_LAYOUT_COMPACT;

  const bool isSwap = GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES;
  uint8_t targetSID = GetSIDFromUID(targetPlayer->GetUID());
  //uint8_t targetTeam = m_Slots[targetSID].GetTeam();

  const uint8_t teamAll = GetOneVsAllTeamAll();
  if (teamAll == 0xFF) return false;
  const uint8_t teamOne = GetOneVsAllTeamOne(teamAll);

  // Move the alone user to its own team.
  if (isSwap) {
    const uint8_t swapSID = GetSelectableTeamSlotBack(teamOne, static_cast<uint8_t>(m_Slots.size()), static_cast<uint8_t>(m_Slots.size()), true);
    if (swapSID == 0xFF) {
      return false;
    }
    SwapSlots(targetSID, swapSID);
    targetSID = swapSID; // Sync slot index on swap.
  } else {
    CGameSlot* slot = GetSlot(targetSID);
    slot->SetTeam(teamOne);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }

  // Move the rest of users.
  if (isSwap) {
    uint8_t endObserverSID = static_cast<uint8_t>(m_Slots.size());
    uint8_t endAllSID = endObserverSID;
    uint8_t SID = static_cast<uint8_t>(m_Slots.size()) - 1;
    while (SID != 0xFF) {
      if (SID == targetSID || m_Slots[SID].GetTeam() == teamAll || m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        --SID;
        continue;
      }

      uint8_t swapSID = GetSelectableTeamSlotBack(teamAll, SID, endAllSID, true);
      bool toObservers = swapSID == 0xFF; // Alliance team is full.
      if (toObservers) {
        if (m_Slots[SID].GetIsComputer()) {
          return false;
        }
        swapSID = GetSelectableTeamSlotBack(m_Map->GetVersionMaxSlots(), SID, endObserverSID, true);
        if (swapSID == 0xFF) {
          return false;
        }
      }
      if (!SwapSlots(SID, swapSID)) {
        Print(ByteArrayToDecString(InspectSlot(SID)->GetByteArray()));
        Print(ByteArrayToDecString(InspectSlot(swapSID)->GetByteArray()));
        return false;
      } else if (toObservers) {
        endObserverSID = swapSID;
      } else {
        endAllSID = swapSID;
      }
      CloseAllTeamSlots(teamOne);
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      --SID;
    }
  } else {
    uint8_t remainingSlots = m_Map->GetMapNumControllers() - GetNumControllers();
    if (remainingSlots > 0) {
      uint8_t SID = static_cast<uint8_t>(m_Slots.size());
      while (SID--) {
        if (SID == targetSID) continue;
        uint8_t wasTeam = m_Slots[SID].GetTeam();
        m_Slots[SID].SetTeam(teamAll);
        m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
        if (wasTeam == m_Map->GetVersionMaxSlots()) {
          if (--remainingSlots == 0) break;
        }
      }
    }
  }
  m_CustomLayout = CUSTOM_LAYOUT_ONE_VS_ALL;
  m_CustomLayoutData = make_pair(teamOne, teamAll);
  return true;
}

bool CGame::GetIsAutoStartDue() const
{
  if (m_Users.empty() || m_CountDownStarted || m_AutoStartRequirements.empty()) {
    return false;
  }
  if (!m_ControllersBalanced && m_Config.m_AutoStartRequiresBalance) {
    return false;
  }

  const int64_t Time = GetTime();
  for (const auto& requirement : m_AutoStartRequirements) {
    if (requirement.first <= m_ControllersReadyCount && requirement.second <= Time) {
      return GetCanStartGracefulCountDown();
    }
  }

  return false;
}

string CGame::GetAutoStartText() const
{
  if (m_AutoStartRequirements.empty()) {
    return "Autostart is not set.";
  }

  int64_t Time = GetTime();
  vector<string> fragments; 
  for (const auto& requirement : m_AutoStartRequirements) {
    if (requirement.first == 0 && requirement.second <= Time) {
      fragments.push_back("now");
    } else if (requirement.first == 0) {
      fragments.push_back("in " + DurationLeftToString(requirement.second - Time));
    } else if (requirement.second <= Time) {
      fragments.push_back("with " + to_string(requirement.first) + " players");
    } else {
      fragments.push_back("with " + to_string(requirement.first) + "+ players after " + DurationLeftToString(requirement.second - Time));
    }
  }

  if (fragments.size() == 1) {
    return "Autostarts " + fragments[0] + ".";
  }

  return "Autostarts " + JoinVector(fragments, "or", false) + ".";
}

string CGame::GetReadyStatusText() const
{
  string notReadyFragment;
  if (m_ControllersNotReadyCount > 0) {
    if (m_Config.m_BroadcastCmdToken.empty()) {
      notReadyFragment = " Use " + m_Config.m_PrivateCmdToken + "ready when you are.";
    } else {
      notReadyFragment = " Use " + m_Config.m_BroadcastCmdToken + "ready when you are.";
    }
  }
  if (m_ControllersReadyCount == 0) {
    return "No players ready yet." + notReadyFragment;
  }

  if (m_ControllersReadyCount == 1) {
    return "One player is ready." + notReadyFragment;
  }

  return to_string(m_ControllersReadyCount) + " players are ready." + notReadyFragment;
}

string CGame::GetCmdToken() const
{
  return m_Config.m_BroadcastCmdToken.empty() ? m_Config.m_PrivateCmdToken : m_Config.m_BroadcastCmdToken;
}

void CGame::SendAllAutoStart() const
{
  SendAllChat(GetAutoStartText());
}

uint32_t CGame::GetGameType() const
{
  uint32_t mapGameType = 0;
  if (m_DisplayMode == GAME_PRIVATE) mapGameType |= MAPGAMETYPE_PRIVATEGAME;
  if (m_RestoredGame) {
    mapGameType |= MAPGAMETYPE_SAVEDGAME;
  } else {
    mapGameType |= MAPGAMETYPE_UNKNOWN0;
    mapGameType |= m_Map->GetMapGameType();
  }
  return mapGameType;
}

uint32_t CGame::CalcGameFlags() const
{
  return m_Map->GetGameConvertedFlags();
}

string CGame::GetSourceFilePath() const {
  if (m_RestoredGame) {
    return m_RestoredGame->GetClientPath();
  } else {
    return m_Map->GetClientPath();
  }
}

array<uint8_t, 4> CGame::GetSourceFileHash() const
{
  if (m_RestoredGame) {
    return m_RestoredGame->GetSaveHash();
  } else {
    return m_Map->GetMapScriptsWeakHash();
  }
}

array<uint8_t, 20> CGame::GetSourceFileSHA1() const
{
  return m_Map->GetMapScriptsSHA1();
}

array<uint8_t, 20> CGame::GetSourceFileMapHash() const
{
  return m_Map->GetMapScriptsHash();
}

array<uint8_t, 2> CGame::GetAnnounceWidth() const
{
  if (GetIsProxyReconnectable()) {
    // use an invalid map width/height to indicate reconnectable games
    return GPSProtocol::SEND_GPSS_DIMENSIONS();
  }
  if (m_RestoredGame) return {0, 0};
  return m_Map->GetMapWidth();
}
array<uint8_t, 2> CGame::GetAnnounceHeight() const
{
  if (GetIsProxyReconnectable()) {
    // use an invalid map width/height to indicate reconnectable games
    return GPSProtocol::SEND_GPSS_DIMENSIONS();
  }
  if (m_RestoredGame) return {0, 0};
  return m_Map->GetMapHeight();
}

void CGame::SendVirtualHostPlayerInfo(CConnection* user) const
{
  if (m_VirtualHostUID == 0xFF) {
    return;
  }

  const std::array<uint8_t, 4> IP = {0, 0, 0, 0};

  Send(user, GameProtocol::SEND_W3GS_PLAYERINFO(m_VirtualHostUID, GetLobbyVirtualHostName(), IP, IP));
}

void CGame::SendFakeUsersInfo(CConnection* user) const
{
  if (m_FakeUsers.empty()) {
    return;
  }

  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    Send(user, fakeUser.GetPlayerInfoBytes());
  }
}

void CGame::SendJoinedPlayersInfo(CConnection* connection) const
{
  for (auto& otherPlayer : m_Users) {
    if (otherPlayer->GetDeleteMe()) {
      continue;
    }
    if (connection->GetType() == INCON_TYPE_PLAYER && static_cast<GameUser::CGameUser*>(connection) == otherPlayer) {
      continue;
    }
    Send(connection,
      GameProtocol::SEND_W3GS_PLAYERINFO_EXCLUDE_IP(otherPlayer->GetUID(), otherPlayer->GetDisplayName()/*, otherPlayer->GetIPv4(), otherPlayer->GetIPv4Internal()*/)
    );
  }
}

void CGame::SendIncomingPlayerInfo(GameUser::CGameUser* user) const
{
  for (auto& otherPlayer : m_Users) {
    if (otherPlayer == user)
      continue;
    if (otherPlayer->GetDeleteMe())
      break;
    otherPlayer->Send(
      GameProtocol::SEND_W3GS_PLAYERINFO_EXCLUDE_IP(user->GetUID(), user->GetDisplayName()/*, user->GetIPv4(), user->GetIPv4Internal()*/)
    );
  }
}

void CGame::SendWelcomeMessage(GameUser::CGameUser *user) const
{
  for (size_t i = 0; i < m_Aura->m_Config.m_Greeting.size(); i++) {
    string::size_type matchIndex;
    string Line = m_Aura->m_Config.m_Greeting[i];
    if (Line.substr(0, 12) == "{SHORTDESC?}") {
      if (m_Map->GetMapShortDesc().empty()) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 12) == "{SHORTDESC!}") {
      if (!m_Map->GetMapShortDesc().empty()) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 6) == "{URL?}") {
      if (GetMapSiteURL().empty()) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 6) == "{URL!}") {
      if (!GetMapSiteURL().empty()) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 11) == "{FILENAME?}") {
      size_t LastSlash = m_MapPath.rfind('\\');
      if (LastSlash == string::npos || LastSlash > m_MapPath.length() - 6) {
        continue;
      }
      Line = Line.substr(11);
    }
    if (Line.substr(0, 12) == "{AUTOSTART?}") {
      if (m_AutoStartRequirements.empty()) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 11) == "{FILENAME!}") {
      size_t LastSlash = m_MapPath.rfind('\\');
      if (!(LastSlash == string::npos || LastSlash > m_MapPath.length() - 6)) {
        continue;
      }
      Line = Line.substr(11);
    }
    if (Line.substr(0, 10) == "{CREATOR?}") {
      if (m_CreatorText.empty()) {
        continue;
      }
      Line = Line.substr(10);
    }
    if (Line.substr(0, 10) == "{CREATOR!}") {
      if (!m_CreatorText.empty()) {
        continue;
      }
      Line = Line.substr(10);
    }
    if (Line.substr(0, 12) == "{OWNERLESS?}") {
      if (!m_OwnerLess) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 12) == "{OWNERLESS!}") {
      if (m_OwnerLess) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 8) == "{OWNER?}") {
      if (m_OwnerName.empty()) {
        continue;
      }
      Line = Line.substr(8);
    }
    if (Line.substr(0, 8) == "{OWNER!}") {
      if (!m_OwnerName.empty()) {
        continue;
      }
      Line = Line.substr(8);
    }
    if (Line.substr(0, 17) == "{CHECKLASTOWNER?}") {
      if (m_OwnerName == user->GetName() || m_LastOwner != user->GetName()) {
        continue;
      }
      Line = Line.substr(17);
    }
    if (Line.substr(0, 14) == "{REPLACEABLE?}") {
      if (!m_Replaceable) {
        continue;
      }
      Line = Line.substr(14);
    }
    if (Line.substr(0,14) == "{REPLACEABLE!}") {
      if (m_Replaceable) {
        continue;
      }
      Line = Line.substr(14);
    }
    if (Line.substr(0, 6) == "{LAN?}") {
      if (user->GetRealm(false)) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 6) == "{LAN!}") {
      if (!user->GetRealm(false)) {
        continue;
      }
      Line = Line.substr(6);
    }
    while ((matchIndex = Line.find("{CREATOR}")) != string::npos) {
      Line.replace(matchIndex, 9, m_CreatorText);
    }
    while ((matchIndex = Line.find("{HOSTREALM}")) != string::npos) {
      if (m_CreatedFromType == SERVICE_TYPE_REALM) {
        Line.replace(matchIndex, 11, "@" + reinterpret_cast<CRealm*>(m_CreatedFrom)->GetCanonicalDisplayName());
      } else if (m_CreatedFromType == SERVICE_TYPE_IRC) {
        Line.replace(matchIndex, 11, "@" + reinterpret_cast<CIRC*>(m_CreatedFrom)->m_Config.m_HostName);
      } else if (m_CreatedFromType == SERVICE_TYPE_DISCORD) {
        // TODO: {HOSTREALM} Discord
      } else {
        Line.replace(matchIndex, 11, "@" + ToFormattedRealm());
      }
    }
    while ((matchIndex = Line.find("{OWNER}")) != string::npos) {
      Line.replace(matchIndex, 7, m_OwnerName);
    }
    while ((matchIndex = Line.find("{OWNERREALM}")) != string::npos) {
      Line.replace(matchIndex, 12, "@" + ToFormattedRealm(m_OwnerRealm));
    }
    while ((matchIndex = Line.find("{TRIGGER_PRIVATE}")) != string::npos) {
      Line.replace(matchIndex, 17, m_Config.m_PrivateCmdToken);
    }
    while ((matchIndex = Line.find("{TRIGGER_BROADCAST}")) != string::npos) {
      Line.replace(matchIndex, 19, m_Config.m_BroadcastCmdToken);
    }
    while ((matchIndex = Line.find("{TRIGGER_PREFER_PRIVATE}")) != string::npos) {
      Line.replace(matchIndex, 24, m_Config.m_PrivateCmdToken.empty() ? m_Config.m_BroadcastCmdToken : m_Config.m_PrivateCmdToken);
    }
    while ((matchIndex = Line.find("{TRIGGER_PREFER_BROADCAST}")) != string::npos) {
      Line.replace(matchIndex, 26, m_Config.m_BroadcastCmdToken.empty() ? m_Config.m_PrivateCmdToken : m_Config.m_BroadcastCmdToken);
    }
    while ((matchIndex = Line.find("{URL}")) != string::npos) {
      Line.replace(matchIndex, 5, GetMapSiteURL());
    }
    while ((matchIndex = Line.find("{FILENAME}")) != string::npos) {
      Line.replace(matchIndex, 10, GetClientFileName());
    }
    while ((matchIndex = Line.find("{SHORTDESC}")) != string::npos) {
      Line.replace(matchIndex, 11, m_Map->GetMapShortDesc());
    }
    while ((matchIndex = Line.find("{AUTOSTART}")) != string::npos) {
      Line.replace(matchIndex, 11, GetAutoStartText());
    }
    while ((matchIndex = Line.find("{READYSTATUS}")) != string::npos) {
      Line.replace(matchIndex, 13, GetReadyStatusText());
    }
    SendChat(user, Line, LOG_LEVEL_TRACE);
  }
}

void CGame::SendOwnerCommandsHelp(const string& cmdToken, GameUser::CGameUser* user) const
{
  SendChat(user, cmdToken + "open [NUMBER] - opens a slot", LOG_LEVEL_TRACE);
  SendChat(user, cmdToken + "close [NUMBER] - closes a slot", LOG_LEVEL_TRACE);
  SendChat(user, cmdToken + "fill [DIFFICULTY] - adds computers", LOG_LEVEL_TRACE);
  if (m_Map->GetMapNumTeams() > 2) {
    SendChat(user, cmdToken + "ffa - sets free for all game mode", LOG_LEVEL_TRACE);
  }
  SendChat(user, cmdToken + "vsall - sets one vs all game mode", LOG_LEVEL_TRACE);
  SendChat(user, cmdToken + "terminator - sets humans vs computers", LOG_LEVEL_TRACE);
}

void CGame::SendCommandsHelp(const string& cmdToken, GameUser::CGameUser* user, const bool isIntro) const
{
  if (isIntro) {
    SendChat(user, "Welcome, " + user->GetName() + ". Please use " + cmdToken + GetTokenName(cmdToken) + " for commands.", LOG_LEVEL_TRACE);
  } else {
    SendChat(user, "Use " + cmdToken + GetTokenName(cmdToken) + " for commands.", LOG_LEVEL_TRACE);
  }
  if (!isIntro) return;
  SendChat(user, cmdToken + "ping - view your latency", LOG_LEVEL_TRACE);
  SendChat(user, cmdToken + "go - starts the game", LOG_LEVEL_TRACE);
  if (!m_OwnerLess && m_OwnerName.empty()) {
    SendChat(user, cmdToken + "owner - acquire permissions over this game", LOG_LEVEL_TRACE);
  }
  if (MatchOwnerName(user->GetName())) {
    SendOwnerCommandsHelp(cmdToken, user);
  }
  user->SetSentAutoCommandsHelp(true);
}

void CGame::SendAllActionsCallback()
{
  CQueuedActionsFrame& frame = GetFirstActionFrame();
  switch (frame.callback) {
    case ON_SEND_ACTIONS_PAUSE:
      m_Paused = true;
      m_PauseUser = GetUserFromUID(frame.pauseUID);
      m_LastPausedTicks = GetTicks();
      break;
    case ON_SEND_ACTIONS_RESUME:
      m_Paused = false;
      m_PauseUser = nullptr;
      break;
    default:
      break;
  }
  for (GameUser::CGameUser* user : frame.leavers) {
    // TODO: m_BufferingEnabled & BUFFERING_ENABLED_PLAYING
    DLOG_APP_IF(LOG_LEVEL_TRACE, "[" + user->GetName() + "] running scheduled deletion")
    user->SetDeleteMe(true);
  }
  frame.Reset();
}

void CGame::SendGProxyEmptyActions()
{
  if (!GetAnyUsingGProxy()) {
    return;
  }

  const vector<uint8_t> emptyActions = GameProtocol::SEND_W3GS_EMPTY_ACTIONS(m_GProxyEmptyActions);

  // GProxy sends these empty actions itself BEFORE every action received.
  // So we need to match it, to avoid desyncs.
  for (auto& user : m_Users) {
    if (!user->GetGProxyAny()) {
      Send(user, emptyActions);

      // Warcraft III doesn't respond to empty actions,
      // so we need to artificially increase users' sync counters.
      /*
      user->AddSyncCounterOffset(m_GProxyEmptyActions);
      */
    }
  }

  if (m_BufferingEnabled & BUFFERING_ENABLED_PLAYING) {
    m_PlayingBuffer.emplace_back();
  }
}

void CGame::SendAllActions()
{
  if (!m_Paused) {
    m_GameTicks += GetLatency();
  } else {
    m_PausedTicksDeltaSum = GetLatency();
  }

  ++m_SyncCounter;

  SendGProxyEmptyActions();
  vector<uint8_t> actions = GetFirstActionFrame().GetBytes(GetLatency());
  SendAll(actions);

  if (m_BufferingEnabled & BUFFERING_ENABLED_PLAYING) {
    m_PlayingBuffer.push_back(std::move(actions));
  }

  SendAllActionsCallback();

  uint8_t maxOldEqualizerOffset = m_MaxPingEqualizerDelayFrames;
  if (CheckUpdatePingEqualizer()) {
    m_MaxPingEqualizerDelayFrames = UpdatePingEqualizer();
  }
  RunActionsScheduler(m_MaxPingEqualizerDelayFrames, maxOldEqualizerOffset);
}

std::string CGame::GetPrefixedGameName(const CRealm* realm) const
{
  if (realm == nullptr) return m_GameName;
  return realm->GetPrefixedGameName(m_GameName);
}

std::string CGame::GetAnnounceText(const CRealm* realm) const
{
  uint8_t gameVersion = realm ? realm->GetGameVersion() : m_Aura->m_GameVersion;
  uint32_t mapSize = ByteArrayToUInt32(m_Map->GetMapSize(), false);
  string versionPrefix;
  if (gameVersion <= 26 && mapSize > 0x800000) {
    versionPrefix = "[1." + ToDecString(gameVersion) + ".UnlockMapSize] ";
  } else {
    versionPrefix = "[1." + ToDecString(gameVersion) + "] ";

}
  string startedPhrase;
  if (m_IsMirror || m_RestoredGame || m_OwnerName.empty()) {
    startedPhrase = ". (\"" + GetPrefixedGameName(realm) + "\")";
  } else {
    startedPhrase = ". (Started by " + m_OwnerName + ": \"" + GetPrefixedGameName(realm) + "\")";
  }

  string typeWord;
  if (m_RestoredGame) {
    typeWord = "Loaded game";
  } else if (m_DisplayMode == GAME_PRIVATE) {
    typeWord = "Private game";
  } else {
    typeWord = "Game";
  }

  if (m_IsMirror) {
    return versionPrefix + typeWord + " mirrored: " + m_Map->GetServerFileName() + startedPhrase;
  } else {
    return versionPrefix + typeWord + " hosted: " + m_Map->GetServerFileName() + startedPhrase;
  }
}

uint16_t CGame::GetHostPortForDiscoveryInfo(const uint8_t protocol) const
{
  // Uses <net.game_discovery.udp.tcp4_custom_port.value>
  if (protocol == AF_INET)
    return m_Aura->m_Net.m_Config.m_UDPEnableCustomPortTCP4 ? m_Aura->m_Net.m_Config.m_UDPCustomPortTCP4 : m_HostPort;

  // Uses <net.game_discovery.udp.tcp6_custom_port.value>
  if (protocol == AF_INET6)
    return m_Aura->m_Net.m_Config.m_UDPEnableCustomPortTCP6 ? m_Aura->m_Net.m_Config.m_UDPCustomPortTCP6 : m_HostPort;

  return m_HostPort;
}

uint8_t CGame::CalcActiveReconnectProtocols() const
{
  uint8_t protocols = 0;
  for (const auto& user : m_Users) {
    if (!user->GetGProxyAny()) continue;
    if (user->GetGProxyExtended()) {
      protocols |= RECONNECT_ENABLED_GPROXY_EXTENDED;
      if (protocols != RECONNECT_ENABLED_GPROXY_EXTENDED) break;
    } else {
      protocols |= RECONNECT_ENABLED_GPROXY_BASIC;
      if (protocols != RECONNECT_ENABLED_GPROXY_BASIC) break;
    }
  }
  return protocols;
}

string CGame::GetActiveReconnectProtocolsDetails() const
{
  // Must only be used to print to console, because GetName() is used instead of GetDisplayName()
  vector<string> protocols;
  for (const auto& user : m_Users) {
    if (!user->GetGProxyAny()) {
      protocols.push_back("[" + user->GetName() + ": OFF]");
    } else if (user->GetGProxyExtended()) {
      protocols.push_back("[" + user->GetName() + ": EXT]");
    } else {
      protocols.push_back("[" + user->GetName() + ": ON]");
    }
  }
  return JoinVector(protocols, false);
}

bool CGame::CalcAnyUsingGProxy() const
{
  for (const auto& user : m_Users) {
    if (user->GetGProxyAny()) {
      return true;
    }
  }
  return false;
}

bool CGame::CalcAnyUsingGProxyLegacy() const
{
  for (const auto& user : m_Users) {
    if (!user->GetGProxyAny()) continue;
    if (!user->GetGProxyExtended()) {
      return true;
    }
  }
  return false;
}

uint8_t CGame::GetPlayersReadyMode() const {
  return m_Config.m_PlayersReadyMode;
}

CQueuedActionsFrame& CGame::GetFirstActionFrame()
{
  return GetFirstActionFrameNode()->data;
}

CQueuedActionsFrame& CGame::GetLastActionFrame()
{
  return GetLastActionFrameNode()->data;
}

vector<QueuedActionsFrameNode*> CGame::GetFrameNodesInRangeInclusive(const uint8_t startOffset, const uint8_t endOffset)
{
  vector<QueuedActionsFrameNode*> frameNodes;
  frameNodes.reserve(endOffset - startOffset + 1);
  QueuedActionsFrameNode* frameNode = GetFirstActionFrameNode();
  uint8_t offset = startOffset;
  while (offset--) {
    frameNode = frameNode->next;
  }
  offset = endOffset - startOffset + 1;
  while (offset--) {
    frameNodes.push_back(frameNode);
    frameNode = frameNode->next;
  }
  return frameNodes;
}

vector<QueuedActionsFrameNode*> CGame::GetAllFrameNodes()
{
  vector<QueuedActionsFrameNode*> frameNodes;
  frameNodes.reserve(GetMaxEqualizerDelayFrames());
  QueuedActionsFrameNode* frameNode = GetFirstActionFrameNode();
  if (frameNode == nullptr) return frameNodes;
  QueuedActionsFrameNode* lastFrameNode = GetLastActionFrameNode();
  while (frameNode != lastFrameNode) {
    frameNodes.push_back(frameNode);
    frameNode = frameNode->next;
  }
  return frameNodes;
}

void CGame::MergeFrameNodes(vector<QueuedActionsFrameNode*>& frameNodes)
{
  size_t i = 0, frameCount = frameNodes.size();
  CQueuedActionsFrame& targetFrame = frameNodes[i++]->data;
  while (i < frameCount) {
    CQueuedActionsFrame& obsoleteFrame = frameNodes[i]->data;
    targetFrame.MergeFrame(obsoleteFrame);
    m_Actions.remove(frameNodes[i]);
    // When the node is deleted, data is deleted as well.
    delete frameNodes[i];
    ++i;
  }
}

void CGame::ResetUserPingEqualizerDelays()
{
  for (auto& user : m_Users) {
    user->SetPingEqualizerFrameNode(m_Actions.head);
  }
}

bool CGame::CheckUpdatePingEqualizer()
{
  if (!m_Config.m_LatencyEqualizerEnabled) return false;
  // Use m_GameTicks instead of GetTicks() to ensure we don't drift while lag screen is displayed.
  if (m_GameTicks - m_LastPingEqualizerGameTicks < PING_EQUALIZER_PERIOD_TICKS) {
    return false;
  }
  return true;
  
}

uint8_t CGame::UpdatePingEqualizer()
{
  uint8_t maxEqualizerOffset = 0;
  vector<pair<GameUser::CGameUser*, uint32_t>> descendingRTTs = GetDescendingSortedRTT();
  if (descendingRTTs.empty()) return maxEqualizerOffset;
  const uint32_t maxPing = descendingRTTs[0].second;
  bool addedFrame = false;
  for (const pair<GameUser::CGameUser*, uint32_t>& userPing : descendingRTTs) {
    // How much better ping than the worst player?
    const uint32_t framesAheadNowDiscriminator = (maxPing - userPing.second) / GetLatency();
    const uint32_t framesAheadBefore = userPing.first->GetPingEqualizerOffset();
    uint32_t framesAheadNow;
    if (framesAheadNowDiscriminator > framesAheadBefore) {
      framesAheadNow = framesAheadBefore + 1;
      if (!addedFrame && m_MaxPingEqualizerDelayFrames < framesAheadNow && framesAheadNow < m_Config.m_LatencyEqualizerFrames) {
        m_Actions.emplaceAfter(GetLastActionFrameNode());
        addedFrame = true;
      }
      userPing.first->AddDelayPingEqualizerFrame();
    } else if (0 < framesAheadBefore && framesAheadNowDiscriminator < framesAheadBefore) {
      framesAheadNow = framesAheadBefore - 1;
      userPing.first->SubDelayPingEqualizerFrame();
    }
    uint8_t nextOffset = userPing.first->GetPingEqualizerOffset();
    if (nextOffset > maxEqualizerOffset) {
      maxEqualizerOffset = nextOffset;
    }
  }
  m_LastPingEqualizerGameTicks = m_GameTicks;
  return maxEqualizerOffset;
}

vector<pair<GameUser::CGameUser*, uint32_t>> CGame::GetDescendingSortedRTT() const
{
  vector<pair<GameUser::CGameUser*, uint32_t>> sortableUserPings;
  for (auto& user : m_Users) {
     if (!user->GetLeftMessageSent() && !user->GetIsObserver()) {
       sortableUserPings.emplace_back(user, user->GetRTT());
     }
  }
  sort(begin(sortableUserPings), end(sortableUserPings), [](const pair<GameUser::CGameUser*, uint32_t> a, const pair<GameUser::CGameUser*, uint32_t> b) {
    return a.second > b.second;
  });
  return sortableUserPings;
}

uint16_t CGame::GetDiscoveryPort(const uint8_t protocol) const
{
  return m_Aura->m_Net.GetUDPPort(protocol);
}

vector<uint8_t> CGame::GetGameDiscoveryInfo(const uint8_t gameVersion, const uint16_t hostPort)
{
  vector<uint8_t> info = *(GetGameDiscoveryInfoTemplate());
  WriteUint32(info, gameVersion, m_GameDiscoveryInfoVersionOffset);
  uint32_t slotsOff = static_cast<uint32_t>(m_Slots.size() == GetSlotsOpen() ? m_Slots.size() : GetSlotsOpen() + 1);
  uint32_t uptime = GetUptime();
  WriteUint32(info, slotsOff, m_GameDiscoveryInfoDynamicOffset);
  WriteUint32(info, uptime, m_GameDiscoveryInfoDynamicOffset + 4);
  WriteUint16(info, hostPort, m_GameDiscoveryInfoDynamicOffset + 8);
  return info;
}

vector<uint8_t>* CGame::GetGameDiscoveryInfoTemplate()
{
  if (!m_GameDiscoveryInfoChanged && !m_GameDiscoveryInfo.empty()) {
    return &m_GameDiscoveryInfo;
  }
  m_GameDiscoveryInfo = GetGameDiscoveryInfoTemplateInner(&m_GameDiscoveryInfoVersionOffset, &m_GameDiscoveryInfoDynamicOffset);
  m_GameDiscoveryInfoChanged = false;
  return &m_GameDiscoveryInfo;
}

vector<uint8_t> CGame::GetGameDiscoveryInfoTemplateInner(uint16_t* gameVersionOffset, uint16_t* dynamicInfoOffset) const
{
  // we send 12 for SlotsTotal because this determines how many UID's Warcraft 3 allocates
  // we need to make sure Warcraft 3 allocates at least SlotsTotal + 1 but at most 12 UID's
  // this is because we need an extra UID for the virtual host user (but we always delete the virtual host user when the 12th person joins)
  // however, we can't send 13 for SlotsTotal because this causes Warcraft 3 to crash when sharing control of units
  // nor can we send SlotsTotal because then Warcraft 3 crashes when playing maps with less than 12 UID's (because of the virtual host user taking an extra UID)
  // we also send 12 for SlotsOpen because Warcraft 3 assumes there's always at least one user in the game (the host)
  // so if we try to send accurate numbers it'll always be off by one and results in Warcraft 3 assuming the game is full when it still needs one more user
  // the easiest solution is to simply send 12 for both so the game will always show up as (1/12) users

  // note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)
  // note: we do not use m_Map->GetMapGameType because none of the filters are set when broadcasting to LAN (also as you might expect)

  return GameProtocol::SEND_W3GS_GAMEINFO_TEMPLATE(
    gameVersionOffset, dynamicInfoOffset,
    GetGameType(),
    GetGameFlags(),
    GetAnnounceWidth(),
    GetAnnounceHeight(),
    m_GameName,
    GetIndexVirtualHostName(),
    GetSourceFilePath(),
    GetSourceFileHash(),
    static_cast<uint32_t>(m_Slots.size()), // Total Slots
    m_HostCounter,
    m_EntryKey
  );
}

void CGame::AnnounceToRealm(CRealm* realm)
{
  if (m_DisplayMode == GAME_NONE) return;
  realm->SendGameRefresh(m_DisplayMode, this);
}

void CGame::AnnounceDecreateToRealms()
{
  for (auto& realm : m_Aura->m_Realms) {
    if (m_IsMirror && realm->GetIsMirror())
      continue;

    realm->ResetGameChatAnnouncement();
    realm->ResetGameBroadcastData();
  }
}

void CGame::AnnounceToAddress(string& addressLiteral, uint8_t gameVersion)
{
  if (gameVersion == 0) gameVersion = m_Aura->m_GameVersion;
  optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(addressLiteral);
  if (!maybeAddress.has_value())
    return;

  sockaddr_storage* address = &(maybeAddress.value());
  SetAddressPort(address, 6112);
  if (isLoopbackAddress(address)) {
    m_Aura->m_Net.Send(address, GetGameDiscoveryInfo(gameVersion, m_HostPort));
  } else {
    m_Aura->m_Net.Send(address, GetGameDiscoveryInfo(gameVersion, GetHostPortForDiscoveryInfo(GetInnerIPVersion(address))));
  }
}

void CGame::ReplySearch(sockaddr_storage* address, CSocket* socket, uint8_t gameVersion)
{
  if (gameVersion == 0) gameVersion = m_Aura->m_GameVersion;
  if (isLoopbackAddress(address)) {
    socket->SendReply(address, GetGameDiscoveryInfo(gameVersion, m_HostPort));
  } else {
    socket->SendReply(address, GetGameDiscoveryInfo(gameVersion, GetHostPortForDiscoveryInfo(GetInnerIPVersion(address))));
  }
}

void CGame::SendGameDiscoveryCreate(uint8_t gameVersion) const
{
  vector<uint8_t> packet = GameProtocol::SEND_W3GS_CREATEGAME(gameVersion, m_HostCounter);
  m_Aura->m_Net.SendGameDiscovery(packet, m_Config.m_ExtraDiscoveryAddresses);
}

void CGame::SendGameDiscoveryCreate() const
{
  uint8_t version = m_SupportedGameVersionsMin;
  while (version <= m_SupportedGameVersionsMax) {
    if (GetIsSupportedGameVersion(version)) {
      SendGameDiscoveryCreate(version);
    }
    ++version;
  }
}

void CGame::SendGameDiscoveryDecreate() const
{
  vector<uint8_t> packet = GameProtocol::SEND_W3GS_DECREATEGAME(m_HostCounter);
  m_Aura->m_Net.SendGameDiscovery(packet, m_Config.m_ExtraDiscoveryAddresses);
}

void CGame::SendGameDiscoveryRefresh() const
{
  vector<uint8_t> packet = GameProtocol::SEND_W3GS_REFRESHGAME(
    m_HostCounter,
    static_cast<uint32_t>(m_Slots.size() == GetSlotsOpen() ? 1 : m_Slots.size() - GetSlotsOpen()),
    static_cast<uint32_t>(m_Slots.size())
  );
  m_Aura->m_Net.SendGameDiscovery(packet, m_Config.m_ExtraDiscoveryAddresses);

  // Send to active VLAN connections
  if (m_Aura->m_Net.m_Config.m_VLANEnabled) {
    for (auto& serverConnections : m_Aura->m_Net.m_ManagedConnections) {
      for (auto& connection : serverConnections.second) {
        if (connection->GetDeleteMe()) continue;
        if (connection->GetIsVLAN() && connection->GetGameVersion() > 0 && GetIsSupportedGameVersion(connection->GetGameVersion())) {
          SendGameDiscoveryInfoVLAN(connection);
        }
      }
    }
  }
}

void CGame::SendGameDiscoveryInfo(uint8_t gameVersion)
{
  // See CNet::SendGameDiscovery()

  if (!m_Aura->m_Net.SendBroadcast(GetGameDiscoveryInfo(gameVersion, GetHostPortForDiscoveryInfo(AF_INET)))) {
    // Ensure the game is available at loopback.
    DLOG_APP_IF(LOG_LEVEL_TRACE2, "sending IPv4 GAMEINFO packet to IPv4 Loopback (game port " + to_string(m_HostPort) + ")")
    m_Aura->m_Net.SendLoopback(GetGameDiscoveryInfo(gameVersion, m_HostPort));
  }

  for (auto& address : m_Config.m_ExtraDiscoveryAddresses) {
    if (isLoopbackAddress(&address)) continue; // We already ensure sending loopback packets above.
    bool isIPv6 = GetInnerIPVersion(&address) == AF_INET6;
    if (isIPv6 && !m_Aura->m_Net.m_SupportTCPOverIPv6) {
      continue;
    }
    m_Aura->m_Net.Send(&address, GetGameDiscoveryInfo(gameVersion, GetHostPortForDiscoveryInfo(isIPv6 ? AF_INET6 : AF_INET)));
  }

  // Send to active UDP in TCP tunnels and VLAN connections
  if (m_Aura->m_Net.m_Config.m_EnableTCPWrapUDP || m_Aura->m_Net.m_Config.m_VLANEnabled) {
    for (auto& serverConnections : m_Aura->m_Net.m_ManagedConnections) {
      for (auto& connection : serverConnections.second) {
        if (connection->GetDeleteMe()) continue;
        if (connection->GetIsUDPTunnel()) {
          connection->Send(GetGameDiscoveryInfo(gameVersion, GetHostPortForDiscoveryInfo(connection->GetUsingIPv6() ? AF_INET6 : AF_INET)));
        }
        if (connection->GetIsVLAN() && connection->GetGameVersion() > 0 && GetIsSupportedGameVersion(connection->GetGameVersion())) {
          SendGameDiscoveryInfoVLAN(connection);
        }
      }
    }
  }
}

void CGame::SendGameDiscoveryInfoVLAN(CGameSeeker* gameSeeker) const
{
  array<uint8_t, 4> IP = {0, 0, 0, 0};
  gameSeeker->Send(
    VLANProtocol::SEND_VLAN_GAMEINFO(
      true /* TFT */,
      gameSeeker->GetGameVersion(),
      GetGameType(),
      GetGameFlags(),
      GetAnnounceWidth(),
      GetAnnounceHeight(),
      m_GameName,
      GetIndexVirtualHostName(),
      GetUptime(), // dynamic
      GetSourceFilePath(),
      GetSourceFileHash(),
      static_cast<uint32_t>(m_Slots.size()), // Total Slots
      static_cast<uint32_t>(m_Slots.size() == GetSlotsOpen() ? m_Slots.size() : GetSlotsOpen() + 1),
      IP,
      GetHostPortForDiscoveryInfo(AF_INET),
      m_HostCounter,
      m_EntryKey
    )
  );
}

void CGame::SendGameDiscoveryInfo()
{
  uint8_t version = m_SupportedGameVersionsMin;
  while (version <= m_SupportedGameVersionsMax) {
    if (GetIsSupportedGameVersion(version)) {
      SendGameDiscoveryInfo(version);
    }
    ++version;
  }
}

/*
 * EventUserDeleted is called when CGame event loop identifies that a CGameUser has the m_DeleteMe flag
 * This flag is set by
 * - SendAllActionsCallback (after the game started, or failed to start)
 * - StopPlayers
 * - EventUserAfterDisconnect (only in the game lobby)
 */
void CGame::EventUserDeleted(GameUser::CGameUser* user, void* fd, void* send_fd)
{
  if (m_Exiting) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "deleting user [" + user->GetName() + "]: " + user->GetLeftReason())
  } else {
    LOG_APP_IF(LOG_LEVEL_INFO, "deleting user [" + user->GetName() + "]: " + user->GetLeftReason())
  }

  if (!user->GetIsObserver()) {
    m_LastPlayerLeaveTicks = GetTicks();
    m_LastPingEqualizerGameTicks = 0;
  }

  // W3GS_PLAYERLEAVE messages may follow ACTION_PAUSE or ACTION_RESUME (or none),
  // so ensure we don't leave m_PauseUser as a dangling pointer.
  if (m_PauseUser == user) {
    m_PauseUser = nullptr;
  }

  if (m_GameLoading || m_GameLoaded) {
    for (auto& otherPlayer : m_SyncPlayers[user]) {
      UserList& BackList = m_SyncPlayers[otherPlayer];
      auto BackIterator = std::find(BackList.begin(), BackList.end(), user);
      if (BackIterator == BackList.end()) {
      } else {
        *BackIterator = std::move(BackList.back());
        BackList.pop_back();
      }
    }
    m_SyncPlayers.erase(user);
    m_HadLeaver = true;
  } else if (!m_LobbyLoading && m_Config.m_LobbyOwnerReleaseLANLeaver) {
    if (MatchOwnerName(user->GetName()) && m_OwnerRealm == user->GetRealmHostName() && user->GetRealmHostName().empty()) {
      ReleaseOwner();
    }
  }

  // send the left message if we haven't sent it already
  if (!user->GetLeftMessageSent()) {
    if (user->GetLagging()) {
      LOG_APP_IF(LOG_LEVEL_INFO, "global lagger update (-" + user->GetName() + ")")
      SendAll(GameProtocol::SEND_W3GS_STOP_LAG(user));
    }
    SendLeftMessage(user, (m_GameLoaded && !user->GetIsObserver()) || (!user->GetIsLeaver() && user->GetAnyKicked()));
  }

  // abort the countdown if there was one in progress, but only if the user who left is actually a controller, or otherwise relevant.
  if (m_CountDownStarted && !m_CountDownFast && !m_GameLoading && !m_GameLoaded) {
    if (!user->GetIsObserver() || GetSlotsOccupied() < m_HCLCommandString.size()) {
      // Intentionally reveal the name of the lobby leaver (may be trolling.)
      SendAllChat("Countdown stopped because [" + user->GetName() + "] left!");
      m_CountDownStarted = false;
    } else {
      // Observers that leave during countdown are replaced by fake observers.
      // This ensures the integrity of many things related to game slots.
      // e.g. this allows m_ControllersWithMap to remain unchanged.
      const uint8_t replaceSID = GetEmptyObserverSID();
      const uint8_t replaceUID = GetNewUID();
      CreateFakeUserInner(replaceSID, replaceUID, "User[" + ToDecString(replaceSID + 1) + "]");
      m_FakeUsers.back().SetObserver(true);
      CGameSlot* slot = GetSlot(replaceSID);
      slot->SetTeam(m_Map->GetVersionMaxSlots());
      slot->SetColor(m_Map->GetVersionMaxSlots());
      LOG_APP_IF(LOG_LEVEL_INFO, "replaced leaving observer by fake user (SID=" + ToDecString(replaceSID) + "|UID=" + ToDecString(replaceUID) + ")")
    }
  }

  // abort the votekick

  if (!m_KickVotePlayer.empty()) {
    SendAllChat("A votekick against user [" + m_KickVotePlayer + "] has been cancelled");
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  // record everything we need to know about the user for storing in the database later
  // since we haven't stored the game yet (it's not over yet!) we can't link the gameuser to the game
  // see the destructor for where these CDBGamePlayers are stored in the database
  // we could have inserted an incomplete record on creation and updated it later but this makes for a cleaner interface

  if (m_GameLoading || m_GameLoaded) {
    // When a user leaves from an already loaded game, their slot remains unchanged
    const CGameSlot* slot = InspectSlot(GetSIDFromUID(user->GetUID()));
    CDBGamePlayer* dbPlayer = GetDBPlayerFromColor(slot->GetColor());
    if (dbPlayer) {
      dbPlayer->SetLeftTime(m_GameTicks / 1000);
    }

    // keep track of the last user to leave for the !banlast command
    // ignore the last user leaving, as well as the second-to-last (forfeit)
    if (m_Users.size() > 2 && !m_ExitingSoon) {
      for (auto& bannable : m_Bannables) {
        if (bannable->GetName() == user->GetName()) {
          m_LastLeaverBannable = bannable;
        }
      }
    }
  }

  if ((m_GameLoading || m_GameLoaded || m_ExitingSoon) && !user->GetIsObserver()) {
    // end the game if there aren't any players left
    // but only if the user who left isn't an observer
    // this allows parties of 2+ observers to watch AI vs AI
    const uint8_t numJoinedPlayers = GetNumJoinedPlayers();
    if (numJoinedPlayers == 0) {
      LOG_APP_IF(LOG_LEVEL_INFO, "gameover timer started: no players left")
      StartGameOverTimer();
    } else if (!GetIsGameOverTrusted()) {
      if (numJoinedPlayers == 1 && GetNumComputers() == 0) {
        LOG_APP_IF(LOG_LEVEL_INFO, "gameover timer started: remaining 1 p | 0 comp | " + ToDecString(GetNumJoinedObservers()) + " obs")
        StartGameOverTimer();
      }
    }
  }

  // Flush queued data before the socket is destroyed.
  if (!user->GetDisconnected()) {
    user->GetSocket()->DoSend(static_cast<fd_set*>(send_fd));
  }
}

void CGame::EventLobbyLastPlayerLeaves()
{
  if (m_CustomLayout != CUSTOM_LAYOUT_FFA) {
    ResetLayout(false);
  }
}

void CGame::ReportAllPings() const
{
  UserList SortedPlayers = m_Users;
  if (SortedPlayers.empty()) return;

  if (m_Lagging) {
    sort(begin(SortedPlayers), end(SortedPlayers), [](const GameUser::CGameUser* a, const GameUser::CGameUser* b) {
      return a->GetNormalSyncCounter() < b->GetNormalSyncCounter();
    });
  } else {
    sort(begin(SortedPlayers), end(SortedPlayers), [](const GameUser::CGameUser* a, const GameUser::CGameUser* b) {
      return a->GetOperationalRTT() > b->GetOperationalRTT();
    });
  }
  vector<string> pingsText;
  for (auto i = begin(SortedPlayers); i != end(SortedPlayers); ++i) {
    pingsText.push_back((*i)->GetDisplayName() + ": " + (*i)->GetDelayText(false));
  }
  
  SendAllChat(JoinVector(pingsText, false));

  if (m_Lagging) {
    GameUser::CGameUser* worstLagger = SortedPlayers[0];
    if (worstLagger->GetDisconnected() && worstLagger->GetGProxyAny()) {
      ImmutableUserList waitingReconnectPlayers = GetWaitingReconnectPlayers();
      uint8_t laggerCount = CountLaggingPlayers() - static_cast<uint8_t>(waitingReconnectPlayers.size());
      string laggerText;
      if (laggerCount > 0) {
        laggerText = " (+" + ToDecString(laggerCount) + " other laggers)";
      }
      SendAllChat(ToNameListSentence(waitingReconnectPlayers) + " disconnected, but may reconnect" + laggerText);
    } else {
      string syncDelayText = worstLagger->GetSyncText();
      if (!syncDelayText.empty()) {
        uint8_t laggerCount = CountLaggingPlayers();
        if (laggerCount > 1) {
          SendAllChat(ToDecString(laggerCount) + " laggers - [" + worstLagger->GetDisplayName() + "] is " + syncDelayText);
        } else {
          SendAllChat("[" + worstLagger->GetDisplayName() + "] is " + syncDelayText);
        }
      }
    }
  }
}

void CGame::ResetDropVotes()
{
  for (auto& eachPlayer : m_Users) {
    eachPlayer->SetDropVote(false);
  }
}

void CGame::ResetOwnerSeen()
{
  m_LastOwnerSeen = GetTicks();
}

void CGame::SetLaggingPlayerAndUpdate(GameUser::CGameUser* user)
{
  int64_t Time = GetTime(), Ticks = GetTicks();
  if (!user->GetLagging()) {
    ResetDropVotes();

    if (!GetLagging()) {
      m_Lagging = true;
      m_StartedLaggingTime = Time;
      m_LastLagScreenResetTime = Time;
      m_LastLagScreenTime = Time;
    }

    // Report lagging users:
    // - Just disconnected user
    // - Players outside safe sync limit
    // Since the disconnected user has already been flagged with SetGProxyDisconnectNoticeSent, they get
    // excluded from the output vector of CalculateNewLaggingPlayers(),
    // So we have to add them afterwards.
    UserList laggingPlayers = CalculateNewLaggingPlayers();
    laggingPlayers.push_back(user);
    for (auto& laggingPlayer : laggingPlayers) {
      laggingPlayer->SetLagging(true);
      laggingPlayer->SetStartedLaggingTicks(Ticks);
      laggingPlayer->ClearStalePings();
    }
    LOG_APP_IF(LOG_LEVEL_INFO, "global lagger update (+" + ToNameListSentence(laggingPlayers) + ")")
    SendAll(GameProtocol::SEND_W3GS_START_LAG(laggingPlayers));
  }
}

void CGame::SetEveryoneLagging()
{
  if (GetLagging()) {
    return;
  }
  int64_t Time = GetTime(), Ticks = GetTicks();

  ResetDropVotes();

  m_Lagging = true;
  m_StartedLaggingTime = Time;
  m_LastLagScreenResetTime = Time;
  m_LastLagScreenTime = Time;

  for (auto& user : m_Users) {
    user->SetLagging(true);
    user->SetStartedLaggingTicks(Ticks);
    user->ClearStalePings();
  }
}

pair<int64_t, int64_t> CGame::GetReconnectWaitTicks() const
{
  return make_pair(
    static_cast<int64_t>(m_GProxyEmptyActions + 1) * 60000,
    m_Aura->m_Net.m_Config.m_ReconnectWaitTicks
  );
}

void CGame::ReportRecoverableDisconnect(GameUser::CGameUser* user)
{
  int64_t Time = GetTime(), Ticks = GetTicks();
  if (Time - user->GetLastGProxyWaitNoticeSentTime() < 20) {
    return;
  }

  int64_t timeRemaining = 0;
  pair<int64_t, int64_t> ticksRemaining = GetReconnectWaitTicks();
  if (user->GetGProxyExtended()) {
    timeRemaining = Ticks - user->GetStartedLaggingTicks() - ticksRemaining.second;
  } else {
    timeRemaining = Ticks - user->GetStartedLaggingTicks() - ticksRemaining.first;
  }
  if (timeRemaining <= 0) {
    return;
  }

  SendAllChat(user->GetUID(), "Please wait for me to reconnect (time limit: " + to_string(timeRemaining) + " seconds)");
  user->SetLastGProxyWaitNoticeSentTime(Time);
}

void CGame::OnRecoverableDisconnect(GameUser::CGameUser* user)
{
  user->SudoModeEnd();

  if (!user->GetLagging()) {
    SetLaggingPlayerAndUpdate(user);
  }

  ReportRecoverableDisconnect(user);
}

void CGame::EventUserAfterDisconnect(GameUser::CGameUser* user, bool fromOpen)
{
  if (!m_GameLoading && !m_GameLoaded && !m_CountDownFast) {
    if (!fromOpen) {
      const uint8_t SID = GetSIDFromUID(user->GetUID());
      OpenSlot(SID, true); // kick = true
    }
    user->SetDeleteMe(true);
  } else {
    // Let's avoid sending leave messages during game load.
    // Also, once the game is loaded, ensure all the users' actions will be sent before the leave message is sent.
    Resume(user, user->GetPingEqualizerFrame(), true);
    QueueLeftMessage(user);
  }

  if (m_GameLoading && !user->GetFinishedLoading() && !m_Config.m_LoadInGame) {
    const vector<uint8_t> packet = GameProtocol::SEND_W3GS_GAMELOADED_OTHERS(user->GetUID());
    m_LoadingVirtualBuffer.reserve(m_LoadingVirtualBuffer.size() + packet.size());
    AppendByteArrayFast(m_LoadingVirtualBuffer, packet);
    SendAll(packet);
  }
}

void CGame::EventUserDisconnectTimedOut(GameUser::CGameUser* user)
{
  if (user->GetDisconnected()) return;
  if (user->GetGProxyAny() && m_GameLoaded) {
    if (!user->GetGProxyDisconnectNoticeSent()) {
      user->UnrefConnection();
      user->SetGProxyDisconnectNoticeSent(true);
      if (user->GetGProxyExtended()) {
        SendAllChat(user->GetDisplayName() + " has disconnected, but is using GProxyDLL and may reconnect");
      } else {
        SendAllChat(user->GetDisplayName() + " has disconnected, but is using GProxy++ and may reconnect");
      }
    }
    OnRecoverableDisconnect(user);
    return;
  }

  // not only do we not do any timeouts if the game is lagging, we allow for an additional grace period of 10 seconds
  // this is because Warcraft 3 stops sending packets during the lag screen
  // so when the lag screen finishes we would immediately disconnect everyone if we didn't give them some extra time

  if (GetTime() - m_LastLagScreenTime >= 10) {
    if (!user->HasLeftReason()) {
      user->SetLeftReason("has lost the connection (timed out)");
      user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
    }
    user->CloseConnection(); // automatically sets ended (reconnect already not enabled)
    TrySaveOnDisconnect(user, false);
  }
}

void CGame::EventUserDisconnectSocketError(GameUser::CGameUser* user)
{
  if (user->GetDisconnected()) return;
  if (user->GetGProxyAny() && m_GameLoaded) {
    if (!user->GetGProxyDisconnectNoticeSent()) {
      string errorString = user->GetConnectionErrorString();
      user->UnrefConnection();
      user->SetGProxyDisconnectNoticeSent(true);
      SendAllChat(user->GetDisplayName() + " has disconnected (connection error - " + errorString + ") but is using GProxy++ and may reconnect");
    }

    OnRecoverableDisconnect(user);
    return;
  }

  if (!user->HasLeftReason()) {
    user->SetLeftReason("has lost the connection (connection error - " + user->GetSocket()->GetErrorString() + ")");
    user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  }
  if (user->GetLagging()) {
    StopLagger(user, user->GetLeftReason());
  } else {
    user->CloseConnection(); // automatically sets ended (reconnect already not enabled)
  }
  TrySaveOnDisconnect(user, false);
}

void CGame::EventUserDisconnectConnectionClosed(GameUser::CGameUser* user)
{
  if (user->GetDisconnected()) return;
  if (user->GetGProxyAny() && m_GameLoaded) {
    if (!user->GetGProxyDisconnectNoticeSent()) {
      user->UnrefConnection();
      user->SetGProxyDisconnectNoticeSent(true);
      SendAllChat(user->GetDisplayName() + " has terminated the connection, but is using GProxy++ and may reconnect");
    }

    OnRecoverableDisconnect(user);
    return;
  }

  if (!user->HasLeftReason()) {
    user->SetLeftReason("has terminated the connection");
    user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  }
  if (user->GetLagging()) {
    StopLagger(user, user->GetLeftReason());
  } else {
    user->CloseConnection(); // automatically sets ended (reconnect already not enabled)
  }
  TrySaveOnDisconnect(user, false);
}

void CGame::EventUserDisconnectGameProtocolError(GameUser::CGameUser* user, bool canRecover)
{
  if (user->GetDisconnected()) return;
  if (canRecover && user->GetGProxyAny() && m_GameLoaded) {
    if (!user->GetGProxyDisconnectNoticeSent()) {
      user->UnrefConnection();
      user->SetGProxyDisconnectNoticeSent(true);
      SendAllChat(user->GetDisplayName() + " has disconnected (protocol error) but is using GProxy++ and may reconnect");
    }

    OnRecoverableDisconnect(user);
    return;
  }

  if (!user->HasLeftReason()) {
    if (canRecover) {
      user->SetLeftReason("has lost the connection (protocol error)");
    } else {
      user->SetLeftReason("has lost the connection (unrecoverable protocol error)");
    }
    user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  }
  if (user->GetLagging()) {
    StopLagger(user, user->GetLeftReason());
  } else {
    user->DisableReconnect();
    user->CloseConnection(); // automatically sets ended
  }
  TrySaveOnDisconnect(user, false);
}

void CGame::EventUserDisconnectGameAbuse(GameUser::CGameUser* user)
{
  if (user->GetDisconnected()) return;
  if (!user->HasLeftReason()) {
    user->SetLeftReason("was kicked by anti-abuse");
    user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  }
  user->DisableReconnect();
  user->CloseConnection(); // automatically sets ended
  user->AddKickReason(GameUser::KickReason::ABUSER);
}

void CGame::EventUserKickGProxyExtendedTimeout(GameUser::CGameUser* user)
{
  if (user->GetDeleteMe()) return;
  StopLagger(user, "failed to reconnect in time");
  TrySaveOnDisconnect(user, false);
  ResetDropVotes();
}

void CGame::EventUserKickUnverified(GameUser::CGameUser* user)
{
  if (user->GetDisconnected()) return;
  if (!user->HasLeftReason()) {
    user->SetLeftReason("has been kicked because they are not verified by their realm");
  }
  user->CloseConnection();
  user->AddKickReason(GameUser::KickReason::SPOOFER);
}

void CGame::EventUserKickHandleQueued(GameUser::CGameUser* user)
{
  if (user->GetDisconnected())
    return;

  if (m_CountDownStarted) {
    user->ClearKickByTicks();
    return;
  }

  user->CloseConnection();
  // left reason, left code already assigned when queued
}

void CGame::SendChatMessage(const GameUser::CGameUser* user, const CIncomingChatPlayer* chatPlayer) const
{
  if (m_GameLoading && !m_Config.m_LoadInGame) {
    return;
  }

  // Never allow observers/referees to send private messages to users.
  // Referee rulings/warnings are expected to be public.
  const bool forcePrivateChat = user->GetIsObserver() && (m_GameLoading || m_GameLoaded);
  const bool forceOnlyToObservers = forcePrivateChat && (
    m_Map->GetMapObservers() != MAPOBS_REFEREES || (m_UsesCustomReferees && !user->GetIsPowerObserver())
  );
  const vector<uint8_t>& extraFlags = chatPlayer->GetExtraFlags();
  if (forceOnlyToObservers) {
    vector<uint8_t> overrideObserverUIDs = GetChatObserverUIDs(chatPlayer->GetFromUID());
    vector<uint8_t> overrideExtraFlags = {CHAT_RECV_OBS, 0, 0, 0};
    if (overrideObserverUIDs.empty()) {
      LOG_APP_IF(LOG_LEVEL_INFO, "[Obs/Ref] --nobody listening to [" + user->GetName() + "] --")
    } else {
      Send(overrideObserverUIDs, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromUID(), overrideObserverUIDs, chatPlayer->GetFlag(), overrideExtraFlags, chatPlayer->GetMessage()));
    }
  } else if (forcePrivateChat) {
    if (m_Map->GetMapObservers() == MAPOBS_REFEREES && extraFlags[0] != CHAT_RECV_OBS) {
      if (!m_MuteAll) {
        vector<uint8_t> overrideTargetUIDs = GetChatUIDs(chatPlayer->GetFromUID());
        vector<uint8_t> overrideExtraFlags = {CHAT_RECV_ALL, 0, 0, 0};
        if (!overrideTargetUIDs.empty()) {
          Send(overrideTargetUIDs, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromUID(), overrideTargetUIDs, chatPlayer->GetFlag(), overrideExtraFlags, chatPlayer->GetMessage()));
          if (extraFlags[0] != CHAT_RECV_ALL) {
            LOG_APP_IF(LOG_LEVEL_INFO, "[Obs/Ref] overriden into [All]")
          }
        }
      } else if (extraFlags[0] != CHAT_RECV_ALL) { 
        LOG_APP_IF(LOG_LEVEL_INFO, "[Obs/Ref] overriden into [All], but muteAll is active (message from [" + user->GetName() + "] discarded)")
      }
    } else {
      // enforce observer-only chat, just in case rogue clients are doing funny things
      vector<uint8_t> overrideTargetUIDs = GetChatObserverUIDs(chatPlayer->GetFromUID());
      vector<uint8_t> overrideExtraFlags = {CHAT_RECV_OBS, 0, 0, 0};
      if (!overrideTargetUIDs.empty()) {
        Send(overrideTargetUIDs, GameProtocol::SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromUID(), overrideTargetUIDs, chatPlayer->GetFlag(), overrideExtraFlags, chatPlayer->GetMessage()));
        if (extraFlags[0] != CHAT_RECV_OBS) {
          LOG_APP_IF(LOG_LEVEL_INFO, "[Obs/Ref] enforced server-side")
        }
      }
    }
  } else {
    Send(chatPlayer->GetToUIDs(), GameProtocol::SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromUID(), chatPlayer->GetToUIDs(), chatPlayer->GetFlag(), chatPlayer->GetExtraFlags(), chatPlayer->GetMessage()));
  }
}

void CGame::QueueLeftMessage(GameUser::CGameUser* user) const
{
  CQueuedActionsFrame& frame = user->GetPingEqualizerFrame();
  frame.leavers.push_back(user);
  user->TrySetEnding();
  DLOG_APP_IF(LOG_LEVEL_TRACE, "[" + user->GetName() + "] scheduled for deletion in " + ToDecString(user->GetPingEqualizerOffset()) + " frames")
}

void CGame::SendLeftMessage(GameUser::CGameUser* user, const bool sendChat) const
{
  // This function, together with GetLeftMessage and SetLeftMessageSent,
  // controls which UIDs Aura considers available.
  if (sendChat) {
    if (!user->GetIsLeaver()) {
      SendAllChat(user->GetExtendedName() + " " + user->GetLeftReason() + ".");
    } else if (user->GetRealm(false)) {
      // Note: Not necessarily spoof-checked
      SendAllChat(user->GetUID(), user->GetLeftReason() + " [" + user->GetExtendedName() + "].");
    } else {
      SendAllChat(user->GetUID(), user->GetLeftReason());
    }
  }
  SendAll(GameProtocol::SEND_W3GS_PLAYERLEAVE_OTHERS(user->GetUID(), GetIsLobbyStrict() ? PLAYERLEAVE_LOBBY : user->GetLeftCode()));
  user->SetLeftMessageSent(true);
  user->SetStatus(USERSTATUS_ENDED);
}

bool CGame::SendEveryoneElseLeftAndDisconnect(const string& reason) const
{
  bool anyStopped = false;
  for (auto& p1 : m_Users) {
    for (auto& p2 : m_Users) {
      if (p1 == p2 || p2->GetLeftMessageSent()) {
        continue;
      }
      Send(p1, GameProtocol::SEND_W3GS_PLAYERLEAVE_OTHERS(p2->GetUID(), PLAYERLEAVE_DISCONNECT));
    }
    for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
      Send(p1, fakeUser.GetGameQuitBytes(PLAYERLEAVE_DISCONNECT));
    }
    p1->DisableReconnect();
    p1->SetLagging(false);
    if (!p1->HasLeftReason()) {
      p1->SetLeftReason(reason);
      p1->SetLeftCode(PLAYERLEAVE_DISCONNECT);
    }
    p1->SetLeftMessageSent(true);
    if (p1->GetGProxyAny()) {
      // Let GProxy know that it should give up at reconnecting.
      Send(p1, GameProtocol::SEND_W3GS_PLAYERLEAVE_OTHERS(p1->GetUID(), PLAYERLEAVE_DISCONNECT));
    }
    p1->CloseConnection();
    p1->SetStatus(USERSTATUS_ENDED);
    if (!p1->GetDisconnected()) {
      anyStopped = true;
    }
  }
  return anyStopped;
}

bool CGame::GetIsHiddenPlayerNames() const
{
  return m_IsHiddenPlayerNames;
}

void CGame::ShowPlayerNamesGameStartLoading() {
  if (!m_IsHiddenPlayerNames) return;

  m_IsHiddenPlayerNames = false;

  for (auto& p1 : m_Users) {
    for (auto& p2 : m_Users) {
      if (p1 == p2 || p2->GetLeftMessageSent()) {
        continue;
      }
      Send(p1, GameProtocol::SEND_W3GS_PLAYERLEAVE_OTHERS(p2->GetUID(), PLAYERLEAVE_LOBBY));
      Send(p1, GameProtocol::SEND_W3GS_PLAYERINFO_EXCLUDE_IP(p2->GetUID(), p2->GetDisplayName()/*, user->GetIPv4(), user->GetIPv4Internal()*/));
    }
  }
}

void CGame::ShowPlayerNamesInGame() {
  m_IsHiddenPlayerNames = false;
}

void CGame::EventUserCheckStatus(GameUser::CGameUser* user)
{
  if (user->GetDisconnected())
    return;

  if (m_CountDownStarted) {
    user->SetStatusMessageSent(true);
    return;
  }

  bool hideNames = m_IsHiddenPlayerNames || m_Config.m_HideInGameNames == HIDE_IGN_ALWAYS || m_Config.m_HideInGameNames == HIDE_IGN_HOST;
  if (m_Config.m_HideInGameNames == HIDE_IGN_AUTO && m_Map->GetMapNumControllers() >= 3) {
    hideNames = true;
  }

  bool IsOwnerName = MatchOwnerName(user->GetName());
  string OwnerFragment;
  if (user->GetIsOwner(nullopt)) {
    OwnerFragment = " (game owner)";
  } else if (IsOwnerName) {
    OwnerFragment = " (unverified game owner, send me a whisper: \"sc\")";
  }

  string GProxyFragment;
  if (m_Aura->m_Net.m_Config.m_AnnounceGProxy && GetIsProxyReconnectable() && !hideNames) {
    if (user->GetGProxyExtended()) {
      GProxyFragment = " is using GProxyDLL, a Warcraft III plugin to protect against disconnections. See: <" + m_Aura->m_Net.m_Config.m_AnnounceGProxySite + ">";
    } else if (user->GetGProxyAny()) {
      if (GetIsProxyReconnectableLong()) {
        GProxyFragment = " is using an outdated GProxy++. Please upgrade to GProxyDLL at: <" + m_Aura->m_Net.m_Config.m_AnnounceGProxySite + ">";
      } else {
        GProxyFragment = " is using GProxy, a Warcraft III plugin to protect against disconnections. See: <" + m_Aura->m_Net.m_Config.m_AnnounceGProxySite + ">";
      }
    }
  }
  
  user->SetStatusMessageSent(true);
  if (OwnerFragment.empty() && GProxyFragment.empty()) {
    if (m_Aura->m_Net.m_Config.m_AnnounceIPv6 && user->GetUsingIPv6() && !hideNames) {
      SendAllChat(user->GetDisplayName() + " joined the game over IPv6.");
    }
    return;
  }

  if (hideNames) {
    if (m_IsHiddenPlayerNames) {
      SendChat(user, "[" + user->GetName() + "]" + OwnerFragment + " joined the game as [" + user->GetDisplayName() + "]");
    } else {
      SendChat(user, "[" + user->GetName() + "]" + OwnerFragment + " joined the game.");
    }
    return;
  }

  string IPv6Fragment;
  if (user->GetUsingIPv6() && !hideNames) {
    IPv6Fragment = ". (Joined over IPv6).";
  }
  if (!OwnerFragment.empty() && !GProxyFragment.empty()) {
    SendAllChat(user->GetDisplayName() + OwnerFragment + GProxyFragment + IPv6Fragment);
  } else if (!OwnerFragment.empty()) {
    if (user->GetUsingIPv6()) {
      SendAllChat(user->GetDisplayName() + OwnerFragment + " joined the game over IPv6.");
    } else {
      SendAllChat(user->GetDisplayName() + OwnerFragment + " joined the game.");
    }
  } else {
    SendAllChat(user->GetDisplayName() + GProxyFragment + IPv6Fragment);
  }
}

GameUser::CGameUser* CGame::JoinPlayer(CConnection* connection, CIncomingJoinRequest* joinRequest, const uint8_t SID, const uint8_t UID, const uint8_t HostCounterID, const string JoinedRealm, const bool IsReserved, const bool IsUnverifiedAdmin)
{
  // If realms are reloaded, HostCounter may change.
  // However, internal realm IDs maps to constant realm input IDs.
  // Hence, CGamePlayers are created with references to internal realm IDs.
  uint32_t internalRealmId = HostCounterID;
  CRealm* matchingRealm = nullptr;
  if (HostCounterID >= 0x10) {
    matchingRealm = m_Aura->GetRealmByHostCounter(HostCounterID);
    if (matchingRealm) internalRealmId = matchingRealm->GetInternalID();
  }

  GameUser::CGameUser* Player = new GameUser::CGameUser(this, connection, UID == 0xFF ? GetNewUID() : UID, internalRealmId, JoinedRealm, joinRequest->GetName(), joinRequest->GetIPv4Internal(), IsReserved);
  // Now, socket belongs to GameUser::CGameUser. Don't look for it in CConnection.

  m_Users.push_back(Player);
  connection->SetSocket(nullptr);
  connection->SetDeleteMe(true);

  if (matchingRealm) {
    Player->SetWhoisShouldBeSent(
      IsUnverifiedAdmin || MatchOwnerName(Player->GetName()) || !HasOwnerSet() ||
      matchingRealm->GetIsFloodImmune() || matchingRealm->GetHasEnhancedAntiSpoof()
    );
  }

  if (GetIsCustomForces()) {
    m_Slots[SID] = CGameSlot(m_Slots[SID].GetType(), Player->GetUID(), SLOTPROG_RST, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColor(), m_Map->GetLobbyRace(&m_Slots[SID]));
  } else {
    m_Slots[SID] = CGameSlot(m_Slots[SID].GetType(), Player->GetUID(), SLOTPROG_RST, SLOTSTATUS_OCCUPIED, 0, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), m_Map->GetLobbyRace(&m_Slots[SID]));
    SetSlotTeamAndColorAuto(SID);
  }
  Player->SetObserver(m_Slots[SID].GetTeam() == m_Map->GetVersionMaxSlots());

  // send slot info to the new user
  // the SLOTINFOJOIN packet also tells the client their assigned UID and that the join was successful.

  Player->Send(GameProtocol::SEND_W3GS_SLOTINFOJOIN(Player->GetUID(), Player->GetSocket()->GetPortLE(), Player->GetIPv4(), m_Slots, m_RandomSeed, GetLayout(), m_Map->GetMapNumControllers()));

  SendIncomingPlayerInfo(Player);

  // send virtual host info and fake users info (if present) to the new user.

  SendVirtualHostPlayerInfo(Player);
  SendFakeUsersInfo(Player);
  SendJoinedPlayersInfo(Player);

  // send a map check packet to the new user.

  if (m_Aura->m_GameVersion >= 23) {
    Player->Send(GameProtocol::SEND_W3GS_MAPCHECK(m_MapPath, m_Map->GetMapSize(), m_Map->GetMapCRC32(), m_Map->GetMapScriptsWeakHash(), (!(m_Aura->m_GameVersion > 30) ? m_Map->GetMapScriptsSHA1() : m_Map->GetMapScriptsHash()));
  } else {
    Player->Send(GameProtocol::SEND_W3GS_MAPCHECK(m_MapPath, m_Map->GetMapSize(), m_Map->GetMapCRC32(), m_Map->GetMapScriptsWeakHash()));
  }

  // send slot info to everyone, so the new user gets this info twice but everyone else still needs to know the new slot layout.
  SendAllSlotInfo();
  UpdateReadyCounters();

  if (GetIPFloodHandler() == ON_IPFLOOD_NOTIFY) {
    CheckIPFlood(joinRequest->GetName(), &(Player->GetSocket()->m_RemoteHost));
  }

  // send a welcome message

  if (!m_RestoredGame) {
    SendWelcomeMessage(Player);
  }

  for (const auto& otherPlayer :  m_Users) {
    if (otherPlayer == Player || otherPlayer->GetLeftMessageSent()) {
      continue;
    }
    if (otherPlayer->GetHasPinnedMessage()) {
      SendChat(otherPlayer->GetUID(), Player, otherPlayer->GetPinnedMessage(), LOG_LEVEL_DEBUG);
    }
  }

  AddProvisionalBannableUser(Player);

  string notifyString = "";
  if (m_Config.m_NotifyJoins && m_Config.m_IgnoredNotifyJoinPlayers.find(joinRequest->GetName()) == m_Config.m_IgnoredNotifyJoinPlayers.end()) {
    notifyString = "\x07";
  }

  if (notifyString.empty()) {
    LOG_APP_IF(LOG_LEVEL_INFO, "user joined (P" + to_string(SID + 1) + "): [" + joinRequest->GetName() + "@" + Player->GetRealmHostName() + "#" + to_string(Player->GetUID()) + "] from [" + Player->GetIPString() + "] (" + Player->GetSocket()->GetName() + ")" + notifyString)
  } else {
    LOG_APP_IF(LOG_LEVEL_NOTICE, "user joined (P" + to_string(SID + 1) + "): [" + joinRequest->GetName() + "@" + Player->GetRealmHostName() + "#" + to_string(Player->GetUID()) + "] from [" + Player->GetIPString() + "] (" + Player->GetSocket()->GetName() + ")" + notifyString)
  }
  if (joinRequest->GetIsCensored()) {
    LOG_APP_IF(LOG_LEVEL_NOTICE, "user [" + joinRequest->GetName() + "] is censored name - was [" + joinRequest->GetOriginalName() + "]")
  }
  return Player;
}

bool CGame::CheckIPFlood(const string joinName, const sockaddr_storage* sourceAddress) const
{
  // check for multiple IP usage
  UserList usersSameIP;
  for (auto& otherPlayer : m_Users) {
    if (joinName == otherPlayer->GetName()) {
      continue;
    }
    // In a lobby, all users are always connected, but
    // this is still a safety measure in case we reuse this method for GProxy or whatever.
    if (GetSameAddresses(sourceAddress, &(otherPlayer->GetSocket()->m_RemoteHost))) {
      usersSameIP.push_back(otherPlayer);
    }
  }

  if (usersSameIP.empty()) {
    return true;
  }

  uint8_t maxPlayersFromSameIp = isLoopbackAddress(sourceAddress) ? m_Config.m_MaxPlayersLoopback : m_Config.m_MaxPlayersSameIP;
  if (static_cast<uint8_t>(usersSameIP.size()) >= maxPlayersFromSameIp) {
    if (GetIPFloodHandler() == ON_IPFLOOD_NOTIFY) {
      SendAllChat("Player [" + joinName + "] has the same IP address as: " + ToNameListSentence(usersSameIP));
    }
    return false;
  }
  return true;
}

bool CGame::EventRequestJoin(CConnection* connection, CIncomingJoinRequest* joinRequest)
{
  if (!GetIsStageAcceptingJoins()) {
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_STARTED));
    return false;
  }
  if (joinRequest->GetName().empty() || joinRequest->GetName().size() > 15) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetOriginalName() + "] invalid name - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }
  if (joinRequest->GetIsCensored() && m_Config.m_UnsafeNameHandler == ON_UNSAFE_NAME_DENY) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetOriginalName() + "] unsafe name - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // identify their joined realm
  // this is only possible because when we send a game refresh via LAN or battle.net we encode an ID value in the 4 most significant bits of the host counter
  // the client sends the host counter when it joins so we can extract the ID value here
  // note: this is not a replacement for spoof checking since it doesn't verify the user's name and it can be spoofed anyway

  string JoinedRealm;
  uint8_t HostCounterID = joinRequest->GetHostCounter() >> 24;
  bool IsUnverifiedAdmin = false;

  CRealm* matchingRealm = nullptr;
  if (HostCounterID >= 0x10) {
    matchingRealm = m_Aura->GetRealmByHostCounter(HostCounterID);
    if (matchingRealm) {
      JoinedRealm = matchingRealm->GetServer();
      IsUnverifiedAdmin = matchingRealm->GetIsModerator(joinRequest->GetName()) || matchingRealm->GetIsAdmin(joinRequest->GetName());
    } else {
      // Trying to join from an unknown realm.
      HostCounterID = 0xF;
    }
  }

  if (HostCounterID < 0x10 && joinRequest->GetEntryKey() != m_EntryKey) {
    // check if the user joining via LAN knows the entry key
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "@" + JoinedRealm + "] used a wrong LAN key (" + to_string(joinRequest->GetEntryKey()) + ") - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
    return false;
  }

  // Odd host counters are information requests
  if (HostCounterID & 0x1) {
    EventBeforeJoin(connection);
    connection->Send(GameProtocol::SEND_W3GS_SLOTINFOJOIN(GetNewUID(), connection->GetSocket()->GetPortLE(), connection->GetIPv4(), m_Slots, m_RandomSeed, GetLayout(), m_Map->GetMapNumControllers()));
    SendVirtualHostPlayerInfo(connection);
    SendFakeUsersInfo(connection);
    SendJoinedPlayersInfo(connection);
    return false;
  }

  if (HostCounterID < 0x10 && HostCounterID != 0) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "@" + JoinedRealm + "] is trying to join over reserved realm " + to_string(HostCounterID) + " - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    if (HostCounterID > 0x2) {
      connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
      return false;
    }
  }

  if (GetUserFromName(joinRequest->GetName(), false)/* && !m_IsHiddenPlayerNames*/) {
    if (m_ReportedJoinFailNames.find(joinRequest->GetName()) == end(m_ReportedJoinFailNames)) {
      if (!m_IsHiddenPlayerNames) {
        // FIXME: Someone can probably figure out whether a given player has joined a lobby by trying to impersonate them, and failing to.
        // An alternative would be no longer preventing joins and, potentially, disambiguating their names at CGame::ShowPlayerNamesGameStartLoading.
        SendAllChat("Entry denied for another user with the same name: [" + joinRequest->GetName() + "@" + JoinedRealm + "]");
      }
      m_ReportedJoinFailNames.insert(joinRequest->GetName());
    }
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "] invalid name (taken) - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (joinRequest->GetName() == GetLobbyVirtualHostName()) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "] spoofer (matches host name) - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (joinRequest->GetName().length() >= 7 && joinRequest->GetName().substr(0, 5) == "User[") {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "] spoofer (matches fake users) - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (GetHMCEnabled() && joinRequest->GetName() == m_Map->GetHMCPlayerName()) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "] spoofer (matches HMC name) - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (joinRequest->GetName() == m_OwnerName && !m_OwnerRealm.empty() && !JoinedRealm.empty() && m_OwnerRealm != JoinedRealm) {
    // Prevent owner homonyms from other realms from joining. This doesn't affect LAN.
    // But LAN has its own rules, e.g. a LAN owner that leaves the game is immediately demoted.
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "@" + JoinedRealm + "] spoofer (matches owner name, but realm mismatch, expected " + m_OwnerRealm + ") - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  if (CheckScopeBanned(joinRequest->GetName(), JoinedRealm, connection->GetIPStringStrict()) ||
    CheckUserBanned(connection, joinRequest, matchingRealm, JoinedRealm) ||
    CheckIPBanned(connection, joinRequest, matchingRealm, JoinedRealm)) {
    // let banned users "join" the game with an arbitrary UID then immediately close the connection
    // this causes them to be kicked back to the chat channel on battle.net
    vector<CGameSlot> Slots = m_Map->GetSlots();
    connection->Send(GameProtocol::SEND_W3GS_SLOTINFOJOIN(1, connection->GetSocket()->GetPortLE(), connection->GetIPv4(), Slots, 0, GetLayout(), m_Map->GetMapNumControllers()));
    return false;
  }

  matchingRealm = nullptr;

  const uint8_t reservedIndex = GetReservedIndex(joinRequest->GetName());
  const bool isReserved = reservedIndex < m_Reserved.size() || (!m_RestoredGame && MatchOwnerName(joinRequest->GetName()) && JoinedRealm == m_OwnerRealm);

  if (m_CheckReservation && !isReserved) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "] missing reservation - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")")
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  if (!GetAllowsIPFlood()) {
    if (!CheckIPFlood(joinRequest->GetName(), &(connection->GetSocket()->m_RemoteHost))) {
      LOG_APP_IF(LOG_LEVEL_WARNING, "ipflood rejected from " + AddressToStringStrict(connection->GetSocket()->m_RemoteHost))
      connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
      return false;
    }
  }

  uint8_t SID = 0xFF;
  uint8_t UID = 0xFF;

  if (m_RestoredGame) {
    uint8_t matchCounter = 0xFF;
    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (!m_RestoredGame->GetSlots()[i].GetIsPlayerOrFake()) {
        continue;
      }
      if (++matchCounter == reservedIndex) {
        SID = i;
        UID = m_RestoredGame->GetSlots()[i].GetUID();
        break;
      }
    }
  } else {
    SID = GetEmptySID(false);

    if (SID == 0xFF && isReserved) {
      // a reserved user is trying to join the game but it's full, try to find a reserved slot

      SID = GetEmptySID(true);
      if (SID != 0xFF) {
        GameUser::CGameUser* kickedPlayer = GetUserFromSID(SID);

        if (kickedPlayer) {
          if (!kickedPlayer->HasLeftReason()) {
            if (m_IsHiddenPlayerNames) {
              kickedPlayer->SetLeftReason("was kicked to make room for a reserved user");
            } else {
              kickedPlayer->SetLeftReason("was kicked to make room for a reserved user [" + joinRequest->GetName() + "]");
            }
          }
          kickedPlayer->CloseConnection();

          // Ensure the userleave message is sent before the reserved userjoin message.
          SendLeftMessage(kickedPlayer, true);
        }
      }
    }

    if (SID == 0xFF && MatchOwnerName(joinRequest->GetName()) && JoinedRealm == m_OwnerRealm) {
      // the owner is trying to join the game but it's full and we couldn't even find a reserved slot, kick the user in the lowest numbered slot
      // updated this to try to find a user slot so that we don't end up kicking a computer

      SID = 0;

      for (uint8_t i = 0; i < m_Slots.size(); ++i) {
        if (m_Slots[i].GetIsPlayerOrFake()) {
          SID = i;
          break;
        }
      }

      GameUser::CGameUser* kickedPlayer = GetUserFromSID(SID);

      if (kickedPlayer) {
        if (!kickedPlayer->HasLeftReason()) {
          if (m_IsHiddenPlayerNames) {
            kickedPlayer->SetLeftReason("was kicked to make room for the owner");
          } else {
            kickedPlayer->SetLeftReason("was kicked to make room for the owner [" + joinRequest->GetName() + "]");
          }
        }
        kickedPlayer->CloseConnection();
        // Ensure the userleave message is sent before the game owner' userjoin message.
        SendLeftMessage(kickedPlayer, true);
      }
    }
  }

  if (SID >= static_cast<uint8_t>(m_Slots.size())) {
    connection->Send(GameProtocol::SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // we have a slot for the new user
  // make room for them by deleting the virtual host user if we have to

  if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && GetSlotsOpen() == 1 && GetNumJoinedUsersOrFake() > 1)
    DeleteVirtualHost();

  EventBeforeJoin(connection);
  JoinPlayer(connection, joinRequest, SID, UID, HostCounterID, JoinedRealm, isReserved, IsUnverifiedAdmin);
  return true;
}

void CGame::EventBeforeJoin(CConnection* connection)
{
  if (connection->GetIsUDPTunnel()) {
    vector<uint8_t> packet = {GPSProtocol::Magic::GPS_HEADER, GPSProtocol::Magic::UDPFIN, 4, 0};
    connection->Send(packet);
  }
}

bool CGame::CheckUserBanned(CConnection* connection, CIncomingJoinRequest* joinRequest, CRealm* matchingRealm, string& hostName)
{
  // check if the new user's name is banned
  bool isSelfServerBanned = matchingRealm && matchingRealm->IsBannedPlayer(joinRequest->GetName(), hostName);
  bool isBanned = isSelfServerBanned;
  if (!isBanned && m_CreatedFromType == SERVICE_TYPE_REALM && matchingRealm != reinterpret_cast<const CRealm*>(m_CreatedFrom)) {
    isBanned = reinterpret_cast<const CRealm*>(m_CreatedFrom)->IsBannedPlayer(joinRequest->GetName(), hostName);
  }
  if (!isBanned && m_CreatedFromType != SERVICE_TYPE_REALM) {
    isBanned = m_Aura->m_DB->GetIsUserBanned(joinRequest->GetName(), hostName, string());
  }
  if (isBanned) {
    string scopeFragment;
    if (isSelfServerBanned) {
      scopeFragment = "in its own realm";
    } else {
      scopeFragment = "in creator's realm";
    }

    // don't allow the user to spam the chat by attempting to join the game multiple times in a row
    if (m_ReportedJoinFailNames.find(joinRequest->GetName()) == end(m_ReportedJoinFailNames)) {
      LOG_APP_IF(LOG_LEVEL_INFO, "user [" + joinRequest->GetName() + "@" + hostName + "|" + connection->GetIPString() + "] entry denied - banned " + scopeFragment)
      if (!m_IsHiddenPlayerNames) {
        SendAllChat("[" + joinRequest->GetName() + "@" + hostName + "] is trying to join the game, but is banned");
      }
      m_ReportedJoinFailNames.insert(joinRequest->GetName());
    } else {
      LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "@" + hostName + "|" + connection->GetIPString() + "] entry denied - banned " + scopeFragment)
    }
  }
  return isBanned;
}

bool CGame::CheckIPBanned(CConnection* connection, CIncomingJoinRequest* joinRequest, CRealm* matchingRealm, string& hostName)
{
  if (isLoopbackAddress(connection->GetRemoteAddress())) {
    return false;
  }
  // check if the new user's IP is banned
  bool isSelfServerBanned = matchingRealm && matchingRealm->IsBannedIP(connection->GetIPStringStrict());
  bool isBanned = isSelfServerBanned;
  if (!isBanned && m_CreatedFromType == SERVICE_TYPE_REALM && matchingRealm != reinterpret_cast<const CRealm*>(m_CreatedFrom)) {
    isBanned = reinterpret_cast<const CRealm*>(m_CreatedFrom)->IsBannedIP(connection->GetIPStringStrict());
  }
  if (!isBanned && m_CreatedFromType != SERVICE_TYPE_REALM) {
    isBanned = m_Aura->m_DB->GetIsIPBanned(connection->GetIPStringStrict(), string());
  }
  if (isBanned) {
    string scopeFragment;
    if (isSelfServerBanned) {
      scopeFragment = "in its own realm";
    } else {
      scopeFragment = "in creator's realm";
    }

    // don't allow the user to spam the chat by attempting to join the game multiple times in a row
    if (m_ReportedJoinFailNames.find(joinRequest->GetName()) == end(m_ReportedJoinFailNames)) {
      LOG_APP_IF(LOG_LEVEL_INFO, "user [" + joinRequest->GetName() + "@" + hostName + "|" + connection->GetIPString() + "] entry denied - IP-banned " + scopeFragment)
      if (!m_IsHiddenPlayerNames) {
        SendAllChat("[" + joinRequest->GetName() + "@" + hostName + "] is trying to join the game, but is IP-banned");
      }
      m_ReportedJoinFailNames.insert(joinRequest->GetName());
    } else {
      LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + joinRequest->GetName() + "@" + hostName + "|" + connection->GetIPString() + "] entry denied - IP-banned " + scopeFragment)
    }
  }
  return isBanned;
}

bool CGame::EventUserLeft(GameUser::CGameUser* user, const uint32_t clientReason)
{
  if (user->GetDisconnected()) return false;
  if (m_GameLoading || m_GameLoaded || clientReason == PLAYERLEAVE_GPROXY) {
    LOG_APP_IF(LOG_LEVEL_INFO, "user [" + user->GetName() + "] left the game (" + GameProtocol::LeftCodeToString(clientReason) + ")");
  }

  // this function is only called when a client leave packet is received, not when there's a socket error or kick
  // however, clients not only send the leave packet by a user clicking on Quit Game
  // clients also will send a leave packet if the server sends unexpected data

  if (clientReason == PLAYERLEAVE_GPROXY && (user->GetGProxyAny() || GetIsLobbyStrict() /* in case GProxy handshake could not be completed*/)) {
    user->SetLeftReason("Game client disconnected automatically");
    user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  } else {
    if (!user->HasLeftReason()) {
      user->SetLeftReason("Leaving the game voluntarily");
      user->SetLeftCode(PLAYERLEAVE_LOST);
    } else {
      user->SetLeftReason("left (" + user->GetLeftReason() + ")");
    }
    user->SetIsLeaver(true);
  }
  if (user->GetLagging()) {
    StopLagger(user, user->GetLeftReason());
  } else {
    user->DisableReconnect();
    user->CloseConnection();
  }
  TrySaveOnDisconnect(user, true);
  return true;
}

void CGame::EventUserLoaded(GameUser::CGameUser* user)
{
  string role = user->GetIsObserver() ? "observer" : "player";
  LOG_APP_IF(LOG_LEVEL_DEBUG, role + " [" + user->GetName() + "] finished loading in " + ToFormattedString(static_cast<double>(user->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds")

  // Update stats
  const CGameSlot* slot = InspectSlot(GetSIDFromUID(user->GetUID()));
  CDBGamePlayer* dbPlayer = GetDBPlayerFromColor(slot->GetColor());
  if (dbPlayer) {
    dbPlayer->SetLoadingTime(user->GetFinishedLoadingTicks() - m_StartedLoadingTicks);
  }

  if (!m_Config.m_LoadInGame) {
    vector<uint8_t> packet = GameProtocol::SEND_W3GS_GAMELOADED_OTHERS(user->GetUID());
    if (m_BufferingEnabled & BUFFERING_ENABLED_LOADING) {
      AppendByteArrayFast(m_LoadingRealBuffer, packet);
    }
    SendAll(packet);
  } else { // load-in-game
    Send(user, m_LoadingRealBuffer);
    if (!m_LoadingVirtualBuffer.empty()) {
      // CGame::EventUserLoaded - Fake users loaded
      Send(user, m_LoadingVirtualBuffer);
    }
    // GProxy sends m_GProxyEmptyActions additional empty actions for every action received.
    // So we need to match it, to avoid desyncs.
    if (user->GetGProxyAny()) {
      Send(user, GameProtocol::SEND_W3GS_EMPTY_ACTIONS(m_BeforePlayingEmptyActions));
    } else {
      Send(user, GameProtocol::SEND_W3GS_EMPTY_ACTIONS(m_BeforePlayingEmptyActions * (1 + m_GProxyEmptyActions)));
    }

    // Warcraft III doesn't respond to empty actions,
    // so we need to artificially increase users' sync counters.
    /*
    user->AddSyncCounterOffset(1);
    */

    user->SetLagging(false);
    user->SetStartedLaggingTicks(0);
    RemoveFromLagScreens(user);
    user->SetStatus(USERSTATUS_PLAYING);
    UserList laggingPlayers = GetLaggingUsers();
    if (laggingPlayers.empty()) {
      m_Lagging = false;
    }
    if (m_Lagging) {
      LOG_APP_IF(LOG_LEVEL_INFO, "@[" + user->GetName() + "] lagger update (+" + ToNameListSentence(laggingPlayers) + ")")
      Send(user, GameProtocol::SEND_W3GS_START_LAG(laggingPlayers));
      LogApp("[LoadInGame] Waiting for " + to_string(laggingPlayers.size()) + " other players to load the game...");

      if (laggingPlayers.size() >= 3) {
        SendChat(user, "[" + user->GetName() + "], please wait for " + to_string(laggingPlayers.size()) + " players to load the game...");
      } else {
        SendChat(user, "[" + user->GetName() + "], please wait for " + ToNameListSentence(laggingPlayers) + " to load the game...");
      }
    }
  }
}

bool CGame::EventUserAction(GameUser::CGameUser* user, CIncomingAction& action)
{
  if (!m_GameLoading && !m_GameLoaded) {
    return false;
  }

  if (action.GetLength() > 1027) {
    return false;
  }

  const uint8_t actionType = action.GetSniffedType();
  CQueuedActionsFrame& actionFrame = user->GetPingEqualizerFrame();

  if (!action.GetImmutableAction().empty()) {
    DLOG_APP_IF(LOG_LEVEL_TRACE2, "[" + user->GetName() + "] offset +" + ToDecString(user->GetPingEqualizerOffset()) + " | action 0x" + ToHexString(static_cast<uint32_t>((action.GetImmutableAction())[0])) + ": [" + ByteArrayToHexString((action.GetImmutableAction())) + "]")
  }

  if (actionType == ACTION_CHAT_TRIGGER && (m_Config.m_LogCommands || m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG))) {
    const vector<uint8_t>& actionBytes = action.GetImmutableAction();
    if (actionBytes.size() >= 10) {
      const uint8_t* chatMessageStart = actionBytes.data() + 9;
      const uint8_t* chatMessageEnd = actionBytes.data() + FindNullDelimiterOrStart(actionBytes, 9);
      if (chatMessageStart < chatMessageEnd) {
        const string chatMessage = GetStringAddressRange(chatMessageStart, chatMessageEnd);
        if (m_Config.m_LogCommands) {
          m_Aura->LogPersistent(GetLogPrefix() + "[CMD] ["+ user->GetExtendedName() + "] " + chatMessage);
        }
        // Enable --log-level debug to figure out HMC map-specific constants
        LOG_APP_IF(LOG_LEVEL_DEBUG, "Message by [" + user->GetName() + "]: <<" + chatMessage + ">> triggered: [" + to_string(ByteArrayToUInt32(actionBytes, false, 1)) + " | " + to_string(ByteArrayToUInt32(actionBytes, false, 5)) + "]")
      }
    }
  }

  if (m_CustomStats && action.GetImmutableAction().size() >= 6) {
    if (!m_CustomStats->RecvAction(user->GetUID(), action)) {
      delete m_CustomStats;
      m_CustomStats = nullptr;
    }
  }
  if (m_DotaStats && action.GetImmutableAction().size() >= 6) {
    if (m_DotaStats->ProcessAction(user->GetUID(), action) && !GetIsGameOver()) {
      LOG_APP_IF(LOG_LEVEL_INFO, "gameover timer started (dota stats class reported game over)")
      StartGameOverTimer(true);
    }
  }

  actionFrame.AddAction(std::move(action));

  switch (actionType) {
    case ACTION_SAVE:
      LOG_APP_IF(LOG_LEVEL_INFO, "[" + user->GetName() + "] is saving the game")
      SendAllChat("[" + user->GetDisplayName() + "] is saving the game");
      SaveEnded(0xFF, actionFrame);
      if (user->GetCanSave()) {
        user->DropRemainingSaves();
        if (user->GetIsNativeReferee() && !user->GetCanSave()) {
          SendChat(user, "NOTE: You have reached the maximum allowed saves for this game.");
        }
      } else {
        // Game engine lets referees save without limit nor throttle whatsoever.
        // This path prevents save-spamming leading to unplayable games.
        EventUserDisconnectGameAbuse(user);
      }
      break;
    case ACTION_SAVE_ENDED:
      LOG_APP_IF(LOG_LEVEL_INFO, "[" + user->GetName() + "] finished saving the game")
      break;
    case ACTION_PAUSE:
      LOG_APP_IF(LOG_LEVEL_INFO, "[" + user->GetName() + "] paused the game")
      if (!user->GetIsNativeReferee()) {
        user->DropRemainingPauses();
      }
      if (actionFrame.callback != ON_SEND_ACTIONS_PAUSE) {
        actionFrame.callback = ON_SEND_ACTIONS_PAUSE;
        actionFrame.pauseUID = user->GetUID();
      }
      break;
    case ACTION_RESUME:
      if (m_PauseUser) {
        LOG_APP_IF(LOG_LEVEL_INFO, "[" + user->GetName() + "] resumed the game (was paused by [" + m_PauseUser->GetName() + "])")
      } else {
        LOG_APP_IF(LOG_LEVEL_INFO, "[" + user->GetName() + "] resumed the game")
      }
      actionFrame.callback = ON_SEND_ACTIONS_RESUME;
      break;
    case ACTION_CHAT_TRIGGER: {
      // Already logged. Do not extract action here, since it has already been moved.
      break;
    }
    case ACTION_SYNC_INT: {
      // This is the W3MMD action type.
      // FIXME: more than one action may be sent in a single packet, but the length of each action isn't explicitly represented in the packet
      // so we ought to parse all the actions and calculate their lengths based on their types
      break;
    }
    default:
      break;
  }

  return true;
}

void CGame::EventUserKeepAlive(GameUser::CGameUser* user)
{
  if (!m_GameLoading && !m_GameLoaded) {
    return;
  }

  bool canConsumeFrame = true;
  UserList& otherPlayers = m_SyncPlayers[user];

  if (!otherPlayers.empty() && m_SyncCounter < SYNCHRONIZATION_CHECK_MIN_FRAMES) {
    // Add a grace period in order for any desync warnings to be displayed in chat (rather than just in chat logs!)
    return;
  }

  for (auto& otherPlayer: otherPlayers) {
    if (otherPlayer == user) {
      canConsumeFrame = false;;
      break;
    }

    if (!otherPlayer->HasCheckSums()) {
      canConsumeFrame = false;
      break;
    }
  }

  if (!canConsumeFrame) {
    return;
  }

  const uint32_t MyCheckSum = user->GetCheckSums()->front();
  user->GetCheckSums()->pop();
  ++m_SyncCounterChecked;

  bool DesyncDetected = false;
  UserList DesyncedPlayers;
  UserList::iterator it = otherPlayers.begin();
  while (it != otherPlayers.end()) {
    if ((*it)->GetCheckSums()->front() == MyCheckSum) {
      (*it)->GetCheckSums()->pop();
      ++it;
    } else {
      DesyncDetected = true;
      UserList& BackList = m_SyncPlayers[*it];
      auto BackIterator = std::find(BackList.begin(), BackList.end(), user);
      if (BackIterator == BackList.end()) {
      } else {
        *BackIterator = std::move(BackList.back());
        BackList.pop_back();
      }

      DesyncedPlayers.push_back(*it);
      std::iter_swap(it, otherPlayers.end() - 1);
      otherPlayers.pop_back();
    }
  }
  if (DesyncDetected) {
    m_Desynced = true;
    string syncListText = ToNameListSentence(m_SyncPlayers[user]);
    string desyncListText = ToNameListSentence(DesyncedPlayers);
    if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
      LogApp("===== !! Desync detected !! ======================================");
      if (m_Config.m_LoadInGame) {
        LogApp("Frame " + to_string(m_SyncCounterChecked) + " | Load in game: ENABLED");
      } else {
        LogApp("Frame " + to_string(m_SyncCounterChecked) + " | Load in game: DISABLED");
      }
      LogApp("User [" + user->GetName() + "] (" + user->GetDelayText(true) + ") Reconnection: " + user->GetReconnectionText());
      LogApp("User [" + user->GetName() + "] is synchronized with " + to_string(m_SyncPlayers[user].size()) + " user(s): " + syncListText);
      LogApp("User [" + user->GetName() + "] is no longer synchronized with " + desyncListText);
      if (GetAnyUsingGProxy()) {
        LogApp("GProxy: " + GetActiveReconnectProtocolsDetails());
      }
      LogApp("==================================================================");
    }

    if (GetHasDesyncHandler()) {
      SendAllChat("Warning! Desync detected (" + user->GetDisplayName() + " (" + user->GetDelayText(true) + ") may not be in the same game as " + desyncListText);
      if (!GetAllowsDesync()) {
        StopDesynchronized("was automatically dropped after desync");
      }
    }
  }
}

void CGame::EventUserChatToHost(GameUser::CGameUser* user, CIncomingChatPlayer* chatPlayer)
{
  if (chatPlayer->GetFromUID() == user->GetUID())
  {
    if (chatPlayer->GetType() == GameProtocol::ChatToHostType::CTH_MESSAGE || chatPlayer->GetType() == GameProtocol::ChatToHostType::CTH_MESSAGEEXTRA)
    {
      // relay the chat message to other users

      bool shouldRelay = !user->GetMuted();
      const vector<uint8_t>& extraFlags = chatPlayer->GetExtraFlags();
      const bool isLobbyChat = extraFlags.empty();
      if (isLobbyChat == (m_GameLoading || m_GameLoaded)) {
        // Racing condition
        return;
      }

      // calculate timestamp

      string chatTypeFragment;
      if (isLobbyChat) {
        Log("[" + user->GetDisplayName() + "] " + chatPlayer->GetMessage());
        if (m_MuteLobby) {
          shouldRelay = false;
        }
      } else {
        if (extraFlags[0] == CHAT_RECV_ALL) {
          chatTypeFragment = "[All] ";

          if (m_MuteAll) {
            // don't relay ingame messages targeted for all users if we're currently muting all
            // note that any commands will still be processed
            shouldRelay = false;
          }
        } else if (extraFlags[0] == CHAT_RECV_ALLY) {
          chatTypeFragment = "[Allies] ";
        } else if (extraFlags[0] == CHAT_RECV_OBS) {
          // [Observer] or [Referees]
          chatTypeFragment = "[Observer] ";
        } else if (!m_MuteAll) {
          // also don't relay in-game private messages if we're currently muting all
          uint8_t privateTarget = extraFlags[0] - 2;
          chatTypeFragment = "[Private " + ToDecString(privateTarget) + "] ";
        }

        Log(chatTypeFragment + "[" + user->GetDisplayName() + "] " + chatPlayer->GetMessage());
      }

      // handle bot commands
      {
        CRealm* realm = user->GetRealm(false);
        CCommandConfig* commandCFG = realm ? realm->GetCommandConfig() : m_Aura->m_Config.m_LANCommandCFG;
        const bool commandsEnabled = commandCFG->m_Enabled && (
          !realm || !(commandCFG->m_RequireVerified && !user->IsRealmVerified())
        );
        bool isCommand = false;
        const uint8_t activeSmartCommand = user->GetSmartCommand();
        user->ClearSmartCommand();
        if (commandsEnabled) {
          const string message = chatPlayer->GetMessage();
          string cmdToken, command, payload;
          uint8_t tokenMatch = ExtractMessageTokensAny(message, m_Config.m_PrivateCmdToken, m_Config.m_BroadcastCmdToken, cmdToken, command, payload);
          isCommand = tokenMatch != COMMAND_TOKEN_MATCH_NONE;
          if (isCommand) {
            user->SetUsedAnyCommands(true);
            // If we want users identities hidden, we must keep bot responses private.
            if (shouldRelay) {
              if (!GetIsHiddenPlayerNames()) SendChatMessage(user, chatPlayer);
              shouldRelay = false;
            }
            shared_ptr<CCommandContext> ctx = nullptr;
            try {
              ctx = make_shared<CCommandContext>(m_Aura, commandCFG, this, user, !m_MuteAll && !GetIsHiddenPlayerNames() && (tokenMatch == COMMAND_TOKEN_MATCH_BROADCAST), &std::cout);
            } catch (...) {}
            if (ctx) ctx->Run(cmdToken, command, payload);
          } else if (message == "?trigger") {
            if (shouldRelay) {
              if (!GetIsHiddenPlayerNames()) SendChatMessage(user, chatPlayer);
              shouldRelay = false;
            }
            SendCommandsHelp(m_Config.m_BroadcastCmdToken.empty() ? m_Config.m_PrivateCmdToken : m_Config.m_BroadcastCmdToken, user, false);
          } else if (message == "/p" || message == "/ping" || message == "/game") {
            // Note that when the WC3 client is connected to a realm, all slash commands are sent to the bnet server.
            // Therefore, these commands are only effective over LAN.
            if (shouldRelay) {
              if (!GetIsHiddenPlayerNames()) SendChatMessage(user, chatPlayer);
              shouldRelay = false;
            }
            shared_ptr<CCommandContext> ctx = nullptr;
            try {
              ctx = make_shared<CCommandContext>(m_Aura, commandCFG, this, user, false, &std::cout);
            } catch (...) {}
            if (ctx) {
              cmdToken = m_Config.m_PrivateCmdToken;
              command = message.substr(1);
              ctx->Run(cmdToken, command, payload);
            }
          } else if (isLobbyChat && !user->GetUsedAnyCommands()) {
            if (shouldRelay) {
              if (!GetIsHiddenPlayerNames()) SendChatMessage(user, chatPlayer);
              shouldRelay = false;
            }
            if (!CheckSmartCommands(user, message, activeSmartCommand, commandCFG) && !user->GetSentAutoCommandsHelp()) {
              bool anySentCommands = false;
              for (const auto& otherPlayer : m_Users) {
                if (otherPlayer->GetUsedAnyCommands()) anySentCommands = true;
              }
              if (!anySentCommands) {
                SendCommandsHelp(m_Config.m_BroadcastCmdToken.empty() ? m_Config.m_PrivateCmdToken : m_Config.m_BroadcastCmdToken, user, true);
              }
            }
          }
        }
        if (!isCommand) {
          user->ClearLastCommand();
        }
        if (shouldRelay) {
          SendChatMessage(user, chatPlayer);
          shouldRelay = false;
        }
        bool logMessage = false;
        for (const auto& word : m_Config.m_LoggedWords) {
          if (chatPlayer->GetMessage().find(word) != string::npos) {
            logMessage = true;
            break;
          }
        }
        if (logMessage) {
          m_Aura->LogPersistent(GetLogPrefix() + chatTypeFragment + "["+ user->GetExtendedName() + "] " + chatPlayer->GetMessage());
        }
      }
    }
    else
    {
      if (!m_CountDownStarted && !m_RestoredGame) {
        switch (chatPlayer->GetType()) {
          case GameProtocol::ChatToHostType::CTH_TEAMCHANGE:
            EventUserChangeTeam(user, chatPlayer->GetByte());
            break;
          case GameProtocol::ChatToHostType::CTH_COLOURCHANGE:
            EventUserChangeColor(user, chatPlayer->GetByte());
            break;
          case GameProtocol::ChatToHostType::CTH_RACECHANGE:
            EventUserChangeRace(user, chatPlayer->GetByte());
            break;
          case GameProtocol::ChatToHostType::CTH_HANDICAPCHANGE:
            EventUserChangeHandicap(user, chatPlayer->GetByte());
            break;
          default:
            break;
        }
      }
    }
  }
}

void CGame::EventUserChangeTeam(GameUser::CGameUser* user, uint8_t team)
{
  // user is requesting a team change

  if (m_Locked || user->GetIsActionLocked()) {
    SendChat(user, "You are not allowed to change your alignment.");
    return;
  }

  if (team > m_Map->GetVersionMaxSlots()) {
    return;
  }

  if (team == m_Map->GetVersionMaxSlots()) {
    if (m_Map->GetMapObservers() != MAPOBS_ALLOWED && m_Map->GetMapObservers() != MAPOBS_REFEREES) {
      return;
    }
  } else if (team >= m_Map->GetMapNumTeams()) {
    return;
  }

  uint8_t SID = GetSIDFromUID(user->GetUID());
  const CGameSlot* slot = InspectSlot(SID);
  if (!slot) {
    return;
  }

  if (team == slot->GetTeam()) {
    if (!SwapEmptyAllySlot(SID)) return;
  } else if (m_CustomLayout & CUSTOM_LAYOUT_LOCKTEAMS) {
    if (m_IsDraftMode) {
      SendChat(user, "This lobby has draft mode enabled. Only team captains may assign users.");
    } else {
      switch (m_CustomLayout) {
        case CUSTOM_LAYOUT_ONE_VS_ALL:
        SendChat(user, "This is a One-VS-All lobby. You may not switch to another team.");
          break;
        case CUSTOM_LAYOUT_HUMANS_VS_AI:
          SendChat(user, "This is a humans VS AI lobby. You may not switch to another team.");
          break;
        case CUSTOM_LAYOUT_FFA:
          SendChat(user, "This is a free-for-all lobby. You may not switch to another team.");
          break;
        default:
          SendChat(user, "This lobby has a custom teams layout. You may not switch to another team.");
          break;
      }
    } 
  } else {
    SetSlotTeam(GetSIDFromUID(user->GetUID()), team, false);
  }
}

void CGame::EventUserChangeColor(GameUser::CGameUser* user, uint8_t colour)
{
  // user is requesting a colour change

  if (m_Locked || user->GetIsActionLocked()) {
    SendChat(user, "You are not allowed to change your player color.");
    return;
  }

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    // user should directly choose a different slot instead.
    return;
  }

  if (colour >= m_Map->GetVersionMaxSlots()) {
    return;
  }

  uint8_t SID = GetSIDFromUID(user->GetUID());

  if (SID < m_Slots.size()) {
    // make sure the user isn't an observer

    if (m_Slots[SID].GetTeam() == m_Map->GetVersionMaxSlots()) {
      return;
    }

    if (!SetSlotColor(SID, colour, false)) {
      LOG_APP_IF(LOG_LEVEL_DEBUG, user->GetName() + " failed to switch to color " + to_string(static_cast<uint16_t>(colour)))
    }
  }
}

void CGame::EventUserChangeRace(GameUser::CGameUser* user, uint8_t race)
{
  // user is requesting a race change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    return;

  if (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES) {
    SendChat(user, "This game lobby has forced random races.");
    return;
  }

  if (m_Locked || user->GetIsActionLocked()) {
    SendChat(user, "You are not allowed to change your race.");
    return;
  }

  if (race != SLOTRACE_HUMAN && race != SLOTRACE_ORC && race != SLOTRACE_NIGHTELF && race != SLOTRACE_UNDEAD && race != SLOTRACE_RANDOM)
    return;

  CGameSlot* slot = GetSlot(GetSIDFromUID(user->GetUID()));
  if (slot) {
    slot->SetRace(race | SLOTRACE_SELECTABLE);
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
}

void CGame::EventUserChangeHandicap(GameUser::CGameUser* user, uint8_t handicap)
{
  // user is requesting a handicap change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    return;

  if (handicap != 50 && handicap != 60 && handicap != 70 && handicap != 80 && handicap != 90 && handicap != 100)
    return;

  if (m_Locked || user->GetIsActionLocked()) {
    SendChat(user, "You are not allowed to change your handicap.");
    return;
  }

  CGameSlot* slot = GetSlot(GetSIDFromUID(user->GetUID()));
  if (slot) {
    slot->SetHandicap(handicap);
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
}

void CGame::EventUserDropRequest(GameUser::CGameUser* user)
{
  if (!m_GameLoaded) {
    return;
  }

  if (m_Lagging) {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + user->GetName() + "] voted to drop laggers")
    SendAllChat("Player [" + user->GetDisplayName() + "] voted to drop laggers");

    // check if at least half the users voted to drop
    uint8_t votesCount = 0;
    for (auto& eachPlayer : m_Users) {
      if (eachPlayer->GetDropVote()) {
        ++votesCount;
      }
    }

    if (static_cast<uint8_t>(m_Users.size()) < 2 * votesCount) {
      StopLaggers("lagged out (dropped by vote)");
    }
  }
}

bool CGame::EventUserMapSize(GameUser::CGameUser* user, CIncomingMapSize* mapSize)
{
  if (m_GameLoading || m_GameLoaded) {
    return true;
  }

  int64_t Time = GetTime();
  uint32_t MapSize = ByteArrayToUInt32(m_Map->GetMapSize(), false);

  CRealm* JoinedRealm = user->GetRealm(false);
  uint32_t MaxUploadSize = m_Aura->m_Net.m_Config.m_MaxUploadSize;
  if (JoinedRealm)
    MaxUploadSize = JoinedRealm->GetMaxUploadSize();

  if (mapSize->GetSizeFlag() != 1 || mapSize->GetMapSize() != MapSize) {
    // the user doesn't have the map
    bool IsMapTooLarge = MapSize > MaxUploadSize * 1024;
    bool ShouldTransferMap = (
      m_Map->GetMapFileIsValid() && m_Aura->m_Net.m_Config.m_AllowTransfers != MAP_TRANSFERS_NEVER &&
      (user->GetDownloadAllowed() || (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC && !IsMapTooLarge)) &&
      (m_Aura->m_StartedGames.size() < m_Aura->m_Config.m_MaxStartedGames) &&
      (m_Aura->m_StartedGames.empty() || !m_Aura->m_Net.m_Config.m_HasBufferBloat)
    );
    if (ShouldTransferMap) {
      if (!user->GetDownloadStarted() && mapSize->GetSizeFlag() == 1) {
        // inform the client that we are willing to send the map

        LOG_APP_IF(LOG_LEVEL_DEBUG, "map download started for user [" + user->GetName() + "]")
        Send(user, GameProtocol::SEND_W3GS_STARTDOWNLOAD(GetHostUID()));
        user->SetDownloadStarted(true);
        user->SetStartedDownloadingTicks(GetTicks());
      } else {
        user->SetLastMapPartAcked(mapSize->GetMapSize());
      }
    } else if (!user->GetMapKicked()) {
      user->AddKickReason(GameUser::KickReason::MAP_MISSING);
      user->KickAtLatest(GetTicks() + m_Config.m_LacksMapKickDelay);
      if (!user->HasLeftReason()) {
        if (m_Remade) {
          user->SetLeftReason("autokicked - they don't have the map (remade game)");
        } else if (m_Aura->m_Net.m_Config.m_AllowTransfers != MAP_TRANSFERS_AUTOMATIC) {
          // Even if manual, claim they are disabled.
          user->SetLeftReason("autokicked - they don't have the map, and it cannot be transferred (disabled)");
        } else if (
          (m_Aura->m_StartedGames.size() >= m_Aura->m_Config.m_MaxStartedGames) ||
          (!m_Aura->m_StartedGames.empty() && m_Aura->m_Net.m_Config.m_HasBufferBloat)
        ) {
          user->SetLeftReason("autokicked - they don't have the map, and it cannot be transferred (bufferbloat)");
        } else if (IsMapTooLarge) {
          user->SetLeftReason("autokicked - they don't have the map, and it cannot be transferred (too large)");
        } else if (m_Map->HasMismatch()) {
          user->SetLeftReason("autokicked - they don't have the map, and it cannot be transferred (invalid)");
        } else if (!m_Map->GetMapFileIsValid()) {
          user->SetLeftReason("autokicked - they don't have the map, and it cannot be transferred (missing)");
        }
      }
      if (GetMapSiteURL().empty()) {
        SendChat(user, "" + user->GetName() + ", please download the map before joining. (Kick in " + to_string(m_Config.m_LacksMapKickDelay / 1000) + " seconds...)");
      } else {
        SendChat(user, "" + user->GetName() + ", please download the map from <" + GetMapSiteURL() + "> before joining. (Kick in " + to_string(m_Config.m_LacksMapKickDelay / 1000) + " seconds...)");
      }
    }
  } else if (user->GetDownloadStarted()) {
    // calculate download rate
    const double Seconds = static_cast<double>(GetTicks() - user->GetStartedDownloadingTicks()) / 1000.f;
    //const double Rate    = static_cast<double>(MapSize) / 1024.f / Seconds;
    LOG_APP_IF(LOG_LEVEL_DEBUG, "map download finished for user [" + user->GetName() + "] in " + ToFormattedString(Seconds) + " seconds")
    SendAllChat("Player [" + user->GetDisplayName() + "] downloaded the map in " + ToFormattedString(Seconds) + " seconds"/* (" + ToFormattedString(Rate) + " KB/sec)"*/);
    user->SetDownloadFinished(true);
    user->SetFinishedDownloadingTime(Time);
    EventUserMapReady(user);
  } else {
    EventUserMapReady(user);
  }

  uint8_t NewDownloadStatus = static_cast<uint8_t>(static_cast<float>(mapSize->GetMapSize()) / MapSize * 100.f);
  if (NewDownloadStatus > 100) {
    NewDownloadStatus = 100;
  }

  CGameSlot* slot = GetSlot(GetSIDFromUID(user->GetUID()));
  if (slot && slot->GetDownloadStatus() != NewDownloadStatus) {
    // only send the slot info if the download status changed
    slot->SetDownloadStatus(NewDownloadStatus);

    // we don't actually send the new slot info here
    // this is an optimization because it's possible for a user to download a map very quickly
    // if we send a new slot update for every percentage change in their download status it adds up to a lot of data
    // instead, we mark the slot info as "out of date" and update it only once in awhile
    // (once per second when this comment was made)

    m_SlotInfoChanged |= (SLOTS_DOWNLOAD_PROGRESS_CHANGED);
  }

  return true;
}

void CGame::EventUserPongToHost(GameUser::CGameUser* user)
{
  if (m_CountDownStarted || user->GetDisconnected()) {
    return;
  }

  if (!user->GetLatencySent() && user->GetIsRTTMeasuredConsistent()) {
    SendChat(user, user->GetName() + ", your latency is " + user->GetDelayText(false), LOG_LEVEL_DEBUG);
    user->SetLatencySent(true);
  }

  if ((!user->GetIsReady() && user->GetMapReady() && !user->GetIsObserver()) &&
    (!m_CountDownStarted && !m_ChatOnly && m_Aura->m_StartedGames.size() < m_Aura->m_Config.m_MaxStartedGames) &&
    (user->GetReadyReminderIsDue() && user->GetIsRTTMeasuredConsistent())) {
    if (!m_AutoStartRequirements.empty()) {
      switch (GetPlayersReadyMode()) {
        case READY_MODE_EXPECT_RACE: {
          SendChat(user, "Choose your race for the match to automatically start (or type " + GetCmdToken() + "ready)");
          break;
        }
        case READY_MODE_EXPLICIT: {
          SendChat(user, "Type " + GetCmdToken() + "ready for the match to automatically start.");
          break;
        }
      }
      user->SetReadyReminded();
    }
  }

  // autokick users with excessive pings but only if they're not reserved and we've received at least 3 pings from them
  // see the Update function for where we send pings

  uint32_t LatencyMilliseconds = user->GetOperationalRTT();
  if (LatencyMilliseconds >= m_Config.m_AutoKickPing && !user->GetIsReserved() && !user->GetIsOwner(nullopt)) {
    if (m_Users.size() > 1 && user->GetIsRTTMeasuredBadConsistent()) {
      if (!user->HasLeftReason()) {
        user->SetLeftReason("autokicked - excessive ping of " + to_string(LatencyMilliseconds) + "ms");
      }
      user->AddKickReason(GameUser::KickReason::HIGH_PING);
      user->KickAtLatest(GetTicks() + HIGH_PING_KICK_DELAY);
      if (!user->GetHasHighPing()) {
        SendAllChat("Player [" + user->GetDisplayName() + "] has an excessive ping of " + to_string(LatencyMilliseconds) + "ms. Autokicking...");
        user->SetHasHighPing(true);
      }
    }
  } else {
    user->RemoveKickReason(GameUser::KickReason::HIGH_PING);
    if (!user->GetAnyKicked() && user->GetKickQueued()) {
      user->ClearKickByTicks();
    }
    if (user->GetHasHighPing()) {
      bool HasHighPing = LatencyMilliseconds >= m_Config.m_SafeHighPing;
      if (!HasHighPing) {
        user->SetHasHighPing(HasHighPing);
        SendAllChat("Player [" + user->GetDisplayName() + "] ping went down to " + to_string(LatencyMilliseconds) + "ms");
      } else if (LatencyMilliseconds >= m_Config.m_WarnHighPing && user->GetPongCounter() % 4 == 0) {
        // Still high ping. We need to keep sending these intermittently (roughly every 20-25 seconds), so that
        // users don't assume that lack of news is good news.
        SendChat(user, user->GetName() + ", you have a high ping of " + to_string(LatencyMilliseconds) + "ms");
      }
    } else {
      bool HasHighPing = LatencyMilliseconds >= m_Config.m_WarnHighPing;
      if (HasHighPing) {
        user->SetHasHighPing(HasHighPing);
        SendAllChat("Player [" + user->GetDisplayName() + "] has a high ping of " + to_string(LatencyMilliseconds) + "ms");
      }
    }
  }
}

void CGame::EventUserMapReady(GameUser::CGameUser* user)
{
  if (user->GetMapReady()) {
    return;
  }
  user->SetMapReady(true);
  UpdateReadyCounters();
}

// keyword: EventGameLoading
void CGame::EventGameStartedLoading()
{
  if (GetUDPEnabled())
    SendGameDiscoveryDecreate();

  // encode the HCL command string in the slot handicaps
  // here's how it works:
  //  the user inputs a command string to be sent to the map
  //  it is almost impossible to send a message from the bot to the map so we encode the command string in the slot handicaps
  //  this works because there are only 6 valid handicaps but Warcraft III allows the bot to set up to 256 handicaps
  //  we encode the original (unmodified) handicaps in the new handicaps and use the remaining space to store a short message
  //  only occupied slots deliver their handicaps to the map and we can send one character (from a list) per handicap
  //  when the map finishes loading, assuming it's designed to use the HCL system, it checks if anyone has an invalid handicap
  //  if so, it decodes the message from the handicaps and restores the original handicaps using the encoded values
  //  the meaning of the message is specific to each map and the bot doesn't need to understand it
  //  e.g. you could send game modes, # of rounds, level to start on, anything you want as long as it fits in the limited space available
  //  note: if you attempt to use the HCL system on a map that does not support HCL the bot will drastically modify the handicaps
  //  since the map won't automatically restore the original handicaps in this case your game will be ruined

  if (!m_HCLCommandString.empty())
  {
    if (m_HCLCommandString.size() <= GetSlotsOccupied())
    {
      string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

      if (m_HCLCommandString.find_first_not_of(HCLChars) == string::npos)
      {
        uint8_t EncodingMap[256];
        uint8_t j = 0;

        for (auto& encode : EncodingMap)
        {
          // the following 7 handicap values are forbidden

          if (j == 0 || j == 50 || j == 60 || j == 70 || j == 80 || j == 90 || j == 100)
            ++j;

          encode = j++;
        }

        uint8_t CurrentSlot = 0;

        for (auto& character : m_HCLCommandString)
        {
          while (m_Slots[CurrentSlot].GetSlotStatus() != SLOTSTATUS_OCCUPIED)
            ++CurrentSlot;

          uint8_t HandicapIndex = (m_Slots[CurrentSlot].GetHandicap() - 50) / 10;
          uint8_t CharIndex     = static_cast<uint8_t>(HCLChars.find(character));
          m_Slots[CurrentSlot++].SetHandicap(EncodingMap[HandicapIndex + CharIndex * 6]);
        }

        m_SlotInfoChanged |= SLOTS_HCL_INJECTED;
        LOG_APP_IF(LOG_LEVEL_DEBUG, "successfully encoded mode as HCL string [" + m_HCLCommandString + "]")
      } else {
        LOG_APP_IF(LOG_LEVEL_ERROR, "failed to encode game mode as HCL string [" + m_HCLCommandString + "] because it contains invalid characters")
      }
    } else {
      LOG_APP_IF(LOG_LEVEL_INFO, "failed to encode game mode as HCL string [" + m_HCLCommandString + "] because there aren't enough occupied slots")
    }
  }

  m_StartedLoadingTicks    = GetTicks();
  m_LastLagScreenResetTime = GetTime();

  // Remove the virtual host user to ensure consistent game state and networking.
  DeleteVirtualHost();

  if (m_RestoredGame) {
    const uint8_t activePlayers = static_cast<uint8_t>(GetNumJoinedUsersOrFake()); // though it shouldn't be possible to manually add fake users
    const uint8_t expectedPlayers = m_RestoredGame->GetNumHumanSlots();
    if (activePlayers < expectedPlayers) {
      if (m_IsAutoVirtualPlayers) {
        // Restored games do not allow custom fake users, so we should only reach this point with actual users joined.
        // This code path is triggered by !fp enable.
        const uint8_t addedCounter = FakeAllSlots();
        LOG_APP_IF(LOG_LEVEL_INFO, "resuming " + to_string(expectedPlayers) + "-user game. " + to_string(addedCounter) + " virtual users added.")
      } else {
        LOG_APP_IF(LOG_LEVEL_INFO, "resuming " + to_string(expectedPlayers) + "-user game. " + ToDecString(expectedPlayers - activePlayers) + " missing.")
      }
    }
  }

  if (m_IsHiddenPlayerNames && m_Config.m_HideInGameNames != HIDE_IGN_ALWAYS) {
    ShowPlayerNamesGameStartLoading();
  }

  if (!m_RestoredGame && GetSlotsOpen() > 0) {
    // Assign an available slot to our virtual host.
    // That makes it a fake user.

    // This is an AWFUL hack to please WC3Stats.com parser
    // https://github.com/wc3stats/w3lib/blob/4e96ea411e01a41c5492b85fd159a0cb318ea2b8/src/w3g/Model/W3MMD.php#L140-L157

    if (m_Map->GetMapType() == "evergreen" && GetNumComputers() > 0) {
      m_Config.m_LobbyVirtualHostName = "AMAI Insane";
    }

    if (m_Map->GetMapObservers() == MAPOBS_REFEREES) {
      if (CreateFakeObserver(true)) ++m_JoinedVirtualHosts;
    } else {
      if (m_Map->GetMapObservers() == MAPOBS_ALLOWED && GetNumJoinedObservers() > 0 && GetNumFakeObservers() == 0) {
        if (CreateFakeObserver(true)) ++m_JoinedVirtualHosts;
      }
      if (m_IsAutoVirtualPlayers && GetNumJoinedPlayersOrFake() < 2) {
        if (CreateFakePlayer(true)) ++m_JoinedVirtualHosts;
      }
    }
  }

  if (m_Config.m_EnableJoinObserversInProgress && !GetIsCustomForces()) {
    // TODO: Join-in-progress select observer slot
    // Let's also choose the virtual host fake player, if it exists.
  }

  //if (GetNumJoinedUsersOrFake() < 2) {
    // This is a single-user game. Neither chat events nor bot commands will work.
    // Keeping the virtual host does no good - The game client then refuses to remain in the game.
  //}

  // send a final slot info update for HCL, or in case there are pending updates
  if (m_SlotInfoChanged != 0) {
    SendAllSlotInfo();
    UpdateReadyCounters();
  }

  m_ReconnectProtocols = CalcActiveReconnectProtocols();
  if (m_GProxyEmptyActions > 0 && m_ReconnectProtocols == RECONNECT_ENABLED_GPROXY_EXTENDED) {
    m_GProxyEmptyActions = 0;
    for (const auto& user : m_Users) {
      if (user->GetGProxyAny()) {
        user->UpdateGProxyEmptyActions();
      }
    }
  }

  m_GameLoading = true;

  // since we use a fake countdown to deal with leavers during countdown the COUNTDOWN_START and COUNTDOWN_END packets are sent in quick succession
  // send a start countdown packet

  {
    vector<uint8_t> packet = GameProtocol::SEND_W3GS_COUNTDOWN_START();
    SendAll(packet);
  }

  // send an end countdown packet

  {
    vector<uint8_t> packet = GameProtocol::SEND_W3GS_COUNTDOWN_END();
    SendAll(packet);
  }

  for (auto& user : m_Users) {
    user->SetStatus(USERSTATUS_LOADING_SCREEN);
    user->SetWhoisShouldBeSent(false);
  }

  // record the number of starting users
  // fake observers are counted, this is a feature to prevent premature game ending
  m_StartPlayers = GetNumJoinedPlayersOrFakeUsers() - m_JoinedVirtualHosts;
  LOG_APP_IF(LOG_LEVEL_INFO, "started loading: " + ToDecString(GetNumJoinedPlayers()) + " p | " + ToDecString(GetNumComputers()) + " comp | " + ToDecString(GetNumJoinedObservers()) + " obs | " + to_string(m_FakeUsers.size() - m_JoinedVirtualHosts) + " fake | " + ToDecString(m_JoinedVirtualHosts) + " vhost | " + ToDecString(m_ControllersWithMap) + " controllers")

  // When load-in-game is disabled, m_LoadingVirtualBuffer also includes
  // load messages for disconnected real players, but we let automatic resizing handle that.

  m_LoadingVirtualBuffer.reserve(5 * m_FakeUsers.size());
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    // send a game loaded packet for each fake user
    AppendByteArrayFast(m_LoadingVirtualBuffer, fakeUser.GetGameLoadedBytes());
  }

  if (GetAnyUsingGProxy()) {
    // Always send an empty action.
    // This ensures GProxy clients are correctly initialized, and 
    // keeps complexity in check. It's just 6 bytes, too...
    //
    // NOTE: It's specially important when load-in-game is enabled.

    ++m_BeforePlayingEmptyActions;
  }

  m_Actions.emplaceBack();
  m_CurrentActionsFrame = m_Actions.head;
  ResetUserPingEqualizerDelays();

  // enable stats

  if (!m_RestoredGame && m_Map->GetMapMetaDataEnabled()) {
    if (m_Map->GetMapType() == "dota") {
      if (m_StartPlayers < 6) {
        LOG_APP_IF(LOG_LEVEL_DEBUG, "[STATS] not using dotastats due to too few users")
      } else if (!m_ControllersBalanced || !m_FakeUsers.empty()) {
        LOG_APP_IF(LOG_LEVEL_DEBUG, "[STATS] not using dotastats due to imbalance")
      } else {
        m_DotaStats = new CDotaStats(this);
      }
    } else {
      m_CustomStats = new CW3MMD(this);
    }
  }

  for (auto& user : m_Users) {
    uint8_t SID = GetSIDFromUID(user->GetUID());
    // Do not exclude observers yet, so they can be searched in commands.
    m_DBGamePlayers.push_back(new CDBGamePlayer(
      user->GetName(),
      user->GetRealmHostName(),
      user->GetIPStringStrict(),
      m_Slots[SID].GetColor()
    ));
  }

  for (auto& user : m_Users) {
    UserList otherPlayers;
    for (auto& otherPlayer : m_Users) {
      if (otherPlayer != user) {
        otherPlayers.push_back(otherPlayer);
      }
    }
    m_SyncPlayers[user] = otherPlayers;
  }

  if (m_Map->GetMapObservers() != MAPOBS_REFEREES) {
    for (auto& user : m_Users) {
      if (user->GetIsObserver()) {
        user->SetCannotPause();
        user->SetCannotSave();
      }
    }
  }

  if (m_Map->GetHMCEnabled()) {
    const uint8_t SID = m_Map->GetHMCSlot() - 1;
    const CGameSlot* slot = InspectSlot(SID);
    if (slot && slot->GetIsPlayerOrFake() && !GetUserFromSID(SID)) {
      const CGameVirtualUser* virtualUserMatch = InspectVirtualUserFromSID(SID);
      if (virtualUserMatch && !virtualUserMatch->GetIsObserver()) {
        m_HMCEnabled = true;
      }
    }
  }

  m_ReconnectProtocols = CalcActiveReconnectProtocols();

  // release map data from memory
  ClearLoadedMapChunk();
  m_Map->ClearMapFileContents();

  if (m_BufferingEnabled & BUFFERING_ENABLED_LOADING) {
    // Preallocate memory for all SEND_W3GS_GAMELOADED_OTHERS packets
    m_LoadingRealBuffer.reserve(5 * m_Users.size());
  }

  if (m_Config.m_LoadInGame) {
    for (const auto& user : m_Users) {
      vector<uint8_t> packet = GameProtocol::SEND_W3GS_GAMELOADED_OTHERS(user->GetUID());
      AppendByteArray(m_LoadingRealBuffer, packet);
    }

    // Only when load-in-game is enabled, initialize everyone's m_Lagging flag to true
    // this ensures CGame::UpdateLoaded() will send W3GS_STOP_LAG messages only when appropriate.
    SetEveryoneLagging();
  }

  // and finally reenter battle.net chat
  AnnounceDecreateToRealms();

  ClearBannableUsers();
  UpdateBannableUsers();
}

void CGame::AddProvisionalBannableUser(const GameUser::CGameUser* user)
{
  const bool isOversized = m_Bannables.size() > GAME_BANNABLE_MAX_HISTORY_SIZE;
  bool matchedSameName = false, matchedShrink = false;
  size_t matchIndex = 0, shrinkIndex = 0;
  while (matchIndex < m_Bannables.size()) {
    if (user->GetName() == m_Bannables[matchIndex]->GetName()) {
      matchedSameName = true;
      break;
    }
    if (isOversized && !matchedShrink && GetUserFromName(m_Bannables[matchIndex]->GetName(), true) == nullptr) {
      shrinkIndex = matchIndex;
      matchedShrink = true;
    }
    matchIndex++;
  }

  if (matchedSameName) {
    delete m_Bannables[matchIndex];
  } else if (matchedShrink) {
    delete m_Bannables[shrinkIndex];
    m_Bannables.erase(m_Bannables.begin() + shrinkIndex);
  }

  CDBBan* bannable = new CDBBan(
    user->GetName(),
    user->GetRealmDataBaseID(false),
    string(), // auth server
    user->GetIPStringStrict(),
    string(), // date
    string(), // expiry
    false, // temporary ban (permanent == false)
    string(), // moderator
    string() // reason
  );

  if (matchedSameName) {
    m_Bannables[matchIndex] = bannable;
  } else {
    m_Bannables.push_back(bannable);
  }

  m_LastLeaverBannable = bannable;
}

void CGame::ClearBannableUsers()
{
  for (auto& bannable : m_Bannables) {
    delete bannable;
  }
  m_Bannables.clear();
  m_LastLeaverBannable = nullptr;
}

void CGame::UpdateBannableUsers()
{
  // record everything we need to ban each user in case we decide to do so later
  // this is because when a user leaves the game an admin might want to ban that user
  // but since the user has already left the game we don't have access to their information anymore
  // so we create a "potential ban" for each user and only store it in the database if requested to by an admin

  for (auto& user : m_Users) {
    m_Bannables.push_back(new CDBBan(
      user->GetName(),
      user->GetRealmDataBaseID(false),
      string(), // auth server
      user->GetIPStringStrict(),
      string(), // date
      string(), // expiry
      false, // temporary ban (permanent == false)
      string(), // moderator
      string() // reason
    ));
  }
}

bool CGame::ResolvePlayerObfuscation() const
{
  if (m_Config.m_HideInGameNames == HIDE_IGN_ALWAYS || m_Config.m_HideInGameNames == HIDE_IGN_HOST) {
    return true;
  }
  if (m_Config.m_HideInGameNames == HIDE_IGN_NEVER) {
    return false;
  }

  if (m_ControllersWithMap < 3) {
    return false;
  }

  unordered_set<uint8_t> activeTeams = {};
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Map->GetVersionMaxSlots()) {
      continue;
    }
    if (activeTeams.find(slot.GetTeam()) != activeTeams.end()) {
      return false;
    }
    activeTeams.insert(slot.GetTeam());
  }

  return true;
}

void CGame::RunPlayerObfuscation()
{
  m_IsHiddenPlayerNames = ResolvePlayerObfuscation();

  if (m_IsHiddenPlayerNames) {
    vector<uint8_t> pseudonymUIDs = vector<uint8_t>(GetPlayers().size());
    uint8_t i = static_cast<uint8_t>(pseudonymUIDs.size());
    while (i--) {
      pseudonymUIDs[i] = i;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(begin(pseudonymUIDs), end(pseudonymUIDs), gen);

    i = 0;
    for (auto& player : m_Users) {
      if (player->GetIsObserver() || player->GetLeftMessageSent()) {
        continue;
      }
      player->SetPseudonymUID(pseudonymUIDs[i++]);
    }
  }
}

bool CGame::CheckSmartCommands(GameUser::CGameUser* user, const std::string& message, const uint8_t activeCmd, CCommandConfig* commandCFG)
{
  if (message.length() >= 2) {
    string prefix = ToLowerCase(message.substr(0, 2));
    if (prefix[0] == 'g' && prefix[1] == 'o' && message.find_first_not_of("goGO") == string::npos && !HasOwnerInGame()) {
      if (activeCmd == SMART_COMMAND_GO) {
        shared_ptr<CCommandContext> ctx = nullptr;
        try {
          ctx = make_shared<CCommandContext>(m_Aura, commandCFG, this, user, false, &std::cout);
        } catch (...) {
          return true;
        }
        string cmdToken = m_Config.m_PrivateCmdToken;
        string command = "start";
        string payload;
        ctx->Run(cmdToken, command, payload);
      } else {
        user->SetSmartCommand(SMART_COMMAND_GO);
        SendChat(user, "You may type [" + message + "] again to start the game.");
      }
      return true;
    }
  }
  return false;
}

void CGame::EventGameLoaded()
{
  RunPlayerObfuscation();

  LOG_APP_IF(LOG_LEVEL_INFO, "finished loading: " + ToDecString(GetNumJoinedPlayers()) + " p | " + ToDecString(GetNumComputers()) + " comp | " + ToDecString(GetNumJoinedObservers()) + " obs | " + to_string(m_FakeUsers.size() - m_JoinedVirtualHosts) + " fake | " + ToDecString(m_JoinedVirtualHosts) + " vhost")

  // send shortest, longest, and personal load times to each user

  const GameUser::CGameUser* Shortest = nullptr;
  const GameUser::CGameUser* Longest  = nullptr;

  uint8_t majorityThreshold = static_cast<uint8_t>(m_Users.size() / 2);
  ImmutableUserList DesyncedPlayers;
  if (m_Users.size() >= 2) {
    for (const auto& user : m_Users) {
      if (user->GetFinishedLoading()) {
        if (!Shortest || user->GetFinishedLoadingTicks() < Shortest->GetFinishedLoadingTicks()) {
          Shortest = user;
        } else if (Shortest && (!Longest || user->GetFinishedLoadingTicks() > Longest->GetFinishedLoadingTicks())) {
          Longest = user;
        }
      }

      if (m_SyncPlayers[user].size() < majorityThreshold) {
        DesyncedPlayers.push_back(user);
      }
    }
  }

  for (const auto& user : m_Users) {
    user->SetStatus(USERSTATUS_PLAYING);
    if (user->GetIsNativeReferee()) {
      // Natively, referees get unlimited saves. But we limit them to 3 in multiplayer games.
      user->SetRemainingSaves(m_Users.size() >= 2 ? GAME_SAVES_PER_REFEREE_ANTIABUSE : GAME_SAVES_PER_REFEREE_DEFAULT);
    }
  }

  ImmutableUserList players = GetPlayers();
  if (players.size() <= 2) {
    m_PlayedBy = ToNameListSentence(players, true);
  } else {
    m_PlayedBy = players[0]->GetName() + ", and others";
  }

  if (Shortest && Longest) {
    SendAllChat("Shortest load by user [" + Shortest->GetDisplayName() + "] was " + ToFormattedString(static_cast<double>(Shortest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");
    SendAllChat("Longest load by user [" + Longest->GetDisplayName() + "] was " + ToFormattedString(static_cast<double>(Longest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");
  }
  const uint8_t numDisconnectedPlayers = m_StartPlayers + m_JoinedVirtualHosts - GetNumJoinedPlayersOrFakeUsers();
  if (0 < numDisconnectedPlayers) {
    SendAllChat(ToDecString(numDisconnectedPlayers) + " user(s) disconnected during game load.");
  }
  if (!DesyncedPlayers.empty()) {
    if (GetHasDesyncHandler()) {
      SendAllChat("Some users desynchronized during game load: " + ToNameListSentence(DesyncedPlayers));
      if (!GetAllowsDesync()) {
        StopDesynchronized("was automatically dropped after desync");
      }
    }
  }

  for (auto& user : m_Users) {
    if (user->GetFinishedLoading()) {
      SendChat(user, "Your load time was " + ToFormattedString(static_cast<double>(user->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");
    }
  }

  // GProxy hangs trying to reconnect
  if (GetIsSinglePlayerMode() && !GetAnyUsingGProxy()) {
    SendAllChat("HINT: Single-user game detected. In-game commands will be DISABLED.");
    // FIXME? This creates a large lag spike client-side.
    // Tested at 793b88d5 (2024-09-07): caused the WC3 client to straight up quit the game.
    // Tested at e6fd6133 (2024-09-25): correctly untracks wormwar.ini (yet lags), correctly untracks lastrefugeamai.ini --observers=no
    SendEveryoneElseLeftAndDisconnect("single-player game untracked");
  }

  if (!(m_BufferingEnabled & BUFFERING_ENABLED_PLAYING)) {
    // These buffers serve no purpose anymore.
    m_LoadingRealBuffer = vector<uint8_t>();
    m_LoadingVirtualBuffer = vector<uint8_t>();
  }

  // move the game to the games in progress vector
  if (m_Config.m_EnableJoinObserversInProgress || m_Config.m_EnableJoinPlayersInProgress) {
    m_Aura->TrackGameJoinInProgress(this);
  }

  HandleGameLoadedStats();
}

void CGame::HandleGameLoadedStats()
{
  if (!m_Config.m_SaveStats) {
    return;
  }
  vector<string> exportPlayerNames;
  vector<uint8_t> exportPlayerIDs;
  vector<uint8_t> exportSlotIDs;
  vector<uint8_t> exportColorIDs;

  for (uint8_t SID = 0; SID < static_cast<uint8_t>(m_Slots.size()); ++SID) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot->GetIsPlayerOrFake()) {
      continue;
    }
    const GameUser::CGameUser* user = GetUserFromSID(SID);
    exportSlotIDs.push_back(SID);
    exportColorIDs.push_back(slot->GetColor());
    if (user == nullptr) {
      const CGameVirtualUser* virtualUserMatch = InspectVirtualUserFromSID(SID);
      if (virtualUserMatch) {
        exportPlayerNames.push_back(string());
        exportPlayerIDs.push_back(virtualUserMatch->GetUID());
      }
    } else {
      exportPlayerNames.push_back(user->GetName());
      exportPlayerIDs.push_back(user->GetUID());
    }
  }

  int64_t Ticks = GetTicks();
  if (!m_Aura->m_DB->Begin()) {
    LOG_APP_IF(LOG_LEVEL_WARNING, "[STATS] failed to begin transaction for game loaded data")
    return;
  }
  m_Aura->m_DB->UpdateLatestHistoryGameId(m_GameHistoryId);

  m_Aura->m_DB->GameAdd(
    m_GameHistoryId,
    m_CreatorText,
    m_Map->GetClientPath(),
    PathToString(m_Map->GetServerPath()),
    m_Map->GetMapCRC32(),
    exportPlayerNames,
    exportPlayerIDs,
    exportSlotIDs,
    exportColorIDs
  );

  for (auto& dbPlayer : m_DBGamePlayers) {
    if (dbPlayer->GetColor() == m_Map->GetVersionMaxSlots()) {
      continue;
    }
    m_Aura->m_DB->UpdateGamePlayerOnStart(
      dbPlayer->GetName(),
      dbPlayer->GetServer(),
      dbPlayer->GetIP(),
      m_GameHistoryId
    );
  }
  if (!m_Aura->m_DB->Commit()) {
    LOG_APP_IF(LOG_LEVEL_WARNING, "[STATS] failed to commit transaction for game loaded data")
  } else {
    LOG_APP_IF(LOG_LEVEL_DEBUG, "[STATS] commited game loaded data in " + to_string(GetTicks() - Ticks) + " ms")
  }
}

bool CGame::GetIsRemakeable()
{
  if (!m_Map || m_RestoredGame || m_FromAutoReHost) {
    return false;
  }
  return true;
}

void CGame::Remake()
{
  m_Config.m_SaveStats = false;

  Reset();

  int64_t Time = GetTime();
  int64_t Ticks = GetTicks();

  m_FromAutoReHost = false;
  m_GameTicks = 0;
  m_CreationTime = Time;
  m_LastPingTime = Time;
  m_LastRefreshTime = Time;
  m_LastDownloadTicks = Time;
  m_LastDownloadCounterResetTicks = Ticks;
  m_LastCountDownTicks = 0;
  m_StartedLoadingTicks = 0;
  m_FinishedLoadingTicks = 0;
  m_LastActionSentTicks = 0;
  m_LastActionLateBy = 0;
  m_LastPausedTicks = 0;
  m_PausedTicksDeltaSum = 0;
  m_StartedLaggingTime = 0;
  m_LastLagScreenTime = 0;
  m_PingReportedSinceLagTimes = 0;
  m_LastUserSeen = Ticks;
  m_LastOwnerSeen = Ticks;
  m_StartedKickVoteTime = 0;
  m_LastCustomStatsUpdateTime = 0;
  m_GameOverTime = nullopt;
  m_LastPlayerLeaveTicks = nullopt;
  m_LastLagScreenResetTime = 0;
  m_SyncCounter = 0;
  m_SyncCounterChecked = 0;
  m_MaxPingEqualizerDelayFrames = 0;
  m_LastPingEqualizerGameTicks = 0;

  m_DownloadCounter = 0;
  m_CountDownCounter = 0;
  m_StartPlayers = 0;
  m_ControllersBalanced = false;
  m_ControllersReadyCount = 0;
  m_ControllersNotReadyCount = 0;
  m_ControllersWithMap = 0;
  m_AutoStartRequirements.clear();
  m_CustomLayout = 0;

  m_IsAutoVirtualPlayers = false;
  m_VirtualHostUID = 0xFF;
  m_SlotInfoChanged = 0;
  m_JoinedVirtualHosts = 0;
  m_ReconnectProtocols = 0;
  //m_Replaceable = false;
  //m_Replacing = false;
  //m_PublicStart = false;
  m_Locked = false;
  m_CountDownStarted = false;
  m_CountDownFast = false;
  m_CountDownUserInitiated = false;
  m_GameLoading = false;
  m_GameLoaded = false;
  m_LobbyLoading = true;
  m_Lagging = false;
  m_Desynced = false;
  m_IsDraftMode = false;
  m_IsHiddenPlayerNames = false;
  m_HadLeaver = false;
  m_UsesCustomReferees = false;
  m_SentPriorityWhois = false;
  m_Remaking = true;
  m_Remade = false;
  m_GameDiscoveryInfoChanged = true;
  m_HMCEnabled = false;
  m_BufferingEnabled = BUFFERING_ENABLED_NONE;
  m_BeforePlayingEmptyActions = 0;

  m_HostCounter = m_Aura->NextHostCounter();
  InitPRNG();
  InitSlots();

  m_KickVotePlayer.clear();
}

uint8_t CGame::GetSIDFromUID(uint8_t UID) const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetUID() == UID)
      return i;
  }

  return 0xFF;
}

GameUser::CGameUser* CGame::GetUserFromUID(uint8_t UID) const
{
  for (auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && user->GetUID() == UID)
      return user;
  }

  return nullptr;
}

GameUser::CGameUser* CGame::GetUserFromSID(uint8_t SID) const
{
  if (SID >= static_cast<uint8_t>(m_Slots.size()))
    return nullptr;

  const uint8_t UID = m_Slots[SID].GetUID();

  for (auto& user : m_Users)
  {
    if (!user->GetLeftMessageSent() && user->GetUID() == UID)
      return user;
  }

  return nullptr;
}

string CGame::GetUserNameFromUID(uint8_t UID) const
{
  for (auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && user->GetUID() == UID)
      return user->GetName();
  }

  return string();
}

bool CGame::HasOwnerSet() const
{
  return !m_OwnerName.empty();
}

bool CGame::HasOwnerInGame() const
{
  if (!HasOwnerSet()) return false;
  GameUser::CGameUser* MaybeOwner = GetUserFromName(m_OwnerName, false);
  if (!MaybeOwner) return false;
  return MaybeOwner->GetIsOwner(nullopt);
}

GameUser::CGameUser* CGame::GetUserFromName(string name, bool sensitive) const
{
  if (!sensitive) {
    transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  }

  for (auto& user : m_Users) {
    if (!user->GetDeleteMe()) {
      string testName = sensitive ? user->GetName() : user->GetLowerName();
      if (testName == name) {
        return user;
      }
    }
  }

  return nullptr;
}

uint8_t CGame::GetUserFromNamePartial(const string& name, GameUser::CGameUser*& matchPlayer) const
{
  uint8_t matches = 0;
  if (name.empty()) {
    matchPlayer = nullptr;
    return matches;
  }

  string inputLower = ToLowerCase(name);

  // try to match each user with the passed string (e.g. "Varlock" would be matched with "lock")

  for (auto& user : m_Users) {
    if (!user->GetDeleteMe()) {
      string testName = user->GetLowerName();
      if (testName.find(inputLower) != string::npos) {
        ++matches;
        matchPlayer = user;

        // if the name matches exactly stop any further matching

        if (testName == inputLower) {
          matches = 1;
          break;
        }
      }
    }
  }

  if (matches != 1) {
    matchPlayer = nullptr;
  }
  return matches;
}

uint8_t CGame::GetUserFromDisplayNamePartial(const string& name, GameUser::CGameUser*& matchPlayer) const
{
  uint8_t matches = 0;
  if (name.empty()) {
    matchPlayer = nullptr;
    return matches;
  }

  string inputLower = ToLowerCase(name);

  // try to match each user with the passed string (e.g. "Varlock" would be matched with "lock")

  for (auto& user : m_Users) {
    if (!user->GetDeleteMe()) {
      string testName = ToLowerCase(user->GetDisplayName());
      if (testName.find(inputLower) != string::npos) {
        ++matches;
        matchPlayer = user;

        // if the name matches exactly stop any further matching

        if (testName == inputLower) {
          matches = 1;
          break;
        }
      }
    }
  }

  if (matches != 1) {
    matchPlayer = nullptr;
  }
  return matches;
}

CDBGamePlayer* CGame::GetDBPlayerFromColor(uint8_t colour) const
{
  if (colour == m_Map->GetVersionMaxSlots()) {
    // Observers are not stored
    return nullptr;
  }
  for (const auto& user : m_DBGamePlayers) {
    if (user->GetColor() == colour) {
      return user;
    }
  }
  return nullptr;
}

uint8_t CGame::GetBannableFromNamePartial(const string& name, CDBBan*& matchBanPlayer) const
{
  uint8_t matches = 0;
  if (name.empty()) {
    matchBanPlayer = nullptr;
    return matches;
  }

  string inputLower = ToLowerCase(name);

  // try to match each user with the passed string (e.g. "Varlock" would be matched with "lock")

  for (auto& bannable : m_Bannables) {
    string testName = ToLowerCase(bannable->GetName());

    if (testName.find(inputLower) != string::npos) {
      ++matches;
      matchBanPlayer = bannable;

      // if the name matches exactly stop any further matching

      if (testName == inputLower) {
        matches = 1;
        break;
      }
    }
  }

  if (matches != 1) {
    matchBanPlayer = nullptr;
  }
  return matches;
}

GameUser::CGameUser* CGame::GetPlayerFromColor(uint8_t colour) const
{
  for (uint8_t i = 0; i < m_Slots.size(); ++i)
  {
    if (m_Slots[i].GetColor() == colour)
      return GetUserFromSID(i);
  }

  return nullptr;
}

uint8_t CGame::GetColorFromUID(uint8_t UID) const
{
  const CGameSlot* slot = InspectSlot(GetSIDFromUID(UID));
  if (!slot) return 0xFF;
  return slot->GetColor();
}

uint8_t CGame::GetNewUID() const
{
  // find an unused UID for a new user to use

  for (uint8_t TestUID = 1; TestUID < 0xFF; ++TestUID)
  {
    if (TestUID == m_VirtualHostUID)
      continue;

    bool inUse = false;
    for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
      if (fakeUser.GetUID() == TestUID) {
        inUse = true;
        break;
      }
    }
    if (inUse) {
      continue;
    }
    for (auto& user : m_Users) {
      if (!user->GetLeftMessageSent() && (user->GetUID() == TestUID || user->GetOldUID() == TestUID)) {
        inUse = true;
        break;
      }
    }
    if (!inUse) {
      return TestUID;
    }
  }

  // this should never happen

  return 0xFF;
}

uint8_t CGame::GetNewPseudonymUID() const
{
  // find an unused Pseudonym UID

  bool inUse = false;
  for (uint8_t TestUID = 1; TestUID < 0xFF; ++TestUID) {
    for (auto& user : m_Users) {
      if (!user->GetLeftMessageSent() && user->GetPseudonymUID() == TestUID) {
        inUse = true;
        break;
      }
    }
    if (!inUse) {
      return TestUID;
    }
  }

  // this should never happen

  return 0xFF;
}

uint8_t CGame::GetNewTeam() const
{
  bitset<MAX_SLOTS_MODERN> usedTeams;
  for (auto& slot : m_Slots) {
    if (slot.GetColor() == m_Map->GetVersionMaxSlots()) continue;
    if (slot.GetSlotStatus() != SLOTSTATUS_OCCUPIED) continue;
    usedTeams.set(slot.GetTeam());
  }
  const uint8_t endTeam = m_Map->GetMapNumTeams();
  for (uint8_t team = 0; team < endTeam; ++team) {
    if (!usedTeams.test(team)) {
      return team;
    }
  }
  return m_Map->GetVersionMaxSlots();
}

uint8_t CGame::GetNewColor() const
{
  bitset<MAX_SLOTS_MODERN> usedColors;
  for (auto& slot : m_Slots) {
    if (slot.GetColor() == m_Map->GetVersionMaxSlots()) continue;
    usedColors.set(slot.GetColor());
  }
  for (uint8_t color = 0; color < m_Map->GetVersionMaxSlots(); ++color) {
    if (!usedColors.test(color)) {
      return color;
    }
  }
  return m_Map->GetVersionMaxSlots(); // should never happen
}

uint8_t CGame::SimulateActionUID(const uint8_t actionType, GameUser::CGameUser* user, const bool isDisconnect)
{
  // Full observers can never pause/resume/save the game.
  const uint8_t userCanSendActions = user && isDisconnect && !user->GetLeftMessageSent() && !(user->GetIsObserver() && m_Map->GetMapObservers() == MAPOBS_ALLOWED);

  // Note that the game client desyncs if the UID of an actual user is used.
  switch (actionType) {
    case ACTION_PAUSE: {
      if (userCanSendActions && user->GetCanPause()) {
        return user->GetUID();
      }
      
      for (CGameVirtualUser& fakeUser : m_FakeUsers) {
        if (fakeUser.GetCanPause()) {
          // Referees could get unlimited pauses, but that's abusable, so we limit them just like regular players.
          fakeUser.DropRemainingPauses();
          return fakeUser.GetUID();
        }
      }
      return 0xFF;
    }
    case ACTION_RESUME: {
      if (userCanSendActions) {
        return user->GetUID();
      }

      for (CGameVirtualUser& fakeUser : m_FakeUsers) {
        if (fakeUser.GetCanResume()) {
          return fakeUser.GetUID();
        }
      }

      return 0xFF;
    }

    case ACTION_SAVE: {
      if (userCanSendActions && user->GetCanSave()) {
        return user->GetUID();
      }
      for (CGameVirtualUser& fakeUser : m_FakeUsers) {
        if (fakeUser.GetCanSave()) {
          // Referees could get unlimited saves, but that's abusable, so we limit them just like regular players.
          fakeUser.DropRemainingSaves();
          return fakeUser.GetUID();
        }
      }
      return 0xFF;
    }

    default: {
      return 0xFF;
    }
  }
}

uint8_t CGame::HostToMapCommunicationUID() const
{
  if (!GetHMCEnabled()) return 0xFF;
  const uint8_t SID = m_Map->GetHMCSlot() - 1;
  const CGameVirtualUser* virtualUserMatch = InspectVirtualUserFromSID(SID);
  if (!virtualUserMatch) return 0xFF;
  return virtualUserMatch->GetUID();
}

bool CGame::GetHasAnyActiveTeam() const
{
  bitset<MAX_SLOTS_MODERN> usedTeams;
  for (const auto& slot : m_Slots) {
    const uint8_t team = slot.GetTeam();
    if (team == m_Map->GetVersionMaxSlots()) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      if (usedTeams.test(team)) {
        return true;
      } else {
        usedTeams.set(team);
      }
    }
  }
  return false;
}

bool CGame::GetHasAnyUser() const
{
  if (m_Users.empty()) {
    return false;
  }

  for (const auto& user : m_Users) {
    if (!user->GetDeleteMe()) {
      return true;
    }
  }
  return false;
}

bool CGame::GetIsPlayerSlot(const uint8_t SID) const
{
  const CGameSlot* slot = InspectSlot(SID);
  if (!slot || !slot->GetIsPlayerOrFake()) return false;
  const GameUser::CGameUser* user = GetUserFromSID(SID);
  if (user == nullptr) return false;
  return !user->GetDeleteMe();
}

bool CGame::GetHasAnotherPlayer(const uint8_t ExceptSID) const
{
  uint8_t SID = ExceptSID;
  do {
    SID = (SID + 1) % m_Slots.size();
  } while (!GetIsPlayerSlot(SID) && SID != ExceptSID);
  return SID != ExceptSID;
}

std::vector<uint8_t> CGame::GetChatUIDs() const
{
  std::vector<uint8_t> result;

  for (auto& user : m_Users) {
    if (user->GetLeftMessageSent() || user->GetIsInLoadingScreen()) {
      continue;
    }
    result.push_back(user->GetUID());
  }

  return result;
}

std::vector<uint8_t> CGame::GetChatUIDs(uint8_t excludeUID) const
{
  std::vector<uint8_t> result;

  for (auto& user : m_Users)
  {
    if (!user->GetLeftMessageSent() && user->GetUID() != excludeUID)
      result.push_back(user->GetUID());
  }

  return result;
}

std::vector<uint8_t> CGame::GetObserverUIDs() const
{
  std::vector<uint8_t> result;
  for (auto& user : m_Users) {
    if (!user->GetLeftMessageSent() && user->GetIsObserver())
      result.push_back(user->GetUID());
  }
  return result;
}

std::vector<uint8_t> CGame::GetChatObserverUIDs(uint8_t excludeUID) const
{
  std::vector<uint8_t> result;
  for (auto& user : m_Users) {
    if (user->GetLeftMessageSent() || user->GetIsInLoadingScreen()) continue;
    if (user->GetIsObserver() && user->GetUID() != excludeUID) {
      result.push_back(user->GetUID());
    }
  }

  return result;
}

uint8_t CGame::GetPublicHostUID() const
{
  // First try to use a fake user.
  // But fake users are not available while the game is loading.
  if (!m_GameLoading && !m_FakeUsers.empty()) {
    // After loaded, we need to carefully consider who to speak as.
    if (!m_GameLoading && !m_GameLoaded) {
      return m_FakeUsers.back().GetUID();
    }
    for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
      if (fakeUser.GetIsObserver() && m_Map->GetMapObservers() != MAPOBS_REFEREES) {
        continue;
      }
      return fakeUser.GetUID();
    }
  }

  // try to find the owner next

  for (auto& user : m_Users) {
    if (user->GetLeftMessageSent()) {
      continue;
    }
    if (user->GetIsObserver() && m_Map->GetMapObservers() != MAPOBS_REFEREES) {
      continue;
    }
    if (MatchOwnerName(user->GetName())) {
      if (user->IsRealmVerified() && user->GetRealmHostName() == m_OwnerRealm) {
        return user->GetUID();
      }
      if (user->GetRealmHostName().empty() && m_OwnerRealm.empty()) {
        return user->GetUID();
      }
      break;
    }
  }

  // okay then, just use the first available user
  uint8_t fallbackUID = 0xFF;

  for (auto& user : m_Users) {
    if (user->GetLeftMessageSent()) {
      continue;
    }
    if (user->GetCanUsePublicChat()) {
      return user->GetUID();
    } else if (fallbackUID == 0xFF) {
      fallbackUID = user->GetUID();
    }
  }

  return fallbackUID;
}

uint8_t CGame::GetHiddenHostUID() const
{
  // First try to use a fake user.
  // But fake users are not available while the game is loading.

  vector<uint8_t> availableUIDs;
  //vector<uint8_t> availableRefereeUIDs;

  if (!m_GameLoading && !m_FakeUsers.empty()) {
    for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
      if (fakeUser.GetIsObserver() && m_Map->GetMapObservers() != MAPOBS_REFEREES) {
        continue;
      }
      if (fakeUser.GetIsObserver()) {
        //availableRefereeUIDs.push_back(static_cast<uint8_t>(fakePlayer));
        return fakeUser.GetUID();
      } else {
        availableUIDs.push_back(fakeUser.GetUID());
      }
    }
  }

  uint8_t fallbackUID = 0xFF;
  for (auto& user : m_Users) {
    if (user->GetLeftMessageSent() || user->GetIsInLoadingScreen()) {
      continue;
    }
    if (user->GetCanUsePublicChat()) {
      if (user->GetIsObserver()) {
        //availableRefereeUIDs.push_back(user->GetUID());
        return user->GetUID();
      } else {
        availableUIDs.push_back(user->GetUID());
      }
    } else if (fallbackUID == 0xFF) {
      fallbackUID = user->GetUID();
    }
  }

  if (!availableUIDs.empty()) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distribution(1, static_cast<int>(availableUIDs.size()));
    return availableUIDs[distribution(gen) - 1];
  }

  return fallbackUID;
}

uint8_t CGame::GetHostUID() const
{
  // return the user to be considered the host (it can be any user)
  // mainly used for sending text messages from the bot

  if (m_VirtualHostUID != 0xFF) {
    return m_VirtualHostUID;
  }

  if (GetIsHiddenPlayerNames()) {
    return GetHiddenHostUID();
  } else {
    return GetPublicHostUID();
  }
}

FileChunkTransient CGame::GetMapChunk(size_t start)
{
  FileChunkTransient chunk = m_Map->GetMapFileChunk(start);
  // Ensure the SharedByteArray isn't deallocated
  SetLoadedMapChunk(chunk.bytes);
  return chunk;
}

CGameSlot* CGame::GetSlot(const uint8_t SID)
{
  if (SID > m_Slots.size()) return nullptr;
  return &(m_Slots[SID]);
}

const CGameSlot* CGame::InspectSlot(const uint8_t SID) const
{
  if (SID > m_Slots.size()) return nullptr;
  return &(m_Slots[SID]);
}

uint8_t CGame::GetEmptySID(bool reserved) const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  // look for an empty slot for a new user to occupy
  // if reserved is true then we're willing to use closed or occupied slots as long as it wouldn't displace a user with a reserved slot

  uint8_t skipHMC = GetHMCSID();
  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() != SLOTSTATUS_OPEN) {
      continue;
    }
    return i;
  }

  if (reserved)
  {
    // no empty slots, but since user is reserved give them a closed slot

    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_CLOSED && i != skipHMC) {
        return i;
      }
    }

    // no closed slots either, give them an occupied slot but not one occupied by another reserved user
    // first look for a user who is downloading the map and has the least amount downloaded so far

    uint8_t LeastSID = 0xFF;
    uint8_t LeastDownloaded = 100;

    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (!m_Slots[i].GetIsPlayerOrFake()) continue;
      GameUser::CGameUser* Player = GetUserFromSID(i);
      if (Player && !Player->GetIsReserved() && m_Slots[i].GetDownloadStatus() < LeastDownloaded) {
        LeastSID = i;
        LeastDownloaded = m_Slots[i].GetDownloadStatus();
      }
    }

    if (LeastSID != 0xFF) {
      return LeastSID;
    }

    // nobody who isn't reserved is downloading the map, just choose the first user who isn't reserved

    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (!m_Slots[i].GetIsPlayerOrFake()) continue;
      GameUser::CGameUser* Player = GetUserFromSID(i);
      if (Player && !Player->GetIsReserved()) {
        return i;
      }
    }
  }

  return 0xFF;
}

uint8_t CGame::GetHMCSID() const
{
  if (!m_Map->GetHMCEnabled()) return 0xFF;
  uint8_t slot = m_Map->GetHMCSlot();
  if (slot > m_Slots.size()) return 0xFF;
  return slot - 1;
}

uint8_t CGame::GetEmptySID(uint8_t team, uint8_t UID) const
{
  if (m_Slots.size() > 0xFF) {
    return 0xFF;
  }

  // find an empty slot based on user's current slot

  uint8_t StartSlot = GetSIDFromUID(UID);
  if (StartSlot < m_Slots.size()) {
    if (m_Slots[StartSlot].GetTeam() != team) {
      // user is trying to move to another team so start looking from the first slot on that team
      // we actually just start looking from the very first slot since the next few loops will check the team for us

      StartSlot = 0;
    }

    // find an empty slot on the correct team starting from StartSlot

    for (uint8_t i = StartSlot; i < m_Slots.size(); ++i) {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team)
        return i;
    }

    // didn't find an empty slot, but we could have missed one with SID < StartSlot
    // e.g. in the DotA case where I am in slot 4 (yellow), slot 5 (orange) is occupied, and slot 1 (blue) is open and I am trying to move to another slot

    for (uint8_t i = 0; i < StartSlot; ++i) {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team) {
        return i;
      }
    }
  }

  return 0xFF;
}

uint8_t CGame::GetEmptyPlayerSID() const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() != SLOTSTATUS_OPEN) continue;
    if (!GetIsCustomForces()) {
      return i;
    }
    if (m_Slots[i].GetTeam() != m_Map->GetVersionMaxSlots()) {
      return i;
    }
  }

  return 0xFF;
}

uint8_t CGame::GetEmptyObserverSID() const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() != SLOTSTATUS_OPEN) continue;
    if (m_Slots[i].GetTeam() == m_Map->GetVersionMaxSlots()) {
      return i;
    }
  }

  return 0xFF;
}

bool CGame::SwapEmptyAllySlot(const uint8_t SID)
{
  if (!GetIsCustomForces()) {
    return false;
  }
  const uint8_t team = m_Slots[SID].GetTeam();

  // Look for the next ally, starting from the current SID, and wrapping over.
  uint8_t allySID = SID;
  do {
    allySID = (allySID + 1) % m_Slots.size();
  } while (allySID != SID && !(m_Slots[allySID].GetTeam() == team && m_Slots[allySID].GetSlotStatus() == SLOTSTATUS_OPEN));

  if (allySID == SID) {
    return false;
  }
  return SwapSlots(SID, allySID);
}

bool CGame::SwapSlots(const uint8_t SID1, const uint8_t SID2)
{
  if (SID1 >= static_cast<uint8_t>(m_Slots.size()) || SID2 >= static_cast<uint8_t>(m_Slots.size()) || SID1 == SID2) {
    return false;
  }
  uint8_t hmcSID = GetHMCSID();
  if (SID1 == hmcSID || SID2 == hmcSID) {
    return false;
  }

  {
    // Slot1, Slot2 are implementation details
    // Depending on the branch, they may not necessarily match the actual slots after the swap.
    CGameSlot Slot1 = m_Slots[SID1];
    CGameSlot Slot2 = m_Slots[SID2];

    if (!Slot1.GetIsSelectable() || !Slot2.GetIsSelectable()) {
      return false;
    }

    if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
      // don't swap the type, team, colour, race, or handicap
      m_Slots[SID1] = CGameSlot(Slot1.GetType(), Slot2.GetUID(), Slot2.GetDownloadStatus(), Slot2.GetSlotStatus(), Slot2.GetComputer(), Slot1.GetTeam(), Slot1.GetColor(), Slot1.GetRace(), Slot2.GetComputerType(), Slot1.GetHandicap());
      m_Slots[SID2] = CGameSlot(Slot2.GetType(), Slot1.GetUID(), Slot1.GetDownloadStatus(), Slot1.GetSlotStatus(), Slot1.GetComputer(), Slot2.GetTeam(), Slot2.GetColor(), Slot2.GetRace(), Slot1.GetComputerType(), Slot2.GetHandicap());
    } else {
      if (GetIsCustomForces()) {
        // except if custom forces is set, then we must preserve teams...
        const uint8_t teamOne = Slot1.GetTeam();
        const uint8_t teamTwo = Slot2.GetTeam();

        Slot1.SetTeam(teamTwo);
        Slot2.SetTeam(teamOne);

        // additionally, if custom forces is set, and exactly 1 of the slots is observer, then we must also preserve colors
        const uint8_t colorOne = Slot1.GetColor();
        const uint8_t colorTwo = Slot2.GetColor();
        if (teamOne != teamTwo && (teamOne == m_Map->GetVersionMaxSlots() || teamTwo == m_Map->GetVersionMaxSlots())) {
          Slot1.SetColor(colorTwo);
          Slot2.SetColor(colorOne);
        }
      }

      // swap everything (what we swapped already is reverted)
      m_Slots[SID1] = Slot2;
      m_Slots[SID2] = Slot1;
    }
  }

  uint8_t i = static_cast<uint8_t>(m_FakeUsers.size());
  while (i--) {
    uint8_t fakeSID = m_FakeUsers[i].GetSID();
    if (fakeSID == SID1) {
      m_FakeUsers[i].SetSID(SID2);
      m_FakeUsers[i].SetObserver(SID2 == m_Map->GetVersionMaxSlots());
    } else if (fakeSID == SID2) {
      m_FakeUsers[i].SetSID(SID1);
      m_FakeUsers[i].SetObserver(SID1 == m_Map->GetVersionMaxSlots());
    }
  }

  // Players that are at given slots afterwards.
  GameUser::CGameUser* PlayerOne = GetUserFromSID(SID1);
  GameUser::CGameUser* PlayerTwo = GetUserFromSID(SID2);
  if (PlayerOne) {
    PlayerOne->SetObserver(m_Slots[SID1].GetTeam() == m_Map->GetVersionMaxSlots());
    if (PlayerOne->GetIsObserver()) {
      PlayerOne->SetPowerObserver(PlayerOne->GetIsObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
      PlayerOne->ClearUserReady();
    }
  }
  if (PlayerTwo) {
    PlayerTwo->SetObserver(m_Slots[SID2].GetTeam() == m_Map->GetVersionMaxSlots());
    if (PlayerTwo->GetIsObserver()) {
      PlayerTwo->SetPowerObserver(PlayerTwo->GetIsObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
      PlayerTwo->ClearUserReady();
    }
  }

  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  return true;
}

bool CGame::OpenSlot(const uint8_t SID, const bool kick)
{
  const CGameSlot* slot = InspectSlot(SID);
  if (!slot || !slot->GetIsSelectable()) {
    return false;
  }
  if (m_Map->GetHMCEnabled() && SID + 1 == m_Map->GetHMCSlot()) {
    return false;
  }

  GameUser::CGameUser* user = GetUserFromSID(SID);
  if (user && !user->GetDeleteMe()) {
    if (!kick) return false;
    if (!user->HasLeftReason()) {
      user->SetLeftReason("was kicked when opening a slot");
    }
    // fromOpen = true, so that EventUserAfterDisconnect doesn't call OpenSlot() itself
    user->CloseConnection(true);
  } else if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) {
    ResetLayout(false);
  }
  if (user && m_CustomLayout == CUSTOM_LAYOUT_ONE_VS_ALL && slot->GetTeam() == m_CustomLayoutData.first) {
    ResetLayout(false);
  }
  if (!user && slot->GetIsPlayerOrFake()) {
    DeleteFakeUser(SID);
  }
  if (GetIsCustomForces()) {
    m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, slot->GetTeam(), slot->GetColor(), m_Map->GetLobbyRace(slot));
  } else {
    m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), SLOTRACE_RANDOM);
  }
  if (user && !GetHasAnotherPlayer(SID)) {
    EventLobbyLastPlayerLeaves();
  }
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  return true;
}

bool CGame::OpenSlot()
{
  uint8_t skipHMC = GetHMCSID();
  uint8_t SID = 0;
  while (SID < m_Slots.size()) {
    if (SID != skipHMC && m_Slots[SID].GetSlotStatus() == SLOTSTATUS_CLOSED) {
      return OpenSlot(SID, false);
    }
    ++SID;
  }
  return false;
}

bool CGame::CanLockSlotForJoins(const uint8_t SID)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot || !slot->GetIsSelectable()) {
    return false;
  }
  if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) {
    // Changing a closed slot for anything doesn't decrease the
    // amount of slots available for humans.
    return true;
  }
  const uint8_t openSlots = static_cast<uint8_t>(GetSlotsOpen());
  if (openSlots >= 2) {
    return true;
  }
  if (slot->GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
    if (openSlots >= 1) return true;
    return GetHasAnotherPlayer(SID);
  }

  return GetHasAnyUser();
}

bool CGame::CloseSlot(const uint8_t SID, const bool kick)
{
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  const CGameSlot* slot = InspectSlot(SID);
  const uint8_t openSlots = static_cast<uint8_t>(GetSlotsOpen());
  GameUser::CGameUser* user = GetUserFromSID(SID);
  if (user && !user->GetDeleteMe()) {
    if (!kick) return false;
    if (!user->HasLeftReason()) {
      user->SetLeftReason("was kicked when closing a slot");
    }
    user->CloseConnection();
  }
  if (slot->GetSlotStatus() == SLOTSTATUS_OPEN && openSlots == 1 && GetNumJoinedUsersOrFake() > 1) {
    DeleteVirtualHost();
  }
  if (!user && slot->GetIsPlayerOrFake()) {
    DeleteFakeUser(SID);
  }

  if (GetIsCustomForces()) {
    m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RST, SLOTSTATUS_CLOSED, SLOTCOMP_NO, slot->GetTeam(), slot->GetColor(), m_Map->GetLobbyRace(slot));
  } else {
    m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RST, SLOTSTATUS_CLOSED, SLOTCOMP_NO, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), SLOTRACE_RANDOM);
  }
  
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  return true;
}

bool CGame::CloseSlot()
{
  uint8_t SID = 0;
  while (SID < m_Slots.size()) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
      return CloseSlot(SID, false);
    }
    ++SID;
  }
  return false;
}

bool CGame::ComputerSlot(uint8_t SID, uint8_t skill, bool kick)
{
  if (SID >= static_cast<uint8_t>(m_Slots.size()) || skill > SLOTCOMP_HARD) {
    return false;
  }
  if (SID == GetHMCSID()) {
    return false;
  }

  CGameSlot Slot = m_Slots[SID];
  if (!Slot.GetIsSelectable()) {
    return false;
  }
  if (Slot.GetSlotStatus() != SLOTSTATUS_OCCUPIED && GetNumControllers() == m_Map->GetMapNumControllers()) {
    return false;
  }
  if (Slot.GetTeam() == m_Map->GetVersionMaxSlots()) {
    if (GetIsCustomForces()) {
      return false;
    }
  }
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  GameUser::CGameUser* Player = GetUserFromSID(SID);
  if (Player && !Player->GetDeleteMe()) {
    if (!kick) return false;
    if (!Player->HasLeftReason()) {
      Player->SetLeftReason("was kicked when creating a computer in a slot");
    }
    Player->CloseConnection();
  }

  // ignore layout, override computers
  if (ComputerSlotInner(SID, skill, true, true)) {
    if (GetSlotsOpen() == 0 && GetNumJoinedUsersOrFake() > 1) DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
  return true;
}

bool CGame::SetSlotTeam(const uint8_t SID, const uint8_t team, const bool force)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot || slot->GetTeam() == team || !slot->GetIsSelectable()) {
    return false;
  }
  if (GetIsCustomForces()) {
    const uint8_t newSID = GetSelectableTeamSlotFront(team, static_cast<uint8_t>(m_Slots.size()), static_cast<uint8_t>(m_Slots.size()), force);
    if (newSID == 0xFF) return false;
    return SwapSlots(SID, newSID);
  } else {
    const bool fromObservers = slot->GetTeam() == m_Map->GetVersionMaxSlots();
    const bool toObservers = team == m_Map->GetVersionMaxSlots();
    if (toObservers && !slot->GetIsPlayerOrFake()) return false;
    if (fromObservers && !toObservers && GetNumControllers() >= m_Map->GetMapNumControllers()) {
      // Observer cannot become controller if the map's controller limit has been reached.
      return false;
    }

    slot->SetTeam(team);
    if (toObservers || fromObservers) {
      if (toObservers) {
        slot->SetColor(m_Map->GetVersionMaxSlots());
        slot->SetRace(SLOTRACE_RANDOM);
      } else {
        slot->SetColor(GetNewColor());
        if (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          slot->SetRace(SLOTRACE_RANDOM);
        } else {
          slot->SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
        }
      }

      GameUser::CGameUser* user = GetUserFromUID(slot->GetUID());
      if (user) {
        user->SetObserver(toObservers);
        if (toObservers) {
          user->SetPowerObserver(!m_UsesCustomReferees && m_Map->GetMapObservers() == MAPOBS_REFEREES);
          user->ClearUserReady();
        } else {
          user->SetPowerObserver(false);
        }
      } else {
        CGameVirtualUser* virtualUserMatch = GetVirtualUserFromSID(SID);
        if (virtualUserMatch) {
          virtualUserMatch->SetObserver(toObservers);
        }
      }
    }

    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
    return true;
  }
}

bool CGame::SetSlotColor(const uint8_t SID, const uint8_t colour, const bool force)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot || slot->GetColor() == colour || !slot->GetIsSelectable()) {
    return false;
  }

  if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED || slot->GetTeam() == m_Map->GetVersionMaxSlots()) {
    // Only allow active users to choose their colors.
    //
    // Open/closed slots do actually have a color when Fixed Player Settings is enabled,
    // but I'm still not providing API for it.
    return false;
  }

  CGameSlot* takenSlot = nullptr;
  uint8_t takenSID = 0xFF;

  // if the requested color is taken, try to exchange colors
  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    CGameSlot* matchSlot = &(m_Slots[i]);
    if (matchSlot->GetColor() != colour) continue;
    if (!matchSlot->GetIsSelectable()) {
      return false;
    }
    if (!force) {
      // user request - may only use the color of an unoccupied slot
      // closed slots are okay, too (note that they only have a valid color when using Custom Forces)
      if (matchSlot->GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
        return false;
      }
    }
    takenSlot = matchSlot;
    takenSID = i;
    break;
  }

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    // With fixed user settings we try to swap slots.
    // This is not exposed to EventUserChangeColor,
    // but it's useful for !color.
    //
    // Old: !swap 3 7
    // Now: !color Arthas, teal
    if (!takenSlot) {
      // But we found no slot to swap with.
      return false;
    } else {
      SwapSlots(SID, takenSID); // Guaranteed to succeed at this point;
      m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
      return true;
    }
  } else {
    if (takenSlot) takenSlot->SetColor(m_Slots[SID].GetColor());
    m_Slots[SID].SetColor(colour);
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
    return true;
  }
}

void CGame::SetSlotTeamAndColorAuto(const uint8_t SID)
{
  // Custom Forces must use m_Slots[SID].GetColor() / m_Slots[SID].GetTeam()
  if (GetLayout() != MAPLAYOUT_ANY) return;
  CGameSlot* slot = GetSlot(SID);
  if (!slot) return;
  if (GetNumControllers() >= m_Map->GetMapNumControllers()) {
    return;
  }
  switch (GetCustomLayout()) {
    case CUSTOM_LAYOUT_ONE_VS_ALL:
      slot->SetTeam(m_CustomLayoutData.second);
      break;
    case CUSTOM_LAYOUT_HUMANS_VS_AI:
      if (slot->GetIsPlayerOrFake()) {
        slot->SetTeam(m_CustomLayoutData.first);
      } else {
        slot->SetTeam(m_CustomLayoutData.second);
      }      
      break;
    case CUSTOM_LAYOUT_FFA:
      slot->SetTeam(GetNewTeam());
      break;
    case CUSTOM_LAYOUT_DRAFT:
      // Player remains as observer until someone picks them.
      break;
    default: {
      bool otherTeamError = false;
      uint8_t otherTeam = m_Map->GetVersionMaxSlots();
      uint8_t numSkipped = 0;
      for (uint8_t i = 0; i < m_Slots.size(); ++i) {
        const CGameSlot* otherSlot = InspectSlot(i);
        if (otherSlot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
          if (i < SID) ++numSkipped;
          continue;
        }
        if (otherSlot->GetTeam() == m_Map->GetVersionMaxSlots()) {
          if (i < SID) ++numSkipped;
        } else if (otherTeam != m_Map->GetVersionMaxSlots()) {
          otherTeamError = true;
        } else {
          otherTeam = otherSlot->GetTeam();
        }
      }
      if (m_Map->GetMapNumControllers() == 2 && !otherTeamError && otherTeam < 2) {
        // Streamline team selection for 1v1 maps
        slot->SetTeam(1 - otherTeam);
      } else {
        slot->SetTeam((SID - numSkipped) % m_Map->GetMapNumTeams());
      }
      break;
    }
  }
  slot->SetColor(GetNewColor());
}

void CGame::OpenAllSlots()
{
  uint8_t skipHMC = GetHMCSID();
  bool anyChanged = false;
  uint8_t i = static_cast<uint8_t>(m_Slots.size());
  while (i--) {
    if (i != skipHMC && m_Slots[i].GetSlotStatus() == SLOTSTATUS_CLOSED) {
      m_Slots[i].SetSlotStatus(SLOTSTATUS_OPEN);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
}

uint8_t CGame::GetFirstCloseableSlot()
{
  bool hasPlayer = false;
  uint8_t firstSID = 0xFF;
  for (uint8_t SID = 0; SID < m_Slots.size(); ++SID) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
      if (firstSID == 0xFF) firstSID = SID + 1;
      if (hasPlayer) break;
    } else if (GetIsPlayerSlot(SID)) {
      hasPlayer = true;
      if (firstSID != 0xFF) break;
    }
  }

  if (hasPlayer) return 0;
  return firstSID;
}

bool CGame::CloseAllTeamSlots(const uint8_t team)
{
  const uint8_t firstSID = GetFirstCloseableSlot();
  if (firstSID == 0xFF) return false;

  bool anyChanged = false;
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (firstSID < SID--) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[SID].GetTeam() == team) {
      m_Slots[SID].SetSlotStatus(SLOTSTATUS_CLOSED);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    if (GetNumJoinedUsersOrFake() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }

  return anyChanged;
}

bool CGame::CloseAllTeamSlots(const bitset<MAX_SLOTS_MODERN> occupiedTeams)
{
  if (!GetIsCustomForces()) return false;
  const uint8_t firstSID = GetFirstCloseableSlot();
  if (firstSID == 0xFF) return false;

  bool anyChanged = false;
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (firstSID < SID--) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && occupiedTeams.test(m_Slots[SID].GetTeam())) {
      m_Slots[SID].SetSlotStatus(SLOTSTATUS_CLOSED);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    if (GetNumJoinedUsersOrFake() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }

  return anyChanged;
}

bool CGame::CloseAllSlots()
{
  const uint8_t firstSID = GetFirstCloseableSlot();
  if (firstSID == 0xFF) return false;

  bool anyChanged = false;
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (firstSID < SID--) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
      m_Slots[SID].SetSlotStatus(SLOTSTATUS_CLOSED);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    if (GetNumJoinedUsersOrFake() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }

  return anyChanged;
}

bool CGame::ComputerSlotInner(const uint8_t SID, const uint8_t skill, const bool ignoreLayout, const bool overrideComputers)
{
  const CGameSlot* slot = InspectSlot(SID);
  if ((!ignoreLayout || GetIsPlayerSlot(SID)) && slot->GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
    return false;
  }
  if (!overrideComputers && slot->GetIsComputer()) {
    return false;
  }
  if (SID == GetHMCSID()) {
    return false;
  }

  // !comp NUMBER bypasses current layout, so it may
  // add computers in closed slots (in regular layouts), or
  // in open slots (in HUMANS_VS_AI)
  // if it does, reset the layout
  bool resetLayout = false;
  if (m_CustomLayout == CUSTOM_LAYOUT_HUMANS_VS_AI) {
    if (slot->GetSlotStatus() == SLOTSTATUS_OPEN || (GetIsCustomForces() && slot->GetTeam() != m_CustomLayoutData.second)) {
      if (ignoreLayout) {
        resetLayout = true;
      } else {
        return false;
      }
    }
  } else {
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) {
      if (!ignoreLayout) {
        return false;
      }
    }
  }
  if (GetIsCustomForces()) {
    if (slot->GetTeam() == m_Map->GetVersionMaxSlots()) {
      return false;
    }
    if (slot->GetIsPlayerOrFake()) DeleteFakeUser(SID);
    m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RDY, SLOTSTATUS_OCCUPIED, SLOTCOMP_YES, slot->GetTeam(), slot->GetColor(), m_Map->GetLobbyRace(slot), skill);
    if (resetLayout) ResetLayout(false);
  } else {
    if (slot->GetIsPlayerOrFake()) DeleteFakeUser(SID);
    m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RDY, SLOTSTATUS_OCCUPIED, SLOTCOMP_YES, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), m_Map->GetLobbyRace(slot), skill);
    SetSlotTeamAndColorAuto(SID);
  }
  return true;
}

bool CGame::ComputerNSlots(const uint8_t skill, const uint8_t expectedCount, const bool ignoreLayout, const bool overrideComputers)
{
  uint8_t currentCount = GetNumComputers();
  if (expectedCount == currentCount) {
    // noop
    return true;
  }

  if (expectedCount < currentCount) {
    uint8_t SID = static_cast<uint8_t>(m_Slots.size());
    while (SID--) {
      if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetIsComputer()) {
        if (OpenSlot(SID, false) && --currentCount == expectedCount) {
          if (m_CustomLayout == CUSTOM_LAYOUT_HUMANS_VS_AI && currentCount == 0) ResetLayout(false);
          return true;
        }
      }
    }
    return false;
  }

  if (m_Map->GetMapNumControllers() <= GetNumControllers()) {
    return false;
  }

  const bool hasUsers = GetHasAnyUser(); // Ensure this is called outside the loop.
  uint8_t remainingControllers = m_Map->GetMapNumControllers() - GetNumControllers();
  if (!hasUsers) --remainingControllers; // Refuse to lock the last slot
  if (expectedCount - currentCount > remainingControllers) {
    return false;
  }
  uint8_t remainingComputers = overrideComputers ? expectedCount : (expectedCount - currentCount);
  uint8_t SID = 0;
  while (0 < remainingComputers && SID < m_Slots.size()) {
    // overrideComputers false means only newly added computers are counted in remainingComputers
    if (ComputerSlotInner(SID, skill, ignoreLayout, overrideComputers)) {
      --remainingComputers;
    }
    ++SID;
  }

  if (GetSlotsOpen() == 0 && GetNumJoinedUsersOrFake() > 1) DeleteVirtualHost();
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);

  return remainingComputers == 0;
}

bool CGame::ComputerAllSlots(const uint8_t skill)
{
  if (m_Map->GetMapNumControllers() <= GetNumControllers()) {
    return false;
  }

  const bool hasUsers = GetHasAnyUser(); // Ensure this is called outside the loop.
  uint32_t remainingSlots = m_Map->GetMapNumControllers() - GetNumControllers();

  // Refuse to lock the last slot
  if (!hasUsers && m_Slots.size() == m_Map->GetMapNumControllers()) {
    --remainingSlots;
  }

  if (remainingSlots == 0) {
    return false;
  }

  uint8_t SID = 0;
  while (0 < remainingSlots && SID < m_Slots.size()) {
    // don't ignore layout, don't override computers
    if (ComputerSlotInner(SID, skill, false, false)) {
      --remainingSlots;
    }
    ++SID;
  }

  if (GetSlotsOpen() == 0 && GetNumJoinedUsersOrFake() > 1) DeleteVirtualHost();
  m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  return true;
}

void CGame::ShuffleSlots()
{
  // we only want to shuffle the user slots (exclude observers)
  // that means we need to prevent this function from shuffling the open/closed/computer slots too
  // so we start by copying the user slots to a temporary vector

  vector<CGameSlot> PlayerSlots;

  for (auto& slot : m_Slots) {
    if (slot.GetIsPlayerOrFake() && slot.GetTeam() != m_Map->GetVersionMaxSlots()) {
      PlayerSlots.push_back(slot);
    }
  }

  // now we shuffle PlayerSlots

  if (GetIsCustomForces())
  {
    // rather than rolling our own probably broken shuffle algorithm we use random_shuffle because it's guaranteed to do it properly
    // so in order to let random_shuffle do all the work we need a vector to operate on
    // unfortunately we can't just use PlayerSlots because the team/colour/race shouldn't be modified
    // so make a vector we can use

    vector<uint8_t> SIDs;

    for (uint8_t i = 0; i < PlayerSlots.size(); ++i)
      SIDs.push_back(i);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(begin(SIDs), end(SIDs), g);

    // now put the PlayerSlots vector in the same order as the SIDs vector

    vector<CGameSlot> Slots;

    // as usual don't modify the type/team/colour/race

    for (uint8_t i = 0; i < SIDs.size(); ++i)
      Slots.emplace_back(PlayerSlots[SIDs[i]].GetType(), PlayerSlots[SIDs[i]].GetUID(), PlayerSlots[SIDs[i]].GetDownloadStatus(), PlayerSlots[SIDs[i]].GetSlotStatus(), PlayerSlots[SIDs[i]].GetComputer(), PlayerSlots[i].GetTeam(), PlayerSlots[i].GetColor(), PlayerSlots[i].GetRace());

    PlayerSlots = Slots;
  }
  else
  {
    // regular game
    // it's easy when we're allowed to swap the team/colour/race!

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(begin(PlayerSlots), end(PlayerSlots), g);
  }

  // now we put m_Slots back together again

  auto              CurrentPlayer = begin(PlayerSlots);
  vector<CGameSlot> Slots;

  for (auto& slot : m_Slots) {
    if (slot.GetIsPlayerOrFake() && slot.GetTeam() != m_Map->GetVersionMaxSlots()) {
      Slots.push_back(*CurrentPlayer);
      ++CurrentPlayer;
    } else {
      Slots.push_back(slot);
    }
  }

  m_Slots = Slots;
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
}

void CGame::ReportSpoofed(const string& server, GameUser::CGameUser* user)
{
  if (!m_IsHiddenPlayerNames) {
    SendAllChat("Name spoof detected. The real [" + user->GetName() + "@" + server + "] is not in this game.");
  }
  if (GetIsLobbyStrict() && MatchOwnerName(user->GetName())) {
    if (!user->HasLeftReason()) {
      user->SetLeftReason("autokicked - spoofing the game owner");
    }
    user->CloseConnection();
  }
}

void CGame::AddToRealmVerified(const string& server, GameUser::CGameUser* Player, bool sendMessage)
{
  // Must only be called on a lobby, for many reasons (e.g. GetName() used instead of GetDisplayName())
  Player->SetRealmVerified(true);
  if (sendMessage) {
    if (!m_IsHiddenPlayerNames && MatchOwnerName(Player->GetName()) && m_OwnerRealm == Player->GetRealmHostName()) {
      SendAllChat("Identity accepted for game owner [" + Player->GetName() + "@" + server + "]");
    } else {
      SendChat(Player, "Identity accepted for [" + Player->GetName() + "@" + server + "]");
    }
  }
}

void CGame::AddToReserved(const string& name)
{
  if (m_RestoredGame && m_Reserved.size() >= m_Map->GetVersionMaxSlots()) {
    return;
  }

  string inputLower = ToLowerCase(name);

  // check that the user is not already reserved
  for (const auto& element : m_Reserved) {
    string matchLower = ToLowerCase(element);
    if (matchLower == inputLower) {
      return;
    }
  }

  m_Reserved.push_back(name);

  // upgrade the user if they're already in the game

  for (auto& user : m_Users) {
    string matchLower = ToLowerCase(user->GetName());

    if (matchLower == inputLower) {
      user->SetReserved(true);
      break;
    }

    // Reserved users are never kicked for latency reasons.
    user->RemoveKickReason(GameUser::KickReason::HIGH_PING);
    if (!user->GetAnyKicked() && user->GetKickQueued()) {
      user->ClearKickByTicks();
    }
  }
}

void CGame::RemoveFromReserved(const string& name)
{
  if (m_Reserved.empty()) return;

  uint8_t index = GetReservedIndex(name);
  if (index == 0xFF) {
    return;
  }
  m_Reserved.erase(m_Reserved.begin() + index);

  // demote the user if they're already in the game
  GameUser::CGameUser* matchPlayer = GetUserFromName(name, false);
  if (matchPlayer) {
    matchPlayer->SetReserved(false);
  }
}

void CGame::RemoveAllReserved()
{
  m_Reserved.clear();
  for (auto& user : m_Users) {
    user->SetReserved(false);
  }
}

bool CGame::MatchOwnerName(const string& name) const
{
  string matchLower = ToLowerCase(name);
  string ownerLower = ToLowerCase(m_OwnerName);
  return matchLower == ownerLower;
}

uint8_t CGame::GetReservedIndex(const string& name) const
{
  string inputLower = ToLowerCase(name);

  uint8_t index = 0;
  while (index < m_Reserved.size()) {
    string matchLower = ToLowerCase(m_Reserved[index]);
    if (matchLower == inputLower) {
      break;
    }
    ++index;
  }

  if (index == m_Reserved.size()) {
    return 0xFF;
  }

  return index;
}

string CGame::GetBannableIP(const string& name, const string& hostName) const
{
  for (const CDBBan* bannable : m_Bannables) {
    if (bannable->GetName() == name && bannable->GetServer() == hostName) {
      return bannable->GetIP();
    }
  }
  return string();
}

bool CGame::GetIsScopeBanned(const string& rawName, const string& hostName, const string& addressLiteral) const
{
  string name = ToLowerCase(rawName);

  bool checkIP = false;
  if (!addressLiteral.empty()) {
    optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(addressLiteral);
    checkIP = maybeAddress.has_value() && !isLoopbackAddress(&(maybeAddress.value()));
  }
  for (const CDBBan* ban : m_ScopeBans) {
    if (ban->GetName() == name && ban->GetServer() == hostName) {
      return true;
    }
    if (checkIP && ban->GetIP() == addressLiteral) {
      return true;
    }
  }
  return false;
}

bool CGame::CheckScopeBanned(const string& rawName, const string& hostName, const string& addressLiteral)
{
  if (GetIsScopeBanned(rawName, hostName, addressLiteral)) {
    if (m_ReportedJoinFailNames.find(rawName) == end(m_ReportedJoinFailNames)) {
      LOG_APP_IF(LOG_LEVEL_INFO, "user [" + rawName + "@" + hostName + "|" + addressLiteral + "] entry denied: game-scope banned")
      SendAllChat("[" + rawName + "@" + hostName + "] is trying to join the game, but is banned");
      m_ReportedJoinFailNames.insert(rawName);
    } else {
      LOG_APP_IF(LOG_LEVEL_DEBUG, "user [" + rawName + "@" + hostName + "|" + addressLiteral + "] entry denied: game-scope banned")
    }
    return true;
  }
  return false;
}

bool CGame::AddScopeBan(const string& rawName, const string& hostName, const string& addressLiteral)
{
  if (m_ScopeBans.size() >= MAX_SCOPE_BANS) {
    return false;
  }

  string name = ToLowerCase(rawName);

  m_ScopeBans.push_back(new CDBBan(
    name,
    hostName,
    string(), // auth server
    addressLiteral,
    string(), // date
    string(), // expiry
    false, // temporary ban (permanent == false)
    string(), // moderator
    string() // reason
  ));
  return true;
}

bool CGame::RemoveScopeBan(const string& rawName, const string& hostName)
{
  string name = ToLowerCase(rawName);

  for (auto it = begin(m_ScopeBans); it != end(m_ScopeBans); ++it) {
    if ((*it)->GetName() == name && (*it)->GetServer() == hostName) {
      it = m_ScopeBans.erase(it);
      return true;
    }
  }
  return false;
}

vector<uint32_t> CGame::GetPlayersFramesBehind() const
{
  uint8_t i = static_cast<uint8_t>(m_Users.size());
  vector<uint32_t> framesBehind(i, 0);
  while (i--) {
    if (m_Users[i]->GetIsObserver()) {
      continue;
    }
    if (m_SyncCounter <= m_Users[i]->GetNormalSyncCounter()) {
      continue;
    }
    framesBehind[i] = m_SyncCounter - m_Users[i]->GetNormalSyncCounter();
  }
  return framesBehind;
}

UserList CGame::GetLaggingUsers() const
{
  UserList laggingPlayers;
  if (!m_Lagging) return laggingPlayers;
  for (const auto& user : m_Users) {
    if (!user->GetLagging()) {
      continue;
    }
    laggingPlayers.push_back(user);
  }
  return laggingPlayers;
}

uint8_t CGame::CountLaggingPlayers() const
{
  uint8_t count = 0;
  if (!m_Lagging) return count;
  for (const auto& user : m_Users) {
    if (!user->GetLagging()) {
      continue;
    }
    ++count;
  }
  return count;
}

UserList CGame::CalculateNewLaggingPlayers() const
{
  UserList laggingPlayers;
  if (!m_Lagging) return laggingPlayers;
  for (const auto& user : m_Users) {
    if (user->GetIsObserver()) {
      continue;
    }
    if (user->GetLagging() || user->GetGProxyDisconnectNoticeSent() || user->GetDisconnectedUnrecoverably()) {
      continue;
    }
    if (!user->GetFinishedLoading() || user->GetIsBehindFramesNormal(GetSyncLimitSafe())) {
      laggingPlayers.push_back(user);
    }
  }
  return laggingPlayers;
}

void CGame::RemoveFromLagScreens(GameUser::CGameUser* user) const
{
  for (const auto& otherUser : m_Users) {
    if (user == otherUser || otherUser->GetIsInLoadingScreen()) {
      continue;
    }
    LOG_APP_IF(LOG_LEVEL_INFO, "@[" + otherUser->GetName() + "] lagger update (-" + user->GetName() + ")")
    Send(otherUser, GameProtocol::SEND_W3GS_STOP_LAG(user));
  }
}

void CGame::ResetLagScreen()
{
  const UserList laggingPlayers = GetLaggingUsers();
  if (laggingPlayers.empty()) {
    return;
  }
  const vector<uint8_t> startLagPacket = GameProtocol::SEND_W3GS_START_LAG(laggingPlayers);
  const bool anyUsingGProxy = GetAnyUsingGProxy();

  if (m_GameLoading) {
    ++m_BeforePlayingEmptyActions;
  }

  for (auto& user : m_Users) {
    if (user->GetFinishedLoading()) {
      for (auto& otherUser : m_Users) {
        if (!otherUser->GetLagging()) continue;
        LOG_APP_IF(LOG_LEVEL_INFO, "@[" + user->GetName() + "] lagger update (-" + otherUser->GetName() + ")")
        Send(user, GameProtocol::SEND_W3GS_STOP_LAG(otherUser));
      }

      Send(user, GameProtocol::GetEmptyAction());
      /*
      user->AddSyncCounterOffset(1);
      */

      // GProxy sends these empty actions itself for every action received.
      // So we need to match it, to avoid desyncs.
      if (anyUsingGProxy && !user->GetGProxyAny()) {
        Send(user, GameProtocol::SEND_W3GS_EMPTY_ACTIONS(m_GProxyEmptyActions));

        // Warcraft III doesn't respond to empty actions,
        // so we need to artificially increase users' sync counters.
        /*
        user->AddSyncCounterOffset(m_GProxyEmptyActions);
        */
      }

      LOG_APP_IF(LOG_LEVEL_INFO, "@[" + user->GetName() + "] lagger update (+" + ToNameListSentence(laggingPlayers) + ")")
      Send(user, startLagPacket);

      if (m_GameLoading) {
        SendChat(user, "Please wait for " + to_string(laggingPlayers.size()) + " player(s) to load the game.");
      }
    } else {
      // Warcraft III doesn't respond to empty actions,
      // so we need to artificially increase users' sync counters.
      /*
      user->AddSyncCounterOffset(1);
      if (anyUsingGProxy && !user->GetGProxyAny()) {
        user->AddSyncCounterOffset(m_GProxyEmptyActions);
      }
      */
    }
  }

  m_LastLagScreenResetTime = GetTime();
}

void CGame::ResetLatency()
{
  m_Config.m_Latency = m_Aura->m_GameDefaultConfig->m_Latency;
  m_Config.m_SyncLimit = m_Aura->m_GameDefaultConfig->m_SyncLimit;
  m_Config.m_SyncLimitSafe = m_Aura->m_GameDefaultConfig->m_SyncLimitSafe;
  for (auto& user : m_Users)  {
    user->ResetSyncCounterOffset();
  }
}

void CGame::NormalizeSyncCounters() const
{
  for (auto& user : m_Users) {
    if (user->GetIsObserver()) continue;
    uint32_t normalSyncCounter = user->GetNormalSyncCounter();
    if (m_SyncCounter <= normalSyncCounter) {
      continue;
    }
    user->AddSyncCounterOffset(m_SyncCounter - normalSyncCounter);
  }
}

bool CGame::GetIsReserved(const string& name) const
{
  return GetReservedIndex(name) < m_Reserved.size();
}

bool CGame::GetIsProxyReconnectable() const
{
  if (m_IsMirror) return 0 != m_Config.m_ReconnectionMode;
  return 0 != (m_Aura->m_Net.m_Config.m_ProxyReconnect & m_Config.m_ReconnectionMode);
}

bool CGame::GetIsProxyReconnectableLong() const
{
  if (m_IsMirror) return 0 != (m_Config.m_ReconnectionMode & RECONNECT_ENABLED_GPROXY_EXTENDED);
  return 0 != ((m_Aura->m_Net.m_Config.m_ProxyReconnect & m_Config.m_ReconnectionMode) & RECONNECT_ENABLED_GPROXY_EXTENDED);
}

bool CGame::IsDownloading() const
{
  // returns true if at least one user is downloading the map

  for (auto& user : m_Users)
  {
    if (user->GetDownloadStarted() && !user->GetDownloadFinished())
      return true;
  }

  return false;
}

void CGame::UncacheOwner()
{
  for (auto& user : m_Users) {
    user->SetOwner(false);
  }
}

void CGame::SetOwner(const string& name, const string& realm)
{
  m_OwnerName = name;
  m_OwnerRealm = realm;
  m_LastOwnerAssigned = GetTicks();

  UncacheOwner();

  GameUser::CGameUser* user = GetUserFromName(name, false);
  if (user && user->GetRealmHostName() == realm) {
    user->SetOwner(true);

    // Owner is never kicked for latency reasons.
    user->RemoveKickReason(GameUser::KickReason::HIGH_PING);
    if (!user->GetAnyKicked() && user->GetKickQueued()) {
      user->ClearKickByTicks();
    }
  }
}

void CGame::ReleaseOwner()
{
  if (m_Exiting) {
    return;
  }
  LOG_APP_IF(LOG_LEVEL_INFO, "[LOBBY: "  + m_GameName + "] Owner \"" + m_OwnerName + "@" + ToFormattedRealm(m_OwnerRealm) + "\" removed.")
  m_LastOwner = m_OwnerName;
  m_OwnerName.clear();
  m_OwnerRealm.clear();
  UncacheOwner();
  ResetLayout(false);
  m_Locked = false;
  SendAllChat("This game is now ownerless. Type " + GetCmdToken() + "owner to take ownership of this game.");
}

void CGame::ResetDraft()
{
  m_IsDraftMode = true;
  for (auto& user : m_Users) {
    user->SetDraftCaptain(0);
  }
}

void CGame::ResetTeams(const bool alsoCaptains)
{
  if (!(m_Map->GetMapObservers() == MAPOBS_ALLOWED || m_Map->GetMapObservers() == MAPOBS_REFEREES)) {
    return;
  }
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (SID--) {
    CGameSlot* slot = GetSlot(SID);
    if (slot->GetTeam() == m_Map->GetVersionMaxSlots()) continue;
    if (!slot->GetIsPlayerOrFake()) continue;
    if (!alsoCaptains) {
      GameUser::CGameUser* user = GetUserFromSID(SID);
      if (user && user->GetIsDraftCaptain()) continue;
    }
    if (!SetSlotTeam(SID, m_Map->GetVersionMaxSlots(), false)) {
      break;
    }
  }
}

void CGame::ResetSync()
{
  m_SyncCounter = 0;
  for (auto& TargetPlayer: m_Users) {
    TargetPlayer->SetSyncCounter(0);
  }
}

void CGame::CountKickVotes()
{
  uint32_t Votes = 0, VotesNeeded = static_cast<uint32_t>(ceil((GetNumJoinedPlayers() - 1) * static_cast<float>(m_Config.m_VoteKickPercentage) / 100));
  for (auto& eachPlayer : m_Users) {
    if (eachPlayer->GetKickVote().value_or(false))
      ++Votes;
  }

  if (Votes >= VotesNeeded) {
    GameUser::CGameUser* victim = GetUserFromName(m_KickVotePlayer, true);

    if (victim) {
      if (!victim->HasLeftReason()) {
        victim->SetLeftReason("was kicked by vote");
        victim->SetLeftCode(PLAYERLEAVE_LOST);
      }
      victim->CloseConnection();

      Log("votekick against user [" + m_KickVotePlayer + "] passed with " + to_string(Votes) + "/" + to_string(GetNumJoinedPlayers()) + " votes");
      SendAllChat("A votekick against user [" + m_KickVotePlayer + "] has passed");
    } else {
      LOG_APP_IF(LOG_LEVEL_ERROR, "votekick against user [" + m_KickVotePlayer + "] errored")
    }

    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }
}

bool CGame::GetCanStartGracefulCountDown() const
{
  if (m_CountDownStarted || m_ChatOnly) {
    return false;
  }

  if (m_Aura->m_StartedGames.size() >= m_Aura->m_Config.m_MaxStartedGames) {
    return false;
  }

  if (m_HCLCommandString.size() > GetSlotsOccupied()) {
    return false;
  }

  bool enoughTeams = false;
  uint8_t sameTeam = m_Map->GetVersionMaxSlots();
  for (const auto& slot : m_Slots) {
    if (slot.GetIsPlayerOrFake() && slot.GetDownloadStatus() != 100) {
      GameUser::CGameUser* Player = GetUserFromUID(slot.GetUID());
      if (Player) {
        return false;
      }
    }
    if (slot.GetTeam() != m_Map->GetVersionMaxSlots()) {
      if (sameTeam == m_Map->GetVersionMaxSlots()) {
        sameTeam = slot.GetTeam();
      } else if (sameTeam != slot.GetTeam()) {
        enoughTeams = true;
      }
    }
  }

  if (0 == m_ControllersWithMap) {
    return false;
  } else if (m_ControllersWithMap < 2 && !m_RestoredGame) {
    return false;
  } else if (!enoughTeams) {
    return false;
  }

  if (GetNumJoinedPlayers() >= 2) {
    for (const auto& user : m_Users) {
      if (user->GetIsReserved() || user->GetIsOwner(nullopt) || user->GetIsObserver()) {
        continue;
      }
      if (!user->GetIsRTTMeasuredConsistent()) {
        return false;
      } else if (user->GetPingKicked()) {
        return false;
      }
    }
  }

  for (const auto& user : m_Users) {
    // Skip non-referee observers
    if (!user->GetIsOwner(nullopt) && user->GetIsObserver()) {
      if (m_Map->GetMapObservers() != MAPOBS_REFEREES) continue;
      if (m_UsesCustomReferees && !user->GetIsPowerObserver()) continue;
    }

    CRealm* realm = user->GetRealm(false);
    if (realm && realm->GetUnverifiedCannotStartGame() && !user->IsRealmVerified()) {
      return false;
    }
  }

  if (m_LastPlayerLeaveTicks.has_value() && GetTicks() < m_LastPlayerLeaveTicks.value() + 2000) {
    return false;
  }

  return true;
}

void CGame::StartCountDown(bool fromUser, bool force)
{
  if (m_CountDownStarted)
    return;

  if (m_ChatOnly) {
    SendAllChat("This lobby is in chat-only mode. Please join another hosted game.");
    const CGame* recentLobby = m_Aura->GetMostRecentLobby();
    if (recentLobby && recentLobby != this) {
      SendAllChat("Currently hosting: " + recentLobby->GetStatusDescription());
    }
    return;
  }

  if (m_Aura->m_StartedGames.size() >= m_Aura->m_Config.m_MaxStartedGames) {
    SendAllChat("This game cannot be started while there are " +  to_string(m_Aura->m_Config.m_MaxStartedGames) + " additional games in progress.");
    return;
  }

  if (m_Map->GetHMCEnabled()) {
    const uint8_t SID = m_Map->GetHMCSlot() - 1;
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || !slot->GetIsPlayerOrFake() || GetUserFromSID(SID)) {
      SendAllChat("This game requires a fake player on slot " + ToDecString(SID + 1));
      return;
    }
    const CGameVirtualUser* virtualUserMatch = InspectVirtualUserFromSID(SID);
    if (virtualUserMatch && virtualUserMatch->GetIsObserver()) {
      SendAllChat("This game requires a fake player (not observer) on slot " + ToDecString(SID + 1));
      return;
    }
    if (!virtualUserMatch && m_Map->GetHMCRequired()) {
      SendAllChat("This game requires a fake player on slot " + ToDecString(SID + 1));
      return;
    }
  }

  // if the user sent "!start force" skip the checks and start the countdown
  // otherwise check that the game is ready to start

  uint8_t sameTeam = m_Map->GetVersionMaxSlots();

  if (force) {
    for (const auto& user : m_Users) {
      bool shouldKick = !user->GetMapReady();
      if (!shouldKick) {
        CRealm* realm = user->GetRealm(false);
        if (realm && realm->GetUnverifiedCannotStartGame() && !user->IsRealmVerified()) {
          shouldKick = true;
        }
      }
      if (shouldKick) {
        if (!user->HasLeftReason()) {
          user->SetLeftReason("kicked when starting the game");
        }
        user->CloseConnection();
        CloseSlot(GetSIDFromUID(user->GetUID()), true);
      }
    }
  } else {
    bool ChecksPassed = true;
    bool enoughTeams = false;

    // check if the HCL command string is short enough
    if (m_HCLCommandString.size() > GetSlotsOccupied()) {
      SendAllChat("The HCL command string is too long. Use [" + GetCmdToken() + "go force] to start anyway");
      ChecksPassed = false;
    }

    UserList downloadingUsers;

    // check if everyone has the map
    for (const auto& slot : m_Slots) {
      if (slot.GetIsPlayerOrFake() && slot.GetDownloadStatus() != 100) {
        GameUser::CGameUser* player = GetUserFromUID(slot.GetUID());
        if (player) downloadingUsers.push_back(player);
      }
      if (slot.GetTeam() != m_Map->GetVersionMaxSlots()) {
        if (sameTeam == m_Map->GetVersionMaxSlots()) {
          sameTeam = slot.GetTeam();
        } else if (sameTeam != slot.GetTeam()) {
          enoughTeams = true;
        }
      }
    }
    if (!downloadingUsers.empty()) {
      SendAllChat("Players still downloading the map: " + ToNameListSentence(downloadingUsers));
      ChecksPassed = false;
    } else if (0 == m_ControllersWithMap) {
      SendAllChat("Nobody has downloaded the map yet.");
      ChecksPassed = false;
    } else if (m_ControllersWithMap < 2 && !m_RestoredGame) {
      SendAllChat("Only " + to_string(m_ControllersWithMap) + " user has the map.");
      ChecksPassed = false;
    } else if (!enoughTeams) {
      SendAllChat("Players are not arranged in teams.");
      ChecksPassed = false;
    }

    UserList highPingUsers;
    UserList pingNotMeasuredUsers;
    UserList unverifiedUsers;

    // check if everyone's ping is measured and acceptable
    if (GetNumJoinedPlayers() >= 2) {
      for (const auto& user : m_Users) {
        if (user->GetIsReserved() || user->GetIsOwner(nullopt) || user->GetIsObserver()) {
          continue;
        }
        if (!user->GetIsRTTMeasuredConsistent()) {
          pingNotMeasuredUsers.push_back(user);
        } else if (user->GetPingKicked()) {
          highPingUsers.push_back(user);
        }
      }
    }

    for (const auto& user : m_Users) {
      // Skip non-referee observers
      if (!user->GetIsOwner(nullopt) && user->GetIsObserver()) {
        if (m_Map->GetMapObservers() != MAPOBS_REFEREES) continue;
        if (m_UsesCustomReferees && !user->GetIsPowerObserver()) continue;
      }
      CRealm* realm = user->GetRealm(false);
      if (realm && realm->GetUnverifiedCannotStartGame() && !user->IsRealmVerified()) {
        unverifiedUsers.push_back(user);
      }
    }

    if (!highPingUsers.empty()) {
      SendAllChat("Players with high ping: " + ToNameListSentence(highPingUsers));
      ChecksPassed = false;
    }
    if (!pingNotMeasuredUsers.empty()) {
      SendAllChat("Players NOT yet pinged thrice: " + ToNameListSentence(pingNotMeasuredUsers));
      ChecksPassed = false;
    }
    if (!unverifiedUsers.empty()) {
      SendAllChat("Players NOT verified (whisper sc): " + ToNameListSentence(unverifiedUsers));
      ChecksPassed = false;
    }
    if (m_LastPlayerLeaveTicks.has_value() && GetTicks() < m_LastPlayerLeaveTicks.value() + 2000) {
      SendAllChat("Someone left the game less than two seconds ago!");
      ChecksPassed = false;
    }

    if (!ChecksPassed)
      return;
  }

  m_Replaceable = false;
  m_CountDownStarted = true;
  m_CountDownUserInitiated = fromUser;
  m_CountDownCounter = m_Config.m_LobbyCountDownStartValue;

  if (!m_KickVotePlayer.empty()) {
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  for (auto& user : m_Users) {
    if (!user->GetDisconnected()) {
      user->ResetKickReason();
      user->ResetLeftReason();
    }
    if (user->GetKickQueued()) {
      user->ClearKickByTicks();
    }
  }

  if (GetNumJoinedUsersOrFake() == 1 && (0 == GetSlotsOpen() || m_Map->GetMapObservers() != MAPOBS_REFEREES)) {
    SendAllChat("HINT: Single-user game detected. In-game commands will be DISABLED.");
    if (GetNumOccupiedSlots() != m_Map->GetVersionMaxSlots()) {
      SendAllChat("HINT: To avoid this, you may enable map referees, or add a fake user [" + GetCmdToken() + "fp]");
    }
  }

  if (!m_FakeUsers.empty()) {
    SendAllChat("HINT: " + to_string(m_FakeUsers.size()) + " slots are occupied by fake users.");
  }
}

void CGame::StartCountDownFast(bool fromUser)
{
  StartCountDown(fromUser, true);
  if (m_CountDownStarted) {
    // 500 ms countdown
    m_CountDownCounter = 1;
    m_CountDownFast = true;
  }
}

void CGame::StopCountDown()
{
  m_CountDownStarted = false;
  m_CountDownFast = false;
  m_CountDownUserInitiated = false;
  m_CountDownCounter = 0;
}

bool CGame::StopPlayers(const string& reason)
{
  // disconnect every user and set their left reason to the passed string
  // we use this function when we want the code in the Update function to run before the destructor (e.g. saving users to the database)
  // therefore calling this function when m_GameLoading || m_GameLoaded is roughly equivalent to setting m_Exiting = true
  // the only difference is whether the code in the Update function is executed or not

  bool anyStopped = false;
  for (auto& user : m_Users) {
    if (user->GetDeleteMe()) continue;
    user->SetLeftReason(reason);
    user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
    user->TrySetEnding();
    user->DisableReconnect();
    user->CloseConnection();
    user->SetDeleteMe(true);
    anyStopped = true;
  }
  m_PauseUser = nullptr;
  return anyStopped;
}

void CGame::StopLagger(GameUser::CGameUser* user, const string& reason)
{
  RemoveFromLagScreens(user);
  user->SetLeftReason(reason);
  user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
  user->DisableReconnect();
  user->CloseConnection();
  user->SetLagging(false);

  if (!user->GetIsEndingOrEnded()) {
    Resume(user, user->GetPingEqualizerFrame(), true);
    QueueLeftMessage(user);
  }
}

/*
 * CGame::StopLaggers(const string& reason)
 * When load-in-game is enabled, this will also drop users that haven't finished loading.
 */
void CGame::StopLaggers(const string& reason)
{
  UserList laggingUsers = GetLaggingUsers();
  for (const auto& user : laggingUsers) {
    StopLagger(user, reason);
  }
  for (const auto& user : laggingUsers) {
    if (TrySaveOnDisconnect(user, false)) {
      break;
    }
  }
  ResetDropVotes();
}

void CGame::ResetDropVotes() const
{
  for (auto& user : m_Users) {
    user->SetDropVote(false);
  }
}

void CGame::StopDesynchronized(const string& reason)
{
  uint8_t majorityThreshold = static_cast<uint8_t>(m_Users.size() / 2);
  for (GameUser::CGameUser* user : m_Users) {
    auto it = m_SyncPlayers.find(static_cast<const GameUser::CGameUser*>(user));
    if (it == m_SyncPlayers.end()) {
      continue;
    }
    if ((it->second).size() < majorityThreshold) {
      user->SetLeftReason(reason);
      user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
      user->DisableReconnect();
      user->CloseConnection();

      if (!user->GetIsEndingOrEnded()) {
        Resume(user, user->GetPingEqualizerFrame(), true);
        QueueLeftMessage(user);
      }
    }
  }
}

void CGame::StopLoadPending(const string& reason)
{
  if (m_Config.m_LoadInGame) {
    StopLaggers(reason);
  } else {
    for (GameUser::CGameUser* user : m_Users) {
      if (user->GetFinishedLoading()) {
        continue;
      }
      user->SetLeftReason(reason);
      user->SetLeftCode(PLAYERLEAVE_DISCONNECT);
      user->DisableReconnect();
      user->CloseConnection();
    }
  }
}

string CGame::GetSaveFileName(const uint8_t UID) const
{
  auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &now);
#else
  localtime_r(&now, &timeinfo);
#endif

  ostringstream oss;
  oss << put_time(&timeinfo, "%m-%d_%H-%M");
  return "auto_p" + ToDecString(GetSIDFromUID(UID) + 1) + "_" + oss.str() + ".w3z";
}

bool CGame::Save(GameUser::CGameUser* user, CQueuedActionsFrame& actionFrame, const bool isDisconnect)
{
  const uint8_t UID = SimulateActionUID(ACTION_SAVE, user, isDisconnect);
  if (UID == 0xFF) return false;

  string fileName = GetSaveFileName(UID);
  LOG_APP_IF(LOG_LEVEL_INFO, "saving as " + fileName)

  {
    vector<uint8_t> ActionStart;
    ActionStart.push_back(ACTION_SAVE);
    AppendByteArray(ActionStart, fileName);
    actionFrame.AddAction(std::move(CIncomingAction(UID, ActionStart)));
    actionFrame.AddAction(std::move(CIncomingAction(UID, ACTION_SAVE_ENDED)));
  }

  SaveEnded(UID);
  return true;
}

void CGame::SaveEnded(const uint8_t exceptUID, CQueuedActionsFrame& actionFrame)
{
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    if (fakeUser.GetUID() == exceptUID) {
      continue;
    }
    actionFrame.AddAction(std::move(CIncomingAction(fakeUser.GetUID(), ACTION_SAVE_ENDED)));
  }
}

bool CGame::Pause(GameUser::CGameUser* user, CQueuedActionsFrame& actionFrame, const bool isDisconnect)
{
  const uint8_t UID = SimulateActionUID(ACTION_PAUSE, user, isDisconnect);
  if (UID == 0xFF) return false;

  actionFrame.AddAction(std::move(CIncomingAction(UID, ACTION_PAUSE)));
  if (actionFrame.callback != ON_SEND_ACTIONS_PAUSE) {
    actionFrame.callback = ON_SEND_ACTIONS_PAUSE;
    actionFrame.pauseUID = user->GetUID();
  }
  return true;
}

bool CGame::Resume(GameUser::CGameUser* user, CQueuedActionsFrame& actionFrame, const bool isDisconnect)
{
  const uint8_t UID = SimulateActionUID(ACTION_RESUME, user, isDisconnect);
  if (UID == 0xFF) return false;

  actionFrame.AddAction(std::move(CIncomingAction(UID, ACTION_RESUME)));
  actionFrame.callback = ON_SEND_ACTIONS_RESUME;
  return true;
}

bool CGame::TrySaveOnDisconnect(GameUser::CGameUser* user, const bool isVoluntary)
{
  if (m_SaveOnLeave == SAVE_ON_LEAVE_NEVER) {
    return false;
  }

  if (!m_GameLoaded || m_Users.size() <= 1) {
    // Nobody can actually save this game.
    return false;
  }

  if (GetNumControllers() <= 2) {
    // 1v1 never auto-saves, not even if there are observers,
    // not even if !save enable is active.
    return false;
  }

  if (!GetLaggingUsers().empty()) {
    return false;
  }

  if (m_SaveOnLeave != SAVE_ON_LEAVE_ALWAYS) {
    if (isVoluntary) {
      // Is saving on voluntary leaves pointless?
      //
      // Not necessarily.
      //
      // Even if rage quits are unlikely to be reverted,
      // leavers may be replaced by fake users
      // This is impactful in maps such as X Hero Siege,
      // where leavers' heroes are automatically removed.
      //
      // However, since voluntary leaves are the rule, even
      // when games end normally, saving EVERYTIME will turn out to be annoying.
      //
      // Instead, we can do it only if the game has been running for a preconfigured time.
      // But how much time will be relative according to the map.
      // And it would force usage of mapcfg files...
      //
      // Considering that it's also not exactly trivial to load a game,
      // then automating this would bring about too many cons.
      //
      // Which is why allow autosaving on voluntary leaves,
      // but only if users want so by using !save enable.
      // Sadly, that's not the case in this branch, so no save for you.
      return false;
    } else if (GetTicks() < m_FinishedLoadingTicks + 420000) {
      // By default, leaves before the 7th minute do not autosave.
      return false;
    }
  }

  if (Save(user, true)) {
    Pause(user, true);
    // In FFA games, it's okay to show the real name (instead of GetDisplayName()) when disconnected.
    SendAllChat("Game saved on " + user->GetName() + "'s disconnection.");
    SendAllChat("They may rejoin on reload if an ally sends them their save. Foes' save files will NOT work.");
    return true;
  } else {
    LOG_APP_IF(LOG_LEVEL_WARNING, "Failed to automatically save game on leave")
  }

  return false;
}

bool CGame::Save(GameUser::CGameUser* user, const bool isDisconnect)
{
  return Save(user, GetLastActionFrame(), isDisconnect);
}

void CGame::SaveEnded(const uint8_t exceptUID)
{
  SaveEnded(exceptUID, GetLastActionFrame());
}

bool CGame::Pause(GameUser::CGameUser* user, const bool isDisconnect)
{
  return Pause(user, GetLastActionFrame(), isDisconnect);
}

bool CGame::Resume(GameUser::CGameUser* user, const bool isDisconnect)
{
  return Resume(user, GetLastActionFrame(), isDisconnect);
}

bool CGame::SendChatTrigger(const uint8_t UID, const string& message, const uint32_t firstByte, const uint32_t secondByte)
{
  vector<uint8_t> packet = {ACTION_CHAT_TRIGGER};
  AppendByteArray(packet, firstByte, false);
  AppendByteArray(packet, secondByte, false);
  vector<uint8_t> action;
  AppendByteArrayFast(packet, message);
  AppendByteArray(action, packet);
  GetLastActionFrame().AddAction(std::move(CIncomingAction(UID, action)));
  return true;
}

bool CGame::SendChatTriggerSymmetric(const uint8_t UID, const string& message, const uint8_t firstIdentifier, const uint8_t secondIdentifier)
{
  return SendChatTrigger(UID, message, (secondIdentifier << 8) | firstIdentifier, (secondIdentifier << 8) | firstIdentifier);
}

bool CGame::SendHMC(const string& message)
{
  if (!GetHMCEnabled()) return false;
  const uint8_t triggerID1 = m_Map->GetHMCTrigger1();
  const uint8_t triggerID2 = m_Map->GetHMCTrigger2();
  const uint8_t UID = HostToMapCommunicationUID();
  return SendChatTriggerSymmetric(UID, message, triggerID1, triggerID2);
}

bool CGame::GetIsCheckJoinable() const
{
  return m_Config.m_CheckJoinable;
}

void CGame::SetIsCheckJoinable(const bool nCheckIsJoinable) 
{
  m_Config.m_CheckJoinable = nCheckIsJoinable;
}

bool CGame::GetHasReferees() const
{
  return m_Map->GetMapObservers() == MAPOBS_REFEREES;
}

bool CGame::GetIsSupportedGameVersion(uint8_t nVersion) const {
  return nVersion < 64 && m_SupportedGameVersions.test(nVersion);
}

void CGame::SetSupportedGameVersion(uint8_t nVersion) {
  if (nVersion < 64) m_SupportedGameVersions.set(nVersion);
}

void CGame::OpenObserverSlots()
{
  const uint8_t enabledCount = m_Map->GetVersionMaxSlots() - GetMap()->GetMapNumDisabled();
  if (m_Slots.size() >= enabledCount) return;
  LOG_APP_IF(LOG_LEVEL_DEBUG, "adding " + to_string(enabledCount - m_Slots.size()) + " observer slots")
  while (m_Slots.size() < enabledCount) {
    m_Slots.emplace_back(GetIsCustomForces() ? SLOTTYPE_NONE : SLOTTYPE_USER, 0u, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), SLOTRACE_RANDOM);
  }
}

void CGame::CloseObserverSlots()
{
  uint8_t count = 0;
  uint8_t i = static_cast<uint8_t>(m_Slots.size());
  while (i--) {
    if (m_Slots[i].GetTeam() == m_Map->GetVersionMaxSlots()) {
      m_Slots.erase(m_Slots.begin() + i);
      ++count;
    }
  }
  if (count > 0 && m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
    LogApp("deleted " + to_string(count) + " observer slots");
  }
}

// Virtual host is needed to generate network traffic when only one user is in the game or lobby.
// Fake users may also accomplish the same purpose.
bool CGame::CreateVirtualHost()
{
  if (m_VirtualHostUID != 0xFF)
    return false;

  if (m_GameLoading || m_GameLoaded) {
    // In principle, CGame::CreateVirtualHost() should not be called when the game has started loading,
    // but too many times has that asssumption broke due to faulty logic.
    LOG_APP_IF(LOG_LEVEL_DEBUG, "Rejected creation of virtual host after game started");
    return false;
  }

  m_VirtualHostUID = GetNewUID();

  // When this message is sent because an slot is made available by a leaving user,
  // we gotta ensure that the virtual host join message is sent after the user's leave message.
  if (!m_Users.empty()) {
    const std::array<uint8_t, 4> IP = {0, 0, 0, 0};
    SendAll(GameProtocol::SEND_W3GS_PLAYERINFO(m_VirtualHostUID, GetLobbyVirtualHostName(), IP, IP));
  }
  return true;
}

bool CGame::DeleteVirtualHost()
{
  if (m_VirtualHostUID == 0xFF) {
    return false;
  }

  // When this message is sent because the last slot is filled by an incoming user,
  // we gotta ensure that the virtual host leave message is sent before the user's join message.
  if (!m_Users.empty()) {
    SendAll(GameProtocol::SEND_W3GS_PLAYERLEAVE_OTHERS(m_VirtualHostUID, PLAYERLEAVE_LOBBY));
  }
  m_VirtualHostUID = 0xFF;
  return true;
}

bool CGame::GetHasPvPGNPlayers() const
{
  for (const auto& user : m_Users) {
    if (user->GetRealm(false)) {
      return true;
    }
  }
  return false;
}

CGameVirtualUser* CGame::GetVirtualUserFromSID(const uint8_t SID)
{
  uint8_t i = static_cast<uint8_t>(m_FakeUsers.size());
  while (i--) {
    if (SID == m_FakeUsers[i].GetSID()) {
      return &(m_FakeUsers[i]);
    }
  }
  return nullptr;
}

const CGameVirtualUser* CGame::InspectVirtualUserFromSID(const uint8_t SID) const
{
  uint8_t i = static_cast<uint8_t>(m_FakeUsers.size());
  while (i--) {
    if (SID == m_FakeUsers[i].GetSID()) {
      return &(m_FakeUsers[i]);
    }
  }
  return nullptr;
}

void CGame::CreateFakeUserInner(const uint8_t SID, const uint8_t UID, const string& name)
{
  const bool isCustomForces = GetIsCustomForces();
  if (!m_Users.empty()) {
    const std::array<uint8_t, 4> IP = {0, 0, 0, 0};
    SendAll(GameProtocol::SEND_W3GS_PLAYERINFO(UID, name, IP, IP));
  }
  m_Slots[SID] = CGameSlot(
    m_Slots[SID].GetType(),
    UID,
    SLOTPROG_RDY,
    SLOTSTATUS_OCCUPIED,
    SLOTCOMP_NO,
    isCustomForces ? m_Slots[SID].GetTeam() : m_Map->GetVersionMaxSlots(),
    isCustomForces ? m_Slots[SID].GetColor() : m_Map->GetVersionMaxSlots(),
    m_Map->GetLobbyRace(&m_Slots[SID])
  );
  if (!isCustomForces) SetSlotTeamAndColorAuto(SID);

  m_FakeUsers.emplace_back(this, SID, UID, name).SetObserver(m_Slots[SID].GetTeam() == m_Map->GetVersionMaxSlots());
  m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
}

bool CGame::CreateFakeUser(const bool useVirtualHostName)
{
  // Fake users need not be explicitly restricted in any layout, so let's just use an empty slot.
  uint8_t SID = GetEmptySID(false);
  if (SID >= static_cast<uint8_t>(m_Slots.size())) return false;
  if (!CanLockSlotForJoins(SID)) return false;

  if (GetSlotsOpen() == 1)
    DeleteVirtualHost();

  CreateFakeUserInner(SID, GetNewUID(), useVirtualHostName ? GetLobbyVirtualHostName() : ("User[" + ToDecString(SID + 1) + "]"));
  return true;
}

bool CGame::CreateFakePlayer(const bool useVirtualHostName)
{
  const bool isCustomForces = GetIsCustomForces();
  uint8_t SID = isCustomForces ? GetEmptyPlayerSID() : GetEmptySID(false);
  if (SID >= static_cast<uint8_t>(m_Slots.size())) return false;

  if (isCustomForces && (m_Slots[SID].GetTeam() == m_Map->GetVersionMaxSlots())) {
    return false;
  }
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  if (GetSlotsOpen() == 1)
    DeleteVirtualHost();

  CreateFakeUserInner(SID, GetNewUID(), useVirtualHostName ? GetLobbyVirtualHostName() : ("User[" + ToDecString(SID + 1) + "]"));
  return true;
}

bool CGame::CreateFakeObserver(const bool useVirtualHostName)
{
  if (!(m_Map->GetMapObservers() == MAPOBS_ALLOWED || m_Map->GetMapObservers() == MAPOBS_REFEREES)) {
    return false;
  }

  const bool isCustomForces = GetIsCustomForces();
  uint8_t SID = isCustomForces ? GetEmptyObserverSID() : GetEmptySID(false);
  if (SID >= static_cast<uint8_t>(m_Slots.size())) return false;

  if (isCustomForces && (m_Slots[SID].GetTeam() != m_Map->GetVersionMaxSlots())) {
    return false;
  }
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  if (GetSlotsOpen() == 1)
    DeleteVirtualHost();

  CreateFakeUserInner(SID, GetNewUID(), useVirtualHostName ? GetLobbyVirtualHostName() : ("User[" + ToDecString(SID + 1) + "]"));
  return true;
}

bool CGame::CreateHMCPlayer()
{
  // Fake users need not be explicitly restricted in any layout, so let's just use an empty slot.
  uint8_t SID = m_Map->GetHMCSlot() - 1;
  if (SID >= static_cast<uint8_t>(m_Slots.size())) return false;
  if (!CanLockSlotForJoins(SID)) return false;

  if (GetSlotsOpen() == 1)
    DeleteVirtualHost();

  CreateFakeUserInner(SID, GetNewUID(), m_Map->GetHMCPlayerName());
  return true;
}

bool CGame::DeleteFakeUser(uint8_t SID)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot) return false;
  const bool isHMCSlot = m_Map->GetHMCEnabled() && SID + 1 == m_Map->GetHMCSlot();
  for (auto it = begin(m_FakeUsers); it != end(m_FakeUsers); ++it) {
    if (slot->GetUID() == it->GetUID()) {
      if (GetIsCustomForces()) {
        m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RST, isHMCSlot ? SLOTSTATUS_CLOSED : SLOTSTATUS_OPEN, SLOTCOMP_NO, slot->GetTeam(), slot->GetColor(), /* only important if MAPOPT_FIXEDPLAYERSETTINGS */ m_Map->GetLobbyRace(slot));
      } else {
        m_Slots[SID] = CGameSlot(slot->GetType(), 0, SLOTPROG_RST, isHMCSlot ? SLOTSTATUS_CLOSED : SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), SLOTRACE_RANDOM);
      }
      // Ensure this is sent before virtual host rejoins
      SendAll(it->GetGameQuitBytes(PLAYERLEAVE_LOBBY));
      it = m_FakeUsers.erase(it);
      CreateVirtualHost();
      m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
      return true;
    }
  }
  return false;
}

uint8_t CGame::FakeAllSlots()
{
  // Ensure this is called outside any loops.
  const bool hasUsers = GetHasAnyUser();

  uint8_t addedCounter = 0;
  if (m_RestoredGame) {
    if (m_Reserved.empty()) return 0;
    uint8_t reservedIndex = 0;
    uint8_t reservedEnd = static_cast<uint8_t>(m_Reserved.size()) - static_cast<uint8_t>(!hasUsers);
    for (uint8_t SID = 0; SID < m_Slots.size(); ++SID) {
      if (m_Slots[SID].GetIsPlayerOrFake()) {
        if (++reservedIndex >= reservedEnd) break;
        continue;
      }
      if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
        const CGameSlot* savedSlot = m_RestoredGame->InspectSlot(SID);
        CreateFakeUserInner(SID, savedSlot->GetUID(), m_Reserved[reservedIndex]);
        ++addedCounter;
        if (++reservedIndex >= reservedEnd) break;
      }
    }
  } else {
    uint8_t remainingControllers = m_Map->GetMapNumControllers() - GetNumControllers();
    if (!hasUsers && m_Slots.size() == m_Map->GetMapNumControllers()) {
      --remainingControllers;
    }
    for (uint8_t SID = 0; SID < m_Slots.size(); ++SID) {
      if (m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OPEN) {
        continue;
      }
      CreateFakeUserInner(SID, GetNewUID(), "User[" + ToDecString(SID + 1) + "]");
      ++addedCounter;
      if (0 == --remainingControllers) {
        break;
      }
    }
  }
  if (GetSlotsOpen() == 0 && GetNumJoinedUsersOrFake() > 1) DeleteVirtualHost();
  return addedCounter;
}

void CGame::DeleteFakeUsersLobby()
{
  if (m_FakeUsers.empty())
    return;

  uint8_t hmcSID = GetHMCSID();
  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    const uint8_t SID = fakeUser.GetSID();
    if (GetIsCustomForces()) {
      m_Slots[SID] = CGameSlot(m_Slots[SID].GetType(), 0, SLOTPROG_RST, SID == hmcSID ? SLOTSTATUS_CLOSED : SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Slots[SID].GetTeam(), m_Slots[SID].GetColor(), /* only important if MAPOPT_FIXEDPLAYERSETTINGS */ m_Map->GetLobbyRace(&(m_Slots[SID])));
    } else {
      m_Slots[SID] = CGameSlot(m_Slots[SID].GetType(), 0, SLOTPROG_RST, SID == hmcSID ? SLOTSTATUS_CLOSED : SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Map->GetVersionMaxSlots(), m_Map->GetVersionMaxSlots(), SLOTRACE_RANDOM);
    }
    // Ensure this is sent before virtual host rejoins
    SendAll(fakeUser.GetGameQuitBytes(PLAYERLEAVE_LOBBY));
  }

  m_FakeUsers.clear();
  CreateVirtualHost();
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
}

void CGame::DeleteFakeUsersLoaded()
{
  if (m_FakeUsers.empty())
    return;

  for (const CGameVirtualUser& fakeUser : m_FakeUsers) {
    SendAll(fakeUser.GetGameQuitBytes(PLAYERLEAVE_DISCONNECT));
  }

  m_FakeUsers.clear();
}

void CGame::RemoveCreator()
{
  m_CreatedBy.clear();
  m_CreatedFrom = nullptr;
  m_CreatedFromType = SERVICE_TYPE_INVALID;
}

bool CGame::GetIsStageAcceptingJoins() const
{
  // This method does not care whether this is actually a mirror game. This is intended.
  if (m_LobbyLoading || m_Exiting || GetIsGameOver()) return false;
  if (!m_CountDownStarted) return true;
  if (!m_GameLoaded) return false;
  return m_Config.m_EnableJoinObserversInProgress || m_Config.m_EnableJoinPlayersInProgress;
}

bool CGame::GetUDPEnabled() const
{
  return m_Config.m_UDPEnabled;
}

void CGame::SetUDPEnabled(bool nEnabled)
{
  m_Config.m_UDPEnabled = nEnabled;
}

bool CGame::GetHasDesyncHandler() const
{
  return m_Config.m_DesyncHandler == ON_DESYNC_DROP || m_Config.m_DesyncHandler == ON_DESYNC_NOTIFY;
}

bool CGame::GetAllowsDesync() const
{
  return m_Config.m_DesyncHandler != ON_DESYNC_DROP;
}

uint8_t CGame::GetIPFloodHandler() const
{
  return m_Config.m_IPFloodHandler;
}

bool CGame::GetAllowsIPFlood() const
{
  return m_Config.m_IPFloodHandler != ON_IPFLOOD_DENY;
}

string CGame::GetIndexVirtualHostName() const
{
  return m_Config.m_IndexVirtualHostName;
}

string CGame::GetLobbyVirtualHostName() const
{
  return m_Config.m_LobbyVirtualHostName;
}

uint8_t CGame::CalcMaxEqualizerDelayFrames() const
{
  if (!m_Config.m_LatencyEqualizerEnabled) return 0;
  uint8_t max = 0;
  for (const auto& user : m_Users) {
    uint8_t thisOffset = user->GetPingEqualizerOffset();
    if (max < thisOffset) max = thisOffset;
  }
  // static_assert(max < m_Actions.size());
  return max;
}

uint16_t CGame::GetLatency() const
{
  //if (m_Config.m_LatencyEqualizerEnabled) return m_Config.m_Latency / 2;
  return m_Config.m_Latency;
}

uint32_t CGame::GetSyncLimit() const
{
  return m_Config.m_SyncLimit;
}

uint32_t CGame::GetSyncLimitSafe() const
{
  return m_Config.m_SyncLimitSafe;
}
