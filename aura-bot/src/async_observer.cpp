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

#include "async_observer.h"

#include "aura.h"
#include "config/config_bot.h"
#include "game.h"
#include "game_user.h"
#include "map.h"
#include "net.h"
#include "realm.h"
#include "socket.h"
#include "protocol/game_protocol.h"
#include "protocol/gps_protocol.h"
#include "protocol/vlan_protocol.h"

using namespace std;

//
// CAsyncObserver
//

CAsyncObserver::CAsyncObserver(CConnection* nConnection, CGame* nGame, uint8_t nUID)
  : CConnection(*nConnection),
    m_Game(nGame),
    m_Synchronized(false),
    m_Goal(ASYNC_OBSERVER_GOAL_OBSERVER),
    m_UID(nUID),
    m_SID(nGame->GetSIDFromUID(nUID)),
    m_FrameRate(1),
    m_Offset(0)
{
}

CAsyncObserver::~CAsyncObserver()
{  
}

void CAsyncObserver::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CAsyncObserver::CloseConnection()
{
  m_Socket->Close();
}

void CAsyncObserver::Init()
{
}

uint8_t CAsyncObserver::Update(void* fd, void* send_fd, int64_t timeout)
{
  if (m_DeleteMe || !m_Socket || m_Socket->HasError()) {
    return ASYNC_OBSERVER_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return ASYNC_OBSERVER_DESTROY;
  }

  uint8_t result = ASYNC_OBSERVER_OK;
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
              result = ASYNC_OBSERVER_PROMOTED;
              m_Type = INCON_TYPE_PLAYER;
              m_Socket = nullptr;
            }
            delete joinRequest;
          } else {
            Abort = true;
            break;
          }
          break;

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

    if (Abort && result != ASYNC_OBSERVER_PROMOTED) {
      result = ASYNC_OBSERVER_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return ASYNC_OBSERVER_DESTROY;
  }

  // At this point, m_Socket may have been transferred to GameUser::CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    return ASYNC_OBSERVER_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  return result;
}

void CAsyncObserver::Send(const std::vector<uint8_t>& data)
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}
