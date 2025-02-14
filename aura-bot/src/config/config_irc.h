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

#ifndef AURA_CONFIG_IRC_H_
#define AURA_CONFIG_IRC_H_

#include "../includes.h"
#include "config.h"
#include "config_commands.h"

//
// CIRCConfig
//

struct CIRCConfig
{
  std::string                         m_HostName;
  std::string                         m_NickName;
  std::string                         m_UserName;
  std::string                         m_Password;
  bool                                m_Enabled;
  std::vector<std::string>            m_Channels;
  std::set<std::string>               m_Admins;
  std::set<std::string>               m_SudoUsers;
  uint16_t                            m_Port;
  std::string                         m_PrivateCmdToken;   // a symbol prefix to identify commands and send a private reply
  std::string                         m_BroadcastCmdToken; // a symbol prefix to identify commands and send the reply to everyone
  bool                                m_EnableBroadcast;
  CCommandConfig*                     m_CommandCFG;
  std::string                         m_VerifiedDomain;

  explicit CIRCConfig(CConfig& CFG);
  ~CIRCConfig();

  void Reset();
};

#endif
