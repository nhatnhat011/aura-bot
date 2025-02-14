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

#include "realm.h"

#include <cmath>

#include "auradb.h"
#include "bncsutil_interface.h"
#include "protocol/bnet_protocol.h"
#include "command.h"
#include "config/config.h"
#include "config/config_realm.h"
#include "file_util.h"
#include "game.h"
#include "protocol/game_protocol.h"
#include "game_user.h"
#include "protocol/gps_protocol.h"
#include "hash.h"
#include "map.h"
#include "realm_chat.h"
#include "socket.h"
#include "util.h"

#include "aura.h"

using namespace std;

//
// CRealm
//

CRealm::CRealm(CAura* nAura, CRealmConfig* nRealmConfig)
  : m_Aura(nAura),
    m_Config(*nRealmConfig),
    m_Socket(nullptr),
    m_BNCSUtil(new CBNCSUtilInterface(nRealmConfig->m_UserName, nRealmConfig->m_PassWord)),

    m_GameBroadcast(nullptr),
    m_GameVersion(0),
    m_LastGamePort(6112),
    m_LastGameHostCounter(0),

    m_InternalServerID(nAura->NextServerID()),
    m_ServerIndex(nRealmConfig->m_ServerIndex),
    m_PublicServerID(14 + 2 * nRealmConfig->m_ServerIndex), // First is 16
    m_LastDisconnectedTime(0),
    m_LastConnectionAttemptTime(0),
    m_LastGameListTime(0),
    m_LastAdminRefreshTime(GetTime()),
    m_LastBanRefreshTime(GetTime()),
    m_SessionID(0),
    m_NullPacketsSent(0),
    m_FirstConnect(true),
    m_ReconnectNextTick(true),
    m_WaitingToConnect(true),
    m_LoggedIn(false),
    m_FailedLogin(false),
    m_FailedSignup(false),
    m_HadChatActivity(false),
    m_AnyWhisperRejected(false),
    m_ChatQueuedGameAnnouncement(false),

    m_LoginSalt({}),
    m_LoginServerPublicKey({}),
    m_InfoClientToken({220, 1, 203, 7}),
    m_InfoLogonType({}),
    m_InfoServerToken({}),
    m_InfoMPQFileTime({}),

    m_HostName(nRealmConfig->m_HostName),

    m_ChatQueueJoinCallback(nullptr),
    m_ChatQueueGameHostWhois(nullptr)
{
  m_ReconnectDelay = GetPvPGN() ? 90 : 240;
}

CRealm::~CRealm()
{
  StopConnection(false);

  delete m_Socket;
  delete m_BNCSUtil;

  for (const auto& ptr : m_Aura->m_ActiveContexts) {
    auto ctx = ptr.lock();
    if (!ctx) continue;
    if (ctx->m_SourceRealm == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_SourceRealm = nullptr;
    }
    if (ctx->m_TargetRealm == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_TargetRealm = nullptr;
    }
  }

  for (auto& lobby: m_Aura->m_Lobbies) {
    if (lobby->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(this))) {
      lobby->RemoveCreator();
    }
  }
  for (auto& game : m_Aura->m_StartedGames) {
    if (game->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(this))) {
      game->RemoveCreator();
    }
  }

  if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(this))) {
    m_Aura->m_GameSetup->RemoveCreator();
  }
}

uint32_t CRealm::SetFD(void* fd, void* send_fd, int32_t* nfds) const
{
  if (m_Socket && !m_Socket->HasError() && !m_Socket->HasFin() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

void CRealm::Update(void* fd, void* send_fd)
{
  const int64_t Time = GetTime();

  // we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
  // that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
  // but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

  if (!m_Config.m_Enabled) {
    if (m_Socket && m_Socket->GetConnected()) {
      StopConnection(false);
    }
    return;
  }

  if (!m_Socket) {
    m_Socket = new CTCPClient(AF_INET, m_Config.m_HostName);
    //m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);
  }

  if (m_Socket->HasError() || m_Socket->HasFin())
  {
    // the socket has an error, or the server terminated the connection
    ResetConnection(true);
    PRINT_IF(LOG_LEVEL_INFO, GetLogPrefix() + "waiting " + to_string(m_ReconnectDelay) + " seconds to reconnect")
    return;
  }

  if (m_Socket->GetConnected())
  {
    // the socket is connected and everything appears to be working properly

    if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {

      // extract as many packets as possible from the socket's receive buffer and process them
      string*              RecvBuffer         = m_Socket->GetBytes();
      std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
      uint32_t             LengthProcessed    = 0;
      bool Abort                              = false;

      // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

      while (Bytes.size() >= 4) {
        // bytes 2 and 3 contain the length of the packet
        const uint16_t             Length = ByteArrayToUInt16(Bytes, false, 2);
        if (Length < 4) {
          Abort = true;
          break;
        }
        if (Bytes.size() < Length) break;
        const vector<uint8_t> Data = vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

        // byte 0 is always 255
        if (Bytes[0] == BNETProtocol::Magic::BNET_HEADER)
        {
          // Any BNET packet is fine to reset app-level inactivity timeout.
          m_NullPacketsSent = 0;

          switch (Bytes[1])
          {
            case BNETProtocol::Magic::ZERO:
              // warning: we do not respond to NULL packets with a NULL packet of our own
              // this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
              // official battle.net servers do not respond to NULL packets
              break;

            case BNETProtocol::Magic::GETADVLISTEX:
              if (m_Aura->m_Net.m_Config.m_UDPForwardGameLists) {
                std::vector<uint8_t> relayPacket = {GameProtocol::Magic::W3FW_HEADER, 0, 0, 0};
                std::vector<uint8_t> War3Version = {m_GameVersion, 0, 0, 0};
                std::string ipString = m_Socket->GetIPString();
                AppendByteArray(relayPacket, ipString, true);
                AppendByteArray(relayPacket, static_cast<uint16_t>(6112u), true);
                AppendByteArray(relayPacket, War3Version);
                AppendByteArrayFast(relayPacket, Data);
                AssignLength(relayPacket);
                DPRINT_IF(LOG_LEVEL_TRACE2, GetLogPrefix() + "sending game list to " + AddressToString(m_Aura->m_Net.m_Config.m_UDPForwardAddress) + " (" + to_string(relayPacket.size()) + " bytes)")
                m_Aura->m_Net.Send(&(m_Aura->m_Net.m_Config.m_UDPForwardAddress), relayPacket);
              }

              break;

            case BNETProtocol::Magic::ENTERCHAT: {
              BNETProtocol::EnterChatResult enterChatResult = BNETProtocol::RECEIVE_SID_ENTERCHAT(Data);
              if (enterChatResult.success) {
                m_ChatNickName = GetStringAddressRange(enterChatResult.uniqueNameStart, enterChatResult.uniqueNameEnd);
                ResetGameBroadcastStatus();
                AutoJoinChat();
              }

              break;
            }

            case BNETProtocol::Magic::CHATEVENT: {
              BNETProtocol::IncomingChatResult chatEventResult = BNETProtocol::RECEIVE_SID_CHATEVENT(Data);
              if (chatEventResult.success) {
                ProcessChatEvent(chatEventResult.type, GetStringAddressRange(chatEventResult.userStart, chatEventResult.userEnd), GetStringAddressRange(chatEventResult.messageStart, chatEventResult.messageEnd));
              }
              break;
            }

            case BNETProtocol::Magic::CHECKAD:
              //BNETProtocol::RECEIVE_SID_CHECKAD(Data);
              break;

            case BNETProtocol::Magic::STARTADVEX3:
              if (m_GameBroadcast) {
                if (BNETProtocol::RECEIVE_SID_STARTADVEX3(Data)) {
                  m_Aura->EventBNETGameRefreshSuccess(this);
                } else {
                  PRINT_IF(LOG_LEVEL_NOTICE, GetLogPrefix() + "Failed to publish hosted game")
                  m_Aura->EventBNETGameRefreshError(this);
                }
              }
              break;

            case BNETProtocol::Magic::PING:
              SendAuth(BNETProtocol::SEND_SID_PING(BNETProtocol::RECEIVE_SID_PING(Data)));
              break;

            case BNETProtocol::Magic::AUTH_INFO: {
              BNETProtocol::AuthInfoResult infoResult = BNETProtocol::RECEIVE_SID_AUTH_INFO(Data);
              if (!infoResult.success)
                break;

              copy_n(infoResult.logonType, 4, m_InfoLogonType.begin());
              copy_n(infoResult.serverToken, 4, m_InfoServerToken.begin());
              copy_n(infoResult.mpqFileTime, 8, m_InfoMPQFileTime.begin());
              m_InfoIX86VerFileName = vector<uint8_t>(infoResult.verFileNameStart, infoResult.verFileNameEnd);
              m_InfoValueStringFormula = vector<uint8_t>(infoResult.valueStringFormulaStart, infoResult.valueStringFormulaEnd);

              bool versionSuccess = m_BNCSUtil->HELP_SID_AUTH_CHECK(m_Aura->m_GameInstallPath, &m_Config, GetValueStringFormulaString(), GetIX86VerFileNameString(), GetInfoClientToken(), GetInfoServerToken(), m_Aura->m_GameVersion);
              if (versionSuccess) {
                if (!m_BNCSUtil->CheckValidEXEInfo()) {
                  m_BNCSUtil->SetEXEInfo(m_BNCSUtil->GetDefaultEXEInfo());
                  PRINT_IF(LOG_LEVEL_INFO, GetLogPrefix() + "defaulting to <global_realm.auth_exe_info = " + m_BNCSUtil->GetDefaultEXEInfo() + ">")
                }

                const array<uint8_t, 4>& exeVersion = m_BNCSUtil->GetEXEVersion();
                const array<uint8_t, 4>& exeVersionHash = m_BNCSUtil->GetEXEVersionHash();
                const string& exeInfo = m_BNCSUtil->GetEXEInfo();

                PRINT_IF(LOG_LEVEL_DEBUG,
                  GetLogPrefix() + "attempting to auth as WC3: TFT v" +
                  to_string(exeVersion[3]) + "." + to_string(exeVersion[2]) + std::string(1, char(97 + exeVersion[1])) +
                  " (Build " + to_string(exeVersion[0]) + ") - " +
                  "version hash <" + ByteArrayToDecString(exeVersionHash) + ">"
                )

                SendAuth(BNETProtocol::SEND_SID_AUTH_CHECK(GetInfoClientToken(), exeVersion, exeVersionHash, m_BNCSUtil->GetKeyInfoROC(), m_BNCSUtil->GetKeyInfoTFT(), exeInfo, "Aura"));
                SendAuth(BNETProtocol::SEND_SID_ZERO());
                SendNetworkConfig();
              } else {
                if (m_Aura->MatchLogLevel(LOG_LEVEL_ERROR)) {
                  if (m_Config.m_AuthPasswordHashType == REALM_AUTH_PVPGN) {
                    Print(GetLogPrefix() + "config error - misconfigured <game.install_path>");
                  } else {
                    Print(GetLogPrefix() + "config error - misconfigured <game.install_path>, or <realm_" + to_string(m_ServerIndex) + ".cd_key.roc>, or <realm_" + to_string(m_ServerIndex) + ".cd_key.tft>");
                  }
                  Print(GetLogPrefix() + "bncsutil key hash failed, disconnecting...");
                }
                Disable();
                m_Socket->Disconnect();
              }

              break;
            }

            case BNETProtocol::Magic::AUTH_CHECK: {
              BNETProtocol::AuthCheckResult checkResult = BNETProtocol::RECEIVE_SID_AUTH_CHECK(Data);
              if (checkResult.state == BNETProtocol::KeyResult::GOOD)
              {
                // cd keys accepted
                DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "version OK")
                m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
                SendAuth(BNETProtocol::SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_Config.m_UserName));
              }
              else
              {
                // cd keys not accepted
                switch (checkResult.state)
                {
                  case BNETProtocol::KeyResult::ROC_KEY_IN_USE:
                    PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "logon failed - ROC CD key in use by user [" + GetStringAddressRange(checkResult.descriptionStart, checkResult.descriptionEnd) + "], disconnecting...")
                    break;

                  case BNETProtocol::KeyResult::TFT_KEY_IN_USE:
                    PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "logon failed - TFT CD key in use by user [" + GetStringAddressRange(checkResult.descriptionStart, checkResult.descriptionEnd) + "], disconnecting...");
                    break;

                  case BNETProtocol::KeyResult::OLD_GAME_VERSION:
                  case BNETProtocol::KeyResult::INVALID_VERSION:
                      PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "config error - rejected <realm_" + to_string(m_ServerIndex) + ".auth_exe_version = " + ByteArrayToDecString(m_BNCSUtil->GetEXEVersion()) + ">")
                      PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "config error - rejected <realm_" + to_string(m_ServerIndex) + ".auth_exe_version_hash = " + ByteArrayToDecString(m_BNCSUtil->GetEXEVersionHash()) + ">")
                      PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "logon failed - version not supported, or version hash invalid, disconnecting...")
                    break;

                  default:
                    PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "logon failed - cd keys not accepted, disconnecting...")
                    break;
                }

                Disable();
                m_Socket->Disconnect();
              }

              break;
            }

            case BNETProtocol::Magic::AUTH_ACCOUNTLOGON: {
              BNETProtocol::AuthLoginResult loginResult = BNETProtocol::RECEIVE_SID_AUTH_ACCOUNTLOGON(Data);
              if (loginResult.success) {
                copy_n(loginResult.salt, 32, m_LoginSalt.begin());
                copy_n(loginResult.serverPublicKey, 32, m_LoginServerPublicKey.begin());
                DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "username [" + m_Config.m_UserName + "] OK")
                Login();
              } else {
                DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "username [" + m_Config.m_UserName + "] invalid")
                if (!TrySignup()) {
                  Disable();
                  PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "logon failed - invalid username, disconnecting")
                  m_Socket->Disconnect();
                }
              }

              break;
            }

            case BNETProtocol::Magic::AUTH_ACCOUNTLOGONPROOF:
              if (BNETProtocol::RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(Data)) {
                OnLoginOkay();
              } else {
                m_FailedLogin = true;
                Disable();
                PRINT_IF(LOG_LEVEL_ERROR, GetLogPrefix() + "logon failed - invalid password, disconnecting")
                m_Socket->Disconnect();
              }
              break;

            case BNETProtocol::Magic::AUTH_ACCOUNTSIGNUP:
              if (BNETProtocol::RECEIVE_SID_AUTH_ACCOUNTSIGNUP(Data)) {
                OnSignupOkay();
              } else {
                m_FailedSignup = true;
                Disable();
                PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "sign up failed, disconnecting")
                m_Socket->Disconnect();
              }
              break;

            case BNETProtocol::Magic::FRIENDLIST:
              m_Friends = BNETProtocol::RECEIVE_SID_FRIENDLIST(Data);
              break;

            case BNETProtocol::Magic::CLANMEMBERLIST:
              m_Clan = BNETProtocol::RECEIVE_SID_CLANMEMBERLIST(Data);
              break;

            case BNETProtocol::Magic::GETGAMEINFO:
              PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "got SID_GETGAMEINFO: " + ByteArrayToHexString(Data))
              break;

            case BNETProtocol::Magic::HOSTGAME: {
              if (!GetIsReHoster()) {
                break;
              }

              optional<CConfig> hostedGameConfig = BNETProtocol::RECEIVE_HOSTED_GAME_CONFIG(Data);
              if (!hostedGameConfig.has_value()) {
                PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "got invalid SID_HOSTGAME message")
                break;
              }
              shared_ptr<CCommandContext> ctx = nullptr;
              shared_ptr<CGameSetup> gameSetup = nullptr;
              try {
                ctx = make_shared<CCommandContext>(m_Aura, string(), false, &cout);
                gameSetup = make_shared<CGameSetup>(m_Aura, ctx, &(hostedGameConfig.value()));
              } catch (...) {
                PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "hostgame memory allocation failure")
                break;
              }
              if (!gameSetup->GetMapLoaded()) {
                PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "map is invalid")
                break;
              }
              gameSetup->SetDisplayMode(hostedGameConfig->GetBool("rehost.game.private", false) ? GAME_PRIVATE : GAME_PUBLIC);
              gameSetup->SetMapReadyCallback(MAP_ONREADY_HOST, hostedGameConfig->GetString("rehost.game.name", 1, 31, "Rehosted Game"));
              gameSetup->SetActive();
              gameSetup->LoadMap();
              break;
            }
          }
        }

        LengthProcessed += Length;
        Bytes = vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
      }

      if (Abort) {
        RecvBuffer->clear();
      } else if (LengthProcessed > 0) {
        *RecvBuffer = RecvBuffer->substr(LengthProcessed);
      }
    }

    if (m_Socket->HasError() || m_Socket->HasFin()) {
      return;
    }

    if (GetPvPGN() && m_Socket->GetLastRecv() + REALM_APP_KEEPALIVE_IDLE_TIME < Time) {
      // Many PvPGN servers do not implement TCP Keep Alive. However, all PvPGN servers reply to BNET protocol null packets.
      int64_t expectedNullsSent = ((Time - m_Socket->GetLastRecv() - REALM_APP_KEEPALIVE_IDLE_TIME) / REALM_APP_KEEPALIVE_INTERVAL) + 1;
      if (expectedNullsSent > REALM_APP_KEEPALIVE_MAX_MISSED) {
        PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "socket inactivity timeout")
        ResetConnection(false);
        return;
      }
      if (m_NullPacketsSent < expectedNullsSent) {
        SendAuth(BNETProtocol::SEND_SID_ZERO());
        ++m_NullPacketsSent;
        m_Socket->Flush();
      }
    }

    if (m_LoggedIn) {
      bool waitForPriority = false;
      if (m_ChatQueueJoinCallback && GetInChat()) {
        if (m_ChatQueueJoinCallback->GetIsStale()) {
          delete m_ChatQueueJoinCallback;
          m_ChatQueueJoinCallback = nullptr;
        } else {
          if (CheckWithinChatQuota(m_ChatQueueJoinCallback)) {
            if (SendQueuedMessage(m_ChatQueueJoinCallback)) {
              delete m_ChatQueueJoinCallback;
            }
            m_ChatQueueJoinCallback = nullptr;
          } else {
            waitForPriority = true;
          }
        }
      }
      if (m_ChatQueueGameHostWhois) {
        if (m_ChatQueueGameHostWhois->GetIsStale()) {
          delete m_ChatQueueGameHostWhois;
          m_ChatQueueGameHostWhois = nullptr;
        } else {
          if (CheckWithinChatQuota(m_ChatQueueGameHostWhois)) {
            if (SendQueuedMessage(m_ChatQueueGameHostWhois)) {
              delete m_ChatQueueGameHostWhois;
            }
            m_ChatQueueGameHostWhois = nullptr;
          } else {
            waitForPriority = true;
          }
        }
      }
      if (!waitForPriority) {
        while (!m_ChatQueueMain.empty()) {
          CQueuedChatMessage* nextMessage = m_ChatQueueMain.front();
          if (nextMessage->GetIsStale()) {
            delete nextMessage;
            m_ChatQueueMain.pop();
          } else {
            if (CheckWithinChatQuota(nextMessage)) {
              if (SendQueuedMessage(nextMessage)) {
                delete nextMessage;
              }
              m_ChatQueueMain.pop();
            } else {
              break;
            }
          }
        }
      }
    }

    if (Time - m_LastGameListTime >= 90) {
      TrySendGetGamesList();
    }

    m_Socket->DoSend(static_cast<fd_set*>(send_fd));
    return;
  }

  if (!m_Socket->GetConnected() && !m_Socket->GetConnecting() && !m_WaitingToConnect)
  {
    // the socket was disconnected
    ResetConnection(false);
    return;
  }

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && (m_ReconnectNextTick || (Time - m_LastDisconnectedTime >= m_ReconnectDelay)))
  {
    // attempt to connect to battle.net

    if (!m_FirstConnect) {
      PRINT_IF(LOG_LEVEL_NOTICE, GetLogPrefix() + "reconnecting to [" + m_HostName + ":" + to_string(m_Config.m_ServerPort) + "]...")
    } else {
      if (m_Config.m_BindAddress.has_value()) {
        PRINT_IF(LOG_LEVEL_INFO, GetLogPrefix() + "connecting with local address [" + AddressToString(m_Config.m_BindAddress.value()) + "]...")
      } else {
        DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "connecting...")
      }
    }
    m_FirstConnect = false;
    m_ReconnectNextTick = false;

    sockaddr_storage resolvedAddress;
    if (m_Aura->m_Net.ResolveHostName(resolvedAddress, ACCEPT_ANY, m_Config.m_HostName, m_Config.m_ServerPort)) {
      m_Socket->Connect(m_Config.m_BindAddress, resolvedAddress);
    } else {
      m_Socket->m_HasError = true;
    }
    m_WaitingToConnect          = false;
    m_LastConnectionAttemptTime = Time;
  }

  if (m_Socket->GetConnecting())
  {
    // we are currently attempting to connect to battle.net

    if (m_Socket->CheckConnect())
    {
      // the connection attempt completed
      ++m_SessionID;
      m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);

      PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "connected to [" + m_HostName + "]")
      SendAuth(BNETProtocol::SEND_PROTOCOL_INITIALIZE_SELECTOR());
      m_GameVersion = m_Config.m_AuthWar3Version.has_value() ? m_Config.m_AuthWar3Version.value() : m_Aura->m_GameVersion;
      SendAuth(BNETProtocol::SEND_SID_AUTH_INFO(m_GameVersion, m_Config.m_LocaleID, m_Config.m_CountryShort, m_Config.m_Country));
      m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      m_LastGameListTime       = Time;
      return;
    }
    else if (Time - m_LastConnectionAttemptTime >= 10)
    {
      // the connection attempt timed out (10 seconds)

      PRINT_IF(LOG_LEVEL_WARNING, GetLogPrefix() + "failed to connect to [" + m_HostName + ":" + to_string(m_Config.m_ServerPort) + "]")
      PRINT_IF(LOG_LEVEL_INFO, GetLogPrefix() + "waiting 90 seconds to retry...")
      m_Socket->Reset();
      //m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);
      m_LastDisconnectedTime = Time;
      m_WaitingToConnect     = true;
      return;
    }
  }
}

void CRealm::ProcessChatEvent(const uint32_t eventType, const string& fromUser, const string& message)
{
  bool isWhisper = (eventType == BNETProtocol::IncomingChatEvent::WHISPER);

  if (!m_Socket->GetConnected()) {
    PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "not connected - message from [" + fromUser + "] rejected: [" + message + "]")
    return;
  }

  // handle spoof checking for current game
  // this case covers whispers - we assume that anyone who sends a whisper to the bot with message "spoofcheck" should be considered spoof checked
  // note that this means you can whisper "spoofcheck" even in a public game to manually spoofcheck if the /whois fails

  if (eventType == BNETProtocol::IncomingChatEvent::WHISPER && (message == "s" || message == "sc" || message == "spoofcheck")) {
    if (m_GameBroadcast && !m_GameBroadcast->GetIsMirror()) {
      GameUser::CGameUser* matchUser = m_GameBroadcast->GetUserFromName(fromUser, true);
      if (matchUser) m_GameBroadcast->AddToRealmVerified(m_Config.m_HostName, matchUser, true);
      return;
    }
  }

  if (eventType == BNETProtocol::IncomingChatEvent::WHISPER || eventType == BNETProtocol::IncomingChatEvent::TALK) {
    m_HadChatActivity = true;
  }

  if (eventType == BNETProtocol::IncomingChatEvent::WHISPER || eventType == BNETProtocol::IncomingChatEvent::TALK) {
    if (fromUser == GetLoginName()) {
      return;
    }
    // FIXME: Chat logging kinda sucks
    if (isWhisper) {
      PRINT_IF(LOG_LEVEL_NOTICE, "[WHISPER: " + m_Config.m_UniqueName + "] [" + fromUser + "] " + message)
    } else if (GetShouldLogChatToConsole()) {
      Print("[CHAT: " + m_Config.m_UniqueName + "] [" + fromUser + "] " + message);
    }

    // handle bot commands

    if (eventType == BNETProtocol::IncomingChatEvent::TALK && m_Config.m_IsMirror) {
      // Let bots on servers with <realm_x.mirror = yes> ignore commands at channels
      // (but still accept commands through whispers).
      return;
    }

    if (message.empty()) {
      return;
    }

    string cmdToken, command, payload;
    uint8_t tokenMatch = ExtractMessageTokensAny(message, m_Config.m_PrivateCmdToken, m_Config.m_BroadcastCmdToken, cmdToken, command, payload);
    if (tokenMatch == COMMAND_TOKEN_MATCH_NONE) {
      if (isWhisper) {
        string tokenName = GetTokenName(m_Config.m_PrivateCmdToken);
        string example = m_Aura->m_Net.m_Config.m_AllowDownloads ? "host wc3maps-8" : "host castle";
        QueueWhisper("Hi, " + fromUser + ". Use " + m_Config.m_PrivateCmdToken + tokenName + " for commands. Example: " + m_Config.m_PrivateCmdToken + example, fromUser);
      }
      return;
    }
    shared_ptr<CCommandContext> ctx = nullptr;
    try {
      ctx = make_shared<CCommandContext>(m_Aura, m_Config.m_CommandCFG, this, fromUser, isWhisper, !isWhisper && tokenMatch == COMMAND_TOKEN_MATCH_BROADCAST, &std::cout);
    } catch (...) {
    }
    if (ctx) {
      ctx->Run(cmdToken, command, payload);
    }
  }
  else if (eventType == BNETProtocol::IncomingChatEvent::CHANNEL)
  {
    PRINT_IF(LOG_LEVEL_INFO, GetLogPrefix() + "joined channel [" + message + "]")
    m_CurrentChannel = message;
  } else if (eventType == BNETProtocol::IncomingChatEvent::WHISPERSENT) {
    PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "whisper sent OK [" + message + "]")
    if (!m_ChatSentWhispers.empty()) {
      CQueuedChatMessage* oldestWhisper = m_ChatSentWhispers.front();
      if (oldestWhisper->IsProxySent()) {
        shared_ptr<CCommandContext> fromCtx = oldestWhisper->GetProxyCtx();
        if (fromCtx->CheckActionMessage(message) && !fromCtx->GetPartiallyDestroyed()) {
          fromCtx->SendReply("message sent to " + oldestWhisper->GetReceiver() + ".");
        }
        fromCtx->ClearActionMessage();
      }
      delete oldestWhisper;
      m_ChatSentWhispers.pop();
    }
  } else if (eventType == BNETProtocol::IncomingChatEvent::INFO) {
    bool LogInfo = m_HadChatActivity;

    // extract the first word which we hope is the username
    // this is not necessarily true though since info messages also include channel MOTD's and such

    if (m_GameBroadcast && !m_GameBroadcast->GetIsMirror()) {
      string aboutName;
      string::size_type spIndex = message.find(' ');

      if (spIndex != string::npos)
        aboutName = message.substr(0, spIndex);
      else
        aboutName = message;

      GameUser::CGameUser* aboutPlayer = m_GameBroadcast->GetUserFromName(aboutName, true);
      if (aboutPlayer && aboutPlayer->GetRealmInternalID() == m_InternalServerID) {
        // handle spoof checking for current game
        // this case covers whois results which are used when hosting a public game (we send out a "/whois [player]" for each player)
        // at all times you can still /w the bot with "spoofcheck" to manually spoof check

        if (message.find("Throne in game") != string::npos || message.find("currently in  game") != string::npos || message.find("currently in private game") != string::npos) {
          // note: if the game is rehosted, bnet will not be aware of the game being renamed
          string gameName = GetPrefixedGameName(m_GameBroadcast->GetGameName());
          string::size_type GameNameFoundPos = message.find("\"" + gameName + "\"");
          if (GameNameFoundPos != string::npos && GameNameFoundPos + gameName.length() + 3 == message.length()) {
            m_GameBroadcast->AddToRealmVerified(m_HostName, aboutPlayer, true);
          } else {
            m_GameBroadcast->ReportSpoofed(m_HostName, aboutPlayer);
          }
        } else {
          // [ERROR] Unknown user.
          // [INFO] User was last seen on:
        }
      }
    }
    if (LogInfo) {
      PRINT_IF(LOG_LEVEL_INFO, "[INFO: " + m_Config.m_UniqueName + "] " + message)
    }
  } else if (eventType == BNETProtocol::IncomingChatEvent::NOTICE) {
    // Note that the default English error message <<That user is not logged on.>> is also received in other two circumstances:
    // - When sending /netinfo <USER>, if the bot has admin permissions in this realm.
    // - When sending /ban <USER>, if the bot has admin permissions in this realm.
    //
    // Therefore, abuse of the !SAY command will temporarily break feedback for !TELL/INVITE, until m_ChatSentWhispers is emptied.
    // This is (yet another reason) why !SAY /COMMAND must always be gated behind sudo.
    if (!m_ChatSentWhispers.empty() && message.length() == m_Config.m_WhisperErrorReply.length() && message == m_Config.m_WhisperErrorReply) {
      m_AnyWhisperRejected = true;
      CQueuedChatMessage* oldestWhisper = m_ChatSentWhispers.front();
      if (oldestWhisper->IsProxySent()) {
        shared_ptr<CCommandContext> fromCtx = oldestWhisper->GetProxyCtx();
        if (!fromCtx->GetPartiallyDestroyed()) {
          fromCtx->SendReply(oldestWhisper->GetReceiver() + " is offline.");
        }
        fromCtx->ClearActionMessage();
      }
      delete oldestWhisper;
      m_ChatSentWhispers.pop();
    }
    PRINT_IF(LOG_LEVEL_NOTICE, "[NOTE: " + m_Config.m_UniqueName + "] " + message)
  }
}

uint8_t CRealm::CountChatQuota()
{
  if (m_ChatQuotaInUse.empty()) return 0;
  int64_t minTicks = GetTicks() - static_cast<int64_t>(m_Config.m_FloodQuotaTime) * 60000 - 300; // 300 ms hardcoded latency
  uint16_t spentQuota = 0;
  for (auto it = begin(m_ChatQuotaInUse); it != end(m_ChatQuotaInUse);) {
    if ((*it).first < minTicks) {
      it = m_ChatQuotaInUse.erase(it);
    } else {
      spentQuota += (*it).second;
      if (0xFF <= spentQuota) break;
      ++it;
    }
  }
  if (0xFF < spentQuota) return 0xFF;
  return static_cast<uint8_t>(spentQuota);
}

bool CRealm::CheckWithinChatQuota(CQueuedChatMessage* message)
{
  if (m_Config.m_FloodImmune) return true;
  uint16_t spentQuota = CountChatQuota();
  if (m_Config.m_FloodQuotaLines <= spentQuota) {
    message->SetWasThrottled(true);
    return false;
  }
  const bool success = message->SelectSize(m_Config.m_VirtualLineLength, m_CurrentChannel) + spentQuota <= m_Config.m_FloodQuotaLines;
  if (!success) message->SetWasThrottled(true);
  return success;
}

bool CRealm::SendQueuedMessage(CQueuedChatMessage* message)
{
  bool deleteMessage = true;
  uint8_t selectType;
  Send(message->SelectBytes(m_CurrentChannel, selectType));
  if (message->GetSendsEarlyFeedback()) {
    message->SendEarlyFeedback();
  }
  if (m_Aura->MatchLogLevel(LOG_LEVEL_INFO)) {
    string modeFragment = "sent message <<";
    if (message->GetWasThrottled()) {
      if (selectType == CHAT_RECV_SELECTED_WHISPER) {
        modeFragment = "sent whisper (throttled) <<";
      } else {
        modeFragment = "sent message (throttled) <<";
      }
    } else if (selectType == CHAT_RECV_SELECTED_WHISPER) {
      modeFragment = "sent whisper <<";
    }
    PRINT_IF(LOG_LEVEL_INFO, GetLogPrefix() + modeFragment + message->GetInnerMessage() + ">>")
  }
  if (selectType == CHAT_RECV_SELECTED_WHISPER) {
    m_ChatSentWhispers.push(message);
    if (m_ChatSentWhispers.size() > 25 && !m_AnyWhisperRejected && m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print(GetLogPrefix() + "warning - " + to_string(m_ChatSentWhispers.size()) + " sent whispers have not been confirmed by the server");
      Print(GetLogPrefix() + "warning - <" + m_Config.m_CFGKeyPrefix + "protocol.whisper.error_reply = " + m_Config.m_WhisperErrorReply + "> may not match the language of this realm's system messages.");
    }
    // Caller must not delete the message.
    deleteMessage = false;
  }
  if (!m_Config.m_FloodImmune) {
    uint8_t extraQuota = message->GetVirtualSize(m_Config.m_VirtualLineLength, selectType);
    m_ChatQuotaInUse.push_back(make_pair(GetTicks(), extraQuota));
  }

  switch (message->GetCallback()) {
    case CHAT_CALLBACK_REFRESH_GAME: {
      m_ChatQueuedGameAnnouncement = false;
      CGame* matchLobby = m_Aura->GetLobbyByHostCounterExact(message->GetCallbackData());
      if (!matchLobby) {
        Print(GetLogPrefix() + " !! lobby not found !! host counter 0x" + ToHexString(message->GetCallbackData()));
        if (message->GetIsStale()) {
          Print(GetLogPrefix() + " !! lobby is stale !!");
        } else {
          Print(GetLogPrefix() + " !! lobby is not stale !!");
        }
      } else if (matchLobby->GetIsSupportedGameVersion(GetGameVersion())) {
        matchLobby->AnnounceToRealm(this);
      }
      break;
    }

    default:
      // Do nothing
      break;
  }

  return deleteMessage;
}

bool CRealm::GetEnabled() const
{
  return m_Config.m_Enabled;
}

bool CRealm::GetPvPGN() const
{
  return m_Config.m_AuthPasswordHashType == REALM_AUTH_PVPGN;
}

string CRealm::GetServer() const
{
  return m_Config.m_HostName;
}

uint16_t CRealm::GetServerPort() const
{
  return m_Config.m_ServerPort;
}

string CRealm::GetInputID() const
{
  return m_Config.m_InputID;
}

string CRealm::GetUniqueDisplayName() const
{
  return m_Config.m_UniqueName;
}

string CRealm::GetCanonicalDisplayName() const
{
  return m_Config.m_CanonicalName;
}

string CRealm::GetDataBaseID() const
{
  return m_Config.m_DataBaseID;
}

string CRealm::GetLogPrefix() const
{
  return "[BNET: " + m_Config.m_UniqueName + "] ";
}

bool CRealm::GetShouldLogChatToConsole() const
{
  return m_Config.m_ConsoleLogChat;
}

string CRealm::GetLoginName() const
{
  return m_Config.m_UserName;
}

bool CRealm::GetIsMain() const
{
  return m_Config.m_IsMain;
}

bool CRealm::GetIsReHoster() const
{
  return m_Config.m_IsReHoster;
}

bool CRealm::GetIsMirror() const
{
  return m_Config.m_IsMirror;
}

bool CRealm::GetIsVPN() const
{
  return m_Config.m_IsVPN;
}

bool CRealm::GetUsesCustomIPAddress() const
{
  return m_Config.m_EnableCustomAddress;
}

bool CRealm::GetUsesCustomPort() const
{
  return m_Config.m_EnableCustomPort;
}

const sockaddr_storage* CRealm::GetPublicHostAddress() const
{
  return &(m_Config.m_PublicHostAddress);
}

uint16_t CRealm::GetPublicHostPort() const
{
  return m_Config.m_PublicHostPort;
}

uint32_t CRealm::GetMaxUploadSize() const
{
  return m_Config.m_MaxUploadSize;
}

bool CRealm::GetIsFloodImmune() const
{
  return m_Config.m_FloodImmune;
}

string CRealm::GetPrefixedGameName(const string& gameName) const
{
  if (gameName.length() + m_Config.m_GamePrefix.length() < 31) {
    // Check again just in case m_GamePrefix was reloaded and became prohibitively large.
    return m_Config.m_GamePrefix + gameName;
  } else {
    return gameName;
  }
}

bool CRealm::GetAnnounceHostToChat() const
{
  return m_Config.m_AnnounceHostToChat;
}

bool CRealm::GetHasEnhancedAntiSpoof() const
{
  return (
    (m_Config.m_CommandCFG->m_Enabled && m_Config.m_UnverifiedRejectCommands) ||
    m_Config.m_UnverifiedCannotStartGame || m_Config.m_UnverifiedAutoKickedFromLobby ||
    m_Config.m_AlwaysSpoofCheckPlayers
  );
}

bool CRealm::GetUnverifiedCannotStartGame() const
{
  return m_Config.m_UnverifiedCannotStartGame;
}

bool CRealm::GetUnverifiedAutoKickedFromLobby() const
{
  return m_Config.m_UnverifiedAutoKickedFromLobby;
}

CCommandConfig* CRealm::GetCommandConfig() const
{
  return m_Config.m_CommandCFG;
}

string CRealm::GetCommandToken() const
{
  return m_Config.m_PrivateCmdToken;
}

void CRealm::SendGetFriendsList()
{
  Send(BNETProtocol::SEND_SID_FRIENDLIST());
}

void CRealm::SendGetClanList()
{
  Send(BNETProtocol::SEND_SID_CLANMEMBERLIST());
}

void CRealm::SendGetGamesList()
{
  Send(BNETProtocol::SEND_SID_GETADVLISTEX());
  m_LastGameListTime = GetTime();
}

void CRealm::TrySendGetGamesList()
{
  if (m_Config.m_QueryGameLists) {
    SendGetGamesList();
  }
}

void CRealm::SendNetworkConfig()
{
  CGame* lobbyPendingForBroadcast = m_Aura->GetMostRecentLobby();
  if (lobbyPendingForBroadcast && lobbyPendingForBroadcast->GetIsMirror() && !m_Config.m_IsMirror) {
    PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "mirroring public game host " + IPv4ToString(lobbyPendingForBroadcast->GetPublicHostAddress()) + ":" + to_string(lobbyPendingForBroadcast->GetPublicHostPort()))
    SendAuth(BNETProtocol::SEND_SID_PUBLICHOST(lobbyPendingForBroadcast->GetPublicHostAddress(), lobbyPendingForBroadcast->GetPublicHostPort()));
    m_LastGamePort = lobbyPendingForBroadcast->GetPublicHostPort();
  } else if (m_Config.m_EnableCustomAddress) {
    uint16_t port = 6112;
    if (m_Config.m_EnableCustomPort) {
      port = m_Config.m_PublicHostPort;
    } else if (lobbyPendingForBroadcast && lobbyPendingForBroadcast->GetIsLobbyStrict()) {
      port = lobbyPendingForBroadcast->GetHostPort();
    }
    PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "using public game host " + IPv4ToString(AddressToIPv4Array(&(m_Config.m_PublicHostAddress))) + ":" + to_string(port))
    SendAuth(BNETProtocol::SEND_SID_PUBLICHOST(AddressToIPv4Array(&(m_Config.m_PublicHostAddress)), port));
    m_LastGamePort = port;
  } else if (m_Config.m_EnableCustomPort) {
    PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "using public game port " + to_string(m_Config.m_PublicHostPort))
    SendAuth(BNETProtocol::SEND_SID_NETGAMEPORT(m_Config.m_PublicHostPort));
    m_LastGamePort = m_Config.m_PublicHostPort;
  }
}

void CRealm::AutoJoinChat()
{
  const string& targetChannel = m_AnchorChannel.empty() ? m_Config.m_FirstChannel : m_AnchorChannel;
  if (targetChannel.empty()) return;
  Send(BNETProtocol::SEND_SID_JOINCHANNEL(targetChannel));
  PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "joining channel [" + targetChannel + "]")
}

void CRealm::SendEnterChat()
{
  Send(BNETProtocol::SEND_SID_ENTERCHAT());
}

void CRealm::TrySendEnterChat()
{
  if (!m_AnchorChannel.empty() || !m_Config.m_FirstChannel.empty()) {
    SendEnterChat();
  }
}

void CRealm::Send(const vector<uint8_t>& packet)
{
  if (m_LoggedIn) {
    SendAuth(packet);
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE2, GetLogPrefix() + "packet not sent (not logged in)")
  }
}

void CRealm::SendAuth(const vector<uint8_t>& packet)
{
  // This function is public only for the login phase.
  // Though it's also privately used by CRealm::Send.
  DPRINT_IF(LOG_LEVEL_TRACE2, GetLogPrefix() + "sending packet - " + ByteArrayToHexString(packet))
  m_Socket->PutBytes(packet);
}

bool CRealm::TrySignup()
{
  if (m_FailedSignup || !m_Config.m_AutoRegister) {
    return false;
  }
  if (m_Config.m_AuthPasswordHashType != REALM_AUTH_PVPGN) {
    return false;
  }
  Signup();
  return true;
}

void CRealm::Signup()
{
  //if (m_Config.m_AuthPasswordHashType == REALM_AUTH_PVPGN) {
  // exclusive to pvpgn logon
  PRINT_IF(LOG_LEVEL_NOTICE, GetLogPrefix() + "registering new account in PvPGN realm")
  m_BNCSUtil->HELP_PvPGNPasswordHash(m_Config.m_PassWord);
  SendAuth(BNETProtocol::SEND_SID_AUTH_ACCOUNTSIGNUP(m_Config.m_UserName, m_BNCSUtil->GetPvPGNPasswordHash()));
  //}
}

bool CRealm::Login()
{
  if (m_Config.m_AuthPasswordHashType == REALM_AUTH_PVPGN) {
    // pvpgn logon
    DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "using pvpgn logon type")
    m_BNCSUtil->HELP_PvPGNPasswordHash(m_Config.m_PassWord);
    SendAuth(BNETProtocol::SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetPvPGNPasswordHash()));
  } else {
    // battle.net logon
    DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "using battle.net logon type")
    m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF(GetLoginSalt(), GetLoginServerPublicKey());
    SendAuth(BNETProtocol::SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetM1()));
  }
  return true;
}

void CRealm::OnLoginOkay()
{
  m_LoggedIn = true;
  PRINT_IF(LOG_LEVEL_NOTICE, GetLogPrefix() + "logged in as [" + m_Config.m_UserName + "]")

  TrySendGetGamesList();
  SendGetFriendsList();
  SendGetClanList();

  TrySendEnterChat();
  TryQueueGameChatAnnouncement(m_Aura->GetMostRecentLobby());
}

void CRealm::OnSignupOkay()
{
  PRINT_IF(LOG_LEVEL_NOTICE, GetLogPrefix() + "signed up as [" + m_Config.m_UserName + "]")
  m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
  SendAuth(BNETProtocol::SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_Config.m_UserName));
  //Login();
}

CQueuedChatMessage* CRealm::QueueCommand(const string& message, shared_ptr<CCommandContext> fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  if (!m_Config.m_FloodImmune && message.length() > m_Config.m_MaxLineLength) {
    return nullptr;
  }

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  entry->SetMessage(message);
  entry->SetReceiver(RECV_SELECTOR_SYSTEM);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "queued command \"" + entry->GetInnerMessage() + "\"")
  return entry;
}

CQueuedChatMessage* CRealm::QueuePriorityWhois(const string& message)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  if (!m_Config.m_FloodImmune && message.length() > m_Config.m_MaxLineLength) {
    return nullptr;
  }

  if (m_ChatQueueGameHostWhois) {
    delete m_ChatQueueGameHostWhois;
  }
  m_ChatQueueGameHostWhois = new CQueuedChatMessage(this, nullptr, false);
  m_ChatQueueGameHostWhois->SetMessage(message);
  m_ChatQueueGameHostWhois->SetReceiver(RECV_SELECTOR_SYSTEM);
  m_HadChatActivity = true;

  DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "queued fast spoofcheck \"" + m_ChatQueueGameHostWhois->GetInnerMessage() + "\"")
  return m_ChatQueueGameHostWhois;
}

CQueuedChatMessage* CRealm::QueueChatChannel(const string& message, shared_ptr<CCommandContext> fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  if (!m_Config.m_FloodImmune && m_Config.m_MaxLineLength < message.length()) {
    entry->SetMessage(message.substr(0, m_Config.m_MaxLineLength));
  } else {
    entry->SetMessage(message);
  }
  entry->SetReceiver(RECV_SELECTOR_ONLY_PUBLIC);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "queued chat message \"" + entry->GetInnerMessage() + "\"")
  return entry;
}

CQueuedChatMessage* CRealm::QueueChatReply(const uint8_t messageValue, const string& message, const string& user, const uint8_t selector, shared_ptr<CCommandContext> fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  entry->SetMessage(messageValue, message);
  entry->SetReceiver(selector, user);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "queued reply to [" + user + "] - \"" + entry->GetInnerMessage() + "\"")
  return entry;
}

CQueuedChatMessage* CRealm::QueueWhisper(const string& message, const string& user, shared_ptr<CCommandContext> fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  if (!m_Config.m_FloodImmune && (m_Config.m_MaxLineLength - 20u) < message.length()) {
    entry->SetMessage(message.substr(0, m_Config.m_MaxLineLength - 20u));
  } else {
    entry->SetMessage(message);
  }
  entry->SetReceiver(RECV_SELECTOR_ONLY_WHISPER, user);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "queued whisper to [" + user + "] - \"" + entry->GetInnerMessage() + "\"")
  return entry;
}

void CRealm::TryQueueGameChatAnnouncement(const CGame* game)
{
  if (!game || !m_LoggedIn) {
    return;
  }

  if (game->GetIsMirror() && GetIsMirror()) {
    // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
    // Do not display external games in those realms.
    return;
  }

  if (game->GetIsRealmExcluded(GetServer())) {
    return;
  }

  if (game->GetDisplayMode() == GAME_PUBLIC && GetAnnounceHostToChat()) {
    QueueGameChatAnnouncement(game);
    return;
  }
}

CQueuedChatMessage* CRealm::QueueGameChatAnnouncement(const CGame* game, shared_ptr<CCommandContext> fromCtx, const bool isProxy)
{
  if (!m_LoggedIn)
    return nullptr;

  m_ChatQueuedGameAnnouncement = true;

  m_ChatQueueJoinCallback = new CQueuedChatMessage(this, fromCtx, isProxy);
  m_ChatQueueJoinCallback->SetMessage(game->GetAnnounceText(this));
  m_ChatQueueJoinCallback->SetReceiver(RECV_SELECTOR_ONLY_PUBLIC);
  m_ChatQueueJoinCallback->SetCallback(CHAT_CALLBACK_REFRESH_GAME, game->GetHostCounter());
  if (!game->GetIsMirror()) {
    m_ChatQueueJoinCallback->SetValidator(CHAT_VALIDATOR_LOBBY_JOINABLE, game->GetHostCounter());
  }
  m_HadChatActivity = true;

  DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "queued game announcement")
  return m_ChatQueueJoinCallback;
}

void CRealm::TryQueueChat(const string& message, const string& user, bool isPrivate, shared_ptr<CCommandContext> fromCtx, const uint8_t ctxFlags)
{
  // don't respond to non admins if there are more than 25 messages already in the queue
  // this prevents malicious users from filling up the bot's chat queue and crippling the bot
  // in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
  // e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

  if (m_ChatQueueMain.size() >= 25 && !(GetIsFloodImmune() || GetIsModerator(user) || GetIsAdmin(user) || GetIsSudoer(user))) {
    if (m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print(GetLogPrefix() + "warning - " + to_string(m_ChatQueueMain.size()) + " queued messages");
      Print(GetLogPrefix() + message);
      Print("[AURA] Quota exceeded (reply dropped.)");
    }
    return;
  }

  if (isPrivate) {
    QueueWhisper(message, user);
  } else {
    QueueChatChannel(message);
  }
}


void CRealm::SendGameRefresh(const uint8_t displayMode, CGame* game)
{
  if (!m_LoggedIn || GetIsGameBroadcastErrored()) {
    return;
  }

  const uint16_t connectPort = (
    game->GetIsMirror() ? game->GetPublicHostPort() :
    (GetUsesCustomPort() ? GetPublicHostPort() :
    game->GetHostPort())
  );

  // construct a fixed host counter which will be used to identify players from this realm
  // the fixed host counter's highest-order byte will contain a 7 bit ID (0-127), plus a trailing bit to discriminate join from info requests
  // the rest of the fixed host counter will contain the 24 least significant bits of the actual host counter
  // since we're destroying 8 bits of information here the actual host counter should not be greater than 2^24 which is a reasonable assumption
  // when a user joins a game we can obtain the ID from the received host counter
  // note: LAN broadcasts use an ID of 0, IDs 1 to 15 are reserved
  // battle.net refreshes use IDs of 16-255
  const uint32_t hostCounter = game->GetHostCounter() | (game->GetIsMirror() ? 0 : (static_cast<uint32_t>(m_PublicServerID) << 24));
  bool changedAny = m_LastGamePort != connectPort || m_LastGameHostCounter != hostCounter;

  if (m_LastGamePort != connectPort) {
    DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "updating net game port to " + to_string(connectPort))
    // Some PvPGN servers will disconnect if this message is sent while logged in
    Send(BNETProtocol::SEND_SID_NETGAMEPORT(connectPort));
    m_LastGamePort = connectPort;
  }

  if (!changedAny) {
    DPRINT_IF(LOG_LEVEL_TRACE2, GetLogPrefix() + "game refreshed")
  } else {
    int64_t Ticks = GetTicks();
    if (!m_Config.m_IsHostOften && m_GameBroadcastStartTicks.has_value() && Ticks < m_GameBroadcastStartTicks.value() + static_cast<int64_t>(REALM_HOST_COOLDOWN_TICKS)) {
      // Still in cooldown
      DPRINT_IF(LOG_LEVEL_TRACE, GetLogPrefix() + "not registering game... still in cooldown")
      return;
    }
    PRINT_IF(LOG_LEVEL_DEBUG, GetLogPrefix() + "registering game...")
    m_LastGameHostCounter = hostCounter;
    m_GameBroadcastStartTicks = Ticks;
  }

  m_GameBroadcast = game;

  Send(BNETProtocol::SEND_SID_STARTADVEX3(
    displayMode,
    game->GetGameType(),
    game->GetGameFlags(),
    game->GetAnnounceWidth(),
    game->GetAnnounceHeight(),
    GetPrefixedGameName(game->GetGameName()), m_Config.m_UserName,
    game->GetUptime(),
    game->GetSourceFilePath(),
    game->GetSourceFileHash(),
    game->GetSourceFileSHA1(),
    hostCounter,
    game->GetMap()->GetVersionMaxSlots()
  ));

  if (!m_CurrentChannel.empty()) {
    m_AnchorChannel = m_CurrentChannel;
    m_CurrentChannel.clear();
  }
}

void CRealm::QueueGameUncreate()
{
  ResetGameChatAnnouncement();
  Send(BNETProtocol::SEND_SID_STOPADV());
}

void CRealm::ResetGameBroadcastData()
{
  m_GameBroadcast = nullptr;
  ResetGameBroadcastStatus();
  QueueGameUncreate();
  SendEnterChat();
}

void CRealm::StopConnection(bool hadError)
{
  m_LastDisconnectedTime = GetTime();
  m_BNCSUtil->Reset(m_Config.m_UserName, m_Config.m_PassWord);

  if (m_Socket) {
    if (m_Socket->GetConnected()) {
      if (m_Aura->MatchLogLevel(hadError ? LOG_LEVEL_WARNING : LOG_LEVEL_NOTICE)) {
        if (m_Socket->HasFin()) {
          Print(GetLogPrefix() + "remote terminated the connection");
        } else if (hadError) {
          Print(GetLogPrefix() + "disconnected - " + m_Socket->GetErrorString());
        } else {
          Print(GetLogPrefix() + "disconnected");
        }
      }
      m_Socket->Close();
    }
  }

  m_LoggedIn = false;
  m_CurrentChannel.clear();
  m_AnchorChannel.clear();
  m_WaitingToConnect = true;
  ResetGameBroadcastStatus();

  m_ChatQuotaInUse.clear();
  if (m_ChatQueueJoinCallback) {
    delete m_ChatQueueJoinCallback;
    m_ChatQueueJoinCallback = nullptr;
  }
  if (m_ChatQueueGameHostWhois) {
    delete m_ChatQueueGameHostWhois;
    m_ChatQueueGameHostWhois = nullptr;
  }
  while (!m_ChatQueueMain.empty()) {
    delete m_ChatQueueMain.front();
    m_ChatQueueMain.pop();
  }
  while (!m_ChatSentWhispers.empty()) {
    delete m_ChatSentWhispers.front();
    m_ChatSentWhispers.pop();
  }
}

void CRealm::ResetConnection(bool hadError)
{
  StopConnection(hadError);

  if (m_Socket) {
    m_Socket->Reset();
    //m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);
  }
}

bool CRealm::GetIsModerator(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });

  if (m_Aura->m_DB->ModeratorCheck(m_Config.m_DataBaseID, name))
    return true;

  return false;
}

bool CRealm::GetIsAdmin(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  return m_Config.m_Admins.find(name) != m_Config.m_Admins.end();
}

bool CRealm::GetIsSudoer(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  return m_Config.m_SudoUsers.find(name) != m_Config.m_SudoUsers.end();
}

bool CRealm::IsBannedPlayer(string name, string hostName) const
{
  return m_Aura->m_DB->GetIsUserBanned(name, hostName, m_Config.m_DataBaseID);
}

bool CRealm::IsBannedIP(string ip) const
{
  return m_Aura->m_DB->GetIsIPBanned(ip, m_Config.m_DataBaseID);
}

void CRealm::HoldFriends(CGame* game)
{
  for (auto& friend_ : m_Friends)
    game->AddToReserved(friend_);
}

void CRealm::HoldClan(CGame* game)
{
  for (auto& clanmate : m_Clan)
    game->AddToReserved(clanmate);
}

void CRealm::Disable()
{
  m_Config.m_Enabled = false;
}

void CRealm::ResetLogin()
{
  m_FailedLogin = false;
  m_FailedSignup = false;
}

void CRealm::SetConfig(CRealmConfig* realmConfig) {
  m_Config = *realmConfig;
}
