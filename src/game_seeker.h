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

#ifndef AURA_GAMESEEKER_H_
#define AURA_GAMESEEKER_H_

#include "includes.h"
#include "connection.h"

#define GAMESEEKER_OK 0u
#define GAMESEEKER_DESTROY 1u
#define GAMESEEKER_PROMOTED 2u

//
// CGameSeeker
//

class CGameSeeker final : public CConnection
{
public:
  uint8_t m_GameVersion;

  CGameSeeker(CAura* nAura, uint16_t nPort, uint8_t nType, CStreamIOSocket* nSocket);
  CGameSeeker(CConnection* nConnection, uint8_t nType);
  ~CGameSeeker();

  uint8_t GetGameVersion() { return m_GameVersion; }

  // processing functions

  void SetTimeout(const int64_t nTicks);
  void CloseConnection();
  void Init();
  [[nodiscard]] uint8_t Update(void* fd, void* send_fd, int64_t timeout);

  // other functions

  void Send(const std::vector<uint8_t>& data) final;
};

#endif // AURA_GAMESEEKER_H_
