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

#ifndef AURA_IRC_H_
#define AURA_IRC_H_

#include "includes.h"
#include "config/config_irc.h"

#define LF ('\x0A')
#define IRC_TCP_KEEPALIVE_IDLE_TIME 300

class CIRC
{
public:
  CAura*                   m_Aura;
  CTCPClient*              m_Socket;
  int64_t                  m_LastConnectionAttemptTime;
  int64_t                  m_LastPacketTime;
  int64_t                  m_LastAntiIdleTime;
  bool                     m_WaitingToConnect;
  bool                     m_LoggedIn;
  std::string              m_NickName;
  CIRCConfig               m_Config;

  CIRC(CConfig& nCFG);
  ~CIRC();
  CIRC(CIRC&) = delete;

  [[nodiscard]] inline CTCPClient* GetSocket() const { return m_Socket; }
  [[nodiscard]] inline bool GetIsEnabled() const { return m_Config.m_Enabled; }
  [[nodiscard]] bool MatchHostName(const std::string& hostName) const;
  [[nodiscard]] inline bool GetIsLoggedIn() const { return m_LoggedIn; }

  [[nodiscard]] uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds) const;
  void ResetConnection();
  void Disable() { m_Config.m_Enabled = false; }
  void Update(void* fd, void* send_fd);
  void ExtractPackets();
  void Send(const std::string& message);
  void SendUser(const std::string& message, const std::string& target);
  void SendChannel(const std::string& message, const std::string& target);
  void SendAllChannels(const std::string& message);

  [[nodiscard]] bool GetIsModerator(const std::string& nHostName);
  [[nodiscard]] bool GetIsSudoer(const std::string& nHostName);
  [[nodiscard]] CCommandConfig* GetCommandConfig() const;
};

#endif // AURA_IRC_H_
