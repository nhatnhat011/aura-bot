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

#include "config_commands.h"
#include "../command.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CCommandConfig
//

CCommandConfig::CCommandConfig()
 : m_Enabled(true),
   m_RequireVerified(false),

   // Everything is COMMAND_PERMISSIONS_AUTO (auto) by default.
   // When CheckPermissions(permissions, DEFAULT) is called with
   // permissions == COMMAND_PERMISSIONS_AUTO, that's equivalent to
   // CheckPermissions(DEFAULT)

   m_CommonBasePermissions(COMMAND_PERMISSIONS_AUTO),
   m_HostingBasePermissions(COMMAND_PERMISSIONS_AUTO),
   m_ModeratorBasePermissions(COMMAND_PERMISSIONS_AUTO),
   m_AdminBasePermissions(COMMAND_PERMISSIONS_AUTO),
   m_BotOwnerBasePermissions(COMMAND_PERMISSIONS_AUTO),

   m_AliasPermissions(COMMAND_PERMISSIONS_AUTO),
   m_ImportPermissions(COMMAND_PERMISSIONS_AUTO),

   m_HostPermissions(COMMAND_PERMISSIONS_AUTO),
   m_HostRawPermissions(COMMAND_PERMISSIONS_AUTO),
   m_StartPermissions(COMMAND_PERMISSIONS_AUTO),
   m_SayPermissions(COMMAND_PERMISSIONS_AUTO),
   m_TellPermissions(COMMAND_PERMISSIONS_AUTO),
   m_WhoisPermissions(COMMAND_PERMISSIONS_AUTO),
   m_WhoarePermissions(COMMAND_PERMISSIONS_AUTO),
   m_StatsPermissions(COMMAND_PERMISSIONS_AUTO)
{
}

CCommandConfig::CCommandConfig(CConfig& CFG, const string& nKeyPrefix, const bool useDefaultNamespace, const bool requireVerified, const uint8_t commonPermissions, const uint8_t hostingPermissions, const uint8_t moderatorPermissions, const uint8_t adminPermissions, const uint8_t botOwnerPermissions)
  : m_CFGKeyPrefix(nKeyPrefix)
{
  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};

  // Inheritable permissions
  m_CommonBasePermissions = commonPermissions;
  m_HostingBasePermissions = hostingPermissions;
  m_ModeratorBasePermissions = moderatorPermissions;
  m_AdminBasePermissions = adminPermissions;
  m_BotOwnerBasePermissions = botOwnerPermissions;

  m_AliasPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_alias.permissions", commandPermissions, COMMAND_PERMISSIONS_SUDO);
  m_ImportPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_import.permissions", commandPermissions, COMMAND_PERMISSIONS_SUDO);

  m_HostPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_host.permissions", commandPermissions, hostingPermissions);
  m_HostRawPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_hostraw.permissions", commandPermissions, hostingPermissions);
  m_StartPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_start.permissions", commandPermissions, hostingPermissions);
  m_SayPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_say.permissions", commandPermissions, moderatorPermissions);
  m_TellPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_tell.permissions", commandPermissions, moderatorPermissions);
  m_WhoisPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_whois.permissions", commandPermissions, moderatorPermissions);
  m_WhoarePermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_whoare.permissions", commandPermissions, moderatorPermissions);
  m_StatsPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_stats.permissions", commandPermissions, moderatorPermissions);

  m_Enabled = CFG.GetBool(m_CFGKeyPrefix + "commands.enabled", true);
  if (useDefaultNamespace) {
    m_NameSpace = CFG.GetString(m_CFGKeyPrefix + "commands.namespace", "aura");
  } else {
    m_NameSpace = CFG.GetString(m_CFGKeyPrefix + "commands.namespace");
  }
  m_RequireVerified = requireVerified;
}

CCommandConfig::~CCommandConfig() = default;
