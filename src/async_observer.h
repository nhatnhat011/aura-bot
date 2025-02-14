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

#ifndef AURA_ASYNC_OBSERVER_H_
#define AURA_ASYNC_OBSERVER_H_

#include "includes.h"
#include "connection.h"

#define ASYNC_OBSERVER_OK 0u
#define ASYNC_OBSERVER_DESTROY 1u
#define ASYNC_OBSERVER_PROMOTED 2u

//
// CAsyncObserver
//

class CAsyncObserver final : public CConnection
{
public:
  CGame*                                                        m_Game;
  bool                                                          m_Synchronized;
  uint8_t                                                       m_Goal;
  uint8_t                                                       m_UID;
  uint8_t                                                       m_SID;
  uint8_t                                                       m_FrameRate;
  uint32_t                                                      m_Offset;

  CAsyncObserver(CConnection* nConnection, CGame* nGame, uint8_t nUID);
  ~CAsyncObserver();

  // processing functions

  void SetTimeout(const int64_t nTicks);
  void CloseConnection();
  void Init();
  [[nodiscard]] uint8_t Update(void* fd, void* send_fd, int64_t timeout);

  // other functions

  void Send(const std::vector<uint8_t>& data) final;
};

#endif // AURA_ASYNC_OBSERVER_H_
