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

#ifndef AURA_GAME_H_
#define AURA_GAME_H_

#include "includes.h"
#include "list.h"
#include "game_seeker.h"
#include "game_slot.h"
#include "game_setup.h"
#include "save_game.h"
#include "socket.h"
#include "config/config_game.h"

#include <algorithm>

//
// CGame
//

/*
 Nomenclature notes:
 User: GameUser::CGameUser instance, representing a remote game client that successfully joined the game.
 Fake user: (UID, SID) comboes that may occupy game slots. Stored as 16 bits. Higher = SID, Lower = UID.
            WC3 game client cannot distinguish them from actual users.
            Aura never treats them as "users".
 Player: User that does not occupy an observer slot.
 Controller: Any of user, fake user, or AI, that does not occupy an observer slot.
 */



struct CGameLogRecord
{
  int64_t                        m_Ticks;
  std::string                    m_Text;

  inline int64_t                 GetTicks() const { return m_Ticks; }
  inline const std::string&      GetText() const { return m_Text; }
  std::string                    ToString() const;

  CGameLogRecord(int64_t gameTicks, std::string text);
  ~CGameLogRecord();
};

struct CQueuedActionsFrame
{
  // action to be performed after this frame is sent: ON_SEND_ACTIONS_PAUSE, ON_SEND_ACTIONS_RESUME
  uint8_t callback;

  // UID of last user that sent a pause action
  uint8_t pauseUID;

  // total size of the active ActionQueue  
  uint16_t bufferSize;

  // ActionQueue we append new incoming actions to
  ActionQueue* activeQueue;

  // queue of queues of size N
  // first (N-1) queues are sent with SEND_W3GS_INCOMING_ACTION2
  // last queue is sent with SEND_W3GS_INCOMING_ACTION, together with the expected delay til next action (latency)
  std::vector<ActionQueue> actions;

  // when a player leaves, the SEND_W3GS_PLAYERLEAVE_OTHERS is delayed until we are sure all their pending actions have been sent
  // so, if they leave during the game, we must append it to the last CQueuedActionsFrame
  // but if they leave while loading, we may append it to the first CQueuedActionsFrame
  UserList leavers;

  CQueuedActionsFrame();
  ~CQueuedActionsFrame();

  void AddAction(CIncomingAction&& action);
  std::vector<uint8_t> GetBytes(const uint16_t sendInterval) const;
  void MergeFrame(CQueuedActionsFrame& frame);
  bool GetHasActionsBy(const uint8_t fromUID) const;
  bool GetIsEmpty() const;
  size_t GetActionCount() const;
  void Reset();
};

class CGame
{
public:
  CAura* m_Aura;
  CGameConfig m_Config;

private:
  friend class CCommandContext;

protected:
  bool                                                   m_Verbose;
  CTCPServer*                                            m_Socket;                        // listening socket
  CDBBan*                                                m_LastLeaverBannable;            // last ban for the !banlast command - this is a pointer to one of the items in m_Bannables
  std::vector<CDBBan*>                                   m_Bannables;                     // std::vector of potential ban data for the database
  std::vector<CDBBan*>                                   m_ScopeBans;                     // it must be a different vector from m_Bannables, because m_Bannables has unique name data, while m_ScopeBans has unique (name, server) data
  CW3MMD*                                                m_CustomStats;
  CDotaStats*                                            m_DotaStats;                     // class to keep track of game stats such as kills/deaths/assists in dota
  CSaveGame*                                             m_RestoredGame;
  std::vector<CGameSlot>                                 m_Slots;                         // std::vector of slots
  std::vector<CDBGamePlayer*>                            m_DBGamePlayers;                 // std::vector of potential gameuser data for the database
  UserList                                               m_Users;                         // std::vector of players
  std::vector<CConnection*>                              m_Observers;
  CircleDoubleLinkedList<CQueuedActionsFrame>            m_Actions;            // actions to be sent
  QueuedActionsFrameNode*                                m_CurrentActionsFrame;
  std::vector<std::string>                               m_Reserved;                      // std::vector of player names with reserved slots (from the !hold command)
  std::set<std::string>                                  m_ReportedJoinFailNames;         // set of player names to NOT print ban messages for when joining because they've already been printed
  std::vector<CGameVirtualUser>                          m_FakeUsers;                     // the fake player's UIDs (lower 8 bits) and SIDs (higher 8 bits) (if present)
  std::shared_ptr<CMap>                                  m_Map;                           // map data
  uint32_t                                               m_GameFlags;
  GameUser::CGameUser*                                   m_PauseUser;
  std::string                                            m_GameName;                      // game name
  uint64_t                                               m_GameHistoryId;
  std::string                                            m_LastOwner;                     // name of the player who was owner last time the owner was released
  bool                                                   m_FromAutoReHost;
  bool                                                   m_OwnerLess;
  std::string                                            m_OwnerName;                     // name of the player who owns this game (should be considered an admin)
  std::string                                            m_OwnerRealm;                    // self-identified realm of the player who owns the game (spoofable)
  std::string                                            m_CreatorText;                   // who created this game
  std::string                                            m_CreatedBy;                     // name of the battle.net user who created this game
  void*                                                  m_CreatedFrom;                   // battle.net or IRC server the player who created this game was on
  uint8_t                                                m_CreatedFromType;               // type of server m_CreatedFrom is
  std::set<std::string>                                  m_RealmsExcluded;                // battle.net servers where the mirrored game is not to be broadcasted
  std::string                                            m_PlayedBy;
  std::string                                            m_KickVotePlayer;                // the player to be kicked with the currently running kick vote
  std::string                                            m_HCLCommandString;              // the "HostBot Command Library" command std::string, used to pass a limited amount of data to specially designed maps
  std::string                                            m_MapPath;                       // store the map path to save in the database on game end
  std::string                                            m_MapSiteURL;
  int64_t                                                m_GameTicks;                     // ingame ticks
  int64_t                                                m_CreationTime;                  // GetTime when the game was created
  int64_t                                                m_LastPingTime;                  // GetTime when the last ping was sent
  int64_t                                                m_LastRefreshTime;               // GetTime when the last game refresh was sent
  int64_t                                                m_LastDownloadTicks;             // GetTicks when the last map download cycle was performed
  int64_t                                                m_LastDownloadCounterResetTicks; // GetTicks when the download counter was last reset
  int64_t                                                m_LastCountDownTicks;            // GetTicks when the last countdown message was sent
  int64_t                                                m_StartedLoadingTicks;           // GetTicks when the game started loading
  int64_t                                                m_FinishedLoadingTicks;          // GetTicks when the game finished loading
  int64_t                                                m_LastActionSentTicks;           // GetTicks when the last action packet was sent
  int64_t                                                m_LastActionLateBy;              // the number of ticks we were late sending the last action packet by
  int64_t                                                m_LastPausedTicks;               // GetTicks when the game was last paused
  int64_t                                                m_PausedTicksDeltaSum;           // Sum of GetTicks deltas for every game pause
  int64_t                                                m_StartedLaggingTime;            // GetTime when the last lag screen started
  int64_t                                                m_LastLagScreenTime;             // GetTime when the last lag screen was active (continuously updated)
  uint32_t                                               m_PingReportedSinceLagTimes;     // How many times we have sent players' pings since we started lagging
  int64_t                                                m_LastUserSeen;                  // GetTicks when any user was last seen in the lobby
  int64_t                                                m_LastOwnerSeen;                 // GetTicks when the game owner was last seen in the lobby
  int64_t                                                m_LastOwnerAssigned;             // GetTicks when the game owner was assigned
  int64_t                                                m_StartedKickVoteTime;           // GetTime when the kick vote was started
  int64_t                                                m_LastCustomStatsUpdateTime;
  uint8_t                                                m_GameOver;
  std::optional<int64_t>                                 m_GameOverTime;                  // GetTime when the game was over
  std::optional<int64_t>                                 m_GameOverTolerance;
  std::optional<int64_t>                                 m_LastPlayerLeaveTicks;          // GetTicks when the most recent player left the game
  int64_t                                                m_LastLagScreenResetTime;        // GetTime when the "lag" screen was last reset
  uint32_t                                               m_RandomSeed;                    // the random seed sent to the Warcraft III clients
  uint32_t                                               m_HostCounter;                   // a unique game number
  uint32_t                                               m_EntryKey;                      // random entry key for LAN, used to prove that a player is actually joining from LAN
  uint32_t                                               m_SyncCounter;                   // the number of actions sent so far (for determining if anyone is lagging)
  uint32_t                                               m_SyncCounterChecked;            // the number of verified keepalive packets
  uint8_t                                                m_MaxPingEqualizerDelayFrames;
  int64_t                                                m_LastPingEqualizerGameTicks;    // m_GameTicks when ping equalizer was last run

  uint32_t                                               m_DownloadCounter;               // # of map bytes downloaded in the last second
  uint32_t                                               m_CountDownCounter;              // the countdown is finished when this reaches zero
  uint8_t                                                m_StartPlayers;                  // number of players when the game started
  std::vector<std::pair<uint8_t, int64_t>>               m_AutoStartRequirements;
  bool                                                   m_ControllersBalanced;
  uint8_t                                                m_ControllersReadyCount;
  uint8_t                                                m_ControllersNotReadyCount;
  uint8_t                                                m_ControllersWithMap;
  uint8_t                                                m_CustomLayout;
  std::pair<uint8_t, uint8_t>                            m_CustomLayoutData;
  uint16_t                                               m_HostPort;                      // the port to host games on
  bool                                                   m_PublicHostOverride;            // whether to use own m_PublicHostAddress, m_PublicHostPort instead of CRealm's (disables hosting on CRealm mirror instances)
  std::array<uint8_t, 4>                                 m_PublicHostAddress;
  uint16_t                                               m_PublicHostPort;
  uint8_t                                                m_DisplayMode;                   // game state, public or private
  bool                                                   m_IsAutoVirtualPlayers;          // if we should try to add the virtual host as a second (fake) player in single-player games
  uint8_t                                                m_VirtualHostUID;                // host's UID
  uint8_t                                                m_GProxyEmptyActions;            // empty actions used for gproxy protocol
  bool                                                   m_Exiting;                       // set to true and this instance will be deleted next update
  bool                                                   m_ExitingSoon;                   // set to true and this instance will be deleted when no players remain
  uint8_t                                                m_SlotInfoChanged;               // if the slot info has changed and hasn't been sent to the players yet (optimization)
  uint8_t                                                m_JoinedVirtualHosts;
  uint8_t                                                m_ReconnectProtocols;
  bool                                                   m_Replaceable;                   // whether this game can be destroyed when !host command is used inside
  bool                                                   m_Replacing;
  bool                                                   m_PublicStart;                   // if the game owner is the only one allowed to run game commands or not
  bool                                                   m_Locked;                        // if the game owner is the only one allowed to run game commands or not
  bool                                                   m_ChatOnly;                      // if we should ignore game start commands
  bool                                                   m_MuteAll;                       // if we should stop forwarding ingame chat messages targeted for all players or not
  bool                                                   m_MuteLobby;                     // if we should stop forwarding lobby chat messages
  bool                                                   m_IsMirror;                      // if we aren't actually hosting the game, but just broadcasting it
  bool                                                   m_CountDownStarted;              // if the game start countdown has started or not
  bool                                                   m_CountDownFast;
  bool                                                   m_CountDownUserInitiated;
  bool                                                   m_GameLoading;                   // if the game is currently loading or not
  bool                                                   m_GameLoaded;                    // if the game has loaded or not
  bool                                                   m_LobbyLoading;                  // if the lobby is being setup asynchronously
  bool                                                   m_Lagging;                       // if the lag screen is active or not
  bool                                                   m_Paused;                        // if the game is paused or not
  bool                                                   m_Desynced;                      // if the game has desynced or not
  bool                                                   m_IsDraftMode;                   // if players are forbidden to choose their own teams (if so, let team captains use !team, !ffa, !vsall, !vsai, !teams)
  bool                                                   m_IsHiddenPlayerNames;           // if players names are to be obfuscated in most circumstances
  bool                                                   m_HadLeaver;                     // if the game had a leaver after it started
  bool                                                   m_CheckReservation;
  bool                                                   m_UsesCustomReferees;
  bool                                                   m_SentPriorityWhois;
  bool                                                   m_Remaking;
  bool                                                   m_Remade;
  uint8_t                                                m_SaveOnLeave;
  bool                                                   m_HMCEnabled;
  uint8_t                                                m_BufferingEnabled;
  uint32_t                                               m_BeforePlayingEmptyActions;     // counter for game-start empty actions. Used for load-in-game feature.

  SharedByteArray                                        m_LoadedMapChunk;
  std::vector<uint8_t>                                   m_LobbyBuffer;
  std::vector<uint8_t>                                   m_SlotsBuffer;
  std::vector<uint8_t>                                   m_LoadingRealBuffer;             // real W3GS_GAMELOADED messages for real players. In standard load, this buffer is filled in real-time. When load-in-game is enabled, this buffer is prefilled.
  std::vector<uint8_t>                                   m_LoadingVirtualBuffer;          // fake W3GS_GAMELOADED messages for fake players, but also for disconnected real players - for consistent game load, m_LoadingVirtualBuffer is sent after m_LoadingRealBuffer
  std::vector<std::vector<uint8_t>>                      m_PlayingBuffer;

  std::bitset<64>                                        m_SupportedGameVersions;
  uint8_t                                                m_SupportedGameVersionsMin;
  uint8_t                                                m_SupportedGameVersionsMax;

  bool                                                   m_GameDiscoveryInfoChanged;
  std::vector<uint8_t>                                   m_GameDiscoveryInfo;
  uint16_t                                               m_GameDiscoveryInfoVersionOffset;
  uint16_t                                               m_GameDiscoveryInfoDynamicOffset;
  std::map<const GameUser::CGameUser*, UserList>         m_SyncPlayers;     //

  std::queue<CGameLogRecord*>                            m_PendingLogs;
  

public:
  CGame(CAura* nAura, std::shared_ptr<CGameSetup> nGameSetup);
  ~CGame();
  CGame(CGame&) = delete;

  bool                                                   GetExiting() const { return m_Exiting; }
  inline QueuedActionsFrameNode*                         GetFirstActionFrameNode() { return m_CurrentActionsFrame; }
  inline QueuedActionsFrameNode*                         GetLastActionFrameNode() { return m_CurrentActionsFrame->prev; }
  inline CQueuedActionsFrame&                            GetFirstActionFrame();
  inline CQueuedActionsFrame&                            GetLastActionFrame();
  std::vector<QueuedActionsFrameNode*>                   GetFrameNodesInRangeInclusive(const uint8_t startOffset, const uint8_t endOffset);
  std::vector<QueuedActionsFrameNode*>                   GetAllFrameNodes();
  void                                                   MergeFrameNodes(std::vector<QueuedActionsFrameNode*>& frameNodes);
  void                                                   ResetUserPingEqualizerDelays();
  bool                                                   CheckUpdatePingEqualizer();
  uint8_t                                                UpdatePingEqualizer();
  std::vector<std::pair<GameUser::CGameUser*, uint32_t>> GetDescendingSortedRTT() const;
  inline std::shared_ptr<CMap>                           GetMap() const { return m_Map; }
  inline uint32_t                                        GetEntryKey() const { return m_EntryKey; }
  inline uint16_t                                        GetHostPort() const { return m_HostPort; }
  uint16_t                                               GetDiscoveryPort(const uint8_t protocol) const;
  bool                                                   GetIsStageAcceptingJoins() const;
  bool                                                   GetUDPEnabled() const;
  inline bool                                            GetPublicHostOverride() const { return m_PublicHostOverride; }
  inline std::array<uint8_t, 4>                          GetPublicHostAddress() const { return m_PublicHostAddress; }
  inline uint16_t                                        GetPublicHostPort() const { return m_PublicHostPort; }
  inline uint8_t                                         GetDisplayMode() const { return m_DisplayMode; }
  inline uint8_t                                         GetGProxyEmptyActions() const { return m_GProxyEmptyActions; }
  inline std::string                                     GetGameName() const { return m_GameName; }
  inline uint64_t                                        GetGameID() const { return m_GameHistoryId; }
  inline uint8_t                                         GetNumSlots() const { return static_cast<uint8_t>(m_Slots.size()); }
  std::string                                            GetIndexVirtualHostName() const;
  std::string                                            GetLobbyVirtualHostName() const;
  std::string                                            GetPrefixedGameName(const CRealm* realm = nullptr) const;
  std::string                                            GetAnnounceText(const CRealm* realm = nullptr) const;
  inline bool                                            GetFromAutoReHost() const { return m_FromAutoReHost; }
  inline bool                                            GetLockedOwnerLess() const { return m_OwnerLess; }
  inline std::string                                     GetOwnerName() const { return m_OwnerName; }
  inline std::string                                     GetOwnerRealm() const { return m_OwnerRealm; }
  inline std::string                                     GetCreatorName() const { return m_CreatedBy; }
  inline uint8_t                                         GetCreatedFromType() const { return m_CreatedFromType; }
  inline void*                                           GetCreatedFrom() const { return m_CreatedFrom; }
  bool                                                   MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const;
  inline uint32_t                                        GetHostCounter() const { return m_HostCounter; }
  inline int64_t                                         GetLastLagScreenTime() const { return m_LastLagScreenTime; }
  inline bool                                            GetIsReplaceable() const { return m_Replaceable;}
  inline bool                                            GetIsBeingReplaced() const { return m_Replacing;}
  inline bool                                            GetIsPublicStartable() const { return m_PublicStart; }
  inline bool                                            GetLocked() const { return m_Locked; }
  inline bool                                            GetMuteAll() const { return m_MuteAll; }
  inline bool                                            GetCountDownStarted() const { return m_CountDownStarted; }
  inline bool                                            GetCountDownFast() const { return m_CountDownFast; }
  inline bool                                            GetCountDownUserInitiated() const { return m_CountDownUserInitiated; }
  inline bool                                            GetIsMirror() const { return m_IsMirror; }
  inline bool                                            GetIsDraftMode() const { return m_IsDraftMode; }
  bool                                                   GetIsHiddenPlayerNames() const;
  inline bool                                            GetGameLoading() const { return m_GameLoading; }
  inline bool                                            GetGameLoaded() const { return m_GameLoaded; }
  inline bool                                            GetLobbyLoading() const { return m_LobbyLoading; }
  inline bool                                            GetIsLobby() const { return !m_GameLoading && !m_GameLoaded; }
  inline bool                                            GetIsLobbyStrict() const { return !m_IsMirror && !m_GameLoading && !m_GameLoaded; }
  inline bool                                            GetIsRestored() const { return m_RestoredGame != nullptr; }
  inline uint32_t                                        GetSyncCounter() const { return m_SyncCounter; }
  uint8_t                                                GetMaxEqualizerDelayFrames() const { return m_MaxPingEqualizerDelayFrames; }
  uint8_t                                                CalcMaxEqualizerDelayFrames() const;
  uint16_t                                               GetLatency() const;
  uint32_t                                               GetSyncLimit() const;
  uint32_t                                               GetSyncLimitSafe() const;
  inline bool                                            GetLagging() const { return m_Lagging; }
  inline bool                                            GetPaused() const { return m_Paused; }
  inline bool                                            GetIsGameOver() const { return m_GameOver != GAME_ONGOING; }
  inline bool                                            GetIsGameOverTrusted() const { return m_GameOver == GAME_OVER_TRUSTED; }
  uint8_t                                                GetLayout() const;
  uint8_t                                                GetCustomLayout() const { return m_CustomLayout; }
  bool                                                   GetIsCustomForces() const;
  int64_t                                                GetNextTimedActionMicroSeconds() const;
  uint32_t                                               GetSlotsOccupied() const;
  uint32_t                                               GetSlotsOpen() const;
  bool                                                   HasSlotsOpen() const;
  bool                                                   GetIsSinglePlayerMode() const;
  bool                                                   GetHasAnyFullObservers() const;
  bool                                                   GetHasChatSendHost() const;
  bool                                                   GetHasChatRecvHost() const;
  bool                                                   GetHasChatSendPermaHost() const;
  bool                                                   GetHasChatRecvPermaHost() const;
  uint32_t                                               GetNumJoinedUsers() const;
  uint32_t                                               GetNumJoinedUsersOrFake() const;
  uint8_t                                                GetNumJoinedPlayers() const;
  uint8_t                                                GetNumJoinedPlayersOrFake() const;
  uint8_t                                                GetNumJoinedObservers() const;
  uint8_t                                                GetNumJoinedObserversOrFake() const;
  uint8_t                                                GetNumJoinedPlayersOrFakeUsers() const;
  uint8_t                                                GetNumFakePlayers() const;
  uint8_t                                                GetNumFakeObservers() const;
  uint8_t                                                GetNumOccupiedSlots() const;
  uint8_t                                                GetNumPotentialControllers() const;
  uint8_t                                                GetNumControllers() const;
  uint8_t                                                GetNumComputers() const;
  uint8_t                                                GetNumTeamControllersOrOpen(const uint8_t team) const;
  std::string                                            GetClientFileName() const;
  std::string                                            GetMapSiteURL() const { return m_MapSiteURL; }
  inline int64_t                                         GetGameTicks() const { return m_GameTicks; }
  inline int64_t                                         GetLastPausedTicks() const { return m_LastPausedTicks; }
  inline int64_t                                         GetPausedTicksDeltaSum() const { return m_PausedTicksDeltaSum; }
  inline bool                                            GetChatOnly() const { return m_ChatOnly; }
  inline bool                                            GetAnyUsingGProxy() { return m_ReconnectProtocols > 0; }
  std::string                                            GetStatusDescription() const;
  std::string                                            GetEndDescription() const;
  std::string                                            GetCategory() const;
  uint32_t                                               GetGameType() const;
  inline uint32_t                                        GetGameFlags() const { return m_GameFlags; }
  uint32_t                                               CalcGameFlags() const;
  std::string                                            GetSourceFilePath() const;
  std::array<uint8_t, 4>                                 GetSourceFileHash() const;
  std::array<uint8_t, 20>                                GetSourceFileSHA1() const;
  std::array<uint8_t, 2>                                 GetAnnounceWidth() const;
  std::array<uint8_t, 2>                                 GetAnnounceHeight() const;

  std::string                                            GetLogPrefix() const;
  ImmutableUserList                                      GetPlayers() const;
  ImmutableUserList                                      GetObservers() const;
  ImmutableUserList                                      GetUnreadyPlayers() const;
  ImmutableUserList                                      GetWaitingReconnectPlayers() const;
  bool                                                   GetIsAutoStartDue() const;
  std::string                                            GetAutoStartText() const;
  std::string                                            GetReadyStatusText() const;
  std::string                                            GetCmdToken() const;
  CTCPServer*                                            GetSocket() const { return m_Socket; };

  uint16_t                                               GetHostPortForDiscoveryInfo(const uint8_t protocol) const;
  inline bool                                            GetIsRealmExcluded(const std::string& hostName) const { return m_RealmsExcluded.find(hostName) != m_RealmsExcluded.end() ; }
  uint8_t                                                CalcActiveReconnectProtocols() const;
  std::string                                            GetActiveReconnectProtocolsDetails() const;
  bool                                                   CalcAnyUsingGProxy() const;
  bool                                                   CalcAnyUsingGProxyLegacy() const;
  uint8_t                                                GetPlayersReadyMode() const;

  inline void                                            SetExiting(bool nExiting) { m_Exiting = nExiting; }
  inline void                                            SetMapSiteURL(const std::string& nMapSiteURL) { m_MapSiteURL = nMapSiteURL; }
  inline void                                            SetChatOnly(bool nChatOnly) { m_ChatOnly = nChatOnly; }
  void                                                   SetUDPEnabled(bool nEnabled);
  bool                                                   GetHasDesyncHandler() const;
  bool                                                   GetAllowsDesync() const;
  uint8_t                                                GetIPFloodHandler() const;
  bool                                                   GetAllowsIPFlood() const;
  void                                                   UpdateReadyCounters();
  void                                                   ResetDropVotes();
  void                                                   ResetOwnerSeen();
  void                                                   UpdateGameDiscovery() { m_GameDiscoveryInfoChanged = true; }

  inline uint32_t                                        GetUptime() const {
    int64_t time = GetTime();
    if (time < m_CreationTime) return 0;
    return static_cast<uint32_t>(time - m_CreationTime);
  }

  // processing functions

  uint32_t                                               SetFD(void* fd, void* send_fd, int32_t* nfds) const;
  void                                                   UpdateJoinable();
  bool                                                   UpdateLobby();
  void                                                   UpdateLoading();
  void                                                   UpdateLoaded();
  bool                                                   Update(void* fd, void* send_fd);
  void                                                   UpdatePost(void* send_fd) const;
  void                                                   CheckLobbyTimeouts();
  void                                                   RunActionsScheduler(const uint8_t maxNewEqualizerOffset, const uint8_t maxOldEqualizerOffset);

  // logging
  void                                                   LogApp(const std::string& logText) const;
  void                                                   Log(const std::string& logText);
  void                                                   Log(const std::string& logText, int64_t gameTicks);
  void                                                   UpdateLogs();
  void                                                   FlushLogs();
  void                                                   LogSlots();

  // generic functions to send packets to players

  void                                                   Send(CConnection* player, const std::vector<uint8_t>& data) const;
  void                                                   Send(uint8_t UID, const std::vector<uint8_t>& data) const;
  void                                                   Send(const std::vector<uint8_t>& UIDs, const std::vector<uint8_t>& data) const;
  void                                                   SendAsChat(CConnection* player, const std::vector<uint8_t>& data) const;
  void                                                   SendAll(const std::vector<uint8_t>& data) const;
  bool                                                   SendAllAsChat(const std::vector<uint8_t>& data) const;
 

  // functions to send packets to players

  void                                                   SendChat(uint8_t fromUID, GameUser::CGameUser* user, const std::string& message, const uint8_t logLevel = LOG_LEVEL_INFO) const;
  void                                                   SendChat(uint8_t fromUID, uint8_t toUID, const std::string& message, const uint8_t logLevel = LOG_LEVEL_INFO) const;
  void                                                   SendChat(GameUser::CGameUser* user, const std::string& message, const uint8_t logLevel = LOG_LEVEL_INFO) const;
  void                                                   SendChat(uint8_t toUID, const std::string& message, const uint8_t logLevel = LOG_LEVEL_INFO) const;
  bool                                                   SendAllChat(uint8_t fromUID, const std::string& message) const;
  bool                                                   SendAllChat(const std::string& message) const;
  void                                                   SendAllSlotInfo();
  void                                                   SendVirtualHostPlayerInfo(CConnection* player) const;
  void                                                   SendFakeUsersInfo(CConnection* player) const;
  void                                                   SendJoinedPlayersInfo(CConnection* player) const;
  void                                                   SendWelcomeMessage(GameUser::CGameUser* user) const;
  void                                                   SendOwnerCommandsHelp(const std::string& cmdToken, GameUser::CGameUser* user) const;
  void                                                   SendCommandsHelp(const std::string& cmdToken, GameUser::CGameUser* user, const bool isIntro) const;
  void                                                   QueueLeftMessage(GameUser::CGameUser* user) const;
  void                                                   SendLeftMessage(GameUser::CGameUser* user, const bool sendChat) const;
  void                                                   SendChatMessage(const GameUser::CGameUser* user, const CIncomingChatPlayer* chatPlayer) const;
  void                                                   SendGProxyEmptyActions();
  void                                                   SendAllActionsCallback();
  void                                                   SendAllActions();
  void                                                   SendAllAutoStart() const;

  std::vector<uint8_t>                                   GetGameDiscoveryInfo(const uint8_t gameVersion, const uint16_t hostPort);
  std::vector<uint8_t>*                                  GetGameDiscoveryInfoTemplate();
  std::vector<uint8_t>                                   GetGameDiscoveryInfoTemplateInner(uint16_t* gameVersionOffset, uint16_t* dynamicInfoOffset) const;

  void                                                   AnnounceToRealm(CRealm* realm);
  void                                                   AnnounceDecreateToRealms();
  void                                                   AnnounceToAddress(std::string& address, uint8_t gameVersion);
  void                                                   ReplySearch(sockaddr_storage* address, CSocket* socket, uint8_t gameVersion);
  void                                                   SendGameDiscoveryInfo(uint8_t gameVersion);
  void                                                                                SendGameDiscoveryInfo();
  void                                                   SendGameDiscoveryInfoVLAN(CGameSeeker* gameSeeker) const;
  void                                                   SendGameDiscoveryRefresh() const;
  void                                                   SendGameDiscoveryCreate(uint8_t gameVersion) const;
  void                                                   SendGameDiscoveryCreate() const;
  void                                                   SendGameDiscoveryDecreate() const;

  // events
  // note: these are only called while iterating through the m_Potentials or m_Users std::vectors
  // therefore you can't modify those std::vectors and must use the player's m_DeleteMe member to flag for deletion

  void                      EventUserDeleted(GameUser::CGameUser* user, void* fd, void* send_fd);
  void                      EventLobbyLastPlayerLeaves();
  void                      ReportAllPings() const;
  void                      SetLaggingPlayerAndUpdate(GameUser::CGameUser* user);
  void                      SetEveryoneLagging();
  std::pair<int64_t, int64_t> GetReconnectWaitTicks() const;
  void                      ReportRecoverableDisconnect(GameUser::CGameUser* user);
  void                      OnRecoverableDisconnect(GameUser::CGameUser* user);
  bool                      CheckUserBanned(CConnection* connection, CIncomingJoinRequest* joinRequest, CRealm* matchingRealm, std::string& hostName);
  bool                      CheckIPBanned(CConnection* connection, CIncomingJoinRequest* joinRequest, CRealm* matchingRealm, std::string& hostName);
  void                      EventUserDisconnectTimedOut(GameUser::CGameUser* user);
  void                      EventUserDisconnectSocketError(GameUser::CGameUser* user);
  void                      EventUserDisconnectConnectionClosed(GameUser::CGameUser* user);
  void                      EventUserDisconnectGameProtocolError(GameUser::CGameUser* user, bool canRecover);
  void                      EventUserDisconnectGameAbuse(GameUser::CGameUser* user);
  void                      EventUserKickGProxyExtendedTimeout(GameUser::CGameUser* user);
  void                      EventUserKickUnverified(GameUser::CGameUser* user);
  void                      EventUserKickHandleQueued(GameUser::CGameUser* user);
  void                      EventUserAfterDisconnect(GameUser::CGameUser* user, bool fromOpen);
  void                      EventUserCheckStatus(GameUser::CGameUser* user);
  bool                      EventRequestJoin(CConnection* connection, CIncomingJoinRequest* joinRequest);
  void                      EventBeforeJoin(CConnection* connection);
  bool                      EventUserLeft(GameUser::CGameUser* user, const uint32_t clientReason);
  void                      EventUserLoaded(GameUser::CGameUser* user);
  bool                      EventUserAction(GameUser::CGameUser* user, CIncomingAction& action);
  void                      EventUserKeepAlive(GameUser::CGameUser* user);
  void                      EventUserChatToHost(GameUser::CGameUser* user, CIncomingChatPlayer* chatPlayer);
  void                      EventUserChangeTeam(GameUser::CGameUser* user, uint8_t team);
  void                      EventUserChangeColor(GameUser::CGameUser* user, uint8_t colour);
  void                      EventUserChangeRace(GameUser::CGameUser* user, uint8_t race);
  void                      EventUserChangeHandicap(GameUser::CGameUser* user, uint8_t handicap);
  void                      EventUserDropRequest(GameUser::CGameUser* user);
  bool                      EventUserMapSize(GameUser::CGameUser* user, CIncomingMapSize* mapSize);
  void                      EventUserPongToHost(GameUser::CGameUser* user);
  void                      EventUserMapReady(GameUser::CGameUser* user);

  // these events are called outside of any iterations

  void                      EventGameStartedLoading();
  void                      EventGameLoaded();
  void                      HandleGameLoadedStats();
  void                      ReleaseMapBusyTimedLock() const;
  void                      StartGameOverTimer(bool isMMD = false);
  void                      ClearActions();
  void                      Reset();
  bool                      GetIsRemakeable();
  void                      Remake();

  void                      AddProvisionalBannableUser(const GameUser::CGameUser* user);
  void                      ClearBannableUsers();
  void                      UpdateBannableUsers();
  bool                      ResolvePlayerObfuscation() const;
  void                      RunPlayerObfuscation();
  bool                      CheckSmartCommands(GameUser::CGameUser* user, const std::string& message, const uint8_t activeCmd, CCommandConfig* nConfig);

  // other functions

  uint8_t                   GetSIDFromUID(uint8_t UID) const;
  GameUser::CGameUser*                GetUserFromUID(uint8_t UID) const;
  GameUser::CGameUser*                GetUserFromSID(uint8_t SID) const;
  std::string               GetUserNameFromUID(uint8_t UID) const;
  GameUser::CGameUser*                GetUserFromName(std::string name, bool sensitive) const;
  bool                      HasOwnerSet() const;
  bool                      HasOwnerInGame() const;
  uint8_t                   GetUserFromNamePartial(const std::string& name, GameUser::CGameUser*& matchPlayer) const;
  uint8_t                   GetUserFromDisplayNamePartial(const std::string& name, GameUser::CGameUser*& matchPlayer) const;
  uint8_t                   GetBannableFromNamePartial(const std::string& name, CDBBan*& matchBanPlayer) const;
  CDBGamePlayer*            GetDBPlayerFromColor(uint8_t colour) const;
  GameUser::CGameUser*                GetPlayerFromColor(uint8_t colour) const;
  uint8_t                   GetColorFromUID(uint8_t UID) const;
  uint8_t                   GetNewUID() const;
  uint8_t                   GetNewTeam() const;
  uint8_t                   GetNewColor() const;
  uint8_t                   GetNewPseudonymUID() const;
  uint8_t                   SimulateActionUID(const uint8_t actionType, GameUser::CGameUser* user, const bool isDisconnect);
  uint8_t                   HostToMapCommunicationUID() const;
  bool                      GetHasAnyActiveTeam() const;
  bool                      GetHasAnyUser() const;
  bool                      GetIsPlayerSlot(const uint8_t SID) const;
  bool                      GetHasAnotherPlayer(const uint8_t ExceptSID) const;
  bool                      CheckIPFlood(const std::string joinName, const sockaddr_storage* sourceAddress) const;
  std::vector<uint8_t>      GetChatUIDs() const;
  std::vector<uint8_t>      GetChatUIDs(uint8_t excludeUID) const;
  std::vector<uint8_t>      GetObserverUIDs() const;
  std::vector<uint8_t>      GetChatObserverUIDs(uint8_t excludeUID) const;
  uint8_t                   GetPublicHostUID() const;
  uint8_t                   GetHiddenHostUID() const;
  uint8_t                   GetHostUID() const;
  uint8_t                   GetHMCSID() const;
  uint8_t                   GetEmptySID(bool reserved) const;
  uint8_t                   GetEmptySID(uint8_t team, uint8_t UID) const;
  uint8_t                   GetEmptyPlayerSID() const;
  uint8_t                   GetEmptyObserverSID() const;
  inline bool               GetHMCEnabled() const { return m_HMCEnabled; }
  void                      SendIncomingPlayerInfo(GameUser::CGameUser* user) const;
  GameUser::CGameUser*                JoinPlayer(CConnection* connection, CIncomingJoinRequest* joinRequest, const uint8_t SID, const uint8_t UID, const uint8_t HostCounterID, const std::string JoinedRealm, const bool IsReserved, const bool IsUnverifiedAdmin);  
  bool                      CreateVirtualHost();
  bool                      DeleteVirtualHost();
  bool                      GetHasPvPGNPlayers() const;

  // Map transfer
  inline SharedByteArray    GetLoadedMapChunk() { return m_LoadedMapChunk; }
  void                      SetLoadedMapChunk(SharedByteArray nLoadedMapChunk) { m_LoadedMapChunk = nLoadedMapChunk; }
  void                      ClearLoadedMapChunk() { m_LoadedMapChunk.reset(); }
  FileChunkTransient        GetMapChunk(size_t start);

  // Slot manipulation

  CGameSlot* GetSlot(const uint8_t SID);
  const CGameSlot* InspectSlot(const uint8_t SID) const;
  void InitPRNG();
  void InitSlots();
  bool SwapEmptyAllySlot(const uint8_t SID);
  bool SwapSlots(const uint8_t SID1, const uint8_t SID2);
  bool OpenSlot(const uint8_t SID, const bool kick);
  bool CanLockSlotForJoins(uint8_t SID);
  bool CloseSlot(const uint8_t SID, const bool kick);
  bool OpenSlot();
  bool CloseSlot();
  bool ComputerSlotInner(const uint8_t SID, const uint8_t skill, const bool ignoreLayout = false, const bool overrideComputers = false);
  bool ComputerSlot(const uint8_t SID, const uint8_t skill, bool kick);
  bool SetSlotColor(const uint8_t SID, const uint8_t colour, const bool force);
  bool SetSlotTeam(const uint8_t SID, const uint8_t team, const bool force);
  void SetSlotTeamAndColorAuto(const uint8_t SID);

  void OpenObserverSlots();
  void CloseObserverSlots();
  CGameVirtualUser* GetVirtualUserFromSID(const uint8_t SID);
  const CGameVirtualUser* InspectVirtualUserFromSID(const uint8_t SID) const;
  void CreateFakeUserInner(const uint8_t SID, const uint8_t UID, const std::string& name);
  bool CreateFakeUser(const bool useVirtualHostName);
  bool CreateHMCPlayer();
  bool CreateFakePlayer(const bool useVirtualHostName);
  bool CreateFakeObserver(const bool useVirtualHostName);
  bool DeleteFakeUser(uint8_t SID);

  uint8_t FakeAllSlots();
  void DeleteFakeUsersLobby();
  void DeleteFakeUsersLoaded();
  void OpenAllSlots();
  uint8_t GetFirstCloseableSlot();
  bool CloseAllTeamSlots(const uint8_t team);
  bool CloseAllTeamSlots(const std::bitset<MAX_SLOTS_MODERN> occupiedTeams);
  bool CloseAllSlots();
  bool ComputerNSlots(const uint8_t expectedCount, const uint8_t skill, const bool ignoreLayout = false, const bool overrideComputers = false);
  bool ComputerAllSlots(const uint8_t skill);
  void ShuffleSlots();

  void ReportSpoofed(const std::string& server, GameUser::CGameUser* user);
  void AddToRealmVerified(const std::string& server, GameUser::CGameUser* user, bool sendMessage);
  void AddToReserved(const std::string& name);
  void RemoveFromReserved(const std::string& name);
  void RemoveAllReserved();
  bool MatchOwnerName(const std::string& name) const;
  uint8_t GetReservedIndex(const std::string& name) const;

  std::string GetBannableIP(const std::string& name, const std::string& hostName) const;
  bool GetIsScopeBanned(const std::string& name, const std::string& hostName, const std::string& addressLiteral) const;
  bool CheckScopeBanned(const std::string& name, const std::string& hostName, const std::string& addressLiteral);
  bool AddScopeBan(const std::string& name, const std::string& hostName, const std::string& addressLiteral);
  bool RemoveScopeBan(const std::string& name, const std::string& hostName);

  std::vector<uint32_t> GetPlayersFramesBehind() const;
  UserList GetLaggingUsers() const;
  uint8_t CountLaggingPlayers() const;
  UserList CalculateNewLaggingPlayers() const;
  void RemoveFromLagScreens(GameUser::CGameUser* user) const;
  void ResetLagScreen();
  void ResetLatency();
  void NormalizeSyncCounters() const;
  bool GetIsReserved(const std::string& name) const;
  bool GetIsProxyReconnectable() const;
  bool GetIsProxyReconnectableLong() const;
  bool IsDownloading() const;
  void UncacheOwner();
  void SetOwner(const std::string& name, const std::string& realm);
  void ReleaseOwner();
  void ResetDraft();
  void ResetTeams(const bool alsoCaptains);
  void ResetSync();
  void CountKickVotes();
  bool GetCanStartGracefulCountDown() const;
  void StartCountDown(bool fromUser, bool force);
  void StartCountDownFast(bool force);
  void StopCountDown();
  bool SendEveryoneElseLeftAndDisconnect(const std::string& reason) const;
  void ShowPlayerNamesGameStartLoading();
  void ShowPlayerNamesInGame();
  bool StopPlayers(const std::string& reason);
  void StopLagger(GameUser::CGameUser* user, const std::string& reason);
  void StopLaggers(const std::string& reason);
  void StopDesynchronized(const std::string& reason);
  void StopLoadPending(const std::string& reason);
  void ResetDropVotes() const;
  std::string GetSaveFileName(const uint8_t UID) const;
  bool Save(GameUser::CGameUser* user, CQueuedActionsFrame& actionFrame, const bool isDisconnect);
  void SaveEnded(const uint8_t exceptUID, CQueuedActionsFrame& actionFrame);
  bool Pause(GameUser::CGameUser* user, CQueuedActionsFrame& actionFrame, const bool isDisconnect);
  bool Resume(GameUser::CGameUser* user, CQueuedActionsFrame& actionFrame, const bool isDisconnect);
  bool TrySaveOnDisconnect(GameUser::CGameUser* user, const bool isVoluntary);
  bool Save(GameUser::CGameUser* user, const bool isDisconnect);
  void SaveEnded(const uint8_t exceptUID);
  bool Pause(GameUser::CGameUser* user, const bool isDisconnect);
  bool Resume(GameUser::CGameUser* user, const bool isDisconnect);
  inline bool GetIsVerbose() { return m_Verbose; }
  bool SendChatTrigger(const uint8_t UID, const std::string& message, const uint32_t firstByte, const uint32_t secondByte);
  bool SendChatTriggerSymmetric(const uint8_t UID, const std::string& message, const uint8_t firstIdentifier, const uint8_t secondIdentifier);
  bool SendHMC(const std::string& message);
  bool GetIsCheckJoinable() const;
  void SetIsCheckJoinable(const bool nCheckIsJoinable);
  inline bool GetSentPriorityWhois() const { return m_SentPriorityWhois; }
  bool GetHasReferees() const;
  inline bool GetUsesCustomReferees() const { return m_UsesCustomReferees; }
  bool GetIsSupportedGameVersion(uint8_t nVersion) const;
  inline void SetSentPriorityWhois(const bool nValue) { m_SentPriorityWhois = nValue; }
  inline void SetCheckReservation(const bool nValue) { m_CheckReservation = nValue; }
  inline void SetUsesCustomReferees(const bool nValue) { m_UsesCustomReferees = nValue; }
  inline void SetSupportedGameVersion(uint8_t nVersion);
  inline void SetSaveOnLeave(const uint8_t nValue) { m_SaveOnLeave = nValue; }

  inline void SetIsReplaceable(const bool nValue) { m_Replaceable = nValue; }
  inline void SetIsBeingReplaced(const bool nValue) { m_Replacing = nValue; }

  bool GetIsAutoVirtualPlayers() const { return m_IsAutoVirtualPlayers; }
  void SetAutoVirtualPlayers(const bool nEnableVirtualHostPlayer) { m_IsAutoVirtualPlayers = nEnableVirtualHostPlayer; }
  void RemoveCreator();

  uint8_t GetNumEnabledTeamSlots(const uint8_t team) const;
  std::vector<uint8_t> GetNumFixedComputersByTeam() const;
  std::vector<uint8_t> GetPotentialTeamSizes() const;
  std::pair<uint8_t, uint8_t> GetLargestPotentialTeam() const;
  std::pair<uint8_t, uint8_t> GetSmallestPotentialTeam(const uint8_t minSize, const uint8_t exceptTeam) const;
  std::vector<uint8_t> GetActiveTeamSizes() const;
  uint8_t GetSelectableTeamSlotFront(const uint8_t team, const uint8_t endOccupiedSID,const uint8_t endOpenSID, const bool force) const;
  uint8_t GetSelectableTeamSlotBack(const uint8_t team, const uint8_t endOccupiedSID,const uint8_t endOpenSID, const bool force) const;
  uint8_t GetSelectableTeamSlotBackExceptHumanLike(const uint8_t team, const uint8_t endOccupiedSID,const uint8_t endOpenSID, const bool force) const;
  uint8_t GetSelectableTeamSlotBackExceptComputer(const uint8_t team, const uint8_t endOccupiedSID,const uint8_t endOpenSID, const bool force) const;
  bool FindHumanVsAITeams(const uint8_t humanCount, const uint8_t computerCount, std::pair<uint8_t, uint8_t>& teams) const;
  uint8_t GetOneVsAllTeamAll() const;
  uint8_t GetOneVsAllTeamOne(const uint8_t teamAll) const;

  // These are the main game modes
  inline void SetDraftMode(const bool nIsDraftMode) {
    m_IsDraftMode = nIsDraftMode;
    if (nIsDraftMode) {
      m_CustomLayout |= CUSTOM_LAYOUT_DRAFT;
    } else {
      m_CustomLayout &= ~CUSTOM_LAYOUT_DRAFT;
    }
  }

  void ResetLayout(const bool quiet);
  void ResetLayoutIfNotMatching();
  bool SetLayoutFFA();
  bool SetLayoutOneVsAll(const GameUser::CGameUser* user);
  bool SetLayoutTwoTeams();
  bool SetLayoutHumansVsAI(const uint8_t humanTeam, const uint8_t computerTeam);
  bool SetLayoutCompact();
};

#endif // AURA_GAME_H_
