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

#include "config_discord.h"
#include "../util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CDiscordConfig
//

CDiscordConfig::CDiscordConfig(CConfig& CFG)
{
  m_HostName               = ToLowerCase(CFG.GetString("discord.host_name", "discord.com"));
  m_Token                  = CFG.GetString("discord.token");
  m_InviteUrl              = CFG.GetString("discord.invites.url");
  m_Enabled                = CFG.GetBool("discord.enabled", false);

#ifdef DISABLE_DPP
  if (m_Enabled) {
    Print("[CONFIG] warning - <discord.enabled = yes> unsupported in this Aura distribution");
    Print("[CONFIG] warning - <discord.enabled = yes> requires compilation without #define DISABLE_DPP");
    m_Enabled = false;
  }
#endif

  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};
  m_CommandCFG = new CCommandConfig(
    CFG, "discord.", true, CFG.GetBool("discord.unverified_users.reject_commands", false),
    CFG.GetStringIndex("discord.commands.common.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("discord.commands.hosting.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("discord.commands.moderator.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("discord.commands.admin.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("discord.commands.bot_owner.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO)
  );

  vector<string> invitesMode = {"all", "none", "allow_list", "deny_list"};
  m_FilterJoinServersMode = CFG.GetStringIndex("discord.invites.mode", invitesMode, FILTER_ALLOW_ALL);
  m_FilterJoinServersList = CFG.GetUint64Set("discord.invites.list", ',', {});
  m_FilterInstallUsersMode = CFG.GetStringIndex("discord.direct_messages.mode", invitesMode, FILTER_ALLOW_ALL);
  m_FilterInstallUsersList = CFG.GetUint64Set("discord.direct_messages.list", ',', {});
  m_SudoUsers = CFG.GetUint64Set("discord.sudo_users", ',', {});

  if (m_Enabled && m_Token.empty()) {
    CFG.SetFailed();
  }
}

void CDiscordConfig::Reset()
{
  m_CommandCFG = nullptr;
}

CDiscordConfig::~CDiscordConfig()
{
  delete m_CommandCFG;
}
