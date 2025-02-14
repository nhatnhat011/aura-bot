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

#include "config_irc.h"
#include "../util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CIRCConfig
//

CIRCConfig::CIRCConfig(CConfig& CFG)
  : m_Port(6667),
    m_PrivateCmdToken("!"),
    m_BroadcastCmdToken(string())
{
  m_HostName               = ToLowerCase(CFG.GetString("irc.host_name"));
  m_Port                   = CFG.GetUint16("irc.port", 6667);
  m_NickName               = CFG.GetString("irc.nickname");
  m_UserName               = CFG.GetString("irc.username");
  m_Password               = CFG.GetString("irc.password");
  m_Enabled                = CFG.GetBool("irc.enabled", false);
  m_VerifiedDomain         = CFG.GetString("irc.verified_domain");

  m_PrivateCmdToken        = CFG.GetString("irc.commands.trigger", "!");
  m_BroadcastCmdToken      = CFG.GetString("irc.commands.broadcast.trigger");
  m_EnableBroadcast        = CFG.GetBool("irc.commands.broadcast.enabled", false);

  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};
  m_CommandCFG = new CCommandConfig(
    CFG, "irc.", false, CFG.GetBool("irc.unverified_users.reject_commands", false),
    CFG.GetStringIndex("irc.commands.common.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("irc.commands.hosting.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("irc.commands.moderator.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("irc.commands.admin.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("irc.commands.bot_owner.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO)
  );

  if (m_UserName.empty())
    m_UserName = m_NickName;
  if (m_NickName.empty())
    m_NickName = m_UserName;

  m_Channels = CFG.GetList("irc.channels", ',', m_Channels);
  m_Admins = CFG.GetSet("irc.admins", ',', m_Admins);
  m_SudoUsers = CFG.GetSet("irc.sudo_users", ',', m_SudoUsers);

  for (uint8_t i = 0; i < m_Channels.size(); ++i) {
    if (m_Channels[i].length() > 0 && m_Channels[i][0] != '#') {
      m_Channels[i] = "#" + m_Channels[i];
    }
  }

  if (!m_VerifiedDomain.empty()) {
    m_VerifiedDomain = "." + m_VerifiedDomain;
  }

  if (m_Enabled && (m_HostName.empty() || m_NickName.empty() || m_Port == 0)) {
    CFG.SetFailed();
  }
}

void CIRCConfig::Reset()
{
  m_CommandCFG = nullptr;
}

CIRCConfig::~CIRCConfig()
{
  delete m_CommandCFG;
}
