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

#ifndef AURA_GAMEUSER_H_
#define AURA_GAMEUSER_H_

#include "includes.h"
#include "connection.h"

//
// GameUser::CGameUser
//

namespace GameUser
{
  namespace KickReason
  {
    constexpr uint8_t NONE = 0u;
    constexpr uint8_t MAP_MISSING = 1u;
    constexpr uint8_t HIGH_PING = 2u;
    constexpr uint8_t SPOOFER = 4u;
    constexpr uint8_t ABUSER = 8u;
  };

  class CGameUser final : public CConnection
  {
  public:
    CGame*                           m_Game;
    std::array<uint8_t, 4>           m_IPv4Internal;                 // the player's internal IP address as reported by the player when connecting
    std::vector<uint32_t>            m_RTTValues;                    // store the last few (10) pings received so we can take an average
    OptionalTimedUint32              m_MeasuredRTT;
    std::queue<uint32_t>             m_CheckSums;                    // the last few checksums the player has sent (for detecting desyncs)
    std::queue<std::vector<uint8_t>> m_GProxyBuffer;                 // buffer with data used with GProxy++
    std::string                      m_LeftReason;                   // the reason the player left the game
    uint32_t                         m_RealmInternalId;
    std::string                      m_RealmHostName;                // the realm the player joined on (probable, can be spoofed)
    std::string                      m_Name;                         // the player's name
    size_t                           m_TotalPacketsSent;             // the total number of packets sent to the player
    uint32_t                         m_TotalPacketsReceived;         // the total number of packets received from the player
    uint32_t                         m_LeftCode;                     // the code to be sent in W3GS_PLAYERLEAVE_OTHERS for why this player left the game
    uint8_t                          m_Status;
    bool                             m_IsLeaver;
    uint8_t                          m_PingEqualizerOffset;          // how many frames are actions sent by this player offset by ping equalizer
    QueuedActionsFrameNode*          m_PingEqualizerFrameNode;
    uint32_t                         m_PongCounter;
    uint32_t                         m_SyncCounterOffset;            // missed keepalive packets we are gonna ignore
    uint32_t                         m_SyncCounter;                  // the number of keepalive packets received from this player
    int64_t                          m_JoinTicks;                    // GetTime when the player joined the game (used to delay sending the /whois a few seconds to allow for some lag)
    uint32_t                         m_LastMapPartSentOffsetEnd;     // the last mappart sent to the player (for sending more than one part at a time)
    uint32_t                         m_LastMapPartAcked;             // the last mappart acknowledged by the player
    int64_t                          m_StartedDownloadingTicks;      // GetTicks when the player started downloading the map
    int64_t                          m_FinishedDownloadingTime;      // GetTime when the player finished downloading the map
    int64_t                          m_FinishedLoadingTicks;         // GetTicks when the player finished loading the game
    int64_t                          m_StartedLaggingTicks;          // GetTicks when the player started laggin
    int64_t                          m_LastGProxyWaitNoticeSentTime; // GetTime when the last disconnection notice has been sent when using GProxy++
    uint32_t                         m_GProxyReconnectKey;           // the GProxy++ reconnect key
    std::optional<int64_t>           m_KickByTicks;
    std::optional<int64_t>           m_LastGProxyAckTicks;           // GetTime when we last acknowledged GProxy++ packet
    uint8_t                          m_UID;                          // the player's UID
    uint8_t                          m_OldUID;
    uint8_t                          m_PseudonymUID;
    bool                             m_Verified;                     // if the player has spoof checked or not
    bool                             m_Owner;                        // if the player has spoof checked or not
    bool                             m_Reserved;                     // if the player is reserved (VIP) or not
    std::optional<int64_t>           m_SudoMode;                     // if the player has enabled sudo mode, its expiration time
    bool                             m_Observer;                     // if the player is an observer
    bool                             m_PowerObserver;                // if the player is a referee - referees can be demoted to full observers
    bool                             m_WhoisShouldBeSent;            // if a battle.net /whois should be sent for this player or not
    bool                             m_WhoisSent;                    // if we've sent a battle.net /whois for this player yet (for spoof checking)
    bool                             m_MapReady;                     // if we're allowed to download the map or not (used with permission based map downloads)
    std::optional<bool>              m_UserReady;
    bool                             m_Ready;
    std::optional<int64_t>           m_ReadyReminderLastTicks;
    uint8_t                          m_KickReason;                   // bitmask for all the reasons why this user is going to be kicked
    bool                             m_HasHighPing;                  // if last time we checked, the player had high ping
    bool                             m_DownloadAllowed;              // if we're allowed to download the map or not (used with permission based map downloads)
    bool                             m_DownloadStarted;              // if we've started downloading the map or not
    bool                             m_DownloadFinished;             // if we've finished downloading the map or not
    bool                             m_FinishedLoading;              // if the player has finished loading or not
    bool                             m_Lagging;                      // if the player is lagging or not (on the lag screen)
    std::optional<bool>              m_DropVote;                     // if the player voted to drop the laggers or not (on the lag screen)
    std::optional<bool>              m_KickVote;                     // if the player voted to kick a player or not
    bool                             m_Muted;                        // if the player is muted or not
    bool                             m_ActionLocked;                 // if the player is not allowed to use commands, change their race/team/color/handicap or they are
    bool                             m_LeftMessageSent;              // if the playerleave message has been sent or not
    bool                             m_StatusMessageSent;            // if the message regarding player connection mode has been sent or not
    bool                             m_LatencySent;
    bool                             m_UsedAnyCommands;              // if the playerleave message has been sent or not
    bool                             m_SentAutoCommandsHelp;         // if the playerleave message has been sent or not
    uint8_t                          m_SmartCommand;
    int64_t                          m_CheckStatusByTicks;

    bool                             m_GProxy;                       // if the player is using GProxy++
    uint16_t                         m_GProxyPort;                   // port where GProxy will try to reconnect
    bool                             m_GProxyCheckGameID;
    bool                             m_GProxyDisconnectNoticeSent;   // if a disconnection notice has been sent or not when using GProxy++

    bool                             m_GProxyExtended;               // if the player is using GProxyDLL
    uint32_t                         m_GProxyVersion;
    bool                             m_Disconnected;
    int64_t                          m_TotalDisconnectTicks;
    std::optional<int64_t>           m_LastDisconnectTicks;

    std::string                      m_LastCommand;
    uint8_t                          m_TeamCaptain;

    std::string                      m_PinnedMessage;

    // Actions
    uint8_t                          m_RemainingSaves;
    uint8_t                          m_RemainingPauses;

    CGameUser(CGame* game, CConnection* connection, uint8_t nUID, uint32_t nJoinedRealmInternalId, std::string nJoinedRealm, std::string nName, std::array<uint8_t, 4> nInternalIP, bool nReserved);
    ~CGameUser() final;

    [[nodiscard]] uint32_t GetOperationalRTT() const;
    [[nodiscard]] uint32_t GetDisplayRTT() const;
    [[nodiscard]] uint32_t GetRTT() const;
    [[nodiscard]] std::string GetConnectionErrorString() const;
    [[nodiscard]] inline bool                     GetIsReady() const { return m_Ready; }
    [[nodiscard]] inline uint8_t                  GetUID() const { return m_UID; }
    [[nodiscard]] inline uint8_t                  GetOldUID() const { return m_OldUID; }
    [[nodiscard]] inline uint8_t                  GetPseudonymUID() const { return m_PseudonymUID; }
    [[nodiscard]] inline std::string              GetName() const { return m_Name; }
    [[nodiscard]] std::string                     GetLowerName() const;
    [[nodiscard]] std::string                     GetDisplayName() const;
    [[nodiscard]] inline std::array<uint8_t, 4>   GetIPv4Internal() const { return m_IPv4Internal; }
    [[nodiscard]] inline size_t                   GetStoredRTTCount() const { return m_RTTValues.size(); }
    [[nodiscard]] inline bool                     GetIsRTTMeasured() const { return m_MeasuredRTT.has_value() || !m_RTTValues.empty(); }
    [[nodiscard]] inline bool                     GetIsRTTMeasuredConsistent() const { return m_MeasuredRTT.has_value() || GetStoredRTTCount() >= CONSISTENT_PINGS_COUNT; }
    [[nodiscard]] inline bool                     GetIsRTTMeasuredBadConsistent() const { return m_MeasuredRTT.has_value() || GetStoredRTTCount() >= 2; }
    [[nodiscard]] inline uint32_t                 GetPongCounter() const { return m_PongCounter; }
    [[nodiscard]] inline size_t                   GetNumCheckSums() const { return m_CheckSums.size(); }
    [[nodiscard]] inline std::queue<uint32_t>*    GetCheckSums() { return &m_CheckSums; }
    [[nodiscard]] inline bool                     HasCheckSums() const { return !m_CheckSums.empty(); }
    [[nodiscard]] inline bool                     HasLeftReason() const { return !m_LeftReason.empty(); }
    [[nodiscard]] inline std::string              GetLeftReason() const { return m_LeftReason; }
    [[nodiscard]] inline uint32_t                 GetLeftCode() const { return m_LeftCode; }
    [[nodiscard]] inline bool                     GetIsLeaver() const { return m_IsLeaver; }
    [[nodiscard]] inline bool                     GetIsInLoadingScreen() const { return m_Status == USERSTATUS_LOADING_SCREEN; }
    [[nodiscard]] inline bool                     GetIsEnding() const { return m_Status == USERSTATUS_ENDING; }
    [[nodiscard]] inline bool                     GetIsEnded() const { return m_Status == USERSTATUS_ENDED; }
    [[nodiscard]] inline bool                     GetIsEndingOrEnded() const { return m_Status == USERSTATUS_ENDING || m_Status == USERSTATUS_ENDED; }
    [[nodiscard]] inline bool                     GetIsLobbyOrPlaying() const { return m_Status == USERSTATUS_LOBBY || m_Status == USERSTATUS_PLAYING; }
    [[nodiscard]] inline uint8_t                  GetPingEqualizerOffset() const { return m_PingEqualizerOffset; }
    [[nodiscard]] uint32_t                        GetPingEqualizerDelay() const;
    [[nodiscard]] inline QueuedActionsFrameNode*  GetPingEqualizerFrameNode() const { return m_PingEqualizerFrameNode; }
    [[nodiscard]] CQueuedActionsFrame&            GetPingEqualizerFrame();
    [[nodiscard]] CRealm*                         GetRealm(bool mustVerify) const;
    [[nodiscard]] std::string                     GetRealmDataBaseID(bool mustVerify) const;
    [[nodiscard]] inline uint32_t                 GetRealmInternalID() const { return m_RealmInternalId; }
    [[nodiscard]] inline std::string              GetRealmHostName() const { return m_RealmHostName; }
    [[nodiscard]] inline std::string              GetExtendedName() const {
      if (m_RealmHostName.empty()) {
        return m_Name + "@@@LAN/VPN";
      } else {
        return m_Name + "@" + m_RealmHostName;
      }
    }
    [[nodiscard]] inline bool                  IsRealmVerified() const { return m_Verified; }
    [[nodiscard]] inline uint32_t              GetSyncCounter() const { return m_SyncCounter; }
    [[nodiscard]] inline uint32_t              GetNormalSyncCounter() const { return m_SyncCounter + m_SyncCounterOffset; }
    [[nodiscard]] bool                         GetIsBehindFramesNormal(const uint32_t limit) const;
    [[nodiscard]] inline int64_t               GetJoinTicks() const { return m_JoinTicks; }
    [[nodiscard]] inline uint32_t              GetLastMapPartSentOffsetEnd() const { return m_LastMapPartSentOffsetEnd; }
    [[nodiscard]] inline uint32_t              GetLastMapPartAcked() const { return m_LastMapPartAcked; }
    [[nodiscard]] inline int64_t               GetStartedDownloadingTicks() const { return m_StartedDownloadingTicks; }
    [[nodiscard]] inline int64_t               GetFinishedDownloadingTime() const { return m_FinishedDownloadingTime; }
    [[nodiscard]] inline int64_t               GetFinishedLoadingTicks() const { return m_FinishedLoadingTicks; }

    [[nodiscard]] inline int64_t               GetStartedLaggingTicks() const { return m_StartedLaggingTicks; }
    [[nodiscard]] inline int64_t               GetLastGProxyWaitNoticeSentTime() const { return m_LastGProxyWaitNoticeSentTime; }
    [[nodiscard]] inline uint32_t              GetGProxyReconnectKey() const { return m_GProxyReconnectKey; }
    [[nodiscard]] inline bool                  GetGProxyCheckGameID() const { return m_GProxyCheckGameID; }
    [[nodiscard]] inline bool                  GetGProxyAny() const { return m_GProxy; }
    [[nodiscard]] inline bool                  GetGProxyLegacy() const { return m_GProxy && !m_GProxyExtended; }
    [[nodiscard]] inline bool                  GetGProxyExtended() const { return m_GProxyExtended; }
    [[nodiscard]] inline bool                  GetGProxyDisconnectNoticeSent() const { return m_GProxyDisconnectNoticeSent; }
    
    [[nodiscard]] inline bool                  GetDisconnected() const { return m_Disconnected; }
    [[nodiscard]] inline bool                  GetDisconnectedUnrecoverably() const { return m_Disconnected && !m_GProxy; }
    [[nodiscard]] int64_t                      GetTotalDisconnectTicks() const;
    [[nodiscard]] std::string                  GetDelayText(bool displaySync) const;
    [[nodiscard]] std::string                  GetReconnectionText() const;
    [[nodiscard]] std::string                  GetSyncText() const;
    
    [[nodiscard]] inline bool                  GetIsReserved() const { return m_Reserved; }
    [[nodiscard]] bool                         GetIsSudoMode() const;
    [[nodiscard]] bool                         CheckSudoMode();
    void                                       SudoModeStart();
    void                                       SudoModeEnd();
    [[nodiscard]] inline bool                  GetIsObserver() const { return m_Observer; }
    [[nodiscard]] inline bool                  GetIsPowerObserver() const { return m_PowerObserver; }
    [[nodiscard]] bool                         GetIsNativeReferee() const;
    [[nodiscard]] bool                         GetCanUsePublicChat() const;
    [[nodiscard]] inline bool                  GetWhoisShouldBeSent() const { return m_WhoisShouldBeSent; }
    [[nodiscard]] inline bool                  GetWhoisSent() const { return m_WhoisSent; }
    [[nodiscard]] inline bool                  GetDownloadAllowed() const { return m_DownloadAllowed; }
    [[nodiscard]] inline bool                  GetDownloadStarted() const { return m_DownloadStarted; }
    [[nodiscard]] inline bool                  GetDownloadFinished() const { return m_DownloadFinished; }
    [[nodiscard]] inline bool                  GetFinishedLoading() const { return m_FinishedLoading; }
    [[nodiscard]] inline bool                  GetMapReady() const { return m_MapReady; }
    [[nodiscard]] inline bool                  GetMapKicked() const { return (m_KickReason & GameUser::KickReason::MAP_MISSING) != GameUser::KickReason::NONE; }
    [[nodiscard]] inline bool                  GetPingKicked() const { return (m_KickReason & GameUser::KickReason::HIGH_PING) != GameUser::KickReason::NONE; }
    [[nodiscard]] inline bool                  GetSpoofKicked() const { return (m_KickReason & GameUser::KickReason::SPOOFER) != GameUser::KickReason::NONE; }
    [[nodiscard]] inline bool                  GetAbuseKicked() const { return (m_KickReason & GameUser::KickReason::ABUSER) != GameUser::KickReason::NONE; }
    [[nodiscard]] inline bool                  GetAnyKicked() const { return m_KickReason != GameUser::KickReason::NONE; }
    [[nodiscard]] inline bool                  GetHasHighPing() const { return m_HasHighPing; }
    [[nodiscard]] inline bool                  GetKickQueued() const { return m_KickByTicks.has_value(); }
    [[nodiscard]] inline bool                  GetLagging() const { return m_Lagging; }
    [[nodiscard]] inline std::optional<bool>   GetDropVote() const { return m_DropVote; }
    [[nodiscard]] inline std::optional<bool>   GetKickVote() const { return m_KickVote; }
    [[nodiscard]] inline bool                  GetMuted() const { return m_Muted; }
    [[nodiscard]] inline bool                  GetIsActionLocked() const { return m_ActionLocked; }
    [[nodiscard]] inline bool                  GetStatusMessageSent() const { return m_StatusMessageSent; }
    [[nodiscard]] inline bool                  GetLatencySent() const { return m_LatencySent; }
    [[nodiscard]] inline bool                  GetLeftMessageSent() const { return m_LeftMessageSent; }
    [[nodiscard]] inline bool                  GetUsedAnyCommands() const { return m_UsedAnyCommands; }
    [[nodiscard]] inline bool                  GetSentAutoCommandsHelp() const { return m_SentAutoCommandsHelp; }
    [[nodiscard]] inline uint8_t               GetSmartCommand() const { return m_SmartCommand; }
    [[nodiscard]] bool                         UpdateReady();
    [[nodiscard]] bool                         GetIsOwner(std::optional<bool> nAssumeVerified) const;
    [[nodiscard]] inline bool                  GetIsDraftCaptain() { return m_TeamCaptain != 0; }
    [[nodiscard]] inline bool                  GetIsDraftCaptainOf(const uint8_t nTeam) { return m_TeamCaptain == nTeam + 1; }
    [[nodiscard]] inline bool                  GetCanPause() { return m_RemainingPauses > 0; }
    [[nodiscard]] inline bool                  GetCanSave() { return m_RemainingSaves > 0; }

    inline void SetLeftReason(const std::string& nLeftReason) { m_LeftReason = nLeftReason; }
    inline void SetLeftCode(uint32_t nLeftCode) { m_LeftCode = nLeftCode; }
    inline void SetIsLeaver(bool nIsLeaver) { m_IsLeaver = nIsLeaver; }
    inline void SetStatus(uint8_t nStatus) { m_Status = nStatus; }
    inline void TrySetEnding() {
      if (m_Status != USERSTATUS_ENDED) {
        m_Status = USERSTATUS_ENDING;
      }
    }
    inline void SetPingEqualizerOffset(uint8_t nOffset) { m_PingEqualizerOffset = nOffset; }
    void AdvanceActiveGameFrame();
    bool AddDelayPingEqualizerFrame();
    bool SubDelayPingEqualizerFrame();
    void SetPingEqualizerFrameNode(QueuedActionsFrameNode* nFrame) { m_PingEqualizerFrameNode = nFrame; }
    inline void SetSyncCounter(uint32_t nSyncCounter) { m_SyncCounter = nSyncCounter; }
    inline void AddSyncCounterOffset(const uint32_t nOffset) { m_SyncCounterOffset += nOffset; }
    inline void ResetSyncCounterOffset() { m_SyncCounterOffset = 0; }
    inline void SetLastMapPartSentOffsetEnd(uint32_t nLastMapPartSentOffsetEnd) { m_LastMapPartSentOffsetEnd = nLastMapPartSentOffsetEnd; }
    inline void SetLastMapPartAcked(uint32_t nLastMapPartAcked) { m_LastMapPartAcked = nLastMapPartAcked; }
    inline void SetStartedDownloadingTicks(uint64_t nStartedDownloadingTicks) { m_StartedDownloadingTicks = nStartedDownloadingTicks; }
    inline void SetFinishedDownloadingTime(uint64_t nFinishedDownloadingTime) { m_FinishedDownloadingTime = nFinishedDownloadingTime; }
    inline void SetStartedLaggingTicks(uint64_t nStartedLaggingTicks) { m_StartedLaggingTicks = nStartedLaggingTicks; }
    inline void SetRealmVerified(bool nVerified) { m_Verified = nVerified; }
    inline void SetOwner(bool nOwner) { m_Owner = nOwner; }
    inline void SetReserved(bool nReserved) { m_Reserved = nReserved; }
    inline void SetObserver(bool nObserver) { m_Observer = nObserver; }
    inline void SetPseudonymUID(uint8_t nUID) { m_PseudonymUID = nUID; }
    inline void SetPowerObserver(bool nPowerObserver) { m_PowerObserver = nPowerObserver; }
    inline void SetWhoisShouldBeSent(bool nWhoisShouldBeSent) { m_WhoisShouldBeSent = nWhoisShouldBeSent; }
    inline void SetDownloadAllowed(bool nDownloadAllowed) { m_DownloadAllowed = nDownloadAllowed; }
    inline void SetDownloadStarted(bool nDownloadStarted) { m_DownloadStarted = nDownloadStarted; }
    inline void SetDownloadFinished(bool nDownloadFinished) { m_DownloadFinished = nDownloadFinished; }
    inline void SetMapReady(bool nHasMap) { m_MapReady = nHasMap; }
    inline void SetHasHighPing(bool nHasHighPing) { m_HasHighPing = nHasHighPing; }
    inline void SetLagging(bool nLagging) { m_Lagging = nLagging; }
    inline void SetDropVote(bool nDropVote) { m_DropVote = nDropVote; }
    inline void SetKickVote(bool nKickVote) { m_KickVote = nKickVote; }
    inline void SetMuted(bool nMuted) { m_Muted = nMuted; }
    inline void SetActionLocked(bool nActionLocked) { m_ActionLocked = nActionLocked; }
    inline void SetStatusMessageSent(bool nStatusMessageSent) { m_StatusMessageSent = nStatusMessageSent; }
    inline void SetLatencySent(bool nLatencySent) { m_LatencySent = nLatencySent; }
    inline void SetLeftMessageSent(bool nLeftMessageSent) { m_LeftMessageSent = nLeftMessageSent; }
    inline void SetGProxy(bool nGProxy) { m_GProxy = nGProxy; }
    inline void SetGProxyExtended(bool nGProxyExtended) { m_GProxyExtended = nGProxyExtended; }
    inline void SetGProxyDisconnectNoticeSent(bool nGProxyDisconnectNoticeSent) { m_GProxyDisconnectNoticeSent = nGProxyDisconnectNoticeSent; }
    inline void SetLastGProxyWaitNoticeSentTime(uint64_t nLastGProxyWaitNoticeSentTime) { m_LastGProxyWaitNoticeSentTime = nLastGProxyWaitNoticeSentTime; }
    void DisableReconnect();
    inline void SetKickByTicks(int64_t nKickByTicks) { m_KickByTicks = nKickByTicks; }
    inline void ClearKickByTicks() { m_KickByTicks = std::nullopt; }
    inline void AddKickReason(const uint8_t nKickReason) { m_KickReason |= nKickReason; }
    inline void RemoveKickReason(const uint8_t nKickReason) { m_KickReason &= ~nKickReason; }
    inline void ResetKickReason() { m_KickReason = GameUser::KickReason::NONE; }
    inline void KickAtLatest(int64_t nKickByTicks) {
      if (!m_KickByTicks.has_value() || nKickByTicks < m_KickByTicks.value()) {
        m_KickByTicks = nKickByTicks;
      }
    }
    inline void ResetLeftReason() { m_LeftReason.clear(); }
    inline void SetUserReady(bool nReady) { m_UserReady = nReady; }
    inline void ClearUserReady() { m_UserReady = std::nullopt; }

    [[nodiscard]] bool GetReadyReminderIsDue() const;
    void SetReadyReminded();

    [[nodiscard]] inline std::string GetLastCommand() const { return m_LastCommand; }
    inline void ClearLastCommand() { m_LastCommand.clear(); }
    inline void SetLastCommand(const std::string nLastCommand) { m_LastCommand = nLastCommand; }
    inline void SetDraftCaptain(const uint8_t nTeamNumber) { m_TeamCaptain = nTeamNumber; }
    inline void DropRemainingSaves() { --m_RemainingSaves; }
    inline void SetRemainingSaves(uint8_t nCount) { m_RemainingSaves = nCount; }
    inline void SetCannotSave() { m_RemainingSaves = 0; }
    inline void SetUsedAnyCommands(const bool nValue) { m_UsedAnyCommands = nValue; }
    inline void SetSentAutoCommandsHelp(const bool nValue) { m_SentAutoCommandsHelp = nValue; }
    inline void SetSmartCommand(const uint8_t nValue) { m_SmartCommand = nValue; }
    inline void ClearSmartCommand() { m_SmartCommand = SMART_COMMAND_NONE; }
    inline void DropRemainingPauses() { --m_RemainingPauses; }
    inline void SetCannotPause() { m_RemainingPauses = 0; }
    void ClearStalePings();

    [[nodiscard]] inline const std::string& GetPinnedMessage() { return  m_PinnedMessage; }
    [[nodiscard]] inline bool GetHasPinnedMessage() { return !m_PinnedMessage.empty(); }
    inline void SetPinnedMessage(const std::string nPinnedMessage) { m_PinnedMessage = nPinnedMessage; }
    inline void ClearPinnedMessage() { m_PinnedMessage.clear(); }

    void RefreshUID();

    // processing functions

    [[nodiscard]] bool Update(void* fd, int64_t timeout);

    // other functions

    void Send(const std::vector<uint8_t>& data) final;

    void EventGProxyReconnect(CConnection* connection, const uint32_t LastPacket);
    void EventGProxyReconnectInvalid();
    void RotateGProxyReconnectKey() const;
    void CloseConnection(bool fromOpen = false);
    void UnrefConnection(bool deferred = false);

    void InitGProxy(const uint32_t version);
    void ConfirmGProxyExtended(const std::vector<uint8_t>& data);
    void UpdateGProxyEmptyActions() const;
    void CheckGProxyExtendedStartHandShake() const;
  };

  [[nodiscard]] inline std::string ToNameListSentence(ImmutableUserList userList, bool useRealNames = false) {
    if (userList.empty()) return std::string();
    std::vector<std::string> userNames;
    for (const auto& user : userList) {
      if (useRealNames) {
        userNames.push_back("[" + user->GetName() + "]");
      } else {
        userNames.push_back("[" + user->GetDisplayName() + "]");
      }
    }
    return JoinVector(userNames, ", ", false);
  }

  [[nodiscard]] inline std::string ToNameListSentence(UserList userList, bool useRealNames = false) {
    if (userList.empty()) return std::string();
    std::vector<std::string> userNames;
    for (const auto& user : userList) {
      if (useRealNames) {
        userNames.push_back("[" + user->GetName() + "]");
      } else {
        userNames.push_back("[" + user->GetDisplayName() + "]");
      }
    }
    return JoinVector(userNames, ", ", false);
  }
};

#endif // AURA_GAMEUSER_H_
