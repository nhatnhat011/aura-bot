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

#include <utility>

#include "game_seeker.h"

#include "aura.h"
#include "config/config_bot.h"
#include "game.h"
#include "protocol/game_protocol.h"
#include "game_user.h"
#include "protocol/gps_protocol.h"
#include "map.h"
#include "net.h"
#include "realm.h"
#include "socket.h"
#include "protocol/vlan_protocol.h"

using namespace std;

//
// CGameSeeker
//

CGameSeeker::CGameSeeker(CAura* nAura, uint16_t nPort, uint8_t nType, CStreamIOSocket* nSocket)
  : CConnection(nAura, nPort, nSocket),
    m_GameVersion(0)
{
  m_Type = nType;
}

CGameSeeker::CGameSeeker(CConnection* nConnection, uint8_t nType)
  : CConnection(*nConnection),
    m_GameVersion(0)
{
  m_Type = nType;
}

CGameSeeker::~CGameSeeker()
{  
}

void CGameSeeker::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CGameSeeker::CloseConnection()
{
  m_Socket->Close();
}

void CGameSeeker::Init()
{
  switch (m_Type) {
    case INCON_TYPE_UDP_TUNNEL: {
      vector<uint8_t> packet = {GPSProtocol::Magic::GPS_HEADER, GPSProtocol::Magic::UDPACK, 4, 0};
      m_Socket->PutBytes(packet);
      break;
    }
    case INCON_TYPE_VLAN: {
      // do nothing - client should send VLAN_SEARCHGAME
      break;
    }
  }
}

uint8_t CGameSeeker::Update(void* fd, void* send_fd, int64_t timeout)
{
  if (m_DeleteMe || !m_Socket || m_Socket->HasError()) {
    return GAMESEEKER_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return GAMESEEKER_DESTROY;
  }

  uint8_t result = GAMESEEKER_OK;
  bool Abort = false;
  if (m_Type == INCON_TYPE_KICKED_PLAYER) {
    m_Socket->Discard(static_cast<fd_set*>(fd));
  } else if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
    // extract as many packets as possible from the socket's receive buffer and process them
    string*              RecvBuffer         = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    uint32_t             LengthProcessed    = 0;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4) {
      // bytes 2 and 3 contain the length of the packet
      const uint16_t Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      switch (Bytes[0]) {
        case GameProtocol::Magic::W3GS_HEADER:
          if (m_Type != INCON_TYPE_UDP_TUNNEL || !m_Aura->m_Net.m_Config.m_EnableTCPWrapUDP) {
            Abort = true;
            break;
          }
          if (Bytes[1] == GameProtocol::Magic::REQJOIN) {
            CIncomingJoinRequest* joinRequest = GameProtocol::RECEIVE_W3GS_REQJOIN(Data);
            if (!joinRequest) {
              Abort = true;
              break;
            }
            CGame* targetLobby = m_Aura->GetLobbyByHostCounter(joinRequest->GetHostCounter());
            if (!targetLobby || targetLobby->GetIsMirror() || targetLobby->GetLobbyLoading() || targetLobby->GetExiting()) {
              delete joinRequest;
              break;
            }
            joinRequest->UpdateCensored(targetLobby->m_Config.m_UnsafeNameHandler, targetLobby->m_Config.m_PipeConsideredHarmful);
            if (targetLobby->EventRequestJoin(this, joinRequest)) {
              result = GAMESEEKER_PROMOTED;
              m_Type = INCON_TYPE_PLAYER;
              m_Socket = nullptr;
            }
            delete joinRequest;
          } else if (GameProtocol::Magic::SEARCHGAME <= Bytes[1] && Bytes[1] <= GameProtocol::Magic::DECREATEGAME) {
            if (Length > 1024) {
              Abort = true;
              break;
            }
            struct UDPPkt pkt;
            pkt.socket = m_Socket;
            pkt.sender = &(m_Socket->m_RemoteHost);
            memcpy(pkt.buf, Bytes.data(), Length);
            pkt.length = Length;
            m_Aura->m_Net.HandleUDP(&pkt);
          } else {
            Abort = true;
            break;
          }
          break;

        case VLANProtocol::Magic::VLAN_HEADER: {
          if (m_Type != INCON_TYPE_VLAN || !m_Aura->m_Net.m_Config.m_VLANEnabled) {
            Abort = true;
            break;
          }
          if (Bytes[1] == VLANProtocol::Magic::SEARCHGAME) {
            CIncomingVLanSearchGame vlanSearch = VLANProtocol::RECEIVE_VLAN_SEARCHGAME(Data);
            if (vlanSearch.isValid) {
              m_GameVersion = vlanSearch.gameVersion;
              for (const auto& lobby : m_Aura->m_Lobbies) {
                if (!lobby->GetIsMirror() && lobby->GetIsStageAcceptingJoins()) {
                  lobby->SendGameDiscoveryInfoVLAN(this);
                }
              }
              for (const auto& joinableGame : m_Aura->m_JoinInProgressGames) {
                if (!joinableGame->GetIsMirror() && joinableGame->GetIsStageAcceptingJoins()) {
                  joinableGame->SendGameDiscoveryInfoVLAN(this);
                }
              }
            }
          }
          break;
        }

         default:
          Abort = true;
      }

      LengthProcessed += Length;

      if (Abort) {
        // Process no more packets
        break;
      }

      Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
    }

    if (Abort && result != GAMESEEKER_PROMOTED) {
      result = GAMESEEKER_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return GAMESEEKER_DESTROY;
  }

  // At this point, m_Socket may have been transferred to GameUser::CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    return GAMESEEKER_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  return result;
}

void CGameSeeker::Send(const std::vector<uint8_t>& data)
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}
