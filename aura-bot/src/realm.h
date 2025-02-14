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

#ifndef AURA_REALM_H_
#define AURA_REALM_H_

#include "includes.h"
#include "socket.h"
#include "config/config_realm.h"

#include <fstream>

#define PACKET_TYPE_GAME_LIST 10
#define PACKET_TYPE_GAME_REFRESH 8
#define PACKET_TYPE_CHAT_BLOCKING 6
#define PACKET_TYPE_CHAT_JOIN 2
#define PACKET_TYPE_PRIORITY 1
#define PACKET_TYPE_DEFAULT 0

#define REALM_TCP_KEEPALIVE_IDLE_TIME 900
#define REALM_APP_KEEPALIVE_IDLE_TIME 180
#define REALM_APP_KEEPALIVE_INTERVAL 30
#define REALM_APP_KEEPALIVE_MAX_MISSED 4

//
// CRealm
//

class CRealm
{
public:
  CAura* m_Aura;

private:
  CRealmConfig                     m_Config;
  CTCPClient*                      m_Socket;                    // the connection to battle.net
  CBNCSUtilInterface*              m_BNCSUtil;                  // the interface to the bncsutil library (used for logging into battle.net)

  CGame*                           m_GameBroadcast;
  uint8_t                          m_GameVersion;
  std::optional<int64_t>           m_GameBroadcastStartTicks;   // when did we start to broadcast the latest game
  std::optional<bool>              m_GameBroadcastStatus;       // whether the hosted lobby has been successfully broadcasted or not, or it is pending
  uint16_t                         m_LastGamePort;              // game port that PvPGN server recognizes and tells clients to connect to when trying to join our games
  uint32_t                         m_LastGameHostCounter;       // game host counter for the game that is being broadcasted

  uint32_t                         m_InternalServerID;          // internal server ID, maps 1:1 to CRealmConfig::m_InputID
  uint8_t                          m_ServerIndex;               // one-based
  uint8_t                          m_PublicServerID;            // for building host counters, which allows matching game join requests to a realm (or none)
  int64_t                          m_LastDisconnectedTime;      // GetTime when we were last disconnected from battle.net
  int64_t                          m_LastConnectionAttemptTime; // GetTime when we last attempted to connect to battle.net
  int64_t                          m_LastGameListTime;          // GetTime when the last game list request was sent
  int64_t                          m_LastAdminRefreshTime;      // GetTime when the admin list was last refreshed from the database
  int64_t                          m_LastBanRefreshTime;        // GetTime when the ban list was last refreshed from the database
  int64_t                          m_ReconnectDelay;            // interval between two consecutive connect attempts
  uint32_t                         m_SessionID;                 // reconnection counter
  uint32_t                         m_NullPacketsSent;
  bool                             m_FirstConnect;              // if we haven't tried to connect to battle.net yet
  bool                             m_ReconnectNextTick;         // ignore reconnect delay
  bool                             m_WaitingToConnect;          // if we're waiting to reconnect to battle.net after being disconnected
  bool                             m_LoggedIn;                  // if we've logged into battle.net or not
  bool                             m_FailedLogin;               // if we tried to login but failed
  bool                             m_FailedSignup;              // if we tried to sign up but failed
  bool                             m_HadChatActivity;           // whether we've received chat/whisper events
  bool                             m_AnyWhisperRejected;        // whether the realm rejected any whisper because the receiver was not offline.
  bool                             m_ChatQueuedGameAnnouncement;// for !host, !announce

  std::array<uint8_t, 32>          m_LoginSalt;                 // set in RECEIVE_SID_AUTH_ACCOUNTLOGON
  std::array<uint8_t, 32>          m_LoginServerPublicKey;      // set in RECEIVE_SID_AUTH_ACCOUNTLOGON
  std::array<uint8_t, 4>           m_InfoClientToken;           // set in constructor
  std::array<uint8_t, 4>           m_InfoLogonType;             // set in RECEIVE_SID_AUTH_INFO
  std::array<uint8_t, 4>           m_InfoServerToken;           // set in RECEIVE_SID_AUTH_INFO
  std::array<uint8_t, 8>           m_InfoMPQFileTime;           // set in RECEIVE_SID_AUTH_INFO
  std::vector<uint8_t>             m_InfoIX86VerFileName;       // set in RECEIVE_SID_AUTH_INFO
  std::vector<uint8_t>             m_InfoValueStringFormula;    // set in RECEIVE_SID_AUTH_INFO
  std::string                      m_ChatNickName;              // set in RECEIVE_SID_ENTERCHAT

  std::vector<std::string>         m_Friends;                   // std::vector of friends
  std::vector<std::string>         m_Clan;                      // std::vector of clan members
  std::vector<uint8_t>             m_EXEVersion;                // custom exe version for PvPGN users
  std::vector<uint8_t>             m_EXEVersionHash;            // custom exe version hash for PvPGN users
  std::string                      m_CurrentChannel;            // the current chat channel
  std::string                      m_AnchorChannel;             // channel to rejoin automatically
  std::string                      m_HostName;                  // 

  std::queue<CQueuedChatMessage*>             m_ChatQueueMain;
  CQueuedChatMessage*                         m_ChatQueueJoinCallback; // High priority
  CQueuedChatMessage*                         m_ChatQueueGameHostWhois; // Also high priority
  std::queue<CQueuedChatMessage*>             m_ChatSentWhispers;
  std::vector<std::pair<int64_t, uint8_t>>    m_ChatQuotaInUse;

  friend class CCommandContext;

public:
  CRealm(CAura* nAura, CRealmConfig* nRealmConfig);
  ~CRealm();
  CRealm(CRealm&) = delete;

  inline const std::array<uint8_t, 4>&    GetInfoClientToken() const { return m_InfoClientToken; }
  inline const std::array<uint8_t, 4>&    GetInfoLogonType() const { return m_InfoLogonType; }
  inline const std::array<uint8_t, 4>&    GetInfoServerToken() const { return m_InfoServerToken; }
  inline const std::array<uint8_t, 8>&    GetMPQFileTime() const { return m_InfoMPQFileTime; }
  inline const std::vector<uint8_t>       GetIX86VerFileName() const { return m_InfoIX86VerFileName; }
  inline std::string                      GetIX86VerFileNameString() const { return std::string(begin(m_InfoIX86VerFileName), end(m_InfoIX86VerFileName)); }
  inline const std::vector<uint8_t>&      GetValueStringFormula() const { return m_InfoValueStringFormula; }
  inline std::string                      GetValueStringFormulaString() const { return std::string(begin(m_InfoValueStringFormula), end(m_InfoValueStringFormula)); }
  inline const std::array<uint8_t, 32>&   GetLoginSalt() const { return m_LoginSalt; }
  inline const std::array<uint8_t, 32>&   GetLoginServerPublicKey() const { return m_LoginServerPublicKey; }
  inline const std::string&               GetChatNickName() const { return m_ChatNickName; }

  inline CGame*        GetGameBroadcast() const { return m_GameBroadcast; }
  inline uint8_t       GetGameVersion() const { return m_GameVersion; }
  inline bool          GetLoggedIn() const { return m_LoggedIn; }
  inline bool          GetFailedLogin() const { return m_FailedLogin; }
  inline bool          GetFailedSignup() const { return m_FailedSignup; }
  inline CTCPClient*   GetSocket() const { return m_Socket; }
  bool                 GetShouldLogChatToConsole() const;
  inline bool          GetInChat() const { return !m_CurrentChannel.empty(); }
  inline std::string   GetCurrentChannel() const { return m_CurrentChannel; }

  bool                 GetEnabled() const;
  bool                 GetPvPGN() const;
  std::string          GetServer() const;
  uint16_t             GetServerPort() const;
  std::string          GetInputID() const;
  std::string          GetUniqueDisplayName() const;
  std::string          GetCanonicalDisplayName() const;
  std::string          GetDataBaseID() const;
  std::string          GetLogPrefix() const;
  inline uint8_t       GetHostCounterID() const { return m_PublicServerID; }
  inline uint32_t      GetInternalID() const { return m_InternalServerID; }
  std::string          GetLoginName() const;
  bool                 GetIsMain() const;
  bool                 GetIsReHoster() const;
  bool                 GetIsMirror() const;
  bool                 GetIsVPN() const;
  bool                 GetUsesCustomIPAddress() const;
  bool                 GetUsesCustomPort() const;
  uint16_t             GetPublicHostPort() const;
  const sockaddr_storage*    GetPublicHostAddress() const;
  uint32_t             GetMaxUploadSize() const;
  bool                 GetIsFloodImmune() const;
  std::string          GetCommandToken() const;
  std::string          GetPrefixedGameName(const std::string& gameName) const;
  bool                 GetAnnounceHostToChat() const;
  inline bool          GetIsChatQueuedGameAnnouncement() { return m_ChatQueuedGameAnnouncement; }  
  inline bool          GetIsGameBroadcastSettled() { return m_GameBroadcastStatus.has_value(); }
  inline bool          GetIsGameBroadcastSucceeded() { return m_GameBroadcastStatus.value_or(false); }
  inline bool          GetIsGameBroadcastErrored() { return !m_GameBroadcastStatus.value_or(true); }

  bool                 GetHasEnhancedAntiSpoof() const;
  bool                 GetUnverifiedCannotStartGame() const;
  bool                 GetUnverifiedAutoKickedFromLobby() const;
  CCommandConfig*      GetCommandConfig() const;

  // processing functions

  uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds) const;
  void Update(void* fd, void* send_fd);
  void ProcessChatEvent(const uint32_t eventType, const std::string& fromUser, const std::string& nMessage);
  uint8_t CountChatQuota();
  bool CheckWithinChatQuota(CQueuedChatMessage* message);
  bool SendQueuedMessage(CQueuedChatMessage* message);

  // functions to send packets to battle.net

  void Send(const std::vector<uint8_t>& packet);
  void SendAuth(const std::vector<uint8_t>& packet);
  bool TrySignup();
  void Signup();
  bool Login();
  void OnLoginOkay();
  void OnSignupOkay();
  void SendGetFriendsList();
  void SendGetClanList();
  void SendGetGamesList();
  void SendNetworkConfig();
  void AutoJoinChat();
  void SendEnterChat();
  CQueuedChatMessage* QueueCommand(const std::string& message, std::shared_ptr<CCommandContext> fromCtx = nullptr, const bool isProxy = false);
  CQueuedChatMessage* QueuePriorityWhois(const std::string& message);
  CQueuedChatMessage* QueueChatChannel(const std::string& message, std::shared_ptr<CCommandContext> fromCtx = nullptr, const bool isProxy = false);
  CQueuedChatMessage* QueueChatReply(const uint8_t messageValue, const std::string& message, const std::string& user, const uint8_t selector, std::shared_ptr<CCommandContext> fromCtx = nullptr, const bool isProxy = false);
  CQueuedChatMessage* QueueWhisper(const std::string& message, const std::string& user, std::shared_ptr<CCommandContext> fromCtx = nullptr, const bool isProxy = false);
  CQueuedChatMessage* QueueGameChatAnnouncement(const CGame* game, std::shared_ptr<CCommandContext> fromCtx = nullptr, const bool isProxy = false);
  void TryQueueChat(const std::string& chatCommand, const std::string& user, bool isPrivate, std::shared_ptr<CCommandContext> ctx = nullptr, const uint8_t ctxFlags = 0);
  void TryQueueGameChatAnnouncement(const CGame* game);
  void SendGameRefresh(const uint8_t displayMode, CGame* game);
  void QueueGameUncreate();
  void TrySendEnterChat();
  void TrySendGetGamesList();

  void StopConnection(bool hadError);

  void ResolveGameBroadcastStatus(bool nResult) { m_GameBroadcastStatus = nResult; }
  void ResetGameBroadcastData();
  void ResetConnection(bool hadError);
  void ResetGameChatAnnouncement() { m_ChatQueuedGameAnnouncement = false; }
  void ResetGameBroadcastStatus() { m_GameBroadcastStatus = std::nullopt; }

  inline void SetReconnectNextTick(bool nReconnectNextTick) { m_ReconnectNextTick = nReconnectNextTick; };

  // other functions

  bool GetIsModerator(std::string name) const;
  bool GetIsAdmin(std::string name) const;
  bool GetIsSudoer(std::string name) const;
  bool IsBannedPlayer(std::string name, std::string hostName) const;
  bool IsBannedIP(std::string ip) const;
  void HoldFriends(CGame* game);
  void HoldClan(CGame* game);
  void Disable();
  void ResetLogin();

  void SetConfig(CRealmConfig* CFG);

  inline void SetHostCounter(const uint8_t nHostCounter) { m_PublicServerID = nHostCounter; }

  private:
};

#endif // AURA_REALM_H_
