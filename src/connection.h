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

#ifndef AURA_CONNECTION_H_
#define AURA_CONNECTION_H_

#include "includes.h"
#include "socket.h"

#define INCON_UPDATE_OK 0u
#define INCON_UPDATE_DESTROY 1u
#define INCON_UPDATE_PROMOTED 2u
#define INCON_UPDATE_PROMOTED_PASSTHROUGH 3u
#define INCON_UPDATE_RECONNECTED 4u

#define INCON_TYPE_NONE 0u
#define INCON_TYPE_UDP_TUNNEL 1u
#define INCON_TYPE_PLAYER 2u
#define INCON_TYPE_KICKED_PLAYER 3u
#define INCON_TYPE_VLAN 4u

//
// CConnection
//

class CConnection
{
public:
  CAura*                  m_Aura;
  uint16_t                m_Port;
  uint8_t                 m_Type;
  std::optional<int64_t>  m_TimeoutTicks;
  CStreamIOSocket*        m_Socket;
  bool                    m_DeleteMe;

  CConnection(CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket);
  CConnection(const CConnection& nCopyFrom);
  virtual ~CConnection();

  [[nodiscard]] inline CStreamIOSocket*           GetSocket() const { return m_Socket; }
  [[nodiscard]] inline bool                       GetUsingIPv6() const { return m_Socket->GetIsInnerIPv6(); }
  [[nodiscard]] inline std::array<uint8_t, 4>     GetIPv4() const { return m_Socket->GetIPv4(); }
  [[nodiscard]] inline std::string                GetIPString() const { return m_Socket->GetIPString(); }
  [[nodiscard]] inline std::string                GetIPStringStrict() const { return m_Socket->GetIPStringStrict(); }
  [[nodiscard]] inline sockaddr_storage*          GetRemoteAddress() const { return &(m_Socket->m_RemoteHost); }
  [[nodiscard]] inline bool                       GetIsUDPTunnel() const { return m_Type == INCON_TYPE_UDP_TUNNEL; }
  [[nodiscard]] inline bool                       GetIsVLAN() const { return m_Type == INCON_TYPE_VLAN; }
  [[nodiscard]] inline uint8_t                    GetType() const { return m_Type; }
  [[nodiscard]] inline uint16_t                   GetPort() const { return m_Port; }
  [[nodiscard]] inline bool                       GetDeleteMe() const { return m_DeleteMe; }

  inline void SetSocket(CStreamIOSocket* nSocket) { m_Socket = nSocket; }
  inline void SetType(const uint8_t nType) { m_Type = nType; }
  inline void SetDeleteMe(bool nDeleteMe) { m_DeleteMe = nDeleteMe; }

  // processing functions

  void SetTimeout(const int64_t nTicks);

  void CloseConnection();
  [[nodiscard]] uint8_t Update(void* fd, void* send_fd, int64_t timeout);

  // other functions

  virtual void Send(const std::vector<uint8_t>& data);
};

#endif // AURA_CONNECTION_H_
