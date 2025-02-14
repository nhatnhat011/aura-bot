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

#include "aura.h"
#include "auradb.h"
#include "action.h"
#include "protocol/bnet_protocol.h"
#include "command.h"
#include "config/config_commands.h"
#include "hash.h"
#include "realm.h"
#include "game.h"
#include "protocol/game_protocol.h"
#include "game_setup.h"
#include "game_user.h"
#include "irc.h"
#include "map.h"
#include "net.h"
#include "realm_chat.h"

#include <random>
#include <tuple>
#include <future>
#include <algorithm>

#ifndef DISABLE_DPP
#include <dpp/dpp.h>
#endif

using namespace std;

//
// CCommandContext
//

/* In-game command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* game, GameUser::CGameUser* user, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_SourceRealm(user->GetRealm(false)),
    m_TargetRealm(nullptr),
    m_SourceGame(game),
    m_TargetGame(game),
    m_GameUser(user), // m_GameUser is always bound to m_SourceGame
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(user->GetName()),
    m_FromIdentifier(0),
    m_FromWhisper(false),
    m_FromType(FROM_GAME),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(user->GetRealmHostName()),

    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* Command received from BNET but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CRealm* fromRealm, const string& fromName, const bool& isWhisper, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(fromRealm),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_GameUser(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(fromName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_BNET),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(fromRealm->GetServer()),

    m_ChannelName(isWhisper ? string() : fromRealm->GetCurrentChannel()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* Command received from IRC but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CIRC* ircNetwork, const string& channelName, const string& userName, const bool& isWhisper, const string& reverseHostName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_GameUser(nullptr),
    m_IRC(ircNetwork),
    m_DiscordAPI(nullptr),

    m_FromName(userName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_IRC),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(ircNetwork->m_Config.m_HostName),
    m_ReverseHostName(reverseHostName),
    m_ChannelName(channelName),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

#ifndef DISABLE_DPP
/* Command received from Discord but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, dpp::slashcommand_t* discordAPI, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_GameUser(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(discordAPI),

    m_FromName(discordAPI->command.get_issuing_user().username),
    m_FromIdentifier(discordAPI->command.get_issuing_user().id),
    m_FromWhisper(false),
    m_FromType(FROM_DISCORD),
    m_IsBroadcast(true),

    m_Permissions(0),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  try {
    m_ServerName = discordAPI->command.get_guild().name;
    m_ChannelName = discordAPI->command.get_channel().name;
  } catch (...) {
    m_FromWhisper = true;
  }
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}
#endif

/* Command received from elsewhere but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, const string& nFromName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_GameUser(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(nFromName),
    m_FromIdentifier(0),
    m_FromWhisper(false),
    m_FromType(FROM_OTHER),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(string()),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* BNET command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CRealm* fromRealm, const string& fromName, const bool& isWhisper, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(fromRealm),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_GameUser(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(fromName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_BNET),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(0),


    m_ServerName(fromRealm->GetServer()),

    m_ChannelName(isWhisper ? string() : fromRealm->GetCurrentChannel()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* IRC command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CIRC* ircNetwork, const string& channelName, const string& userName, const bool& isWhisper, const string& reverseHostName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_GameUser(nullptr),
    m_IRC(ircNetwork),
    m_DiscordAPI(nullptr),

    m_FromName(userName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_IRC),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(0),

    m_ServerName(ircNetwork->m_Config.m_HostName),
    m_ReverseHostName(reverseHostName),
    m_ChannelName(channelName),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

#ifndef DISABLE_DPP
/* Discord command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, dpp::slashcommand_t* discordAPI, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_GameUser(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(discordAPI),

    m_FromName(discordAPI->command.get_issuing_user().username),
    m_FromIdentifier(discordAPI->command.get_issuing_user().id),
    m_FromWhisper(false),
    m_FromType(FROM_DISCORD),
    m_IsBroadcast(true),
    m_Permissions(0),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  try {
    m_ServerName = discordAPI->command.get_guild().name;
    m_ChannelName = discordAPI->command.get_channel().name;
    Print("[DISCORD] Received slash command in " + m_ServerName + "'s server - channel " + m_ChannelName);
  } catch (...) {
    Print("[DISCORD] Received slash command on " + m_FromName + "'s DM");
    m_ServerName = "users.discord.com";
    m_FromWhisper = true;
  }
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}
#endif

/* Generic command */
CCommandContext::CCommandContext(CAura* nAura, const string& nFromName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(nAura->m_CommandDefaultConfig),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_GameUser(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(nFromName),
    m_FromIdentifier(0),
    m_FromWhisper(false),
    m_FromType(FROM_OTHER),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(0),

    m_ServerName(string()),

    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

void CCommandContext::SetIdentity(const string& userName)
{
  m_FromName = userName;
}

string CCommandContext::GetUserAttribution()
{
  if (m_GameUser) {
    return m_FromName + "@" + ToFormattedRealm(m_ServerName);
  } else if (m_SourceRealm) {
    return m_FromName + "@" + m_SourceRealm->GetServer();
  } else if (m_IRC) {
    return m_FromName + "@" + m_ServerName;
  } else if (m_DiscordAPI) {
    return m_FromName + "@[" + m_ServerName + "].discord.com";
  } else if (!m_FromName.empty()) {
    return m_FromName;
  } else {
    return "[Anonymous]";
  }
}

string CCommandContext::GetUserAttributionPreffix()
{
  if (m_GameUser) {
    return m_TargetGame->GetLogPrefix() + "Player [" + m_FromName + "@" + ToFormattedRealm(m_ServerName) + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_SourceRealm) {
    return m_SourceRealm->GetLogPrefix() + "User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_IRC) {
    return "[IRC] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_DiscordAPI) {
    return "[DISCORD] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (!m_FromName.empty()) {
    return "[SYSTEM] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else {
    return "[ANONYMOUS] (Mode " + ToHexString(m_Permissions) + ") ";
  }
}

void CCommandContext::SetAuthenticated(const bool& nAuthenticated)
{
  m_OverrideVerified = nAuthenticated;
}

void CCommandContext::SetPermissions(const uint16_t nPermissions)
{
  m_OverridePermissions = nPermissions;
}

void CCommandContext::UpdatePermissions()
{
  if (m_OverridePermissions.has_value()) {
    m_Permissions = m_OverridePermissions.value();
    return;
  }

  m_Permissions = 0;

  if (m_IRC) {
    string::size_type suffixSize = m_IRC->m_Config.m_VerifiedDomain.length();
    if (suffixSize > 0) {
      if (m_ReverseHostName.length() > suffixSize &&
        m_ReverseHostName.substr(0, m_ReverseHostName.length() - suffixSize) == m_IRC->m_Config.m_VerifiedDomain
      ) {
        m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
      }
      const bool IsCreatorIRC = (
        m_TargetGame && m_TargetGame->GetCreatedFromType() == SERVICE_TYPE_IRC &&
        reinterpret_cast<CIRC*>(m_TargetGame->GetCreatedFrom()) == m_IRC
      );
      if ((!m_TargetGame || IsCreatorIRC) && m_IRC->GetIsModerator(m_ReverseHostName)) m_Permissions |= USER_PERMISSIONS_CHANNEL_ADMIN;
      if (m_IRC->GetIsSudoer(m_ReverseHostName)) m_Permissions |= USER_PERMISSIONS_BOT_SUDO_SPOOFABLE;
    }
    return;
  }
  if (m_DiscordAPI) {
#ifndef DISABLE_DPP
    if (m_Aura->m_Discord.GetIsSudoer(m_FromIdentifier)) {
      m_Permissions = SET_USER_PERMISSIONS_ALL &~ (USER_PERMISSIONS_BOT_SUDO_OK);
    } else if (m_DiscordAPI->command.get_issuing_user().is_verified()) {
      m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
    }
#endif
    return;
  }

  bool IsRealmVerified = false;
  if (m_OverrideVerified.has_value()) {
    IsRealmVerified = m_OverrideVerified.value();
  } else {
    IsRealmVerified = m_GameUser ? m_GameUser->IsRealmVerified() : m_SourceRealm != nullptr;
  }

  // Trust PvPGN servers on users identities for admin powers. Their impersonation is not a threat we worry about.
  // However, do NOT trust them regarding sudo access, since those commands may cause data deletion or worse.
  // Note also that sudo permissions must be ephemeral, since neither WC3 nor PvPGN TCP connections are secure.
  bool IsOwner = false;
  if (m_GameUser && m_GameUser->m_Game == m_TargetGame) {
    IsOwner = m_GameUser->GetIsOwner(m_OverrideVerified);
  } else if (m_TargetGame) {
    IsOwner = IsRealmVerified && m_TargetGame->MatchOwnerName(m_FromName) && m_ServerName == m_TargetGame->GetOwnerRealm();
  }
  bool IsCreatorRealm = m_TargetGame && m_SourceRealm && m_TargetGame->MatchesCreatedFrom(SERVICE_TYPE_REALM, reinterpret_cast<void*>(m_SourceRealm));
  bool IsRootAdmin = IsRealmVerified && m_SourceRealm != nullptr && (!m_TargetGame || IsCreatorRealm) && m_SourceRealm->GetIsAdmin(m_FromName);
  bool IsAdmin = IsRootAdmin || (IsRealmVerified && m_SourceRealm != nullptr && (!m_TargetGame || IsCreatorRealm) && m_SourceRealm->GetIsModerator(m_FromName));
  bool IsSudoSpoofable = IsRealmVerified && m_SourceRealm != nullptr && m_SourceRealm->GetIsSudoer(m_FromName);

  // GOTCHA: Owners are always treated as players if the game hasn't started yet. Even if they haven't joined.
  if (m_GameUser || (IsOwner && m_TargetGame && m_TargetGame->GetIsLobbyStrict())) {
    m_Permissions |= USER_PERMISSIONS_GAME_PLAYER;
  }

  // Leaver or absent owners are automatically demoted.
  if (IsRealmVerified) {
    m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
  }
  if (IsOwner && (m_GameUser || (m_TargetGame && m_TargetGame->GetIsLobbyStrict()))) {
    m_Permissions |= USER_PERMISSIONS_GAME_OWNER;
  }
  if (IsAdmin) m_Permissions |= USER_PERMISSIONS_CHANNEL_ADMIN;
  if (IsRootAdmin) m_Permissions |= USER_PERMISSIONS_CHANNEL_ROOTADMIN;

  // Sudo is a permission system separate from channels.
  if (IsSudoSpoofable) m_Permissions |= USER_PERMISSIONS_BOT_SUDO_SPOOFABLE;
  if (m_GameUser && m_GameUser->CheckSudoMode()) {
    m_Permissions = SET_USER_PERMISSIONS_ALL;
  }
}

optional<bool> CCommandContext::CheckPermissions(const uint8_t requiredPermissions) const
{
  optional<bool> result;
  switch (requiredPermissions) {
    case COMMAND_PERMISSIONS_DISABLED:
      result = false;
      break;
    case COMMAND_PERMISSIONS_SUDO:
      result = (m_Permissions & USER_PERMISSIONS_BOT_SUDO_OK) > 0;
      break;
    case COMMAND_PERMISSIONS_SUDO_UNSAFE:
      result = (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE) > 0;
      break;
    case COMMAND_PERMISSIONS_ROOTADMIN:
      result = (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) > 0;
      break;
    case COMMAND_PERMISSIONS_ADMIN:
      result = (m_Permissions & (USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) > 0;
      break;
    case COMMAND_PERMISSIONS_VERIFIED_OWNER:
      result = (
        ((m_Permissions & (USER_PERMISSIONS_CHANNEL_VERIFIED | USER_PERMISSIONS_BOT_SUDO_OK)) > 0) &&
        ((m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0)
      );
      // Note: It's possible to be owner without being verified, if they belong to a LAN realm.
      break;
    case COMMAND_PERMISSIONS_OWNER:
      result = (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      break;
    case COMMAND_PERMISSIONS_VERIFIED:
      result = (m_Permissions & (USER_PERMISSIONS_CHANNEL_VERIFIED | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      break;
    case COMMAND_PERMISSIONS_AUTO:
      // Let commands special-case nullopt, or call CheckPermissions().value_or(true)
      break;
    case COMMAND_PERMISSIONS_POTENTIAL_OWNER:
      if (m_TargetGame && !m_TargetGame->HasOwnerSet()) {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_PLAYER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      } else {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      }
      break;
    case COMMAND_PERMISSIONS_START_GAME:
      if (m_TargetGame && (m_TargetGame->m_PublicStart || !m_TargetGame->HasOwnerSet())) {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_PLAYER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      } else {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      }
      break;
    case COMMAND_PERMISSIONS_UNVERIFIED:
      result = true;
      break;
  }

  return result;
}

bool CCommandContext::CheckPermissions(const uint8_t requiredPermissions, const uint8_t autoPermissions) const
{
  // autoPermissions must not be COMMAND_PERMISSIONS_AUTO, nor other unhandled cases
  // (that results in infinite recursion)
  optional<bool> result = CheckPermissions(requiredPermissions);
  if (result.has_value()) return result.value();
  return CheckPermissions(autoPermissions).value_or(false);
}

bool CCommandContext::CheckConfirmation(const string& cmdToken, const string& cmd, const string& payload, const string& errorMessage)
{
  string message = cmdToken + cmd + payload;
  if (m_GameUser) {
    if (m_GameUser->GetLastCommand() == message) {
      m_GameUser->ClearLastCommand();
      return true;
    } else {
      m_GameUser->SetLastCommand(message);
    }
  }
  ErrorReply(errorMessage + "Send the command again to confirm.");
  return false;
}

optional<pair<string, string>> CCommandContext::CheckSudo(const string& message)
{
  optional<pair<string, string>> Result;
  // Allow !su for LAN connections
  if (!m_ServerName.empty() && !(m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
    return Result;
  }
  if (!m_Aura->m_SudoContext) {
    return Result;
  }
  if (m_Aura->m_SudoContext->GetPartiallyDestroyed()) {
    m_Aura->m_SudoContext.reset();
    m_Aura->m_SudoAuthPayload.clear();
    m_Aura->m_SudoExecCommand.clear();
    return Result;
  }
  bool isValidCaller = (
    m_FromName == m_Aura->m_SudoContext->m_FromName &&
    m_SourceRealm == m_Aura->m_SudoContext->m_SourceRealm &&
    m_TargetRealm == m_Aura->m_SudoContext->m_TargetRealm &&
    m_SourceGame == m_Aura->m_SudoContext->m_SourceGame &&
    m_TargetGame == m_Aura->m_SudoContext->m_TargetGame &&
    m_GameUser == m_Aura->m_SudoContext->m_GameUser &&
    m_IRC == m_Aura->m_SudoContext->m_IRC &&
    (!m_DiscordAPI) == (!m_Aura->m_SudoContext->m_DiscordAPI)
  );
  if (isValidCaller && message == m_Aura->m_SudoAuthPayload) {
    LogStream(*m_Output, "[AURA] Confirmed " + m_FromName + " command \"" + m_Aura->m_SudoExecCommand + "\"");
    size_t PayloadStart = m_Aura->m_SudoExecCommand.find(' ');
    string Command, Payload;
    if (PayloadStart != string::npos) {
      Command = m_Aura->m_SudoExecCommand.substr(0, PayloadStart);
      Payload = m_Aura->m_SudoExecCommand.substr(PayloadStart + 1);
    } else {
      Command = m_Aura->m_SudoExecCommand;
    }
    Result.emplace(ToLowerCase(Command), Payload);
    //m_Permissions |= USER_PERMISSIONS_BOT_SUDO_OK;
    m_Permissions = SET_USER_PERMISSIONS_ALL;
  }
  m_Aura->m_SudoContext.reset();
  m_Aura->m_SudoAuthPayload.clear();
  m_Aura->m_SudoExecCommand.clear();
  return Result;
}

bool CCommandContext::GetIsSudo() const
{
  return (0 != (m_Permissions & (USER_PERMISSIONS_BOT_SUDO_OK)));
}

vector<string> CCommandContext::JoinReplyListCompact(const vector<string>& stringList) const
{
  vector<string> result;

  if (m_FromType == FROM_GAME && m_TargetGame) {
    string bufferedLine;
    for (const auto& element : stringList) {
      if (element.size() > 100) {
        if (!bufferedLine.empty()) {
          result.push_back(bufferedLine);
          bufferedLine.clear();
        }
        string leftElement = element;
        do {
          result.push_back(leftElement.substr(0, 100));
          leftElement = leftElement.substr(100);
        } while (leftElement.length() > 100);
        if (!leftElement.empty()) {
          result.push_back(leftElement);
        }
      } else if (bufferedLine.size() + element.size() > 97) {
        result.push_back(bufferedLine);
        bufferedLine = element;
      } else if (bufferedLine.empty()) {
        bufferedLine = element;
      } else {
        bufferedLine += " | " + element;
      }
    }
    if (!bufferedLine.empty()) {
      result.push_back(bufferedLine);
      bufferedLine.clear();
    }
  } else {
    result.push_back(JoinVector(stringList, false));
  }

  return result;
}

void CCommandContext::SendPrivateReply(const string& message, const uint8_t ctxFlags)
{
  if (message.empty())
    return;

  switch (m_FromType) {
    case FROM_GAME: {
      if (!m_TargetGame) break;
      if (message.length() <= 100) {
        m_TargetGame->SendChat(m_GameUser, message);
      } else {
        string leftMessage = message;
        do {
          m_TargetGame->SendChat(m_GameUser, leftMessage.substr(0, 100));
          leftMessage = leftMessage.substr(100);
        } while (leftMessage.length() > 100);
        if (!leftMessage.empty()) {
          m_TargetGame->SendChat(m_GameUser, leftMessage);
        }
      }
      break;
    }

    case FROM_BNET: {
      if (m_SourceRealm) {
        m_SourceRealm->TryQueueChat(message, m_FromName, true, shared_from_this(), ctxFlags);
      }
      break;
    }

    case FROM_IRC: {
      if (m_IRC) {
        m_IRC->SendUser(message, m_FromName);
      }
      break;
    }

#ifndef DISABLE_DPP
    case FROM_DISCORD: {
      if (m_DiscordAPI) {
        m_Aura->m_Discord.SendUser(message, m_FromIdentifier);
      }
      break;
    }
#endif

    default: {
      LogStream(*m_Output, "[AURA] " + message);
    }
  }
}

void CCommandContext::SendReplyCustomFlags(const string& message, const uint8_t ctxFlags) {
  bool AllTarget = ctxFlags & CHAT_SEND_TARGET_ALL;
  bool AllSource = ctxFlags & CHAT_SEND_SOURCE_ALL;
  bool AllSourceSuccess = false;
  if (AllTarget) {
    if (m_TargetGame) {
      m_TargetGame->SendAllChat(message);
      if (m_TargetGame == m_SourceGame) {
        AllSourceSuccess = true;
      }
    }
    if (m_TargetRealm) {
      m_TargetRealm->TryQueueChat(message, m_FromName, false, shared_from_this(), ctxFlags);
      if (m_TargetRealm == m_SourceRealm) {
        AllSourceSuccess = true;
      }
    }
    // IRC/Discord are not valid targets, only sources.
  }
  if (AllSource && !AllSourceSuccess && !m_FromWhisper) {
    if (m_SourceGame) {
      m_SourceGame->SendAllChat(message);
      AllSourceSuccess = true;
    }
    if (m_SourceRealm && !AllSourceSuccess) {
      m_SourceRealm->TryQueueChat(message, m_FromName, false, shared_from_this(), ctxFlags);
      AllSourceSuccess = true;
    }
    if (m_IRC) {
      m_IRC->SendChannel(message, m_ChannelName);
      AllSourceSuccess = true;
    }
#ifndef DISABLE_DPP
    if (m_DiscordAPI) {
      m_DiscordAPI->edit_original_response(dpp::message(message));
      AllSourceSuccess = true;
    }
#endif
  }
  if (!AllSourceSuccess) {
    SendPrivateReply(message, ctxFlags);
  }

  // Write to console if CHAT_LOG_CONSOLE, but only if we haven't written to it in SendPrivateReply
  if (m_FromType != FROM_OTHER && (ctxFlags & CHAT_LOG_CONSOLE)) {
    if (m_TargetGame) {
      LogStream(*m_Output, m_TargetGame->GetLogPrefix() + message);
    } else if (m_SourceRealm) {
      LogStream(*m_Output, m_SourceRealm->GetLogPrefix() + message);
    } else if (m_IRC) {
      LogStream(*m_Output, "[IRC] " + message);
    } else if (m_DiscordAPI) {
      LogStream(*m_Output, "[DISCORD] " + message);
    } else {
      LogStream(*m_Output, "[AURA] " + message);
    }
  }
}

void CCommandContext::SendReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;

  if (m_IsBroadcast) {
    SendReplyCustomFlags(message, ctxFlags | CHAT_SEND_SOURCE_ALL);
  } else {
    SendReplyCustomFlags(message, ctxFlags);
  }
}

void CCommandContext::InfoReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;
  SendReply(message, ctxFlags | CHAT_TYPE_INFO);
}

void CCommandContext::DoneReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;
  SendReply("Done: " + message, ctxFlags | CHAT_TYPE_DONE);
}

void CCommandContext::ErrorReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;
  SendReply("Error: " + message, ctxFlags | CHAT_TYPE_ERROR);
}

void CCommandContext::SendAll(const string& message)
{
  if (message.empty()) return;
  SendReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::InfoAll(const string& message)
{
  if (message.empty()) return;
  InfoReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::DoneAll(const string& message)
{
  if (message.empty()) return;
  DoneReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::ErrorAll(const string& message)
{
  if (message.empty()) return;
  ErrorReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::SendAllUnlessHidden(const string& message)
{
  if (!m_TargetGame) {
    SendAll(message);
  } else {
    SendReply(message, m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames() ? CHAT_SEND_TARGET_ALL : 0);
  }
}

GameUser::CGameUser* CCommandContext::GetTargetUser(const string& target)
{
  GameUser::CGameUser* targetUser = nullptr;
  if (!m_TargetGame) {
    return targetUser;
  }
  if (m_TargetGame->GetIsHiddenPlayerNames()) {
    m_TargetGame->GetUserFromDisplayNamePartial(target, targetUser);
  } else {
    m_TargetGame->GetUserFromNamePartial(target, targetUser);
  }
  return targetUser;
}

GameUser::CGameUser* CCommandContext::RunTargetUser(const string& target)
{
  GameUser::CGameUser* targetUser = nullptr;
  if (!m_TargetGame) {
    return targetUser;
  }

  uint8_t Matches;
  if (m_TargetGame->GetIsHiddenPlayerNames()) {
    Matches = m_TargetGame->GetUserFromDisplayNamePartial(target, targetUser);
  } else {
    Matches = m_TargetGame->GetUserFromNamePartial(target, targetUser);
  }
  if (Matches > 1) {
    ErrorReply("Player [" + target + "] ambiguous.");
  } else if (Matches == 0) {
    ErrorReply("Player [" + target + "] not found.");
  }
  return targetUser;
}

GameUser::CGameUser* CCommandContext::GetTargetUserOrSelf(const string& target)
{
  if (target.empty()) {
    return m_GameUser;
  }

  GameUser::CGameUser* targetUser = nullptr;
  if (!m_TargetGame) return targetUser;
  if (m_TargetGame->GetIsHiddenPlayerNames()) {
    m_TargetGame->GetUserFromDisplayNamePartial(target, targetUser);
  } else {
    m_TargetGame->GetUserFromNamePartial(target, targetUser);
  }
  return targetUser;
}

GameUser::CGameUser* CCommandContext::RunTargetPlayerOrSelf(const string& target)
{
  if (target.empty()) {
    return m_GameUser;
  }

  GameUser::CGameUser* targetUser = nullptr;
  if (!m_TargetGame) {
    ErrorReply("Please specify target user.");
    return targetUser;
  }
  if (m_TargetGame->GetIsHiddenPlayerNames()) {
    m_TargetGame->GetUserFromDisplayNamePartial(target, targetUser);
  } else {
    m_TargetGame->GetUserFromNamePartial(target, targetUser);
  }
  if (!targetUser) {
    ErrorReply("Player [" + target + "] not found.");
  }
  return targetUser;
}

bool CCommandContext::GetParsePlayerOrSlot(const std::string& target, uint8_t& SID, GameUser::CGameUser*& user)
{
  if (!m_TargetGame || target.empty()) {
    return false;
  }
  switch (target[0]) {
    case '#': {
      uint8_t testSID = ParseSID(target.substr(1));
      const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
      if (!slot) {
        return false;
      }
      SID = testSID;
      user = m_TargetGame->GetUserFromUID(slot->GetUID());
      return true;
    }

    case '@': {
      user = GetTargetUser(target.substr(1));
      if (user == nullptr) {
        return false;
      }
      SID = m_TargetGame->GetSIDFromUID(user->GetUID());
      return true;
    }

    default: {
      uint8_t testSID = ParseSID(target);
      const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
      GameUser::CGameUser* testPlayer = GetTargetUser(target.substr(1));
      if ((slot == nullptr) == (testPlayer == nullptr)) {
        return false;
      }
      if (testPlayer == nullptr) {
        SID = testSID;
        user = m_TargetGame->GetUserFromUID(slot->GetUID());
      } else {
        SID = m_TargetGame->GetSIDFromUID(testPlayer->GetUID());
        user = testPlayer;
      }
      return true;
    }
  }
}

bool CCommandContext::RunParsePlayerOrSlot(const std::string& target, uint8_t& SID, GameUser::CGameUser*& user)
{
  if (!m_TargetGame || target.empty()) {
    ErrorReply("Please provide a user @name or #slot.");
    return false;
  }
  switch (target[0]) {
    case '#': {
      uint8_t testSID = ParseSID(target.substr(1));
      const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
      if (!slot) {
        ErrorReply("Slot " + ToDecString(testSID + 1) + " not found.");
        return false;
      }
      SID = testSID;
      user = m_TargetGame->GetUserFromUID(slot->GetUID());
      return true;
    }

    case '@': {
      user = RunTargetUser(target.substr(1));
      if (user == nullptr) {
        return false;
      }
      SID = m_TargetGame->GetSIDFromUID(user->GetUID());
      return true;
    }

    default: {
      uint8_t testSID = ParseSID(target);
      const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
      GameUser::CGameUser* testPlayer = GetTargetUser(target.substr(1));
      if ((slot == nullptr) == (testPlayer == nullptr)) {
        ErrorReply("Please provide a user @name or #slot.");
        return false;
      }
      if (testPlayer == nullptr) {
        SID = testSID;
        user = m_TargetGame->GetUserFromUID(slot->GetUID());
      } else {
        SID = m_TargetGame->GetSIDFromUID(testPlayer->GetUID());
        user = testPlayer;
      }
      return true;
    }
  }
}

bool CCommandContext::GetParseNonPlayerSlot(const std::string& target, uint8_t& SID)
{
  if (!m_TargetGame || target.empty()) {
    return false;
  }

  uint8_t testSID = 0xFF;
  if (target[0] == '#') {
    testSID = ParseSID(target.substr(1));
  } else {
    testSID = ParseSID(target);
  }

  const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
  if (!slot) {
    return false;
  }
  if (m_TargetGame->GetIsPlayerSlot(testSID)) {
    return false;
  }
  SID = testSID;
  return true;
}

bool CCommandContext::RunParseNonPlayerSlot(const std::string& target, uint8_t& SID)
{
  if (!m_TargetGame || target.empty()) {
    ErrorReply("Please provide a user #slot.");
    return false;
  }

  uint8_t testSID = 0xFF;
  if (target[0] == '#') {
    testSID = ParseSID(target.substr(1));
  } else {
    testSID = ParseSID(target);
  }

  const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
  if (!slot) {
    ErrorReply("Slot [" + target + "] not found.");
    return false;
  }
  if (m_TargetGame->GetIsPlayerSlot(testSID)) {
    ErrorReply("Slot is occupied by a player.");
    return false;
  }
  SID = testSID;
  return true;
}

CRealm* CCommandContext::GetTargetRealmOrCurrent(const string& target)
{
  if (target.empty()) {
    return m_SourceRealm;
  }
  string realmId = TrimString(target);
  transform(begin(realmId), end(realmId), begin(realmId), [](char c) { return static_cast<char>(std::tolower(c)); });
  CRealm* exactMatch = m_Aura->GetRealmByInputId(realmId);
  if (exactMatch) return exactMatch;
  return m_Aura->GetRealmByHostName(realmId);
}

bool CCommandContext::GetParseTargetRealmUser(const string& inputTarget, string& nameFragment, string& realmFragment, CRealm*& realm, bool allowNoRealm, bool searchHistory)
{
  if (inputTarget.empty()) {
    return false;
  }

  string target = inputTarget;
  string::size_type realmStart = inputTarget.find('@');
  const bool isFullyQualified = realmStart != string::npos;
  if (isFullyQualified) {
    realmFragment = TrimString(inputTarget.substr(realmStart + 1));
    nameFragment = TrimString(inputTarget.substr(0, realmStart));
    if (!nameFragment.empty() && nameFragment.size() <= MAX_PLAYER_NAME_SIZE) {
      if (allowNoRealm && realmFragment.empty()) {
        return true;
      }
      realm = GetTargetRealmOrCurrent(realmFragment);
      if (realm) {
        realmFragment = realm->GetServer();
      }
      return realm != nullptr;
    }
    // Handle @PLAYER
    target = realmFragment;
    realmFragment.clear();
    nameFragment.clear();
    //isFullyQualified = false;
  }

  if (/*!isFullyQualified && */m_GameUser && m_SourceGame->GetIsHiddenPlayerNames()) {
    return false;
  }

  if (m_GameUser && searchHistory) {
    CDBBan* targetPlayer = nullptr;
    if (m_SourceGame->GetBannableFromNamePartial(target, targetPlayer) != 1) {
      return false;
    }
    realmFragment = targetPlayer->GetServer();
    nameFragment = targetPlayer->GetName();
  } else if (m_GameUser) {
    GameUser::CGameUser* targetPlayer = GetTargetUser(target);
    if (!targetPlayer) {
      return false;
    }
    realmFragment = targetPlayer->GetRealmHostName();
    nameFragment = targetPlayer->GetName();
  } else if (m_SourceRealm) {
    realmFragment = m_ServerName;
    nameFragment = TrimString(target);
  } else {
    return false;
  }

  if (nameFragment.empty() || nameFragment.size() > MAX_PLAYER_NAME_SIZE) {
    return false;
  }

  if (realmFragment.empty()) {
    return true;
  } else {
    realm = GetTargetRealmOrCurrent(realmFragment);
    if (realm) {
      realmFragment = realm->GetServer();
    }
    return realm != nullptr;
  }
}

uint8_t CCommandContext::GetParseTargetServiceUser(const std::string& target, std::string& nameFragment, std::string& locationFragment, void*& location)
{
  bool isRealm = GetParseTargetRealmUser(target, nameFragment, locationFragment, reinterpret_cast<CRealm*&>(location));
  if (isRealm) return SERVICE_TYPE_REALM;
  CGame* matchingGame = GetTargetGame(locationFragment);
  if (matchingGame) {
    if (nameFragment.size() > MAX_PLAYER_NAME_SIZE) {
      location = reinterpret_cast<CRealm*>(matchingGame);
      return SERVICE_TYPE_GAME;
    }
  }
  return SERVICE_TYPE_INVALID;
}

CGame* CCommandContext::GetTargetGame(const string& rawInput)
{
  return m_Aura->GetGameByString(rawInput);
}

void CCommandContext::UseImplicitReplaceable()
{
  if (m_TargetGame) return;

  for (auto it = m_Aura->m_Lobbies.rbegin(); it != m_Aura->m_Lobbies.rend(); ++it) {
    if ((*it)->GetIsReplaceable() && !(*it)->GetCountDownStarted()) {
      m_TargetGame = *it;
    }
  }

  if (m_TargetGame && !GetIsSudo()) {
    UpdatePermissions();
  }
}

void CCommandContext::UseImplicitHostedGame()
{
  if (m_TargetGame) return;

  m_TargetGame = m_Aura->GetMostRecentLobbyFromCreator(m_FromName);
  if (!m_TargetGame) m_TargetGame = m_Aura->GetMostRecentLobby();

  if (m_TargetGame && !GetIsSudo()) {
    UpdatePermissions();
  }
}

void CCommandContext::Run(const string& cmdToken, const string& command, const string& payload)
{
  const static string emptyString;

  UpdatePermissions();

  string Command = command;
  string Payload = payload;

  if (HasNullOrBreak(command) || HasNullOrBreak(payload)) {
    ErrorReply("Invalid input");
    return;
  }

  uint64_t CommandHash = HashCode(Command);

  if (CommandHash == HashCode("su")) {
    // Allow !su for LAN connections
    if (!m_ServerName.empty() && !(m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
      ErrorReply("Forbidden");
      return;
    }
    m_Aura->m_SudoContext = shared_from_this();
    m_Aura->m_SudoAuthPayload = m_Aura->GetSudoAuthPayload(Payload);
    m_Aura->m_SudoExecCommand = Payload;
    SendReply("Sudo command requested. See Aura's console for further steps.");
    Print("[AURA] Sudoer " + GetUserAttribution() + " requests command \"" + cmdToken + Payload + "\"");
    if (m_SourceRealm && m_FromWhisper) {
      Print("[AURA] Confirm from [" + m_ServerName + "] with: \"/w " + m_SourceRealm->GetLoginName() + " " + cmdToken + m_Aura->m_Config.m_SudoKeyWord + " " + m_Aura->m_SudoAuthPayload + "\"");
    } else if (m_IRC || m_DiscordAPI) {
      Print("[AURA] Confirm from [" + m_ServerName + "] with: \"" + cmdToken + m_Aura->m_Config.m_SudoKeyWord + " " + m_Aura->m_SudoAuthPayload + "\"");
    } else {
      Print("[AURA] Confirm from the game client with: \"" + cmdToken + m_Aura->m_Config.m_SudoKeyWord + " " + m_Aura->m_SudoAuthPayload + "\"");
    }
    return;
  }

  if (CommandHash == HashCode(m_Aura->m_Config.m_SudoKeyWord)) {
    optional<pair<string, string>> runOverride = CheckSudo(Payload);
    if (runOverride.has_value()) {
      Command = runOverride->first;
      Payload = runOverride->second;
      CommandHash = HashCode(Command);
    } else {
      if (m_Aura->m_SudoExecCommand.empty()) {
        Print("[AURA] " + GetUserAttribution() + " sent command [" + cmdToken + command + "] with payload [" + payload + "], but " + cmdToken + "su was not requested.");
      } else {
        Print("[AURA] " + GetUserAttribution() + " failed sudo authentication.");
      }
      ErrorReply("Sudo check failure.");
      return;
    }
  }

  const bool isLocked = m_TargetGame && (m_GameUser->GetIsActionLocked() || m_TargetGame->GetLocked());
  if (isLocked && 0 == (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
    LogStream(*m_Output, m_TargetGame->GetLogPrefix() + "Command ignored, the game is locked");
    ErrorReply("Only the game owner and root admins can run game commands when the game is locked.");
    return;
  }

  if (Payload.empty()) {
    LogStream(*m_Output, GetUserAttributionPreffix() + "sent command [" + cmdToken + command + "]");
  } else {
    LogStream(*m_Output, GetUserAttributionPreffix() + "sent command [" + cmdToken + command + "] with payload [" + payload + "]");
  }

  /*********************
   * NON ADMIN COMMANDS *
   *********************/

  switch (CommandHash)
  {
    //
    // !ABOUT
    //

    case HashCode("version"):
    case HashCode("about"): {
      SendReply("Aura " + m_Aura->m_Version + " is a permissive-licensed open source project. Say hi at <" + m_Aura->m_IssuesURL + ">");
      break;
    }

    //
    // !HELP
    //

    case HashCode("git"):
    case HashCode("help"): {
      SendReply("Aura " + m_Aura->m_Version + " is a permissive-licensed open source project. Read more at <" + m_Aura->m_RepositoryURL + ">");
      break;
    }

    //
    // !SC
    //
    case HashCode("sc"): {
      SendReply("To verify your identity and use commands in game rooms, whisper me two letters: sc");
      break;
    }

    //
    // !KEY
    //
    case HashCode("key"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to inspect game net configuration.");
        break;
      }

      SendReply(
        "Name <<" + m_TargetGame->GetGameName() + ">> | ID " +
        to_string(m_TargetGame->GetHostCounter()) + " (" + ToHexString(m_TargetGame->GetHostCounter()) + ")" + " | Key " +
        to_string(m_TargetGame->GetEntryKey()) + " (" + ToHexString(m_TargetGame->GetEntryKey()) + ")"
      );
      break;
    }

    //
    // !SLOT
    //

    case HashCode("slot"):
    {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to inspect raw slots.");
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Payload, SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "slot <PLAYER>");
        break;
      }
      const CGameSlot* slot = m_TargetGame->InspectSlot(SID);
      if (targetPlayer) {
        SendReply("Player " + targetPlayer->GetName() + " (slot #" + ToDecString(SID + 1) + ") = " + ByteArrayToDecString(slot->GetByteArray()));
      } else {
        SendReply("Slot #" + ToDecString(SID + 1) + " = " + ByteArrayToDecString(slot->GetByteArray()));
      }
      break;
    }

    //
    // !CHECKME
    // !CHECK <PLAYER>
    //

    case HashCode("checkme"):
    case HashCode("check"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror()) {
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }
      if (m_TargetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games.");
        break;
      }
      CRealm* targetPlayerRealm = targetPlayer->GetRealm(true);
      bool IsRealmVerified = targetPlayerRealm != nullptr;
      bool IsOwner = targetPlayer->GetIsOwner(nullopt);
      bool IsRootAdmin = IsRealmVerified && targetPlayerRealm->GetIsAdmin(targetPlayer->GetName());
      bool IsAdmin = IsRootAdmin || (IsRealmVerified && targetPlayerRealm->GetIsModerator(targetPlayer->GetName()));
      string SyncStatus;
      if (m_TargetGame->GetGameLoaded()) {
        if (m_TargetGame->m_SyncPlayers[targetPlayer].size() + 1 == m_TargetGame->m_Users.size()) {
          SyncStatus = "Full";
        } else if (m_TargetGame->m_SyncPlayers[targetPlayer].empty()) {
          SyncStatus = "Alone";
        } else {
          SyncStatus = "With: ";
          for (auto& otherPlayer: m_TargetGame->m_SyncPlayers[targetPlayer]) {
            SyncStatus += otherPlayer->GetName() + ", ";
          }
          SyncStatus = SyncStatus.substr(0, SyncStatus.length() - 2);
        }
      }
      string SlotFragment, ReadyFragment;
      if (m_TargetGame->GetIsLobbyStrict()) {
        SlotFragment = "Slot #" + to_string(1 + m_TargetGame->GetSIDFromUID(targetPlayer->GetUID())) + ". ";
        if (targetPlayer->GetIsReady()) {
          ReadyFragment = "Ready. ";
        } else {
          ReadyFragment = "Not ready. ";
        }
      }
      
      string IPVersionFragment;
      if (targetPlayer->GetUsingIPv6()) {
        IPVersionFragment = ", IPv6";
      } else {
        IPVersionFragment = ", IPv4";
      }
      string FromFragment;
      if (m_Aura->m_Net.m_Config.m_EnableGeoLocalization) {
        FromFragment = ", From: " + m_Aura->m_DB->FromCheck(ByteArrayToUInt32(targetPlayer->GetIPv4(), true));
      }
      SendReply("[" + targetPlayer->GetName() + "]. " + SlotFragment + ReadyFragment + "Ping: " + targetPlayer->GetDelayText(true) + IPVersionFragment + ", Reconnection: " + targetPlayer->GetReconnectionText() + FromFragment + (m_TargetGame->GetGameLoaded() ? ", Sync: " + SyncStatus : ""));
      SendReply("[" + targetPlayer->GetName() + "]. Realm: " + (targetPlayer->GetRealmHostName().empty() ? "LAN" : targetPlayer->GetRealmHostName()) + ", Verified: " + (IsRealmVerified ? "Yes" : "No") + ", Reserved: " + (targetPlayer->GetIsReserved() ? "Yes" : "No"));
      if (IsOwner || IsAdmin || IsRootAdmin) {
        SendReply("[" + targetPlayer->GetName() + "]. Owner: " + (IsOwner ? "Yes" : "No") + ", Admin: " + (IsAdmin ? "Yes" : "No") + ", Root Admin: " + (IsRootAdmin ? "Yes" : "No"));
      }
      break;
    }

    //
    // !PING
    //

    case HashCode("pingall"):
    case HashCode("ping"):
    case HashCode("p"): {
      // kick players with ping higher than payload if payload isn't empty
      // we only do this if the game hasn't started since we don't want to kick players from a game in progress

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      optional<uint32_t> kickPing;
      if (!Payload.empty()) {
        if (!m_TargetGame->GetIsLobbyStrict()) {
          ErrorReply("Maximum ping may only be set in a lobby.");
          break;
        }
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("Not allowed to set maximum ping.");
          break;
        }
        try {
          int64_t Value = stoul(Payload);
          if (Value <= 0 || 0xFFFFFFFF < Value) {
            ErrorReply("Invalid maximum ping [" + Payload + "].");
            break;
          }
          kickPing = static_cast<uint32_t>(Value);
        } catch (...) {
          ErrorReply("Invalid maximum ping [" + Payload + "].");
          break;
        }
      }

      if (kickPing.has_value())
        m_TargetGame->m_Config.m_AutoKickPing = kickPing.value();

      // copy the m_Users vector so we can sort by descending ping so it's easier to find players with high pings

      vector<GameUser::CGameUser*> SortedPlayers = m_TargetGame->m_Users;
      if (m_TargetGame->GetGameLoaded()) {
        sort(begin(SortedPlayers), end(SortedPlayers), [](const GameUser::CGameUser* a, const GameUser::CGameUser* b) {
          return a->GetNormalSyncCounter() < b->GetNormalSyncCounter();
        });
      } else {
        sort(begin(SortedPlayers), end(SortedPlayers), [](const GameUser::CGameUser* a, const GameUser::CGameUser* b) {
          return a->GetOperationalRTT() > b->GetOperationalRTT();
        });
      }
      bool anyPing = false;
      vector<string> pingsText;
      uint32_t maxPing = 0;
      for (auto i = begin(SortedPlayers); i != end(SortedPlayers); ++i) {
        pingsText.push_back((*i)->GetDisplayName() + ": " + (*i)->GetDelayText(false));
        uint32_t ping = (*i)->GetRTT();
        if (ping == 0) continue; // also skips this iteration if there is no ping data
        anyPing = true;
        if (ping > maxPing) maxPing = ping;
      }

      if (anyPing) {
        SendReply(JoinVector(pingsText, false), !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      } else if (m_Aura->m_Net.m_Config.m_HasBufferBloat && m_TargetGame->IsDownloading()) {
        SendReply("Ping not measured yet (wait for map download.)", !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply("Ping not measured yet.", !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      }

      const uint16_t internalLatency = m_TargetGame->GetLatency();
      const bool suggestLowerLatency = 0 < maxPing && maxPing < internalLatency && REFRESH_PERIOD_MIN_SUGGESTED < internalLatency;
      const bool suggestHigherLatency = 0 < maxPing && internalLatency < maxPing / 4 && REFRESH_PERIOD_MAX_SUGGESTED > internalLatency;
      if (m_TargetGame->m_Config.m_LatencyEqualizerEnabled || suggestLowerLatency || suggestHigherLatency) {
        string refreshText = "Internal latency is " + to_string(m_TargetGame->GetLatency()) + "ms.";
        string equalizerHeader;
        if (m_TargetGame->m_Config.m_LatencyEqualizerEnabled) {
          equalizerHeader = "Ping equalizer ENABLED. ";
        }
        string suggestionText;
        if (suggestLowerLatency) {
          suggestionText = " Decrease it with " + cmdToken + "latency [VALUE]";
        } else if (suggestHigherLatency) {
          suggestionText = " Increase it with " + cmdToken + "latency [VALUE]";
        }
        SendReply(
          "HINT: " + equalizerHeader + refreshText + suggestionText,
          !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0
        );
      }

      break;
    }

    //
    // !CHECKRACE
    // !RACES
    // !GAME
    //

    case HashCode("game"):
    case HashCode("races"):
    case HashCode("checkrace"): {
      if (!m_TargetGame)
        break;

      vector<string> output;
      output.push_back("Game#" + to_string(m_TargetGame->GetGameID()));

      // GOTCHA: /game - Leavers info is omitted. This affects games with name obfuscation.
      vector<const GameUser::CGameUser*> players = m_TargetGame->GetPlayers();
      for (const auto& player : players) {
        const CGameSlot* slot = m_TargetGame->InspectSlot(m_TargetGame->GetSIDFromUID(player->GetUID()));
        uint8_t race = slot->GetRaceFixed();
        output.push_back("[" + player->GetName() + "] - " + GetRaceName(race));
      }
      vector<string> replyLines = JoinReplyListCompact(output);
      for (const auto& line : replyLines) {
        SendReply(line);
      }
	  if (CommandHash == HashCode("gamedetails")) {
	  }
      break;
    }

    //
    // !STATSDOTA
    // !STATS
    //

    case HashCode("statsdota"):
    case HashCode("stats"): {
      if (
        !(m_SourceGame && m_SourceGame->GetIsLobbyStrict()) &&
        !CheckPermissions(m_Config->m_StatsPermissions, COMMAND_PERMISSIONS_VERIFIED)
      ) {
        ErrorReply("Not allowed to look up stats.");
        break;
      }
      if (Payload.empty() && (!m_TargetGame || m_TargetGame->GetIsHiddenPlayerNames())) {
        ErrorReply("Usage: " + cmdToken + "stats <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "statsdota <PLAYER>");
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }
      const bool isDota = CommandHash == HashCode("statsdota") || m_TargetGame->GetClientFileName().find("DotA") != string::npos;
      const bool isUnverified = targetPlayer->GetRealm(false) != nullptr && !targetPlayer->IsRealmVerified();
      string targetIdentity = "[" + targetPlayer->GetExtendedName() + "]";
      if (isUnverified) targetIdentity += " (unverified)";

      if (isDota) {
        CDBDotAPlayerSummary* DotAPlayerSummary = m_Aura->m_DB->DotAPlayerSummaryCheck(targetPlayer->GetName(), targetPlayer->GetRealmHostName());
        if (!DotAPlayerSummary) {
          SendReply(targetIdentity + " has no registered DotA games.");
          break;
        }
        const string summaryText = (
          targetIdentity +
          " - " + to_string(DotAPlayerSummary->GetTotalGames()) + " games (W/L: " +
          to_string(DotAPlayerSummary->GetTotalWins()) + "/" + to_string(DotAPlayerSummary->GetTotalLosses()) +
          ") Hero K/D/A: " + to_string(DotAPlayerSummary->GetTotalKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalDeaths()) +
          "/" + to_string(DotAPlayerSummary->GetTotalAssists()) +
          " (" + to_string(DotAPlayerSummary->GetAvgKills()) +
          "/" + to_string(DotAPlayerSummary->GetAvgDeaths()) +
          "/" + to_string(DotAPlayerSummary->GetAvgAssists()) +
          ") Creep K/D/N: " + to_string(DotAPlayerSummary->GetTotalCreepKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalCreepDenies()) +
          "/" + to_string(DotAPlayerSummary->GetTotalNeutralKills()) +
          " (" + to_string(DotAPlayerSummary->GetAvgCreepKills()) +
          "/" + to_string(DotAPlayerSummary->GetAvgCreepDenies()) +
          "/" + to_string(DotAPlayerSummary->GetAvgNeutralKills()) +
          ") T/R/C: " + to_string(DotAPlayerSummary->GetTotalTowerKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalRaxKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalCourierKills())
        );
        SendReply(summaryText);
        delete DotAPlayerSummary;
      } else {
        CDBGamePlayerSummary* GamePlayerSummary = m_Aura->m_DB->GamePlayerSummaryCheck(targetPlayer->GetName(), targetPlayer->GetRealmHostName());
        if (!GamePlayerSummary) {
          SendReply(targetIdentity + " has no registered games.");
          break;
        }
        const string summaryText = (
          targetIdentity + " has played " +
          to_string(GamePlayerSummary->GetTotalGames()) + " games with this bot. Average loading time: " +
          ToFormattedString(static_cast<double>(GamePlayerSummary->GetAvgLoadingTime())) + " seconds. Average stay: " +
          to_string(GamePlayerSummary->GetAvgLeftPercent()) + "%"
        );
        SendReply(summaryText);
        delete GamePlayerSummary;
      }

      break;
    }

    //
    // !GETPLAYERS
    // !GETOBSERVERS
    //

    case HashCode("getplayers"):
    case HashCode("getobservers"): {
      if (!Payload.empty()) {
        m_TargetGame = GetTargetGame(Payload);
      } else if (m_SourceGame) {
        m_TargetGame = m_SourceGame;
      } else {
        UseImplicitHostedGame();
      }

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (m_TargetGame->m_DisplayMode == GAME_PRIVATE && !m_GameUser) {
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("This game is private.");
          break;
        }
      }
      string Players = ToNameListSentence(m_TargetGame->GetPlayers());
      string Observers = ToNameListSentence(m_TargetGame->GetObservers());
      if (Players.empty() && Observers.empty()) {
        SendReply("Nobody is in the game.");
        break;
      }
      string PlayersFragment = Players.empty() ? "No players. " : "Players: " + Players + ". ";
      string ObserversFragment = Observers.empty() ? "No observers" : "Observers: " + Observers + ".";
      SendReply(PlayersFragment + ObserversFragment);
      break;
    }

    //
    // !VOTEKICK
    //

    case HashCode("votekick"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded())
        break;

      if (Payload.empty()){
        ErrorReply("Usage: " + cmdToken + "votekick <PLAYERNAME>");
        break;
      }

      if (!m_TargetGame->m_KickVotePlayer.empty()) {
        ErrorReply("Unable to start votekick. Another votekick is in progress");
        break;
      }
      if (m_TargetGame->m_Users.size() <= 2) {
        ErrorReply("Unable to start votekick. There aren't enough players in the game for a votekick");
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Payload, SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "votekick <PLAYERNAME>");
        break;
      }
      if (!targetPlayer) {
        ErrorReply("Slot #" + to_string(SID + 1) + " is not occupied by a user.");
        break;
      }
      if (targetPlayer->GetIsReserved()) {
        ErrorAll("Unable to votekick user [" + targetPlayer->GetName() + "]. That user is reserved and cannot be votekicked");
        break;
      }

      m_TargetGame->m_KickVotePlayer      = targetPlayer->GetName();
      m_TargetGame->m_StartedKickVoteTime = GetTime();

      for (auto& it : m_TargetGame->m_Users)
        it->SetKickVote(false);

      SendReply("Votekick against [" + m_TargetGame->m_KickVotePlayer + "] started by [" + m_FromName + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      if (m_GameUser && m_GameUser != targetPlayer) {
        m_GameUser->SetKickVote(true);
        SendAll("[" + m_GameUser->GetDisplayName() + "] voted to kick [" + m_TargetGame->m_KickVotePlayer + "]. " + to_string(static_cast<uint32_t>(ceil(static_cast<float>(m_TargetGame->GetNumJoinedPlayers() - 1) * static_cast<float>(m_TargetGame->m_Config.m_VoteKickPercentage) / 100)) - 1) + " more votes are needed to pass");
      }
      SendAll("Type " + cmdToken + "yes or " + cmdToken + "no to vote.");

      break;
    }

    //
    // !YES
    //

    case HashCode("yes"): {
      if (!m_GameUser || m_TargetGame->m_KickVotePlayer.empty() || m_GameUser->GetKickVote().value_or(false))
        break;

      uint32_t VotesNeeded = static_cast<uint32_t>(ceil(static_cast<float>(m_TargetGame->GetNumJoinedPlayers() - 1) * static_cast<float>(m_TargetGame->m_Config.m_VoteKickPercentage) / 100));
      m_GameUser->SetKickVote(true);
      m_TargetGame->SendAllChat("[" + m_GameUser->GetDisplayName() + "] voted for kicking [" + m_TargetGame->m_KickVotePlayer + "]. " + to_string(VotesNeeded) + " affirmative votes required to pass");
      m_TargetGame->CountKickVotes();
      break;
    }

    //
    // !NO
    //

    case HashCode("no"): {
      if (!m_GameUser || m_TargetGame->m_KickVotePlayer.empty() || !m_GameUser->GetKickVote().value_or(true))
        break;

      m_GameUser->SetKickVote(false);
      m_TargetGame->SendAllChat("[" + m_GameUser->GetDisplayName() + "] voted against kicking [" + m_TargetGame->m_KickVotePlayer + "].");
      break;
    }

    //
    // !INVITE
    //

    case HashCode("invite"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsStageAcceptingJoins()) {
        // Intentionally allows !invite to fake (mirror) lobbies.
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        break;
      }

      const string MapPath = m_TargetGame->GetMap()->GetClientPath();
      size_t LastSlash = MapPath.rfind('\\');

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, false, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        }
        break;
      }

      // Name of sender and receiver should be included in the message,
      // so that they can be checked in successful whisper acks from the server (BNETProtocol::IncomingChatEvent::WHISPERSENT)
      // Note that the server doesn't provide any way to recognize whisper targets if the whisper fails.
      if (LastSlash != string::npos && LastSlash <= MapPath.length() - 6) {
        m_ActionMessage = targetName + ", " + m_FromName + " invites you to play [" + MapPath.substr(LastSlash + 1) + "]. Join game \"" + m_TargetGame->m_GameName + "\"";
      } else {
        m_ActionMessage = targetName + ", " + m_FromName + " invites you to join game \"" + m_TargetGame->m_GameName + "\"";
      }

      targetRealm->QueueWhisper(m_ActionMessage, targetName, shared_from_this(), true);
      break;
    }

    //
    // !FLIP
    //

    case HashCode("coin"):
    case HashCode("coinflip"):
    case HashCode("flip"): {
      double chance = 0.5;
      if (!Payload.empty()) {
        double chancePercent;
        try {
          chancePercent = stod(Payload);
        } catch (...) {
          ErrorReply("Usage: " + cmdToken + "flip <CHANCE%>");
          break;
        }
        if (chancePercent < 0. || chancePercent > 100.) {
          ErrorReply("Usage: " + cmdToken + "flip <CHANCE%>");
          break;
        }
        chance = chancePercent / 100.;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::bernoulli_distribution bernoulliDist(chance);
      bool result = bernoulliDist(gen);

      SendReply(m_FromName + " flipped a coin and got " + (result ? "heads" : "tails") + ".", !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !ROLL
    //

    case HashCode("roll"): {
      size_t diceStart = Payload.find('d');
      uint16_t rollFaces = 0;
      uint8_t rollCount = 1;

      string rawRollCount = diceStart == string::npos ? "1" : Payload.substr(0, diceStart);
      string rawRollFaces = Payload.empty() ? "100" : diceStart == string::npos ? Payload : Payload.substr(diceStart + 1);

      try {
        rollCount = static_cast<uint8_t>(stoi(rawRollCount));
        rollFaces = static_cast<uint16_t>(stoi(rawRollFaces));
      } catch (...) {
        ErrorReply("Usage: " + cmdToken + "roll <FACES>");
        break;
      }

      if (!(0 < rollCount && rollCount <= 8)) {
        ErrorReply("Invalid dice count: " + to_string(rollCount));
        break;
      }
      if (!(0 < rollFaces && rollFaces <= 10000)) {
        ErrorReply("Invalid dice faces: " + to_string(rollFaces));
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, rollFaces);

      vector<string> gotRolls;
      for (uint8_t i = 1; i <= rollCount; ++i) {
        gotRolls.push_back(to_string(distribution(gen)));
      }

      if (Payload.empty()) {
        SendReply(m_FromName + " rolled " + gotRolls[0] + ".", !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply(m_FromName + " rolled " + to_string(rollCount) + "d" + to_string(rollFaces) + ". Got: " + JoinVector(gotRolls, false) + ".", !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      }
      break;
    }

    //
    // !PICK
    //

    case HashCode("pick"): {
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "pick <OPTION> , <OPTION> , <OPTION>, ...");
        break;
      }

      vector<string> options = SplitArgs(Payload, 1, 24);
      if (options.empty()) {
        ErrorReply("Empty options list.");
        break;
      }
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(options.size()));

      string randomPick = options[distribution(gen) - 1];
      SendReply("Randomly picked: " + randomPick, !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKRACE
    //

    case HashCode("pickrace"): {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(0, 3);
      const uint8_t race = 1 << distribution(gen);
      string randomPick = GetRaceName(race);
      SendReply("Randomly picked: " + randomPick + " race", !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKPLAYER
    //

    case HashCode("pickplayer"): {
      if (!m_TargetGame)
        break;

      vector<const GameUser::CGameUser*> players = m_TargetGame->GetPlayers();
      if (players.empty()) {
        ErrorReply("No players found.");
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(players.size()));
      const GameUser::CGameUser* pickedPlayer = players[distribution(gen) - 1];
      string randomPick = pickedPlayer->GetName();
      SendReply("Randomly picked: " + randomPick, !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKOBS
    //

    case HashCode("pickobserver"):
    case HashCode("pickobs"): {
      if (!m_TargetGame)
        break;

      vector<const GameUser::CGameUser*> players = m_TargetGame->GetObservers();
      if (players.empty()) {
        ErrorReply("No observers found.");
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(players.size()));
      const GameUser::CGameUser* pickedPlayer = players[distribution(gen) - 1];
      string randomPick = pickedPlayer->GetName();
      SendReply("Randomly picked: " + randomPick, !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !ERAS
    //

    case HashCode("eras"): {
      if (Payload.empty() && m_TargetGame && m_TargetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("Usage: " + cmdToken + "eras <PLAYER|COUNTRY|COLOR>");
        break;
      }

      string countryName = Payload;
      std::transform(std::begin(countryName), std::end(countryName), std::begin(countryName), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (countryName == "sweden" || countryName == "peru") {
        SendReply("Slot #1: " + GetColorName(0));
      } else if (countryName == "england" || countryName == "chile") {
        SendReply("Slot #2: " + GetColorName(1));
      } else if (countryName == "russia" || countryName == "bolivia") {
        SendReply("Slot #3: " + GetColorName(2));
      } else if (countryName == "italy" || countryName == "argentina" || countryName == "argentine") {
        SendReply("Slot #4: " + GetColorName(3));
      } else if (countryName == "france" || countryName == "colombia") {
        SendReply("Slot #5: " + GetColorName(4));
      } else if (countryName == "spain" || countryName == "venezuela") {
        SendReply("Slot #6: " + GetColorName(5));
      } else if (countryName == "turkey" || countryName == "turkiye" || countryName == "brasil" || countryName == "brazil") {
        SendReply("Slot #7: " + GetColorName(6));
      } else if (countryName == "poland" || countryName == "mexico") {
        SendReply("Slot #8: " + GetColorName(7));
      } else if (countryName == "germany" || countryName == "ecuador") {
        SendReply("Slot #9: " + GetColorName(8));
      } else if (countryName == "ireland") {
        SendReply("Slot #13: " + GetColorName(12));
      } else if (countryName == "norway") {
        SendReply("Slot #14: " + GetColorName(13));
      } else if (countryName == "iceland") {
        SendReply("Slot #15: " + GetColorName(14));
      } else if (countryName == "greece") {
        SendReply("Slot #16: " + GetColorName(15));
      } else if (countryName == "holland" || countryName == "netherlands") {
        SendReply("Slot #17: " + GetColorName(16));
      } else if (countryName == "romania") {
        SendReply("Slot #18: " + GetColorName(17));
      } else if (countryName == "egypt") {
        SendReply("Slot #19: " + GetColorName(18));
      } else if (countryName == "morocco") {
        SendReply("Slot #20: " + GetColorName(19));
      } else if (countryName == "congo") {
        SendReply("Slot #21: " + GetColorName(20));
      } else if (countryName == "somalia") {
        SendReply("Slot #22: " + GetColorName(21));
      } else {
        uint8_t color = ParseColor(Payload);
        if (color == 0xFF && m_TargetGame) {
          GameUser::CGameUser* targetPlayer = GetTargetUserOrSelf(Payload);
          if (targetPlayer) {
            color = m_TargetGame->GetSIDFromUID(targetPlayer->GetUID());
          }
        }
        if (color == 0) {
          SendReply(GetColorName(color) + " - EU: Sweden, LAT: Peru");
        } else if (color == 1) {
          SendReply(GetColorName(color) + " - EU: England, LAT: Chile");
        } else if (color == 2) {
          SendReply(GetColorName(color) + " - EU: Russia, LAT: Bolivia");
        } else if (color == 3) {
          SendReply(GetColorName(color) + " - EU: Italy, LAT: Argentina");
        } else if (color == 4) {
          SendReply(GetColorName(color) + " - EU: France, LAT: Colombia");
        } else if (color == 5) {
          SendReply(GetColorName(color) + " - EU: Spain, LAT: Venezuela");
        } else if (color == 6) {
          SendReply(GetColorName(color) + " - EU: Turkey, LAT: Brasil");
        } else if (color == 7) {
          SendReply(GetColorName(color) + " - EU: Poland, LAT: Mexico");
        } else if (color == 8) {
          SendReply(GetColorName(color) + " - EU: Germany, LAT: Ecuador");
        } else if (color == 12) {
          SendReply(GetColorName(color) + " - EU: Ireland");
        } else if (color == 13) {
          SendReply(GetColorName(color) + " - EU: Norway");
        } else if (color == 14) {
          SendReply(GetColorName(color) + " - EU: Iceland");
        } else if (color == 15) {
          SendReply(GetColorName(color) + " - EU: Greece");
        } else if (color == 16) {
          SendReply(GetColorName(color) + " - EU: Holland");
        } else if (color == 17) {
          SendReply(GetColorName(color) + " - EU: Romania");
        } else if (color == 18) {
          SendReply(GetColorName(color) + " - EU: Egypt");
        } else if (color == 19) {
          SendReply(GetColorName(color) + " - EU: Morocco");
        } else if (color == 20) {
          SendReply(GetColorName(color) + " - EU: Congo");
        } else if (color == 21) {
          SendReply(GetColorName(color) + " - EU: Somalia");
        } else if (color == 0xFF) {
          ErrorReply("Not a country identifier: [" + Payload + "]");
        } else {
          ErrorReply("No country matches slot #" + ToDecString(color + 1));
        }
      }
      break;
    }

#ifndef DISABLE_DPP
    case HashCode("twrpg"): {
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "twrpg <NOMBRE>");
        break;
      }

      string name = Payload;
      uint8_t matchType = m_Aura->m_DB->FindData(MAP_TYPE_TWRPG, MAP_DATA_TYPE_ANY, name, false);
      if (matchType == MAP_DATA_TYPE_NONE) {
        vector<string> words = Tokenize(name, ' ');
        if (words.size() <= 1) {
          ErrorReply("[" + Payload + "] not found.");
          break;
        }
        string intermediate = words[0];
        words[0] = words[words.size() - 1];
        words[words.size() - 1] = intermediate;
        name = JoinVector(words, " ", false);
        matchType = m_Aura->m_DB->FindData(MAP_TYPE_TWRPG, MAP_DATA_TYPE_ANY, name, false);
        if (matchType == MAP_DATA_TYPE_NONE) {
          ErrorReply("[" + Payload + "] not found.");
          break;
        }
      }
      if (matchType == MAP_DATA_TYPE_ANY) {
        ErrorReply("Did you mean any of these? " + name);
        break;
      }

      vector<string> descriptionLines = m_Aura->m_DB->GetDescription(MAP_TYPE_TWRPG, matchType, name);
      if (descriptionLines.empty()) {
        ErrorReply("Item description not found.");
        break;
      }

      vector<string> replyLines = JoinReplyListCompact(descriptionLines);
      for (const auto& line : replyLines) {
        SendReply(line);
      }
      break;
    }
#endif

    /*****************
     * ADMIN COMMANDS *
     ******************/
     
    //
    // !FROM
    //

    case HashCode("where"):
    case HashCode("from"):
    case HashCode("f"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!m_Aura->m_Net.m_Config.m_EnableGeoLocalization) {
        ErrorReply("Geolocalization is disabled.");
        break;
      }

      if (!CheckPermissions(m_Config->m_CommonBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("Not allowed to check players geolocalization.");
        break;
      }

      string Froms;

      for (auto i = begin(m_TargetGame->m_Users); i != end(m_TargetGame->m_Users); ++i) {
        // we reverse the byte order on the IP because it's stored in network byte order

        Froms += (*i)->GetDisplayName();
        Froms += ": (";
        Froms += m_Aura->m_DB->FromCheck(ByteArrayToUInt32((*i)->GetIPv4(), true));
        Froms += ")";

        if (i != end(m_TargetGame->m_Users) - 1)
          Froms += ", ";
      }

      SendReply(Froms, !m_GameUser || m_GameUser->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PBANLAST
    //

    case HashCode("bl"):
    case HashCode("pbl"):
    case HashCode("pbanlast"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (!m_TargetGame->m_LastLeaverBannable) {
        ErrorReply("No ban candidates stored.");
        break;
      }

      string lower = Payload;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });
      bool isConfirm = lower == "c" || lower == "confirm";

      if (!m_TargetGame->m_LastLeaverBannable->GetSuspect()) {
        if (isConfirm) {
          ErrorReply("Usage: " + cmdToken + "pbanlast");
          break;
        }
        m_TargetGame->m_LastLeaverBannable->SetSuspect(true);
        SendReply("Player [" + m_TargetGame->m_LastLeaverBannable->GetName() + "@" + m_TargetGame->m_LastLeaverBannable->GetServer() + "] was the last leaver.");
        SendReply("Use " + cmdToken + "pbanlast confirm to ban them.");        
        break;
      } else {
        if (!isConfirm) {
          ErrorReply("Usage: " + cmdToken + "pbanlast confirm");
          break;
        }
        string authServer = m_ServerName;
        if (m_SourceRealm) {
          authServer = m_SourceRealm->GetDataBaseID();
        }
        m_Aura->m_DB->BanAdd(
          m_TargetGame->m_LastLeaverBannable->GetName(),
          m_TargetGame->m_LastLeaverBannable->GetServer(),
          authServer,
          m_TargetGame->m_LastLeaverBannable->GetIP(),
          m_FromName,
          "Leaver"
        );
        SendAll("Player [" + m_TargetGame->m_LastLeaverBannable->GetName() + "@" + m_TargetGame->m_LastLeaverBannable->GetServer() + "] was banned by [" + m_FromName + "] on server [" + m_TargetGame->m_LastLeaverBannable->GetAuthServer() + "]");
      }
      break;
    }

    //
    // !CLOSE (close slot)
    //

    case HashCode("close"):
    case HashCode("c"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        if (!m_TargetGame->CloseSlot()) {
          ErrorReply("No slots are open.");
        } else {
          SendReply("One slot closed.");
        }
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, m_TargetGame->GetMap()->GetVersionMaxSlots());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "c <SLOTNUM>");
        break;
      }

      vector<string> failedSlots;
      for (auto& elem : Args) {
        if (elem == 0 || elem > m_TargetGame->GetMap()->GetVersionMaxSlots()) {
          ErrorReply("Usage: " + cmdToken + "c <SLOTNUM>");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        if (!m_TargetGame->CloseSlot(SID, CommandHash == HashCode("close"))) {
          failedSlots.push_back(to_string(elem));
        }
      }
      if (Args.size() == failedSlots.size()) {
        ErrorReply("Failed to close slot.");
      } else if (failedSlots.empty()) {
        if (Args.size() == 1) {
          SendReply("Closed slot #" + to_string(Args[0]) + ".");
        } else {
          SendReply("Closed " + to_string(Args.size()) + " slot(s).");
        }
      } else {
        ErrorReply("Slot(s) " + JoinVector(failedSlots, false) + " cannot be closed.");
      }
      break;
    }

    //
    // !END
    //

    case HashCode("end"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot end the game.");
        break;
      }

      LogStream(*m_Output, m_TargetGame->GetLogPrefix() + "is over (admin ended game) [" + m_FromName + "]");
      m_TargetGame->SendAllChat("Ending the game.");
      m_TargetGame->StopPlayers("was disconnected (admin ended game)");
      break;
    }

    //
    // !URL
    //

    case HashCode("url"):
    case HashCode("link"): {
      if (!m_TargetGame)
        break;

      const string TargetUrl = TrimString(Payload);

      if (TargetUrl.empty()) {
        if (m_TargetGame->GetMapSiteURL().empty()) {
          SendAll("Download URL unknown.");
        } else {
          SendAll("Visit  <" + m_TargetGame->GetMapSiteURL() + "> to download [" + m_TargetGame->GetClientFileName() + "]");
        }
        break;
      }

      if (!m_TargetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      m_TargetGame->SetMapSiteURL(TargetUrl);
      SendReply("Download URL set to [" + TargetUrl + "]", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !GPROXY
    //

    case HashCode("reconnect"):
    case HashCode("gproxy"): {
      SendReply("Protect against disconnections using GProxyDLL, a Warcraft III plugin. See: <" + m_Aura->m_Net.m_Config.m_AnnounceGProxySite + ">");
      break;
    }

    //
    // !MODE
    //

    
    case HashCode("hcl"):
    case HashCode("mode"): {
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      /*if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }*/

      if (Payload.empty()) {
        SendReply("Game mode (HCL) is [" + m_TargetGame->m_HCLCommandString + "]");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (Payload.size() > m_TargetGame->m_Slots.size()) {
        ErrorReply("Unable to set mode (HCL) because it's too long - it must not exceed the amount of occupied game slots");
        break;
      }

      const string checkResult = CheckIsValidHCL(Payload);
      if (!checkResult.empty()) {
        ErrorReply(checkResult);
        break;
      }

      m_TargetGame->m_HCLCommandString = Payload;
      SendAll("Game mode (HCL) set to [" + m_TargetGame->m_HCLCommandString + "]");
      break;
    }

    //
    // !HOLD (hold a slot for someone)
    //

    case HashCode("reserve"):
    case HashCode("hold"): {
      UseImplicitHostedGame();
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, m_TargetGame->GetMap()->GetVersionMaxSlots());

      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "hold <PLAYER1> , <PLAYER2>, ...");
        break;
      }

      vector<string> addedList;
      for (auto& targetName: Args) {
        if (targetName.empty())
          continue;
        const GameUser::CGameUser* targetUser = GetTargetUser(targetName);
        if (targetUser) {
          m_TargetGame->AddToReserved(targetUser->GetName());
          addedList.push_back(targetUser->GetDisplayName());
        } else {
          m_TargetGame->AddToReserved(targetName);
          addedList.push_back(targetName);
        }
      }

      SendAll("Added user(s) to the hold list: " + JoinVector(addedList, false));
      break;
    }

    //
    // !UNHOLD
    //

    case HashCode("unhold"): {
      UseImplicitHostedGame();
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, m_TargetGame->GetMap()->GetVersionMaxSlots());

      if (Args.empty()) {
        m_TargetGame->RemoveAllReserved();
        SendAll("Cleared the reservations list.");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        m_TargetGame->RemoveFromReserved(PlayerName);
      }
      SendAll("Removed user(s) from the reservations list: " + JoinVector(Args, false));
      break;
    }


#ifdef DEBUG
    //
    // !DISCONNECT (disconnect a user)
    //
    case HashCode("disconnect"): {
      UseImplicitHostedGame();
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!GetIsSudo()) {
        ErrorReply("You are not a sudoer, and therefore cannot disconnect a player.");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }

      targetPlayer->GetSocket()->m_HasError = true;
      SendReply("Forcibly disconnected user [" + targetPlayer->GetName() + "]");
      break;
    }
#endif

    //
    // !KICK (kick a user)
    //

    case HashCode("closekick"):
    case HashCode("ckick"):
    case HashCode("kick"):
    case HashCode("k"): {
      UseImplicitHostedGame();
      if (!m_TargetGame || m_TargetGame->GetIsMirror() || (m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded()))
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "kick <PLAYERNAME>");
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Payload, SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "kick <PLAYERNAME>");
        break;
      }
      if (!targetPlayer) {
        ErrorReply("Slot #" + to_string(SID + 1) + " is not occupied by a user.");
        break;
      }

      targetPlayer->CloseConnection();
      //targetPlayer->SetDeleteMe(true);
      targetPlayer->SetLeftReason("was kicked by [" + m_FromName + "]");

      if (m_TargetGame->GetIsLobbyStrict())
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      else
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOST);

      if (m_TargetGame->GetIsLobbyStrict()) {
        bool KickAndClose = CommandHash == HashCode("ckick") || CommandHash == HashCode("closekick");
        if (KickAndClose && !m_TargetGame->GetIsRestored()) {
          m_TargetGame->CloseSlot(SID, false);
        }
      }

      break;
    }

    //
    // !LATENCY (set game latency)
    //

    case HashCode("latency"): {
      UseImplicitHostedGame();      
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (Payload.empty()) {
        SendReply("The game latency is " + to_string(m_TargetGame->GetLatency()) + " ms");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot modify the game latency.");
        break;
      }

      string lower = Payload;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "default" || lower == "reset") {
        m_TargetGame->ResetLatency();
        SendReply("Latency settings reset to default.", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
        break;
      } else if (lower == "ignore" || lower == "bypass" || lower == "normal") {
        if (!m_TargetGame->GetGameLoaded()) {
          ErrorReply("This command must be used after the game has loaded.");
          break;
        }
        m_TargetGame->NormalizeSyncCounters();
        SendReply("Ignoring lagging players. (They may not be able to control their units.)", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "latency [REFRESH]");
        ErrorReply("Usage: " + cmdToken + "latency [REFRESH], [TOLERANCE]");
        break;
      }

      if (Args[0] <= 0 || Args[0] > 60000) {
        // WC3 clients disconnect after a minute without network activity.
        ErrorReply("Invalid game refresh period [" + to_string(Args[0]) + "ms].");
        break;
      }

      double refreshTime = static_cast<double>(Args[0]);
      double refreshFactor = /*m_TargetGame->m_Config.m_LatencyEqualizerEnabled ? 2. : */1.;
      optional<double> tolerance;
      if (Args.size() >= 2) {
        tolerance = static_cast<double>(Args[1]);
        if (tolerance.value() <= LAG_TOLERANCE_MIN_TIME) {
          ErrorReply("Minimum spike tolerance is " + to_string(LAG_TOLERANCE_MIN_TIME) + " ms.");
          break;
        }
        if (tolerance.value() >= LAG_TOLERANCE_MAX_TIME) {
          ErrorReply("Maximum spike tolerance is " + to_string(LAG_TOLERANCE_MAX_TIME) + " ms.");
          break;
        }
      }

      if (refreshTime < REFRESH_PERIOD_MIN) {
        refreshTime = REFRESH_PERIOD_MIN;
      } else if (refreshTime > REFRESH_PERIOD_MAX) {
        refreshTime = REFRESH_PERIOD_MAX;
      }

      const double oldRefresh = m_TargetGame->GetLatency();
      const double oldSyncLimit = m_TargetGame->GetSyncLimit();
      const double oldSyncLimitSafe = m_TargetGame->GetSyncLimitSafe();

      double syncLimit, syncLimitSafe;
      double resolvedTolerance = (
        tolerance.has_value() ?
        tolerance.value() :
        oldSyncLimit * oldRefresh
      );
      syncLimit = resolvedTolerance / refreshTime;
      syncLimitSafe = oldSyncLimitSafe * syncLimit / oldSyncLimit;
      if (syncLimit < 4) syncLimit = 4;
      if (syncLimitSafe < syncLimit / 2) syncLimitSafe = syncLimit / 2;
      if (syncLimitSafe < 1) syncLimitSafe = 1;

      m_TargetGame->m_Config.m_Latency = static_cast<uint16_t>(refreshTime * refreshFactor);
      m_TargetGame->m_Config.m_SyncLimit = static_cast<uint16_t>(syncLimit);
      m_TargetGame->m_Config.m_SyncLimitSafe = static_cast<uint16_t>(syncLimitSafe);

      const uint32_t finalToleranceMilliseconds = (
        static_cast<uint32_t>(m_TargetGame->GetLatency()) *
        static_cast<uint32_t>(m_TargetGame->GetSyncLimit())
      );

      if (refreshTime == REFRESH_PERIOD_MIN) {
        SendReply("Game will be updated at the fastest rate (every " + to_string(m_TargetGame->GetLatency()) + " ms)", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      } else if (refreshTime == REFRESH_PERIOD_MAX) {
        SendReply("Game will be updated at the slowest rate (every " + to_string(m_TargetGame->GetLatency()) + " ms)", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply("Game will be updated with a delay of " + to_string(m_TargetGame->GetLatency()) + "ms.", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      }
      SendReply("Spike tolerance set to " + to_string(finalToleranceMilliseconds) + "ms.", m_TargetGame->GetIsLobbyStrict() || !m_TargetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !EQUALIZER (set ping equalizer)
    //

    case HashCode("equalizer"): {
      UseImplicitHostedGame();      
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot toggle the latency equalizer.");
        break;
      }

      optional<bool> targetValue;
      if (Payload.empty() || Payload == "on" || Payload == "ON") {
        targetValue = true;
      } else if (Payload == "off" || Payload == "OFF") {
        targetValue = false;
      }
      if (!targetValue.has_value()) {
        ErrorReply("Unrecognized setting [" + Payload + "].");
        break;
      }

      if (m_TargetGame->m_Config.m_LatencyEqualizerEnabled == targetValue.value()) {
        if (targetValue.value()) {
          ErrorReply("Latency equalizer is already enabled.");
        } else {
          ErrorReply("Latency equalizer is already disabled.");
        }
        break;
      }

      m_TargetGame->m_Config.m_LatencyEqualizerEnabled = targetValue.value();

      if (!targetValue.value()) {
        if (m_TargetGame->GetGameLoaded()) {
          auto nodes = m_TargetGame->GetAllFrameNodes();
          m_TargetGame->MergeFrameNodes(nodes);
          m_TargetGame->ResetUserPingEqualizerDelays();
        }
        SendReply("Latency equalizer DISABLED.");
      } else {
        SendReply("Latency equalizer ENABLED.");
      }
      break;
    }

    //
    // !OPEN (open slot)
    //

    case HashCode("open"):
    case HashCode("o"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        if (!m_TargetGame->OpenSlot()) {
          ErrorReply("Cannot open further slots.");
        } else {
          SendReply("One slot opened.");
        }
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, m_TargetGame->GetMap()->GetVersionMaxSlots());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "o <SLOTNUM>");
        break;
      }

      vector<string> failedSlots;
      for (auto& elem : Args) {
        if (elem == 0 || elem > m_TargetGame->GetMap()->GetVersionMaxSlots()) {
          ErrorReply("Usage: " + cmdToken + "o <SLOTNUM>");
          break;
        }
        const uint8_t SID = static_cast<uint8_t>(elem) - 1;
        const CGameSlot* slot = m_TargetGame->GetSlot(SID);
        if (!slot || slot->GetSlotStatus() == SLOTSTATUS_OPEN) {
          failedSlots.push_back(to_string(elem));
          continue;
        }
        if (!m_TargetGame->OpenSlot(SID, CommandHash == HashCode("open"))) {
          failedSlots.push_back(to_string(elem));
        }
      }
      if (Args.size() == failedSlots.size()) {
        ErrorReply("Failed to open slot.");
      } else if (failedSlots.empty()) {
        if (Args.size() == 1) {
          SendReply("Opened slot #" + to_string(Args[0]) + ".");
        } else {
          SendReply("Opened " + to_string(Args.size()) + " slot(s).");
        }
      } else {
        ErrorReply("Slot(s) " + JoinVector(failedSlots, false) + " cannot be opened.");
      }
      break;
    }

    //
    // !PUB (create or recreate as public game)
    // !PRIV (create or recreate as private game)
    //

    case HashCode("priv"):
    case HashCode("pub"): {
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "pub <GAMENAME>");
        ErrorReply("Usage: " + cmdToken + "priv <GAMENAME>");
        break;
      }

      if (m_TargetGame) {
        if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted()) {
          break;
        }
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot rehost the game.");
          break;
        }
      } else {
        if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
          ErrorReply("Not allowed to host games.");
          break;
        }
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
      }

      if (m_SourceRealm && m_Aura->m_GameVersion != m_SourceRealm->GetGameVersion() &&
        find(m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.begin(), m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end(), m_SourceRealm->GetGameVersion()) == m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end()
        && !GetIsSudo()) {
        ErrorReply("Hosting games on v1." + to_string(m_SourceRealm->GetGameVersion()) + " is disabled.");
        break;
      }

      if (Payload.length() > m_Aura->m_MaxGameNameSize) {
        ErrorReply("Unable to create game [" + Payload + "]. The game name is too long (the maximum is " + to_string(m_Aura->m_MaxGameNameSize) + " characters)");
        break;
      }

      if (m_TargetGame) {
        SendReply("Trying to rehost with name [" + Payload + "].", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      }

      bool IsPrivate = CommandHash == HashCode("priv");
      if (m_TargetGame) {
        m_TargetGame->m_DisplayMode  = IsPrivate ? GAME_PRIVATE : GAME_PUBLIC;
        m_TargetGame->m_GameName     = Payload;
        m_TargetGame->m_HostCounter  = m_Aura->NextHostCounter();
        m_TargetGame->UpdateGameDiscovery();

        for (auto& realm : m_Aura->m_Realms) {
          if (m_TargetGame->m_IsMirror && realm->GetIsMirror()) {
            continue;
          }
          if (!realm->GetLoggedIn()) {
            continue;
          }
          if (m_TargetGame->m_RealmsExcluded.find(realm->GetServer()) != m_TargetGame->m_RealmsExcluded.end()) {
            continue;
          }

          // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
          // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
          // we assume this won't happen very often since the only downside is a potential false positive, which will soon be corrected
          // (CAura::EventBNETGameRefreshSuccess doesn't do much)

          realm->ResetGameBroadcastData();
        }

        m_TargetGame->m_CreationTime = m_TargetGame->m_LastRefreshTime = GetTime();
      } else {
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
        if (!m_Aura->GetNewGameIsInQuotaConservative()) {
          ErrorReply("Already hosting a game.");
          break;
        }
        m_Aura->m_GameSetup->SetContext(shared_from_this());
        m_Aura->m_GameSetup->SetBaseName(Payload);
        m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
        if (m_Aura->m_Config.m_AutomaticallySetGameOwner) {
          m_Aura->m_GameSetup->SetOwner(m_FromName, m_SourceRealm);
        }
        if (m_SourceRealm) {
          m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
        } else if (m_IRC) {
          m_Aura->m_GameSetup->SetCreator(m_FromName, m_IRC);
        } else if (m_DiscordAPI) {
          m_Aura->m_GameSetup->SetCreator(m_FromName, &m_Aura->m_Discord);
        }

        m_Aura->m_GameSetup->RunHost();
      }
      break;
    }

    //
    // !PUBBY (create public game by other user)
    // !PRIVBY (create private game by other user)
    //

    case HashCode("privby"):
    case HashCode("pubby"): {
      if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      string gameName;
      if (Args.empty() || (gameName = TrimString(Args[1])).empty()) {
        ErrorReply("Usage: " + cmdToken + "pubby <OWNER> , <GAMENAME>");
        ErrorReply("Usage: " + cmdToken + "privby <OWNER> , <GAMENAME>");
        break;
      }

      if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
        break;
      }

      if (!m_Aura->GetNewGameIsInQuotaConservative()) {
        ErrorReply("Already hosting a game.");
        break;
      }

      if (m_SourceRealm && m_Aura->m_GameVersion != m_SourceRealm->GetGameVersion() &&
        find(m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.begin(), m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end(), m_SourceRealm->GetGameVersion()) == m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end()
        && !GetIsSudo()) {
        ErrorReply("Hosting games on v1." + to_string(m_SourceRealm->GetGameVersion()) + " is disabled.");
        break;
      }

      bool IsPrivate = CommandHash == HashCode("privby");

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Args[0], targetName, targetHostName, targetRealm, true)) {
        ErrorReply("Usage: " + cmdToken + "pubby <PLAYERNAME>@<REALM>");
        break;
      }
      m_Aura->m_GameSetup->SetContext(shared_from_this());
      m_Aura->m_GameSetup->SetBaseName(gameName);
      m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
      m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
      m_Aura->m_GameSetup->SetOwner(targetName, targetRealm ? targetRealm : m_SourceRealm);
      m_Aura->m_GameSetup->RunHost();
      break;
    }

    //
    // !REMAKE
    //

    case HashCode("remake"):
    case HashCode("rmk"): {
      if (!m_TargetGame) {
        ErrorReply("No game is selected.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot remake the game.");
        break;
      }

      if (!m_TargetGame->GetGameLoading() && !m_TargetGame->GetGameLoaded()) {
        ErrorReply("This game has not started yet.");
        break;
      }

      if (!m_Aura->GetNewGameIsInQuotaConservative()) {
        ErrorReply("There is already a lobby being hosted.");
        break;
      }

      if (!m_TargetGame->GetIsRemakeable()) {
        ErrorReply("This game cannot be remade.");
        break;
      }
      m_TargetGame->SendAllChat("Please rejoin the remade game <<" + m_TargetGame->GetGameName() + ">>.");
      m_TargetGame->SendEveryoneElseLeftAndDisconnect("was disconnected (admin remade game)");
      m_TargetGame->Remake();
      // TODO: Debug - broadcast seems broken
      break;
    }

    //
    // !START
    //

    case HashCode("start"):
    case HashCode("vs"):
    case HashCode("go"):
    case HashCode("g"):
    case HashCode("s"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_START_GAME)) {
        ErrorReply("You are not allowed to start this game.");
        break;
      }

      uint32_t ConnectionCount = m_TargetGame->GetNumJoinedUsersOrFake();
      if (ConnectionCount == 0) {
        ErrorReply("Not enough players have joined.");
        break;
      }

      bool IsForce = Payload == "force" || Payload == "f";
      if (IsForce && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot forcibly start it.");
        break;
      }

      m_TargetGame->StartCountDown(true, IsForce);
      break;
    }

    //
    // !QUICKSTART
    //

    case HashCode("sn"):
    case HashCode("startn"):
    case HashCode("quickstart"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot quickstart it.");
        break;
      }

      if (m_TargetGame->m_LastPlayerLeaveTicks.has_value() && GetTicks() < m_TargetGame->m_LastPlayerLeaveTicks.value() + 2000) {
        ErrorReply("Someone left the game less than two seconds ago.");
        break;
      }
      m_TargetGame->StartCountDownFast(true);
      break;
    }

    //
    // !FREESTART
    //

    case HashCode("freestart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit the game permissions.");
        break;
      }

      optional<bool> TargetValue;
      if (Payload.empty() || Payload == "on" || Payload == "ON") {
        TargetValue = true;
      } else if (Payload == "off" || Payload == "OFF") {
        TargetValue = false;
      }
      if (!TargetValue.has_value()) {
        ErrorReply("Unrecognized setting [" + Payload + "].");
        break;
      }
      m_TargetGame->m_PublicStart = TargetValue.value();
      if (m_TargetGame->m_PublicStart) {
        SendAll("Anybody may now use the " + cmdToken + "start command.");
      } else {
        SendAll("Only the game owner may now use the " + cmdToken + "start command.");
      }
      break;
    }

    //
    // !AUTOSTART
    // !AS
    //

    case HashCode("addas"):
    case HashCode("as"):
    case HashCode("autostart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (Payload.empty()) {
        m_TargetGame->SendAllAutoStart();
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "autostart <slots> , <minutes>");
        break;
      }

      uint8_t minReadyControllers = static_cast<uint8_t>(Args[0]);
      uint32_t MinMinutes = 0;
      if (Args.size() >= 2) {
        MinMinutes = Args[1];
      }

      if (minReadyControllers > m_TargetGame->GetMap()->GetMapNumControllers()) {
        ErrorReply("This map does not allow " + to_string(minReadyControllers) + " players.");
        break;
      }

      if (minReadyControllers <= m_TargetGame->m_ControllersReadyCount) {
        // Misuse protection. Make sure the user understands AI players are added.
        ErrorReply("There are already " + to_string(m_TargetGame->m_ControllersReadyCount) + " players ready. Use " + cmdToken + "start instead.");
        break;
      }

      int64_t time = GetTime();
      int64_t dueTime = time + static_cast<int64_t>(MinMinutes) * 60;
      if (dueTime < time) {
        ErrorReply("Failed to set timed start after " + to_string(MinMinutes) + " minutes.");
        break;
      }
      if (CommandHash != HashCode("addas")) {
        m_TargetGame->m_AutoStartRequirements.clear();
      }
      m_TargetGame->m_AutoStartRequirements.push_back(make_pair(minReadyControllers, dueTime));
      m_TargetGame->SendAllAutoStart();
      break;
    }

    //
    // !CLEARAUTOSTART
    //

    case HashCode("clearas"):
    case HashCode("clearautostart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (m_TargetGame->m_AutoStartRequirements.empty()) {
        ErrorReply("There are no active autostart conditions.");
        break;
      }
      m_TargetGame->m_AutoStartRequirements.clear();
      SendReply("Autostart removed.");
      break;
    }

    //
    // !SWAP (swap slots)
    //

    case HashCode("swap"):
    case HashCode("sw"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      bool onlyDraft = false;
      if (!GetIsSudo()) {
        if ((m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES) && (onlyDraft = m_TargetGame->GetIsDraftMode())) {
          if (!m_GameUser || !m_GameUser->GetIsDraftCaptain()) {
            ErrorReply("Draft mode is enabled. Only draft captains may assign teams.");
            break;
          }
        } else if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>" + HelpMissingComma(Payload));
        break;
      }

      GameUser::CGameUser* userOne = nullptr;
      GameUser::CGameUser* userTwo = nullptr;
      uint8_t slotNumOne = 0xFF;
      uint8_t slotNumTwo = 0xFF;
      if (!RunParsePlayerOrSlot(Args[0], slotNumOne, userOne) || !RunParsePlayerOrSlot(Args[1], slotNumTwo, userTwo)) {
        break;
      }
      if (slotNumOne == slotNumTwo) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>");
        break;
      }

      if (onlyDraft) {

        //
        // Swapping is allowed in these circumstances
        // 1. Both slots belong to own authorized team.
        // 2. The following conditions hold simultaneously:
        //     i. One slot belongs to own authorized team, and the other slot is on a different team.
        //    ii. One slot is controlled by a user, and the other slot is empty. 
        //

        const CGameSlot* slotOne = m_TargetGame->GetSlot(slotNumOne);
        const CGameSlot* slotTwo = m_TargetGame->GetSlot(slotNumTwo);

        if (slotOne->GetSlotStatus() == SLOTSTATUS_CLOSED || slotTwo->GetSlotStatus() == SLOTSTATUS_CLOSED) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
        if (slotOne->GetSlotStatus() == SLOTSTATUS_OPEN && slotTwo->GetSlotStatus() == SLOTSTATUS_OPEN) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }

        // Targetting either
        // - OCCUPIED, OCCUPIED
        // - OPEN, OCCUPIED
        // - OCCUPIED, OPEN

        if (slotOne->GetTeam() != slotTwo->GetTeam()) {
          // Ensure user is already captain of the targetted player, or captain of the team we want to move it to.
          if (!m_GameUser->GetIsDraftCaptainOf(slotOne->GetTeam()) && !m_GameUser->GetIsDraftCaptainOf(slotTwo->GetTeam())) {
            // Attempting to swap two slots of different unauthorized teams.
            ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
            break;
          }
          // One targetted slot is authorized. The other one belongs to another team.
          // Allow snatching or donating the player, but not trading it for another player.
          // Non-players cannot be transferred.
          if ((userOne == nullptr) == (userTwo == nullptr)) {
            ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
            break;
          }
          if (userOne == nullptr) {
            // slotOne is guaranteed to be occupied by a non-user
            // slotTwo is guaranteed to be occupied by a user
            if (slotOne->GetSlotStatus() != SLOTSTATUS_OPEN) {
              ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
              break;
            }
          } else {
            // slotTwo is guaranteed to be occupied by a non-user
            // slotOne is guaranteed to be occupied by a user
            if (slotTwo->GetSlotStatus() != SLOTSTATUS_OPEN) {
              ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
              break;
            }
          }
        } else if (!m_GameUser->GetIsDraftCaptainOf(slotOne->GetTeam())) {
          // Both targetted slots belong to the same team, but not to the authorized team.
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        } else {
          // OK. Attempting to swap two slots within own team.
          // They could do it themselves tbh. But let's think of the afks.
        }
      }

      if (!m_TargetGame->SwapSlots(slotNumOne, slotNumTwo)) {
        ErrorReply("These slots cannot be swapped.");
        break;
      }
      m_TargetGame->ResetLayoutIfNotMatching();
      if ((userOne != nullptr) && (userTwo != nullptr)) {
        SendReply("Swapped " + userOne->GetName() + " with " + userTwo->GetName() + ".");
      } else if (!userOne && !userTwo) {
        SendReply("Swapped slots " + ToDecString(slotNumOne + 1) + " and " + ToDecString(slotNumTwo + 1) + ".");
      } else if (userOne) {
        SendReply("Swapped user [" + userOne->GetName() + "] to slot " + ToDecString(slotNumTwo + 1) + ".");
      } else {
        SendReply("Swapped user [" + userTwo->GetName() + "] to slot " + ToDecString(slotNumOne + 1) + ".");
      }
      break;
    }

    //
    // !UNHOST
    //

    case HashCode("unhost"):
    case HashCode("uh"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        // Intentionally allows !unhost for fake (mirror) lobbies.
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      LogStream(*m_Output, m_TargetGame->GetLogPrefix() + "is over (admin cancelled game) [" + m_FromName + "]");
      SendReply("Aborting " + m_TargetGame->GetStatusDescription());
      m_TargetGame->m_Exiting = true;
      m_TargetGame->StopPlayers("was disconnected (admin cancelled game)");
      break;
    }

    //
    // !DOWNLOAD
    // !DL
    //

    case HashCode("download"):
    case HashCode("dl"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      if (
        m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_NEVER ||
        !m_TargetGame->GetMap()->GetMapFileIsValid() ||
        m_Aura->m_StartedGames.size() >= m_Aura->m_Config.m_MaxStartedGames
      ) {
        if (m_TargetGame->GetMapSiteURL().empty()) {
          ErrorAll("Cannot transfer the map.");
        } else {
          ErrorAll("Cannot transfer the map. Please download it from <" + m_TargetGame->GetMapSiteURL() + ">");
        }
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }

      if (targetPlayer->GetDownloadStarted() || targetPlayer->GetDownloadFinished()) {
        ErrorReply("Player [" + targetPlayer->GetName() + "] is already downloading the map.");
        break;
      }

      const CGameSlot* slot = m_TargetGame->InspectSlot(m_TargetGame->GetSIDFromUID(targetPlayer->GetUID()));
      if (!slot || slot->GetDownloadStatus() == 100) {
        ErrorReply("Map transfer failed unexpectedly.", CHAT_LOG_CONSOLE);
        break;
      }

      SendReply("Map download started for [" + targetPlayer->GetName() + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      m_TargetGame->Send(targetPlayer, GameProtocol::SEND_W3GS_STARTDOWNLOAD(m_TargetGame->GetHostUID()));
      targetPlayer->SetDownloadAllowed(true);
      targetPlayer->SetDownloadStarted(true);
      targetPlayer->SetStartedDownloadingTicks(GetTicks());
      targetPlayer->RemoveKickReason(GameUser::KickReason::MAP_MISSING);
      if (!targetPlayer->GetAnyKicked() && targetPlayer->GetKickQueued()) {
        targetPlayer->ClearKickByTicks();
      }
      break;
    }

    //
    // !DROP
    //

    case HashCode("drop"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!m_TargetGame->GetLagging()) {
        ErrorReply("Nobody is currently lagging.");
        break;
      }

      m_TargetGame->StopLaggers("lagged out (dropped by admin)");
      break;
    }

    //
    // !REFEREE
    //

    case HashCode("referee"): {
      UseImplicitHostedGame();

      // Don't allow during countdown for transparency purposes.
      if (!m_TargetGame || m_TargetGame->GetIsMirror() ||
        (m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded()))
        break;

      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }
      if (!targetPlayer->GetIsObserver()) {
        ErrorReply("[" + targetPlayer->GetName() + "] is not an observer.");
        break;
      }

      if (m_TargetGame->GetMap()->GetMapObservers() != MAPOBS_REFEREES) {
        ErrorReply("This game does not allow referees.");
        break;
      }

      m_TargetGame->SetUsesCustomReferees(true);
      for (auto& otherPlayer: m_TargetGame->m_Users) {
        if (otherPlayer->GetIsObserver())
          otherPlayer->SetPowerObserver(false);
      }
      targetPlayer->SetPowerObserver(true);
      SendAll("Player [" + targetPlayer->GetName() + "] was promoted to referee (Other observers may only use observer chat)");
      break;
    }

    //
    // !MUTE
    //

    case HashCode("mute"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot mute people.");
        break;
      }

      if (m_TargetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games. Use " + cmdToken + "muteall from the game lobby next time.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "mute [PLAYERNAME]");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUser(Payload);
      if (!targetPlayer) {
        break;
      }
      if (!GetIsSudo()) {
        if (targetPlayer == m_GameUser) {
          ErrorReply("Cannot mute yourself.");
          break;
        }
        if (m_SourceRealm && (
          m_SourceRealm->GetIsAdmin(targetPlayer->GetName()) || (
            m_SourceRealm->GetIsModerator(targetPlayer->GetName()) &&
            !CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN)
          )
        )) {
          ErrorReply("User [" + targetPlayer->GetName() + "] is an admin on server [" + m_ServerName + "]");
          break;
        }
      }
      if (targetPlayer->GetMuted()) {
        ErrorReply("User [" + targetPlayer->GetName() + "] is already muted.");
        break;
      }
      targetPlayer->SetMuted(true);
      SendAll("[" + targetPlayer->GetName() + "] was muted by [" + m_FromName + "]");
      break;
    }

    //
    // !MUTEALL
    //

    case HashCode("muteall"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror() || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot disable \"All\" chat channel.");
        break;
      }

      if (m_TargetGame->m_MuteAll) {
        ErrorReply("Global and private chats are already muted.");
        break;
      }

      if (m_TargetGame->GetIsHiddenPlayerNames() && m_TargetGame->GetGameLoaded()) {
        ErrorReply("Chat can only be toggled from the game lobby for incognito mode games.");
        break;
      }

      SendAll("Global and private chats muted (allied chat is unaffected)");
      m_TargetGame->m_MuteAll = true;
      break;
    }

    //
    // !ABORT (abort countdown)
    // !A
    //

    case HashCode("abort"):
    case HashCode("a"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot abort game start.");
        break;
      }

      if (m_TargetGame->GetCountDownStarted()) {
        m_TargetGame->StopCountDown();
        if (m_TargetGame->GetIsAutoStartDue()) {
          m_TargetGame->m_AutoStartRequirements.clear();
          SendAll("Countdown stopped by " + m_FromName + ". Autostart removed.");
        } else {
          SendAll("Countdown stopped by " + m_FromName + ".");
        }
      } else {
        m_TargetGame->m_AutoStartRequirements.clear();
        SendAll("Autostart removed.");
      }
      break;
    }

    //
    // !CHECKNETWORK
    //
    case HashCode("checknetwork"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsStageAcceptingJoins()) {
        ErrorReply("Use this command when you are hosting a game lobby.");
        break;
      }
      uint8_t checkMode = HEALTH_CHECK_ALL | HEALTH_CHECK_VERBOSE;
      if (!m_Aura->m_Net.m_SupportTCPOverIPv6) {
        checkMode &= ~HEALTH_CHECK_PUBLIC_IPV6;
        checkMode &= ~HEALTH_CHECK_LOOPBACK_IPV6;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("Not allowed to check network status.");
        break;
      }
      const bool TargetAllRealms = Payload == "*" || (Payload.empty() && !m_SourceRealm);
      CRealm* targetRealm = nullptr;
      if (!TargetAllRealms) {
        targetRealm = GetTargetRealmOrCurrent(Payload);
        if (!targetRealm) {
          ErrorReply("Usage: " + cmdToken + "checknetwork *");
          ErrorReply("Usage: " + cmdToken + "checknetwork <REALM>");
          break;
        }
      }

      if (!m_Aura->m_Net.QueryHealthCheck(shared_from_this(), checkMode, targetRealm, m_TargetGame)) {
        ErrorReply("Already testing the network.");
        break;
      }

      SendReply("Testing network connectivity...");
      break;
    }

    //
    // !PORTFORWARD
    //

#ifndef DISABLE_MINIUPNP
    case HashCode("portforward"): {
      UseImplicitHostedGame();

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1, 2);
      if (Args.size() == 1) {
        if (Args[0] == 0 || Args[0] > 0xFFFF) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
        Args.push_back(Args[0]);
      } else if (Args.empty()) {
        if (!Payload.empty() || !m_TargetGame) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
        Args.push_back(m_TargetGame->GetHostPort());
        Args.push_back(m_TargetGame->GetHostPort());
      } else {
        if (Args[0] == 0 || Args[0] > 0xFFFF || Args[1] == 0 || Args[1] > 0xFFFF) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
      }

      uint16_t extPort = static_cast<uint16_t>(Args[0]);
      uint16_t intPort = static_cast<uint16_t>(Args[1]);

      SendReply("Trying to forward external port " + to_string(extPort) + " to internal port " + to_string(intPort) + "...");
      uint8_t result = m_Aura->m_Net.RequestUPnP(NET_PROTOCOL_TCP, extPort, intPort, LOG_LEVEL_INFO, true);
      if (result == 0) {
        ErrorReply("Universal Plug and Play is not supported by the host router.");
      } else if (0 != (result & 1)) {
        SendReply("Opened port " + to_string(extPort) + " with Universal Plug and Play");
      } else {
        SendReply("Unknown results. Try " + cmdToken + "checknetwork");
      }
      break;
    }
#endif

    //
    // !CHECKBAN
    //

    case HashCode("checkban"): {
      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to check ban status.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "checkban <PLAYERNAME>@<REALM>");
        break;
      }

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "checkban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      if (targetRealm) {
        targetHostName = targetRealm->GetDataBaseID();
      }

      vector<string> CheckResult;
      for (auto& realm : m_Aura->m_Realms) {
        if (realm->GetIsMirror()) {
          continue;
        }

        CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, realm->GetDataBaseID());
        if (Ban) {
          CheckResult.push_back("[" + realm->GetServer() + "] - banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
          delete Ban;
        }
      }

      {
        CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, string());
        if (Ban) {
          CheckResult.push_back("[LAN/VPN] - banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
          delete Ban;
        }
      }

      if (CheckResult.empty()) {
        SendReply("[" + targetName + "@" + targetHostName + "] is not banned from any server.");
      } else {
        SendReply("[" + targetName + "@" + targetHostName + "] is banned from " + to_string(CheckResult.size()) + " server(s): " + JoinVector(CheckResult, false));
      }
      break;
    }

    //
    // !LISTBANS
    //

    case HashCode("listbans"): {
      if (!CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN)) {
        ErrorReply("Only root admins may list bans.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "listbans <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to list bans in arbitrary realms.");
        break;
      }
      vector<string> bannedUsers = m_Aura->m_DB->ListBans(targetRealm->GetDataBaseID());
      if (bannedUsers.empty()) {
        SendReply("No users are banned on " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Banned: " + JoinVector(bannedUsers, false));
      break;
    }

    //
    // !IMPORT
    //

    case HashCode("import"): {
      if (!CheckPermissions(m_Config->m_ImportPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to import files to the database.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage " + cmdToken + "import [DATA TYPE]. Supported data types are: aliases");
        break;
      }
      string lower = Payload;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "aliases") {
        m_Aura->LoadMapAliases();
        SendReply("Commited map aliases to SQLite.");
      } else {
        ErrorReply("Usage " + cmdToken + "import [DATA TYPE]. Supported data types are: aliases");
      }
      break;
    }

    //
    // !ALIAS
    //

    case HashCode("alias"): {
      if (!CheckPermissions(m_Config->m_AliasPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to update map aliases.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [FILE NAME]");
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [MAP IDENTIFIER]");
        break;
      }
      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [FILE NAME]");
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [MAP IDENTIFIER]");
        break;
      }
      string alias = Args[0];
      transform(begin(alias), end(alias), begin(alias), [](char c) { return static_cast<char>(std::tolower(c)); });
      if (!m_Aura->m_DB->AliasAdd(alias, Args[1])) {
        ErrorReply("Failed to add alias [" + alias + "]");
      } else {
        SendReply("Added [" + alias + "] as alias to [" + Args[1] + "].");
      }
      break;
    }

    //
    // !ALTS
    //

    case HashCode("alts"): {
      if (!CheckPermissions(m_Config->m_AliasPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to check alternate accounts.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>");
        ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>@<REALM>");
        break;
      }
      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, true, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>");
          ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetIP;
      if (m_TargetGame) {
        targetIP = m_TargetGame->GetBannableIP(targetName, targetHostName);
      }
      vector<string> targetIPs = m_Aura->m_DB->GetIPs(
        targetName,
        targetRealm ? targetRealm->GetDataBaseID() : targetHostName
      );
      if (!targetIP.empty() && std::find(targetIPs.begin(), targetIPs.end(), targetIP) == targetIPs.end()) {
        targetIPs.push_back(targetIP);
      }

      set<string> allAlts;
      for (const auto& ip : targetIPs) {
        vector<string> alts = m_Aura->m_DB->GetAlts(ip);
        for (const auto& alt : alts) {
          allAlts.insert(alt);
        }
      }
      if (allAlts.empty()) {
        SendReply("No alternate accounts found.");
      } else {
        vector<string> allAltsVector = vector<string>(allAlts.begin(), allAlts.end());
        SendReply("Alternate accounts: " + JoinVector(allAltsVector, false));
      }
      break;
    }

    case HashCode("ban"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict()) {
        ErrorReply("This command may only be used in a game lobby, and will only affect it. For persistent bans, use " + cmdToken + "pban .");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot ban users.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      // Demand a reason to ensure transparency.
      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      string inputTarget = Args[0];
      string reason = Args[1];

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(inputTarget, targetName, targetHostName, targetRealm, true, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>");
          ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>@<REALM>");
        }
        break;
      }

      string targetIP = m_TargetGame->GetBannableIP(targetName, targetHostName);
      if (targetIP.empty()) {
        string databaseHostName = targetHostName;
        if (targetRealm) databaseHostName = targetRealm->GetDataBaseID();
        targetIP = m_Aura->m_DB->GetLatestIP(targetName, databaseHostName);
      }

      string emptyAddress;
      if (m_TargetGame->GetIsScopeBanned(targetName, targetHostName, emptyAddress)) {
        ErrorReply("[" + targetName + "@" + targetHostName + "] was already banned from this game.");
        break;
      }

      if (!m_TargetGame->AddScopeBan(targetName, targetHostName, targetIP)) {
        ErrorReply("Failed to ban [" + targetName + "@" + targetHostName + "] from joining this game.");
        break;
      }

      GameUser::CGameUser* targetPlayer = m_TargetGame->GetUserFromName(targetName, false);
      if (targetPlayer && targetPlayer->GetRealm(false) == targetRealm) {
        targetPlayer->CloseConnection();
        //targetPlayer->SetDeleteMe(true);
        targetPlayer->SetLeftReason("was banned by [" + m_FromName + "]");
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      }

      SendReply("[" + targetName + "@" + targetHostName + "] banned from joining this game.");
      Print(m_TargetGame->GetLogPrefix() + "[" + targetName + "@" + targetHostName + "|" + targetIP  + "] banned from joining game.");
      break;
    }

    case HashCode("unban"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict()) {
        ErrorReply("This command may only be used in a game lobby, and will only affect it. For persistent bans, use " + cmdToken + "punban .");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unban users.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "unban <PLAYERNAME>@<REALM>");
        break;
      }

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, true, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "unban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string emptyAddress;
      if (!m_TargetGame->GetIsScopeBanned(targetName, targetHostName, emptyAddress)) {
        ErrorReply("[" + targetName + "@" + targetHostName + "] was not banned from this game.");
        break;
      }

      if (!m_TargetGame->RemoveScopeBan(targetName, targetHostName)) {
        ErrorReply("Failed to unban user [" + targetName + "@" + targetHostName + "].");
        break;
      }

      SendReply("User [" + targetName + "@" + targetHostName + "] will now be allowed to join this game.");
      break;
    }

    //
    // !PBAN
    //

    case HashCode("pban"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to persistently ban players.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      string inputTarget = Args[0];
      string reason = Args[1];

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(inputTarget, targetName, targetHostName, targetRealm, true, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>");
          ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>@<REALM>");
        }
        break;
      }

      string authServer = m_ServerName;
      if (m_SourceRealm) {
        authServer = m_SourceRealm->GetDataBaseID();
      }
      if (targetRealm) {
        targetHostName = targetRealm->GetDataBaseID();
      }

      CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, authServer);
      if (Ban != nullptr) {
        ErrorReply("[" + Ban->GetName() + "@" + Ban->GetServer() + "] was already banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
        delete Ban;
        break;
      }

      string ip = m_Aura->m_DB->GetLatestIP(targetName, targetHostName);

      if (m_SourceRealm && authServer == targetHostName) {
        if (m_SourceRealm->GetIsAdmin(targetName) || (m_SourceRealm->GetIsModerator(targetName) &&
          !CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN))
        ) {
          ErrorReply("User [" + targetName + "] is an admin on server [" + m_ServerName + "]");
          break;
        }
      }

      if (!m_Aura->m_DB->BanAdd(targetName, targetHostName, authServer, ip, m_FromName, reason)) {
        ErrorReply("Failed to execute ban.");
        break;
      }

      GameUser::CGameUser* targetPlayer = m_TargetGame->GetUserFromName(targetName, false);
      if (targetPlayer && targetPlayer->GetRealm(false) == targetRealm) {
        targetPlayer->CloseConnection();
        //targetPlayer->SetDeleteMe(true);
        targetPlayer->SetLeftReason("was persistently banned by [" + m_FromName + "]");
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      }

      SendReply("[" + targetName + "@" + targetHostName + "] banned.");
      break;
    }

    //
    // !UNPBAN
    //

    case HashCode("unpban"):
    case HashCode("punban"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to remove persistent bans.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "punban <PLAYERNAME>@<REALM>");
        break;
      }
      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, true, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "punban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      if (targetRealm) {
        targetHostName = targetRealm->GetDataBaseID();
      }
      string authServer = m_ServerName;
      if (m_SourceRealm) {
        authServer = m_SourceRealm->GetDataBaseID();
      }
      CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, authServer);
      if (Ban) {
        delete Ban;
      } else {
        ErrorReply("User [" + targetName + "@" + targetHostName + "] is not banned @" + ToFormattedRealm(m_ServerName) + ".");
        break;
      }

      if (!m_Aura->m_DB->BanRemove(targetName, targetHostName, authServer)) {
        ErrorReply("Failed to unban user [" + targetName + "@" + targetHostName + "].");
        break;
      }

      SendReply("Unbanned user [" + targetName + "@" + targetHostName + "] on @" + ToFormattedRealm(m_ServerName) + ".");
      break;
    }

    //
    // !CLEARHCL
    //

    case HashCode("clearhcl"): {
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (m_TargetGame->m_HCLCommandString.empty()) {
        ErrorReply("There was no game mode set.");
        break;
      }

      m_TargetGame->m_HCLCommandString.clear();
      SendAll("Game mode reset");
      break;
    }

    //
    // !STATUS
    //

    case HashCode("status"): {
      UseImplicitHostedGame();

      string message = "Status: ";

      for (const auto& realm : m_Aura->m_Realms) {
        string statusFragment;
        if (!realm->GetLoggedIn()) {
          statusFragment = "offline";
        } else if (m_TargetGame && (realm->GetGameBroadcast() != m_TargetGame || realm->GetIsGameBroadcastErrored())) {
          statusFragment = "unlisted";
        } else {
          statusFragment = "online";
        }
        message += "[" + realm->GetUniqueDisplayName() + " - " + statusFragment + "] ";
      }

      if (m_Aura->m_IRC.GetIsEnabled()) {
        message += "[" + m_Aura->m_IRC.m_Config.m_HostName + (!m_Aura->m_IRC.m_WaitingToConnect ? " - online]" : " - offline]");
      }

      if (m_Aura->m_Discord.GetIsEnabled()) {
        message += m_Aura->m_Discord.m_Config.m_HostName + (m_Aura->m_Discord.GetIsConnected() ? " [online]" : " [offline]");
      }

      SendReply(message);
      break;
    }

    //
    // !SENDLAN
    //

    case HashCode("sendlan"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsStageAcceptingJoins()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (m_TargetGame->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      optional<bool> targetValue;
      if (Payload.empty() || Payload == "on" || Payload == "ON") {
        targetValue = true;
      } else if (Payload == "off" || Payload == "OFF") {
        targetValue = false;
      }

      if (targetValue.has_value()) {
        // Turn ON/OFF
        m_TargetGame->SetUDPEnabled(targetValue.value());
        if (targetValue) {
          m_TargetGame->SendGameDiscoveryCreate();
          m_TargetGame->SendGameDiscoveryRefresh();
          if (!m_Aura->m_Net.m_UDPMainServerEnabled)
            m_TargetGame->SendGameDiscoveryInfo(); // Since we won't be able to handle incoming GAME_SEARCH packets
        }
        if (m_TargetGame->GetUDPEnabled()) {
          SendReply("This lobby will now be displayed in the Local Area Network game list");
        } else {
          SendReply("This lobby will no longer be displayed in the Local Area Network game list");
        }
      } else {
        string ip;
        uint16_t port;
        if (!SplitIPAddressAndPortOrDefault(Payload, GAME_DEFAULT_UDP_PORT, ip, port) || port == 0) {
          ErrorReply("Usage: " + cmdToken + "sendlan ON/OFF");
          ErrorReply("Usage: " + cmdToken + "sendlan <IP>");
          break;
        }
        optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(ip, ACCEPT_ANY);
        if (!maybeAddress.has_value()) {
          ErrorReply("Usage: " + cmdToken + "sendlan ON/OFF");
          ErrorReply("Usage: " + cmdToken + "sendlan <IP>");
          break;
        }
        SetAddressPort(&(maybeAddress.value()), port);
        sockaddr_storage* address = &(maybeAddress.value());
        if (GetInnerIPVersion(address) == AF_INET6 && !(m_Aura->m_Net.m_SupportUDPOverIPv6 && m_Aura->m_Net.m_SupportTCPOverIPv6)) {
          ErrorReply("IPv6 support hasn't been enabled. Set <net.ipv6.tcp.enabled = yes>, and <net.udp_ipv6.enabled = yes> if you want to enable it.");
          break;
        }
        if (((address->ss_family == AF_INET6 && isSpecialIPv6Address(reinterpret_cast<struct sockaddr_in6*>(address))) ||
          (address->ss_family == AF_INET && isSpecialIPv4Address(reinterpret_cast<struct sockaddr_in*>(address)))) && !GetIsSudo()) {
          ErrorReply("Special IP address rejected. Add it to <net.game_discovery.udp.extra_clients.ip_addresses> or use sudo if you are sure about this.");
          break;
        }
        if (m_TargetGame->m_Config.m_ExtraDiscoveryAddresses.size() >= UDP_DISCOVERY_MAX_EXTRA_ADDRESSES) {
          ErrorReply("Max sendlan addresses reached.");
          break;
        }
        bool alreadySending = false;
        for (auto& existingAddress : m_TargetGame->m_Config.m_ExtraDiscoveryAddresses) {
          if (GetSameAddressesAndPorts(&existingAddress, address)) {
            alreadySending = true;
            break;
          }
        }
        if (alreadySending && m_TargetGame->GetUDPEnabled()) {
          ErrorReply("Already sending game info to " + Payload);
          break;
        }
        if (!m_TargetGame->GetUDPEnabled()) {
          SendReply("This lobby will now be displayed in the Local Area Network game list");
        }
        m_TargetGame->SetUDPEnabled(true);
        if (!alreadySending) {
          m_TargetGame->m_Config.m_ExtraDiscoveryAddresses.push_back(std::move(maybeAddress.value()));
        }
        SendReply("This lobby will be displayed in the Local Area Network game list for IP " + Payload + ". Make sure your peer has done UDP hole-punching.");
      }

      break;
    }

    //
    // !SENDLANINFO
    //

    case HashCode("sendlaninfo"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsStageAcceptingJoins()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (!Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "sendlaninfo");
        ErrorReply("You may want !sendlan <IP> or !sendlan <ON/OFF> instead");
        break;
      }

      if (m_TargetGame->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      m_TargetGame->SetUDPEnabled(true);
      m_TargetGame->SendGameDiscoveryInfo();
      SendReply("Sent game info to peers.");
      break;
    }

    //
    // !OWNER (set game owner)
    //

    case HashCode("owner"): {
      UseImplicitHostedGame();

      if (!m_TargetGame) {
        ErrorReply("No game found.");
        break;
      }

      if ((!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted() || m_TargetGame->m_OwnerLess) && !GetIsSudo()) {
        ErrorReply("Cannot take ownership of this game.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        if (Payload.empty() && m_TargetGame->HasOwnerSet()) {
          SendReply("The owner is [" + m_TargetGame->m_OwnerName + "@" + ToFormattedRealm(m_TargetGame->m_OwnerRealm) + "]");
        }
        // These checks help with troubleshooting.
        if (!m_TargetGame->MatchOwnerName(m_FromName) || !m_GameUser) {
          ErrorReply("You are not allowed to change the owner of this game.");
        } else if (m_ServerName.empty() != m_TargetGame->m_OwnerRealm.empty()) {
          if (m_TargetGame->m_OwnerRealm.empty()) {
            ErrorReply("You must join from LAN/VPN to use your owner permissions.");
          } else {
            ErrorReply("You must join from [" + m_TargetGame->m_OwnerRealm + "] to use your owner permissions.");
          }
        } else if (!m_TargetGame->m_OwnerRealm.empty() && m_GameUser->GetRealm(true) == nullptr) {
          ErrorReply("You have not verified your identity yet.");
        } else {
          ErrorReply("Permissions not granted.");
        }
        break;
      }

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (Payload.empty()) {
        targetName = m_FromName;
        if (m_SourceRealm) {
          targetRealm = m_SourceRealm;
          targetHostName = m_ServerName;
        }
      } else if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, true, true)) {
        if (!targetHostName.empty()) {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
          break;
        }
        ErrorReply("Usage: " + cmdToken + "owner <PLAYERNAME>");
        ErrorReply("Usage: " + cmdToken + "owner <PLAYERNAME>@<REALM>");
        break;
      }

      GameUser::CGameUser* targetPlayer = m_TargetGame->GetUserFromName(targetName, false);
      if (targetPlayer && targetPlayer->GetRealmHostName() != targetHostName) {
        ErrorReply("[" + targetPlayer->GetExtendedName() + "] is not connected from " + targetHostName);
        break;
      }

      if (!targetPlayer && !targetRealm) {
        // Prevent arbitrary LAN players from getting ownership before joining.
        ErrorReply("[" + targetName + "@" + ToFormattedRealm() + "] must join the game first.");
        break;
      }
      if (!targetPlayer && !CheckConfirmation(cmdToken, command, payload, "Player [" + targetName + "] is not in this game lobby. ")) {
        break;
      }
      if ((targetPlayer && targetPlayer != m_GameUser && !targetRealm && !targetPlayer->IsRealmVerified()) &&
        !CheckConfirmation(cmdToken, command, payload, "Player [" + targetName + "] has not been verified by " + targetHostName + ". ")) {
        break;
      }
      const bool alreadyOwner = m_TargetGame->m_OwnerName == targetName && m_TargetGame->m_OwnerRealm == targetHostName;

      if (targetPlayer) {
        targetPlayer->SetWhoisShouldBeSent(true);
        m_TargetGame->SendOwnerCommandsHelp(cmdToken, targetPlayer);
      }

      if (alreadyOwner) {
        SendAll("[" + targetName + "@" + ToFormattedRealm(targetHostName) + "] is already the owner of this game.");
      } else {
        m_TargetGame->SetOwner(targetName, targetHostName);
        SendReply("Setting game owner to [" + targetName + "@" + ToFormattedRealm(targetHostName) + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      }
      break;
    }

    //
    // !UNOWNER (removes a game owner)
    //

    case HashCode("unowner"): {
      UseImplicitHostedGame();

      if (!m_TargetGame) {
        ErrorReply("No game found.");
        break;
      }

      if ((!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted() || m_TargetGame->m_OwnerLess) && !GetIsSudo()) {
        ErrorReply("Cannot take ownership of this game.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not allowed to change the owner of this game.");
        break;
      }

      if (!m_TargetGame->HasOwnerSet()) {
        ErrorReply("This game has no owner.");
        break;
      }

      m_TargetGame->ReleaseOwner();
      SendReply("Owner removed.");
      break;
    }

    //
    // !SAY
    //

    case HashCode("say"): {
      if (m_Aura->m_Realms.empty())
        break;

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("You are not allowed to advertise.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "say <REALM> , <MESSAGE>");
        break;
      }

      string::size_type MessageStart = Payload.find(',');
      string RealmId = TrimString(Payload.substr(0, MessageStart));
      string Message = TrimString(Payload.substr(MessageStart + 1));
      if (Message.empty()) {
        ErrorReply("Usage: " + cmdToken + "say <REALM> , <MESSAGE>");
        break;
      }
      bool IsCommand = Message[0] == '/';
      if (IsCommand && !GetIsSudo()) {
        ErrorReply("You are not allowed to send bnet commands.");
        break;
      }
      if (!IsCommand && m_Aura->GetIsAdvertisingGames()) {
        ErrorReply("Cannot send bnet chat messages while the bot is hosting a game lobby.");
        break;
      }
      transform(begin(RealmId), end(RealmId), begin(RealmId), [](char c) { return static_cast<char>(std::tolower(c)); });

      const bool ToAllRealms = RealmId.length() == 1 && RealmId[0] == '*';
      bool Success = false;

      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == RealmId) {
          if (IsCommand) {
            bnet->QueueCommand(Message)->SetEarlyFeedback("Command sent.");
          } else {
            bnet->QueueChatChannel(Message)->SetEarlyFeedback("Message sent.");
          }
          Success = true;
        }
      }

      if (!Success) {
        ErrorReply("Realm [" + RealmId + "] not found.");
        break;
      }
      break;
    }

    //
    // !ANNOUNCE
    //

    case HashCode("announce"): {
      UseImplicitHostedGame();

      if (m_Aura->m_Realms.empty())
        break;

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict())
        break;

      if (m_TargetGame->m_DisplayMode == GAME_PRIVATE) {
        ErrorReply("This game is private.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot advertise.");
        break;
      }

      string renameTarget;
      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.size() >= 2) {
        renameTarget = TrimString(Args[1]);
        if (renameTarget.length() > m_Aura->m_MaxGameNameSize) {
          ErrorReply("Unable to rename to [" + renameTarget + "]. The game name is too long (the maximum is " + to_string(m_Aura->m_MaxGameNameSize) + " characters)");
          break;
        }
      } else if (m_TargetGame->GetHasPvPGNPlayers()) {
        // PvGPN servers keep references to all games to which any PvPGN users have joined.
        // Therefore, the bot leaving the game isn't enough for it to be considered unhosted.
        // QueueGameUncreate doesn't seem to work, either.
        ErrorReply("Usage: " + cmdToken + "announce <REALM>, <GAME NAME>");
        break;
      }

      bool toAllRealms = Args[0] == "*";
      CRealm* targetRealm = nullptr;
      if (toAllRealms) {
        if (0 != (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
          ErrorReply("Announcing on all realms requires sudo permissions."); // But not really
          break;
        }
      } else {
        targetRealm = GetTargetRealmOrCurrent(Args[0]);
        if (!targetRealm) {
          ErrorReply("Usage: " + cmdToken + "announce <REALM>, <GAME NAME>");
          break;
        }
        if (!m_TargetGame->GetIsSupportedGameVersion(targetRealm->GetGameVersion())) {
          ErrorReply("Crossplay is not enabled. [" + targetRealm->GetCanonicalDisplayName() + "] is running v1." + to_string(targetRealm->GetGameVersion()));
          break;
        }
      }

      m_TargetGame->m_DisplayMode = GAME_PUBLIC;
      if (Args.size() >= 2) {
        m_TargetGame->m_GameName = renameTarget;
      }
      m_TargetGame->m_HostCounter  = m_Aura->NextHostCounter();
      m_TargetGame->UpdateGameDiscovery();
      string earlyFeedback = "Announcement sent.";
      if (toAllRealms) {
        for (auto& bnet : m_Aura->m_Realms) {
          if (!m_TargetGame->GetIsSupportedGameVersion(bnet->GetGameVersion())) continue;
          bnet->ResetGameBroadcastData();
          bnet->QueueGameChatAnnouncement(m_TargetGame, shared_from_this(), true)->SetEarlyFeedback(earlyFeedback);
        }
      } else {
        targetRealm->ResetGameBroadcastData();
        targetRealm->QueueGameChatAnnouncement(m_TargetGame, shared_from_this(), true)->SetEarlyFeedback(earlyFeedback);
      }
      break;
    }

    //
    // !CLOSEALL
    //

    case HashCode("closeall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (m_TargetGame->CloseAllSlots()) {
        // Also sent if there was nobody in the game, and so all slots except one were closed.
        SendReply("Closed all slots.");
      } else {
        ErrorReply("There are no open slots.");
      }
      
      break;
    }

    //
    // !COMP (computer slot)
    //

    case HashCode("bot"):
    case HashCode("comp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        // ignore layout, don't override computers
        if (!m_TargetGame->ComputerNSlots(SLOTCOMP_HARD, m_TargetGame->GetNumComputers() + 1, true, false)) {
          ErrorReply("No slots available.");
        } else {
          SendReply("Insane computer added.");
        }
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "comp <SLOT> , <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }

      uint8_t SID = 0xFF;
      if (!RunParseNonPlayerSlot(Args[0], SID)) {
        ErrorReply("Usage: " + cmdToken + "comp <SLOT> , <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }
      uint8_t skill = SLOTCOMP_HARD;
      if (Args.size() >= 2) {
        skill = ParseComputerSkill(Args[1]);
      }
      if (!m_TargetGame->ComputerSlot(SID, skill, false)) {
        ErrorReply("Cannot add computer on that slot.");
        break;
      }
      m_TargetGame->ResetLayoutIfNotMatching();
      SendReply("Computer slot added.");
      break;
    }

    //
    // !DELETECOMP (remove computer slots)
    //

    case HashCode("deletecomp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (!m_TargetGame->ComputerNSlots(SLOTCOMP_HARD, 0)) {
        ErrorReply("Failed to remove computer slots.");
        break;
      }
      SendReply("Computer slots removed.");
      break;
    }

    //
    // !COLOR (computer colour change)
    //

    case HashCode("color"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12" + HelpMissingComma(Payload));
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12");
        break;
      }

      uint8_t color = ParseColor(Args[1]);
      if (color >= m_TargetGame->GetMap()->GetVersionMaxSlots()) {
        color = ParseSID(Args[1]);

        if (color >= m_TargetGame->GetMap()->GetVersionMaxSlots()) {
          ErrorReply("Color identifier \"" + Args[1] + "\" is not valid.");
          break;
        }
      }

      if (!m_TargetGame->SetSlotColor(SID, color, true)) {
        ErrorReply("Cannot recolor slot #" + to_string(SID + 1) + " to " + GetColorName(color) + ".");
        break;
      }

      SendReply("Color changed to " + GetColorName(color) + ".");
      break;
    }

    //
    // !HANDICAP (handicap change)
    //

    case HashCode("handicap"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100" + HelpMissingComma(Payload));
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      vector<uint32_t> handicap = SplitNumericArgs(Args[1], 1u, 1u);
      if (handicap.empty() || handicap[0] % 10 != 0 || !(50 <= handicap[0] && handicap[0] <= 100)) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        // The WC3 client is incapable of modifying handicap when Fixed Player Settings is enabled.
        // However, the GUI misleads users into thinking that it can be modified.
        // While it's indeed editable for HCL purposes,
        // we don't intend to forcibly allow its edition outside of HCL context.
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      CGameSlot* slot = m_TargetGame->GetSlot(SID);
      if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Args[0] + " is empty.");
        break;
      }

      if (slot->GetHandicap() == static_cast<uint8_t>(handicap[0])) {
        ErrorReply("Handicap is already at " + Args[1] + "%");
        break;
      }
      slot->SetHandicap(static_cast<uint8_t>(handicap[0]));
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (targetPlayer) {
        SendReply("Player [" + targetPlayer->GetName() + "] handicap is now [" + Args[1] + "].");
      } else {
        SendReply("Race updated.");
      }
      break;
    }

    //
    // !RACE (race change)
    //

    case HashCode("comprace"):
    case HashCode("race"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (m_TargetGame->GetGameLoaded()) {
        ErrorReply("Game already started. Did you mean to check " + cmdToken + "races instead?");
        break;
      }

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "race <RACE> - Race is human/orc/undead/elf/random/roll");
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll" + HelpMissingComma(Payload));
        break;
      }
      if (Args.size() == 1) {
        Args.push_back(Args[0]);
        Args[0] = m_FromName;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }

      if ((!m_GameUser || m_GameUser != targetPlayer) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      uint8_t Race = ParseRace(Args[1]);
      if (Race == SLOTRACE_INVALID) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }
      if (Race == SLOTRACE_PICKRANDOM) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distribution(0, 3);
        Race = 1 << distribution(gen);
      }

      if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (m_TargetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
        ErrorReply("This game has Random Races enabled.");
        break;
      }

      CGameSlot* slot = m_TargetGame->GetSlot(SID);
      if (!slot || slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED || slot->GetTeam() == m_TargetGame->GetMap()->GetVersionMaxSlots()) {
        ErrorReply("Slot " + ToDecString(SID + 1) + " is not playable.");
        break;
      }
      if (Race == (slot->GetRace() & Race)) {
        ErrorReply("Slot " + ToDecString(SID + 1) + " is already [" + GetRaceName(Race) + "] race.");
        break;
      }
      slot->SetRace(Race | SLOTRACE_SELECTABLE);
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (targetPlayer) {
        SendReply("Player [" + targetPlayer->GetName() + "] race is now [" + GetRaceName(Race) + "].");
      } else {
        SendReply("Race updated.");
      }
      break;
    }

    //
    // !TEAM (forced team change)
    //

    case HashCode("compteam"):
    case HashCode("team"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      bool onlyDraft = false;
      if (!GetIsSudo()) {
        if ((onlyDraft = m_TargetGame->GetIsDraftMode())) {
          if (!m_GameUser || !m_GameUser->GetIsDraftCaptain()) {
            ErrorReply("Draft mode is enabled. Only draft captains may assign teams.");
            break;
          }
        } else if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
      }

      if (Payload.empty()) {
        if (m_GameUser) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        if (m_GameUser) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        if (m_GameUser) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      uint8_t targetTeam = 0xFF;
      if (Args.size() >= 2) {
        targetTeam = ParseSID(Args[1]);
      } else {      
        if (!m_GameUser) {
          ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
          break;
        }
        const CGameSlot* slot = m_TargetGame->InspectSlot(m_TargetGame->GetSIDFromUID(m_GameUser->GetUID()));
        if (!slot) {
          ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
          break;
        }

        // If there are several admins in the game,
        // let them directly pick their team members with e.g. !team Arthas
        targetTeam = slot->GetTeam();
      }
      if (targetTeam > m_TargetGame->GetMap()->GetVersionMaxSlots() + 1) { // accept 13/25 as observer
        ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }
      if (onlyDraft && !m_GameUser->GetIsDraftCaptainOf(targetTeam)) {
        ErrorReply("You are not the captain of team " + ToDecString(targetTeam + 1));
        break;
      }

      if (m_TargetGame->GetSlot(SID)->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Args[0] + " is empty.");
        break;
      }

      if (targetTeam == m_TargetGame->GetMap()->GetVersionMaxSlots()) {
        if (m_TargetGame->GetMap()->GetMapObservers() != MAPOBS_ALLOWED && m_TargetGame->GetMap()->GetMapObservers() != MAPOBS_REFEREES) {
          ErrorReply("This game does not have observers enabled.");
          break;
        }
        if (m_TargetGame->m_Slots[SID].GetIsComputer()) {
          ErrorReply("Computer slots cannot be moved to observers team.");
          break;
        }
      } else if (targetTeam > m_TargetGame->GetMap()->GetMapNumTeams()) {
        ErrorReply("This map does not allow Team #" + ToDecString(targetTeam + 1) + ".");
        break;
      }

      if (!m_TargetGame->SetSlotTeam(SID, targetTeam, true)) {
        if (targetPlayer) {
          ErrorReply("Cannot transfer [" + targetPlayer->GetName() + "] to team " + ToDecString(targetTeam + 1) + ".");
        } else {
          ErrorReply("Cannot transfer to team " + ToDecString(targetTeam + 1) + ".");
        }
      } else {
        m_TargetGame->ResetLayoutIfNotMatching();
        if (targetPlayer) {
          SendReply("[" + targetPlayer->GetName() + "] is now in team " + ToDecString(targetTeam + 1) + ".");
        } else {
          SendReply("Transferred to team " + ToDecString(targetTeam + 1) + ".");
        }
      }
      break;
    }

    //
    // !OBSERVER (forced change)
    //

    case HashCode("obs"):
    case HashCode("observer"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "observer <PLAYER>");
        break;
      }

      uint8_t SID = 0xFF;
      GameUser::CGameUser* targetPlayer = nullptr;
      if (!RunParsePlayerOrSlot(Payload, SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "observer <PLAYER>");
        break;
      }

      if (!(m_TargetGame->GetMap()->GetMapObservers() == MAPOBS_ALLOWED || m_TargetGame->GetMap()->GetMapObservers() == MAPOBS_REFEREES)) {
        ErrorReply("This lobby does not allow observers.");
        break;
      }
      if (m_TargetGame->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Payload + " is empty.");
        break;
      }
      if (m_TargetGame->m_Slots[SID].GetIsComputer()) {
        ErrorReply("Computer slots cannot be moved to observers team.");
        break;
      }

      if (!m_TargetGame->SetSlotTeam(SID, m_TargetGame->GetMap()->GetVersionMaxSlots(), true)) {
        if (targetPlayer) {
          ErrorReply("Cannot turn [" + targetPlayer->GetName() + "] into an observer.");
        } else {
          ErrorReply("Cannot turn slot #" + to_string(SID + 1) + " into an observer slot.");
        }
      } else {
        m_TargetGame->ResetLayoutIfNotMatching();
        if (targetPlayer) {
          SendReply("[" + targetPlayer->GetName() + "] is now an observer.");
        } else {
          SendReply("Moved to observers team.");
        }
      }
      break;
    }

    //
    // !FILL (fill all open slots with computers)
    //

    case HashCode("compall"):
    case HashCode("fill"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      uint8_t targetSkill = Payload.empty() ? SLOTCOMP_HARD : ParseComputerSkill(Payload);
      if (targetSkill == SLOTCOMP_INVALID) {
        ErrorReply("Usage: " + cmdToken + "fill <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }
      if (!m_TargetGame->ComputerAllSlots(targetSkill)) {
        if (m_TargetGame->GetCustomLayout() == CUSTOM_LAYOUT_HUMANS_VS_AI) {
          ErrorReply("No remaining slots available (lobby is set to humans vs AI.");
        } else {
          ErrorReply("No remaining slots available.");
        }
      }
      SendReply("Computers added.");
      break;
    }

    //
    // !DRAFT
    //

    case HashCode("draft"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
        ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, m_TargetGame->GetMap()->GetMapNumTeams());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
        ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
        break;
      }

      if (Args.size() == 1) {
        optional<bool> targetValue;
        if (!ParseBoolean(Payload, targetValue)) {
          ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
          ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
          break;
        }
        if (targetValue.value_or(true)) {
          if (m_TargetGame->GetIsDraftMode()) {
            ErrorReply("Draft mode is already enabled.");
            break;
          }
          m_TargetGame->SetDraftMode(true);

          // Only has effect if observers are allowed.
          m_TargetGame->ResetTeams(false);

          SendReply("Draft mode enabled. Only draft captains may assign teams.");
        } else {
          if (!m_TargetGame->GetIsDraftMode()) {
            ErrorReply("Draft mode is already disabled.");
            break;
          }
          m_TargetGame->ResetDraft();
          m_TargetGame->SetDraftMode(false);
          SendReply("Draft mode disabled. Everyone may choose their own team.");
        }
        break;
      }

      m_TargetGame->ResetDraft();
      vector<string> failPlayers;

      uint8_t team = static_cast<uint8_t>(Args.size());
      while (team--) {
        GameUser::CGameUser* user = GetTargetUser(Args[team]);
        if (user) {
          const uint8_t SID = m_TargetGame->GetSIDFromUID(user->GetUID());
          if (m_TargetGame->SetSlotTeam(SID, team, true) ||
            m_TargetGame->InspectSlot(SID)->GetTeam() == team) {
            user->SetDraftCaptain(team + 1);
          } else {
            failPlayers.push_back(user->GetName());
          }
        } else {
          failPlayers.push_back(Args[team]);
        }
      }

      // Only has effect if observers are allowed.
      m_TargetGame->ResetTeams(false);

      if (failPlayers.empty()) {
        SendReply("Draft captains assigned.");
      } else {
        ErrorReply("Draft mode enabled, but failed to assign captains: " + JoinVector(failPlayers, false));
      }
      break;
    }

    //
    //
    // !FFA
    //

    case HashCode("ffa"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetValue;
      if (!ParseBoolean(Payload, targetValue)) {
        ErrorReply("Usage: " + cmdToken + "ffa <ON|OFF>");
        break;
      }
      if (!targetValue.value_or(true)) {
        m_TargetGame->ResetLayout(true);
        SendReply("FFA mode disabled.");
        break;
      }
      if (m_TargetGame->GetMap()->GetMapNumControllers() <= 2) {
        ErrorReply("This map does not support FFA mode.");
        break;
      }
      if (!m_TargetGame->SetLayoutFFA()) {
        m_TargetGame->ResetLayout(true);
        ErrorReply("Cannot arrange a FFA match.");
      } else {
        SendReply("Game set to free-for-all.");
      }
      break;
    }

    //
    // !VSALL
    //

    case HashCode("pro"):
    case HashCode("lynch"):
    case HashCode("vsall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetValue;
      if (ParseBoolean(Payload, targetValue) && targetValue.value_or(true) == false) {
        // Branching here means that you can't actually set a player named "Disable" against everyone else.
        m_TargetGame->ResetLayout(true);
        SendReply("One-VS-All mode disabled.");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }

      const uint8_t othersCount = m_TargetGame->GetNumPotentialControllers() - 1;
      if (othersCount < 2) {
        ErrorReply("There are too few players in the game.");
        break;
      }

      if (!m_TargetGame->SetLayoutOneVsAll(targetPlayer)) {
        m_TargetGame->ResetLayout(true);
        ErrorReply("Cannot arrange a " + ToDecString(othersCount) + "-VS-1 match.");
        
      } else {
        SendReply("Game set to everyone against " + targetPlayer->GetName());
      }

      break;
    }

    //
    // !TERMINATOR
    //

    case HashCode("vsai"):
    case HashCode("terminator"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetValue;
      if (!ParseBoolean(Payload, targetValue)) {
        vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 1u);
        // Special-case max slots so that if someone careless enough types !terminator 12, it just works.
        if (Args.empty() || Args[0] <= 0 || (Args[0] >= m_TargetGame->GetMap()->GetMapNumControllers() && Args[0] != m_TargetGame->GetMap()->GetVersionMaxSlots())) {
          ErrorReply("Usage: " + cmdToken + "terminator <ON|OFF>");
          ErrorReply("Usage: " + cmdToken + "terminator <NUMBER>");
          break;
        }
        m_TargetGame->ResetLayout(false);
        uint8_t computerCount = static_cast<uint8_t>(Args[0]);
        if (computerCount == m_TargetGame->GetMap()->GetVersionMaxSlots()) --computerCount; // Fix 1v12 into 1v11
        // ignore layout, don't override computers
        if (!m_TargetGame->ComputerNSlots(SLOTCOMP_HARD, computerCount, true, false)) {
          ErrorReply("Not enough open slots for " + ToDecString(computerCount) + " computers.");
          break;
        }
      }
      if (!targetValue.value_or(true)) {
        m_TargetGame->ResetLayout(true);
        SendReply("Humans-VS-AI mode disabled.");
        break;
      }
      const uint8_t computersCount = m_TargetGame->GetNumComputers();
      if (computersCount == 0) {
        ErrorReply("No computer slots found. Use [" + cmdToken + "terminator NUMBER] to play against one or more insane computers.");
        break;
      }
      const uint8_t humansCount = static_cast<uint8_t>(m_TargetGame->GetNumJoinedUsersOrFake());
      pair<uint8_t, uint8_t> matchedTeams;
      if (!m_TargetGame->FindHumanVsAITeams(humansCount, computersCount, matchedTeams)) {
        ErrorReply("Not enough open slots to host " + ToDecString(humansCount) + " humans VS " + ToDecString(computersCount) + " computers game.");
        break;
      }

      if (!m_TargetGame->SetLayoutHumansVsAI(matchedTeams.first, matchedTeams.second)) {
        m_TargetGame->ResetLayout(true);
        ErrorReply("Cannot arrange a " + ToDecString(humansCount) + " humans VS " + ToDecString(computersCount) + " computers match.");
      } else {
        SendReply("Game set to versus AI.");
      }
      break;
    }

    //
    // !TEAMS
    //

    case HashCode("teams"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }
      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }
      optional<bool> targetValue;
      if (!ParseBoolean(Payload, targetValue)) {
        ErrorReply("Usage: " + cmdToken + "teams <ON|OFF>");
        break;
      }
      if (!targetValue.value_or(true)) {
        m_TargetGame->ResetLayout(true);
        // This doesn't have any effect, since
        // both CUSTOM_LAYOUT_COMPACT nor CUSTOM_LAYOUT_ISOPLAYERS are
        // missing from CUSTOM_LAYOUT_LOCKTEAMS mask.
        SendReply("No longer enforcing teams.");
        break;
      }
      if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
        if (m_TargetGame->GetMap()->GetMapNumTeams() == 2) {
          // This is common enough to warrant a special case.
          if (m_TargetGame->SetLayoutTwoTeams()) {
            SendReply("Teams automatically arranged.");
            break;
          }
        }
      } else {
        if (m_TargetGame->SetLayoutCompact()) {
          SendReply("Teams automatically arranged.");
          break;
        }
      }
      m_TargetGame->ResetLayout(true);
      ErrorReply("Failed to automatically assign teams.");
      break;
    }

    //
    // !FAKEPLAYER
    //

    case HashCode("fakeplayer"):
    case HashCode("fp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      string inputLower = Payload;
      transform(begin(inputLower), end(inputLower), begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (inputLower == "fill") {
        m_TargetGame->DeleteVirtualHost();
        m_TargetGame->FakeAllSlots();
        break;
      }

      const bool isToggle = inputLower == "disable" || inputLower == "enable";

      if (!isToggle && !Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "fp");
        ErrorReply("Usage: " + cmdToken + "fp <ON|OFF>");
        break;
      }

      if (!isToggle && m_TargetGame->GetIsRestored()) {
        ErrorReply("Usage: " + cmdToken + "fp <ON|OFF>");
        break;
      }

      m_TargetGame->SetAutoVirtualPlayers(inputLower == "enable");
      if (!isToggle && !m_TargetGame->CreateFakeUser(false)) {
        ErrorReply("Cannot add another virtual user");
      } else if (isToggle) {
        if (m_TargetGame->GetIsAutoVirtualPlayers()) {
          SendReply("Automatic virtual players enabled.");
        } else {
          SendReply("Automatic virtual players disabled.");
        }
      }
      break;
    }

    //
    // !DELETEFP

    case HashCode("deletefake"):
    case HashCode("deletefp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        if (!GetIsSudo() || !m_TargetGame->GetGameLoaded()) {
          ErrorReply("Cannot edit this game's slots.");
          break;
        }
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (m_TargetGame->m_FakeUsers.empty()) {
        ErrorReply("No virtual players found.");
        break;
      }

      if (m_TargetGame->GetIsLobbyStrict()) {
        m_TargetGame->DeleteFakeUsersLobby();
      } else {
        m_TargetGame->DeleteFakeUsersLoaded();
      }
      break;
    }

    //
    // !FILLFAKE
    //

    case HashCode("fillfake"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      bool Success = false;
      while (m_TargetGame->CreateFakeUser(false)) {
        Success = true;
      }

      if (!Success) {
        ErrorReply("Cannot add another virtual user");
      }
      break;
    }

    //
    // !PAUSE
    //

    case HashCode("pause"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if ((!m_GameUser || m_TargetGame->GetNumJoinedPlayers() >= 2) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot pause the game.");
        break;
      }

      if (m_TargetGame->GetPaused()) {
        ErrorReply("Game already paused.");
        break;
      }

      if (m_TargetGame->m_FakeUsers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "pause command. Use the game menu instead.");
        break;
      }

      if (m_TargetGame->GetLagging()) {
        ErrorReply("This command cannot be used while the game is lagging.");
        break;
      }

      if (!m_TargetGame->Pause(m_GameUser, false)) {
        ErrorReply("Max pauses reached.");
        break;
      }

      SendReply("Game paused.");
      break;
    }

    //
    // !SAVE
    //

    case HashCode("autosave"):
    case HashCode("save"):
    {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if ((!m_GameUser || m_TargetGame->GetNumJoinedPlayers() >= 2) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot save the game.");
        break;
      }

      if (m_TargetGame->m_FakeUsers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "save command. Use the game menu instead.");
        break;
      }

      if (Payload.empty()) {
        if (m_TargetGame->GetLagging()) {
          ErrorReply("This command cannot be used while the game is lagging.");
          break;
        }

        if (!m_TargetGame->Save(m_GameUser, false)) {
          ErrorReply("Can only save once per player per game session.");
          break;
        }
        SendReply("Saving game...");
        break;
      }

      string lower = Payload;
      transform(begin(Command), end(Command), begin(Command), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "enable") {
        m_TargetGame->SetSaveOnLeave(SAVE_ON_LEAVE_ALWAYS);
        SendReply("Autosave on disconnections enabled.");
      } else if (lower == "disable") {
        m_TargetGame->SetSaveOnLeave(SAVE_ON_LEAVE_NEVER);
        SendReply("Autosave on disconnections disabled.");
      } else {
        ErrorReply("Usage: " + cmdToken + "save");
        ErrorReply("Usage: " + cmdToken + "save <ON|OFF>");
      }
      break;
    }

    //
    // !RESUME
    //

    case HashCode("resume"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if ((!m_GameUser || m_TargetGame->GetNumJoinedPlayers() >= 2) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot resume the game.");
        break;
      }

      if (!m_TargetGame->GetPaused()) {
        ErrorReply("Game is not paused.");
        break;
      }

      if (m_TargetGame->GetLagging()) {
        ErrorReply("This command cannot be used while the game is lagging.");
        break;
      }

      if (m_TargetGame->m_FakeUsers.empty() || !m_TargetGame->Resume(m_GameUser, false)) {
        ErrorReply("This game does not support the " + cmdToken + "resume command. Use the game menu instead.");
        break;
      }

      SendReply("Resuming game...");
      break;
    }

    //
    // !SP
    //

    case HashCode("sp"):
    case HashCode("shuffle"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_TargetGame->ShuffleSlots();
      SendAll("Players shuffled");
      break;
    }

    //
    // !LOCK
    //

    case HashCode("lock"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot lock the game.");
        break;
      }

      if (m_TargetGame->GetLocked()) {
        ErrorReply("Game already locked.");
        break;
      }

      if (m_TargetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("Game can only be locked from the lobby in incognito mode games.");
        break;
      }

      if (Payload.empty()) {
        if (m_TargetGame->GetCountDownStarted()) {
          SendReply("Game is now locked. Only the game owner may use commands.");
        } else {
          string Warning = m_TargetGame->m_OwnerRealm.empty() ? " (Owner joined over LAN - will get removed if they leave.)" : "";
          SendReply("Game is now locked. Only the game owner may use commands, and edit players' races, teams, etc." + Warning);
        }
        m_TargetGame->m_Locked = true;
      } else {
        GameUser::CGameUser* targetPlayer = RunTargetUser(Payload);
        if (!targetPlayer) {
          break;
        }
        if (targetPlayer->GetIsActionLocked()) {
          ErrorReply("Player [" + targetPlayer->GetDisplayName() + "] was already locked.");
          break;
        }
        targetPlayer->SetActionLocked(true);
        if (m_TargetGame->GetCountDownStarted()) {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  locked. They cannot use commands.");
        } else {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  locked. They cannot use commands, nor choose their race, team, etc.");
        }
      }
      break;
    }

    //
    // !OPENALL
    //

    case HashCode("openall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_TargetGame->OpenAllSlots();
      break;
    }

    //
    // !UNLOCK
    //

    case HashCode("unlock"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner nor a root admin, and therefore cannot unlock the game.");
        break;
      }

      if (!m_TargetGame->GetLocked()) {
        ErrorReply("Game is not locked.");
        break;
      }

      if (m_TargetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("Game can only be unlocked from the lobby in incognito mode games.");
        break;
      }

      if (Payload.empty()) {
        if (m_TargetGame->GetCountDownStarted()) {
          SendReply("Game unlocked. Everyone may now use commands.");
        } else {
          SendReply("Game unlocked. Everyone may now use commands, and choose their races, teams, etc.");
        }

        m_TargetGame->m_Locked = false;
      } else {
        GameUser::CGameUser* targetPlayer = RunTargetUser(Payload);
        if (!targetPlayer) {
          break;
        }
        if (!targetPlayer->GetIsActionLocked()) {
          ErrorReply("Player [" + targetPlayer->GetDisplayName() + "] was not locked.");
          break;
        }
        targetPlayer->SetActionLocked(false);
        if (m_TargetGame->GetCountDownStarted()) {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  unlocked. They may now use commands.");
        } else {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  unlocked. They may now use commands, and choose their race, team, etc.");
        }
      }
      break;
    }

    //
    // !UNMUTE
    //

    case HashCode("unmute"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unmute people.");
        break;
      }

      if (m_TargetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games. Use " + cmdToken + "unmuteall from the game lobby next time.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "unmute <PLAYERNAME>");
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetUser(Payload);
      if (!targetPlayer) {
        break;
      }
      if (!targetPlayer->GetMuted()) {
        // Let this be transparent info.
        // And, more crucially, don't show "cannot unmute yourself" to people who aren't muted.
        ErrorReply("Player [" + targetPlayer->GetName() + "] is not muted.");
        break;
      }
      if (targetPlayer == m_GameUser && !GetIsSudo()) {
        ErrorReply("Cannot unmute yourself.");
        break;
      }

      targetPlayer->SetMuted(false);
      SendAll("[" + targetPlayer->GetName() + "] was unmuted by [" + m_FromName + "]");
      break;
    }

    //
    // !UNMUTEALL
    //

    case HashCode("unmuteall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot enable \"All\" chat channel.");
        break;
      }

      if (m_TargetGame->m_MuteAll) {
        ErrorReply("Global chat is not muted.");
        break;
      }

      if (m_TargetGame->GetIsHiddenPlayerNames() && m_TargetGame->GetGameLoaded()) {
        ErrorReply("Chat can only be toggled from the game lobby for incognito mode games.");
        break;
      }

      SendAll("Global chat unmuted");
      m_TargetGame->m_MuteAll = false;
      break;
    }

    //
    // !VOTECANCEL
    //

    case HashCode("votecancel"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot cancel a votekick.");
        break;
      }

      if (m_TargetGame->m_KickVotePlayer.empty()) {
        ErrorReply("There is no active votekick.");
        break;
      }

      SendReply("A votekick against [" + m_TargetGame->m_KickVotePlayer + "] has been cancelled by [" + m_FromName + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      m_TargetGame->m_KickVotePlayer.clear();
      m_TargetGame->m_StartedKickVoteTime = 0;
      break;
    }

    //
    // !W
    //

    case HashCode("tell"):
    case HashCode("w"): {
      UseImplicitHostedGame();

      if (!CheckPermissions(m_Config->m_TellPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("You are not allowed to send whispers.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string::size_type MessageStart = Payload.find(',');

      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string inputName = TrimString(Payload.substr(0, MessageStart));
      string subMessage = TrimString(Payload.substr(MessageStart + 1));

      string nameFragment, locationFragment;
      void* location = nullptr;
      uint8_t targetType = GetParseTargetServiceUser(inputName, nameFragment, locationFragment, location);
      if (targetType == SERVICE_TYPE_REALM) {
        CRealm* matchingRealm = reinterpret_cast<CRealm*>(location);
        if (inputName == matchingRealm->GetLoginName()) {
          ErrorReply("Cannot PM myself.");
          break;
        }

        // Name of sender and receiver should be included in the message,
        // so that they can be checked in successful whisper acks from the server (BNETProtocol::IncomingChatEvent::WHISPERSENT)
        // Note that the server doesn't provide any way to recognize whisper targets if the whisper fails.
        if (m_ServerName.empty()) {
          m_ActionMessage = inputName + ", " + m_FromName + " tells you: <<" + subMessage + ">>";
        } else {
          m_ActionMessage = inputName + ", " + m_FromName + " at " + m_ServerName + " tells you: <<" + subMessage + ">>";
        }
        matchingRealm->QueueWhisper(m_ActionMessage, inputName, shared_from_this(), true);
      } else if (targetType == SERVICE_TYPE_GAME) {
        CGame* matchingGame = reinterpret_cast<CGame*>(location);
        if (matchingGame->GetGameLoaded() && !GetIsSudo()) {
          ErrorReply("Cannot send messages to a game that has already started.");
          break;
        }
        if (matchingGame->GetIsHiddenPlayerNames() && !GetIsSudo()) {
          ErrorReply("Cannot send messages to an incognito mode game.");
          break;
        }
        GameUser::CGameUser* targetPlayer = nullptr;
        if (matchingGame->GetUserFromNamePartial(inputName, targetPlayer) != 1) {
          ErrorReply("Player [" + inputName + "] not found in <<" + matchingGame->GetGameName() + ">>.");
          break;
        }
        if (m_ServerName.empty()) {
          matchingGame->SendChat(targetPlayer, inputName + ", " + m_FromName + " tells you: <<" + subMessage + ">>");
        } else {
          matchingGame->SendChat(targetPlayer, inputName + ", " + m_FromName + " at " + m_ServerName + " tells you: <<" + subMessage + ">>");
        }
        SendReply("Message sent to " + targetPlayer->GetName() + ".");
      } else {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      break;
    }

    //
    // !WHOIS
    //

    case HashCode("whois"): {
      UseImplicitHostedGame();

      if (!CheckPermissions(m_Config->m_WhoisPermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot ask for /whois.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "whois <PLAYERNAME>@<REALM>");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        break;
      }

      CRealm* targetRealm = targetPlayer->GetRealm(false);
      if (!targetRealm) {
        SendReply("Player [" + targetPlayer->GetName() + "] joined from LAN/VPN.");
        break;
      }

      const string Message = "/whois " + targetPlayer->GetName();
      targetRealm->QueueCommand(Message);
      SendReply("Verification requested for [" + targetPlayer->GetName() + "@" + targetRealm->GetServer() + "]");
      break;
    }

    //
    // !WHOARE
    //

    case HashCode("whoare"): {
      UseImplicitHostedGame();

      if (!CheckPermissions(m_Config->m_WhoarePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot ask for /whoare.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "whoare <PLAYERNAME>@<REALM>");
        break;
      }

      string targetName, targetHostName;
      CRealm* targetRealm = nullptr;
      if (!GetParseTargetRealmUser(Payload, targetName, targetHostName, targetRealm, false, true)) {
        if (!targetHostName.empty() && targetHostName != "*") {
          ErrorReply(targetHostName + " is not a valid PvPGN realm.");
          break;
        }
      }
      if (targetHostName.empty()) {
        targetName = Payload;
      }

      bool queryAllRealms = targetHostName.empty() || targetHostName == "*";
      const string Message = "/whois " + targetName;

      bool success = false;
      for (auto& realm : m_Aura->m_Realms) {
        if (realm->GetIsMirror())
          continue;
        if (queryAllRealms || realm->GetServer() == targetRealm->GetServer()) {
          realm->QueueCommand(Message);
          success = true;
        }
      }

      if (success) {
        SendReply("Query sent.");
      } else {
        ErrorReply("No such realm found.");
      }

      break;
    }

    //
    // !VIRTUALHOST
    //

    case HashCode("virtualhost"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "virtualhost <PLAYERNAME>");
        break;
      }

      string targetName = TrimString(Payload);      
      if (targetName.empty() || targetName.length() > 15) {
        ErrorReply("Usage: " + cmdToken + "virtualhost <PLAYERNAME>");
        break;
      }
      if (m_TargetGame->m_Config.m_LobbyVirtualHostName == targetName) {
        ErrorReply("Virtual host [" + targetName + "] is already in the game.");
        break;
      }
      if (m_TargetGame->GetUserFromName(targetName, false)) {
        ErrorReply("Someone is already using the name [" + targetName + "].");
        break;
      }

      m_TargetGame->m_Config.m_LobbyVirtualHostName = targetName;
      if (m_TargetGame->DeleteVirtualHost()) {
        m_TargetGame->CreateVirtualHost();
      }
      break;
    }

  /*****************
   * SUDOER COMMANDS *
   ******************/

    // !GETCLAN
    case HashCode("getclan"): {
      if (!m_SourceRealm)
        break;

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      m_SourceRealm->SendGetClanList();
      SendReply("Fetching clan member list from " + m_ServerName + "...");
      break;
    }

    // !GETFRIENDS
    case HashCode("getfriends"): {
      if (!m_SourceRealm)
        break;

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      m_SourceRealm->SendGetFriendsList();
      SendReply("Fetching friends list from " + m_ServerName + "...");
      break;
    }

    case HashCode("egame"):
    case HashCode("eg"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      size_t CommandStart = Payload.find(',');
      if (CommandStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "eg <GAMEID> , <COMMAND> - See game ids with !listgames");
        break;
      }
      string GameId = TrimString(Payload.substr(0, CommandStart));
      string ExecString = TrimString(Payload.substr(CommandStart + 1));
      if (GameId.empty() || ExecString.empty()) {
        ErrorReply("Usage: " + cmdToken + "eg <GAMEID> , <COMMAND> - See game ids with !listgames");
        break;
      }
      CGame* targetGame = GetTargetGame(GameId);
      if (!targetGame) {
        ErrorReply("Game [" + GameId + "] not found. - See game ids with !listgames");
        break;
      }
      size_t PayloadStart = ExecString.find(' ');
      const string SubCmd = PayloadStart == string::npos ? ExecString : ExecString.substr(0, PayloadStart);
      const string SubPayload = PayloadStart == string::npos ? string() : ExecString.substr(PayloadStart + 1);
      shared_ptr<CCommandContext> ctx = nullptr;
      try {
        if (m_IRC) {
          ctx = make_shared<CCommandContext>(m_Aura, m_Config, targetGame, m_IRC, m_ChannelName, m_FromName, m_FromWhisper, m_ServerName, m_IsBroadcast, &std::cout);
#ifndef DISABLE_DPP
        } else if (m_DiscordAPI) {
          ctx = make_shared<CCommandContext>(m_Aura, m_Config, targetGame, m_DiscordAPI, &std::cout);
#endif
        } else if (m_SourceRealm) {
          ctx = make_shared<CCommandContext>(m_Aura, m_Config, targetGame, m_SourceRealm, m_FromName, m_FromWhisper, m_IsBroadcast, &std::cout);
        } else {
          ctx = make_shared<CCommandContext>(m_Aura, m_Config, targetGame, m_FromName, m_IsBroadcast, &std::cout);
        }
      } catch (...) {
      }
      // Without permission overrides, sudoer would need to type
      // !eg GAMEID, su COMMAND
      // !eg GAMEID, sudo TOKEN COMMAND
      //
      // Instead, we grant all permissions, since !eg requires sudo anyway.
      if (ctx) {
        ctx->SetPermissions(SET_USER_PERMISSIONS_ALL);
        ctx->UpdatePermissions();
        ctx->Run(cmdToken, SubCmd, SubPayload);
      }
      break;
    }

    case HashCode("cachemaps"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      vector<filesystem::path> AllMaps = FilesMatch(m_Aura->m_Config.m_MapPath, FILE_EXTENSIONS_MAP);
      int goodCounter = 0;
      int badCounter = 0;
      
      for (const auto& fileName : AllMaps) {
        string nameString = PathToString(fileName);
        if (nameString.empty()) continue;
        if (CommandHash != HashCode("synccfg") && m_Aura->m_CFGCacheNamesByMapNames.find(fileName) != m_Aura->m_CFGCacheNamesByMapNames.end()) {
          continue;
        }
        if (nameString.find(Payload) == string::npos) {
          continue;
        }

        CConfig MapCFG;
        MapCFG.Set("map.cfg.partial", "1");
        MapCFG.Set("map.path", R"(Maps\Download\)" + nameString);
        MapCFG.Set("map.local_path", nameString);
        string mapType;
        if (nameString.find("_evrgrn3") != string::npos) {
          mapType = "evergreen";
        } else if (nameString.find("DotA") != string::npos) {
          mapType = "dota";
        }
        if (!mapType.empty()) {
          MapCFG.Set("map.type", mapType);
        }
        if (mapType == "evergreen") {
          MapCFG.Set("map.site", "https://www.hiveworkshop.com/threads/351924/");
          MapCFG.Set("map.short_desc", "This map uses Warcraft 3: Reforged game mechanics.");
        }

        MapCFG.Set("downloaded_by", m_FromName);

        shared_ptr<CMap> parsedMap = nullptr;
        try {
          parsedMap = make_shared<CMap>(m_Aura, &MapCFG);
        } catch (...) {
          Print("[AURA] warning - map [" + nameString + "] is not valid.");
          badCounter++;
          continue;
        }
        const bool isValid = parsedMap->GetValid();
        if (!isValid) {
          Print("[AURA] warning - map [" + nameString + "] is not valid.");
          badCounter++;
          continue;
        }

        string CFGName = "local-" + nameString + ".ini";
        filesystem::path CFGPath = m_Aura->m_Config.m_MapCachePath / filesystem::path(CFGName);

        vector<uint8_t> OutputBytes = MapCFG.Export();
        FileWrite(CFGPath, OutputBytes.data(), OutputBytes.size());
        m_Aura->m_CFGCacheNamesByMapNames[fileName] = CFGName;
        goodCounter++;
      }
      SendReply("Initialized " + to_string(goodCounter) + " map config files. " + to_string(badCounter) + " invalid maps found.");
      return;
    }

    //
    // !COUNTMAPS
    // !COUNTCFGS
    //

    case HashCode("countmaps"):
    case HashCode("countcfgs"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      const auto MapCount = FilesMatch(m_Aura->m_Config.m_MapPath, FILE_EXTENSIONS_MAP).size();
      const auto CFGCount = FilesMatch(m_Aura->m_Config.m_MapCFGPath, FILE_EXTENSIONS_CONFIG).size();

      SendReply(to_string(MapCount) + " maps on disk, " + to_string(CFGCount) + " presets on disk, " + to_string(m_Aura->m_CFGCacheNamesByMapNames.size()) + " preloaded.");
      return;
    }

    //
    // !DELETECFG
    // !DELETEMAP
    //

    case HashCode("deletecfg"):
    case HashCode("deletemap"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      if (Payload.empty())
        return;

      string DeletionType = CommandHash == HashCode("deletecfg") ? "ini" : "map";
      filesystem::path Folder = DeletionType == "ini" ? m_Aura->m_Config.m_MapCFGPath : m_Aura->m_Config.m_MapPath;

      if ((DeletionType == "ini" && !IsValidCFGName(Payload)) || (DeletionType == "map" &&!IsValidMapName(Payload))) {
        ErrorReply("Removal failed");
        break;
      }
      filesystem::path FileFragment = Payload;
      if (FileFragment.is_absolute() || FileFragment != FileFragment.filename()) {
        ErrorReply("Removal failed");
        break;
      }

      string InvalidChars = "/\\\0\"'*?:|<>;,";
      if (FileFragment.string().find_first_of(InvalidChars) != string::npos) {
        ErrorReply("Removal failed");
        break;
      }

      filesystem::path TargetPath = (Folder / FileFragment).lexically_normal();
      if (TargetPath.parent_path() != Folder.parent_path() || PathHasNullBytes(TargetPath) || TargetPath.filename().empty()) {
        ErrorReply("Removal failed");
        break;
      }

      if (!FileDelete(TargetPath)) {
        ErrorReply("Removal failed");
        break;
      }
      SendReply("Deleted [" + Payload + "]");
      break;
    }

    //
    // !SAYGAME
    //

    case HashCode("saygameraw"):
    case HashCode("saygame"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      string::size_type MessageStart = Payload.find(',');
      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "saygame <GAMEID> , <MESSAGE>");
        break;
      }
      bool IsRaw = CommandHash == HashCode("saygameraw");
      string GameId = TrimString(Payload.substr(0, MessageStart));
      string Message = TrimString(Payload.substr(MessageStart + 1));
      if (!IsRaw) Message = "[ADMIN] " + Message;
      if (GameId == "*") {
        bool success = false;
        for (const auto& targetGame : m_Aura->m_Lobbies) {
          success = targetGame->SendAllChat(Message) || success;
        }
        for (const auto& targetGame : m_Aura->m_StartedGames) {
          success = targetGame->SendAllChat(Message) || success;
        }
        if (!success) {
          ErrorReply("No games found.");
          break;
        }
        if (!m_SourceGame) {
          SendReply("Sent chat message to all games.");
        }
      } else {
        CGame* targetGame = GetTargetGame(GameId);
        if (!targetGame) {
          ErrorReply("Game [" + GameId + "] not found.");
          break;
        }
        if (targetGame->GetIsMirror()) {
          ErrorReply("Game [" + GameId + "] is a mirror game.");
          break;
        }
        if (!targetGame->SendAllChat(Message)) {
          ErrorReply("Failed to send chat message to [" + targetGame->GetGameName() + "]");
          break;
        }
        if (targetGame != m_SourceGame) {
          SendReply("Sent chat message to [" + targetGame->GetGameName() + "]");
        }
      }
      break;
    }

    //
    // !DISABLE
    //

    case HashCode("disable"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      SendReply("Creation of new games has been disabled. (Active lobbies will not be unhosted.)");
      m_Aura->m_Config.m_Enabled = false;
      break;
    }

    //
    // !ENABLE
    //

    case HashCode("enable"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      SendReply("Creation of new games has been enabled");
      m_Aura->m_Config.m_Enabled = true;
      break;
    }

    //
    // !DISABLEPUB
    //

    case HashCode("disablepub"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may toggle public game creation.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "disablepub <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      if (!targetRealm->m_Config.m_CommandCFG->m_Enabled) {
        ErrorReply("All commands are already completely disabled in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Creation of new games has been temporarily disabled for non-staff at " + targetRealm->GetCanonicalDisplayName() + ". (Active lobbies will not be unhosted.)");
      targetRealm->m_Config.m_CommandCFG->m_HostPermissions = COMMAND_PERMISSIONS_VERIFIED;
      break;
    }

    //
    // !ENABLEPUB
    //

    case HashCode("enablepub"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may toggle public game creation.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "enablepub <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      if (!targetRealm->m_Config.m_CommandCFG->m_Enabled) {
        ErrorReply("All commands are completely disabled in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Creation of new games has been enabled for non-staff at " + targetRealm->GetCanonicalDisplayName());
      targetRealm->m_Config.m_CommandCFG->m_HostPermissions = COMMAND_PERMISSIONS_VERIFIED;
      break;
    }

    //
    // !MAPTRANSFERS
    //

    case HashCode("setdownloads"):
    case HashCode("maptransfers"): {
      if (Payload.empty()) {
        if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_NEVER) {
          SendReply("Map transfers are disabled");
        } else if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC) {
          SendReply("Map transfers are enabled");
        } else {
          SendReply("Map transfers are set to manual");
        }
        break;
      }

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      optional<int32_t> TargetValue;
      try {
        TargetValue = stol(Payload);
      } catch (...) {
      }

      if (!TargetValue.has_value()) {
        ErrorReply("Usage: " + cmdToken + "maptransfers <MODE>: Mode is 0/1/2.");
        break;
      }

      if (TargetValue.value() != MAP_TRANSFERS_NEVER && TargetValue.value() != MAP_TRANSFERS_AUTOMATIC && TargetValue.value() != MAP_TRANSFERS_MANUAL) {
        ErrorReply("Usage: " + cmdToken + "maptransfers <MODE>: Mode is 0/1/2.");
        break;
      }

      m_Aura->m_Net.m_Config.m_AllowTransfers = static_cast<uint8_t>(TargetValue.value());
      if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_NEVER) {
        SendAll("Map transfers disabled.");
      } else if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC) {
        SendAll("Map transfers enabled.");
      } else {
        SendAll("Map transfers set to manual.");
      }
      break;
    }

    //
    // !RELOAD
    //

    case HashCode("reload"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      if (!m_Aura->QueueConfigReload(shared_from_this())) {
        ErrorReply("Reload already scheduled. See the console output.");
      } else {
        SendReply("Reloading configuration files...");
      }
      return;
    }

    //
    // !EXIT
    // !QUIT
    //

    case HashCode("exit"):
    case HashCode("quit"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      if (m_Aura->GetHasGames()) {
        if (Payload != "force" && Payload != "f") {
          ErrorReply("At least one game is in progress, or a lobby is hosted. Use '"+ cmdToken + "exit force' to shutdown anyway");
          break;
        }
      }
      m_Aura->GracefulExit();
      break;
    }

    //
    // !RESTART
    //

    case HashCode("restart"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      if (m_Aura->GetHasGames()) {
        if (Payload != "force" && Payload != "f") {
          ErrorReply("At least one game is in progress, or a lobby is hosted. Use '" + cmdToken + "restart force' to restart anyway");
          break;
        }
      }

      m_Aura->m_Exiting = true;

      // gRestart is defined in aura.cpp
      extern bool gRestart;
      gRestart = true;
      break;
    }

    /*********************
     * ROOTADMIN COMMANDS *
     *********************/

    //
    // !CHECKSTAFF
    //

    case HashCode("checkadmin"):
    case HashCode("checkstaff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list staff.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "checkstaff <NAME>");
        break;
      }

      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      bool IsRootAdmin = m_SourceRealm->GetIsAdmin(Payload);
      bool IsAdmin = IsRootAdmin || m_SourceRealm->GetIsModerator(Payload);
      if (!IsAdmin && !IsRootAdmin)
        SendReply("User [" + Payload + "] is not staff on server [" + m_ServerName + "]");
      else if (IsRootAdmin)
        SendReply("User [" + Payload + "] is a root admin on server [" + m_ServerName + "]");
      else
        SendReply("User [" + Payload + "] is a moderator on server [" + m_ServerName + "]");

      break;
    }

    //
    // !LISTSTAFF
    //

    case HashCode("liststaff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list staff.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "liststaff <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to list staff in arbitrary realms.");
        break;
      }
      vector<string> admins = vector<string>(targetRealm->m_Config.m_Admins.begin(), targetRealm->m_Config.m_Admins.end());
      vector<string> moderators = m_Aura->m_DB->ListModerators(m_SourceRealm->GetDataBaseID());
      if (admins.empty() && moderators.empty()) {
        ErrorReply("No staff has been designated in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      if (!admins.empty()) SendReply("Root admins: " + JoinVector(admins, false));
      if (!moderators.empty()) SendReply("Moderators: " + JoinVector(moderators, false));
      break;
    }

    //
    // !STAFF
    //

    case HashCode("admin"):
    case HashCode("staff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        if (m_SourceGame && !m_SourceGame->m_OwnerLess) {
          ErrorReply("Only root admins may add staff. Did you mean to acquire control of this game? Use " + cmdToken + "owner");
        } else {
          ErrorReply("Only root admins may add staff.");
        }
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "staff <NAME>");
        break;
      }
      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (m_SourceRealm->GetIsModerator(Payload)) {
        ErrorReply("User [" + Payload + "] is already staff on server [" + m_ServerName + "]");
        break;
      }
      if (!m_Aura->m_DB->ModeratorAdd(m_SourceRealm->GetDataBaseID(), Payload)) {
        ErrorReply("Failed to add user [" + Payload + "] as moderator [" + m_ServerName + "]");
        break;
      }
      SendReply("Added user [" + Payload + "] to the moderator database on server [" + m_ServerName + "]");
      break;
    }

    //
    // !DELSTAFF
    //

    case HashCode("delstaff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may change staff.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "delstaff <NAME>");
        break;
      }

      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (m_SourceRealm->GetIsAdmin(Payload)) {
        ErrorReply("User [" + Payload + "] is a root admin on server [" + m_ServerName + "]");
        break;
      }
      if (!m_SourceRealm->GetIsModerator(Payload)) {
        ErrorReply("User [" + Payload + "] is not staff on server [" + m_ServerName + "]");
        break;
      }
      if (!m_Aura->m_DB->ModeratorRemove(m_SourceRealm->GetDataBaseID(), Payload)) {
        ErrorReply("Error deleting user [" + Payload + "] from the moderator database on server [" + m_ServerName + "]");
        break;
      }
      SendReply("Deleted user [" + Payload + "] from the moderator database on server [" + m_ServerName + "]");
      break;
    }

    /*********************
     * ADMIN COMMANDS *
     *********************/

    //
    // !CHECKGAME (check info of a game, either currently hosted, or stored in the database)
    //
  
    case HashCode("checkgame"): {
      if (Payload.empty()) {
        break;
      }
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions");
        break;
      }

      CGame* targetGame = GetTargetGame(Payload);
      if (!targetGame) {
        targetGame = GetTargetGame("game#" + Payload);
      }
      if (targetGame) {
        string Players = ToNameListSentence(targetGame->GetPlayers());
        string Observers = ToNameListSentence(targetGame->GetObservers());
        if (Players.empty() && Observers.empty()) {
          SendReply("Game#" + to_string(targetGame->GetGameID()) + ". Nobody is in the game.");
          break;
        }
        string PlayersFragment = Players.empty() ? "No players. " : "Players: " + Players + ". ";
        string ObserversFragment = Observers.empty() ? "No observers" : "Observers: " + Observers + ".";
        SendReply("Game#" + to_string(targetGame->GetGameID()) + ". " + PlayersFragment + ObserversFragment);
        break;
      }

      uint64_t gameID = 0;
      try {
        long long value = stoll(Payload);
        gameID = static_cast<uint64_t>(value);
      } catch (const exception& e) {
        ErrorReply("Invalid game identifier.");
        break;
      }
      CDBGameSummary* gameSummary = m_Aura->m_DB->GameCheck(gameID);
      if (!gameSummary) {
        ErrorReply("Game #" + to_string(gameID) + " not found in database.");
        break;
      }
      SendReply("Game players: " + JoinVector(gameSummary->GetPlayerNames(), false));
      SendReply("Slot IDs: " + ByteArrayToDecString(gameSummary->GetSIDs()));
      SendReply("Player IDs: " + ByteArrayToDecString(gameSummary->GetUIDs()));
      SendReply("Colors: " + ByteArrayToDecString(gameSummary->GetColors()));
      break;
    }

    //
    // !SUMODE (enable a su mode session for 10 minutes)
    //

    case HashCode("sumode"): {
      string inputLower = ToLowerCase(Payload);

      bool targetValue = false;
      if (inputLower.empty() || inputLower == "on" || inputLower == "start") {
        targetValue = true;
      } else if (inputLower == "off" || inputLower == "end") {
        targetValue = false;
      } else {
        ErrorReply("Usage: " + cmdToken + "sumode <ON|OFF>");
        break;
      }

      if (!GetIsSudo()) {
        if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
          ErrorReply("Requires sudo permissions.");
        } else if (!m_GameUser) {
          ErrorReply("Requires sudo permissions. Please join a game, and use " + cmdToken + " su sumode to start a superuser session");
        } else {
          ErrorReply("Requires sudo permissions. Please use " + cmdToken + " su sumode to start a superuser session");
        }
        break;
      }

      if (!m_GameUser) {
        ErrorReply("SU mode can only be toggled in a game.");
        break;
      }

      if (targetValue == m_GameUser->CheckSudoMode()) {
        if (targetValue) {
          ErrorReply("SU mode is already ENABLED.");
        } else {
          ErrorReply("SU mode is already DISABLED.");
        }
        break;
      }

      if (targetValue) {
        m_GameUser->SudoModeStart();
        SendReply("Sudo session started. You will have unrestricted access to all commands for 10 minutes.");
        SendReply("Your session will be over as soon as you leave the game.");
        SendReply("WARN: Make sure NOT to enable sudo session over a wireless Internet connection.");
        SendReply("(Prefer using per-command sudo to avoid getting hacked.)");
      } else {
        m_GameUser->SudoModeEnd();
        SendReply("Sudo session ended.");
      }
      break;
    }

    //
    // !LOADCFG (load config file)
    //

    case HashCode("loadcfg"): {
      if (Payload.empty()) {
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          SendReply("There is no map/config file loaded.");
          break;
        }
        SendReply("The currently loaded map/config file is: [" + m_Aura->m_GameSetup->GetInspectName() + "]");
        break;
      }
      if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("Another user is hosting a map.");
        break;
      }
      if (m_Aura->m_ExitingSoon) {
        ErrorReply("Aura is shutting down. No games may be hosted.");
        break;
      }

      if (!FileExists(m_Aura->m_Config.m_MapCFGPath)) {
        ErrorReply("Map config path doesn't exist", CHAT_LOG_CONSOLE);
        break;
      }

      shared_ptr<CGameSetup> gameSetup = nullptr;
      try {
        gameSetup = make_shared<CGameSetup>(m_Aura, shared_from_this(), Payload, SEARCH_TYPE_ONLY_CONFIG, SETUP_PROTECT_ARBITRARY_TRAVERSAL, SETUP_PROTECT_ARBITRARY_TRAVERSAL, true /* lucky mode */);
      } catch (...) {
      }
      if (!gameSetup) {
        ErrorReply("Unable to host game");
        break;
      }
      gameSetup->SetActive();
      gameSetup->LoadMap();
      break;
    }

    //
    // !FLUSHDNS
    //

    case HashCode("flushdns"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      m_Aura->m_Net.FlushDNSCache();
      SendReply("Cleared DNS entries");
      break;
    }

    //
    // !QUERY
    //

    case HashCode("query"): {
      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "query <QUERY>");
        ErrorReply("Usage: " + cmdToken + "query <REALM>, <QUERY>");
        ErrorReply("Valid queries are: listgames, printgames, netinfo, quota");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.empty() || (Args.size() < 2 && !m_SourceRealm)) {
        ErrorReply("Usage: " + cmdToken + "query <QUERY>");
        ErrorReply("Usage: " + cmdToken + "query <REALM>, <QUERY>");
        ErrorReply("Valid queries are: listgames, printgames, netinfo, quota");
        break;
      }

      CRealm* targetRealm = m_SourceRealm;
      if (Args.size() > 1) {
        targetRealm = GetTargetRealmOrCurrent(Args[0]);
      }
      if (!targetRealm) {
        ErrorReply("Realm not found.");
        break;
      }

      string lowerQuery = Args[Args.size() - 1];
      transform(begin(lowerQuery), end(lowerQuery), begin(lowerQuery), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lowerQuery == "netinfo" || lowerQuery == "quota" || lowerQuery == "games") {
        targetRealm->QueueCommand("/" + lowerQuery);
        SendReply("Query sent.");
      } else if (lowerQuery == "printgames") {
        targetRealm->QueueCommand("/games");
        SendReply("Query sent.");
      } else if (lowerQuery == "listgames" || lowerQuery == "gamelist" || lowerQuery == "gameslist") {
        int64_t Time = GetTime();
        if (Time - targetRealm->m_LastGameListTime >= 30) {
          targetRealm->SendGetGamesList();
          SendReply("Query sent.");
        } else {
          ErrorReply("Query ignored (antiflood.)");
        }
      }
      break;
    }

    // !CHANNEL (change channel)
    //

    case HashCode("channel"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to invite the bot to another channel.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "channel <CHANNEL>");
        break;
      }
      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (m_SourceRealm->GetGameBroadcast()) {
        ErrorReply("Cannot join a chat channel while hosting a lobby.");
        break;
      }
      if (!m_SourceRealm->QueueCommand("/join " + Payload)) {
        ErrorReply("Failed to join channel.");
        break;
      }
      break;
    }

    //
    // !LISTGAMES
    //

    case HashCode("listgames"):
    case HashCode("getgames"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to list games.");
        break;
      }
      vector<string> currentGames;
      for (const auto& game : m_Aura->m_Lobbies) {
        currentGames.push_back(string("Lobby#")+ to_string(game->GetGameID()) + ": " + game->GetStatusDescription());
      }
      for (const auto& game : m_Aura->m_StartedGames) {
        currentGames.push_back(string("Game#") + to_string(game->GetGameID()) + ": " + game->GetStatusDescription());
      }
      if (currentGames.empty()) {
        SendReply("No games hosted.");
        break;
      }
      SendReply(JoinVector(currentGames, false), CHAT_LOG_CONSOLE);
      break;
    }

    //
    // !MAP (load map file)
    // !HOST (create game)
    //

    case HashCode("map"):
    case HashCode("load"):
    case HashCode("hostpriv"):
    case HashCode("host"): {
      UseImplicitReplaceable();

      if (!CheckPermissions(m_Config->m_HostPermissions, (
        m_TargetGame && m_TargetGame->GetIsLobbyStrict() && m_TargetGame->GetIsReplaceable() ?
        COMMAND_PERMISSIONS_UNVERIFIED :
        COMMAND_PERMISSIONS_ADMIN
      ))) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      bool isHostPublic = CommandHash == HashCode("host");
      bool isHostPrivate = CommandHash == HashCode("hostpriv");
      bool isHostCommand = isHostPublic || isHostPrivate;
      vector<string> Args = isHostCommand ? SplitArgs(Payload, 1, 6) : SplitArgs(Payload, 1, 5);

      if (Args.empty() || Args[0].empty() || (isHostCommand && Args[Args.size() - 1].empty())) {
        if (isHostCommand) {
          ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <GAME NAME>");
          if (m_GameUser || !m_SourceRealm || m_SourceRealm->GetIsFloodImmune()) {
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <GAME NAME>");
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <VISIBILITY> , <GAME NAME>");
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <VISIBILITY> , <RANDOM RACES> , <GAME NAME>");
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <VISIBILITY> , <RANDOM RACES> , <RANDOM HEROES> , <GAME NAME>");
          }
          break;
        }
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          SendReply("There is no map/config file loaded.", CHAT_SEND_SOURCE_ALL);
          break;
        }
        SendReply("The currently loaded map/config file is: [" + m_Aura->m_GameSetup->GetInspectName() + "]", CHAT_SEND_SOURCE_ALL);
        break;
      }
      if (m_SourceRealm && m_Aura->m_GameVersion != m_SourceRealm->GetGameVersion() &&
        find(m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.begin(), m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end(), m_SourceRealm->GetGameVersion()) == m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end()
        && !GetIsSudo()) {
        ErrorReply("Hosting games on v1." + to_string(m_SourceRealm->GetGameVersion()) + " is disabled.");
        break;
      }
      if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("Another user is hosting a game.");
        break;
      }
      if (m_Aura->m_ExitingSoon) {
        ErrorReply("Aura is shutting down. No games may be hosted.");
        break;
      }

      string gameName;
      if (isHostCommand) {
        if (m_TargetGame && m_SourceGame != m_TargetGame && m_TargetGame->GetIsReplaceable() && m_TargetGame->GetHasAnyUser()) {
          // If there are users in the replaceable lobby (and we are not among them), do not replace it.
          m_TargetGame = nullptr;
        }
        const bool isReplace = m_TargetGame && !m_TargetGame->GetCountDownStarted() && m_TargetGame->GetIsReplaceable() && !m_TargetGame->GetIsBeingReplaced();
        if (!(isReplace ? m_Aura->GetNewGameIsInQuotaReplace() : m_Aura->GetNewGameIsInQuota())) {
          ErrorReply("Other lobbies are already being hosted (maximum is " + to_string(m_Aura->m_Config.m_MaxLobbies) + ").");
          break;
        }

        if (Args.size() >= 2) {
          gameName = Args[Args.size() - 1];
          Args.pop_back();
        } else {
          gameName = m_FromName + "'s " + Args[0];
          if (gameName.length() > m_Aura->m_MaxGameNameSize) {
            gameName = m_FromName + "'s game";
            if (gameName.length() > m_Aura->m_MaxGameNameSize) {
              ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <GAME NAME>");
              break;
            }
          }
        }
      }

      CGameExtraOptions* options = new CGameExtraOptions();
      if (2 <= Args.size()) options->ParseMapObservers(Args[1]);
      if (3 <= Args.size()) options->ParseMapVisibility(Args[2]);
      if (4 <= Args.size()) options->ParseMapRandomRaces(Args[3]);
      if (5 <= Args.size()) options->ParseMapRandomHeroes(Args[4]);

      shared_ptr<CGameSetup> gameSetup = nullptr;
      try {
        gameSetup = make_shared<CGameSetup>(m_Aura, shared_from_this(), Args[0], SEARCH_TYPE_ANY, SETUP_PROTECT_ARBITRARY_TRAVERSAL, SETUP_PROTECT_ARBITRARY_TRAVERSAL, isHostCommand /* lucky mode */);
      } catch (...) {
      }
      if (!gameSetup) {
        delete options;
        ErrorReply("Unable to host game", CHAT_SEND_SOURCE_ALL);
        break;
      }
      if (isHostCommand) {
        gameSetup->SetDisplayMode(isHostPrivate ? GAME_PRIVATE : GAME_PUBLIC);
        gameSetup->SetMapReadyCallback(MAP_ONREADY_HOST, gameName);
      }
      gameSetup->SetMapExtraOptions(options);
      gameSetup->SetActive();
      gameSetup->LoadMap();
      break;
    }

    //
    // !MIRROR (mirror game from another server)
    //

    case HashCode("mirror"): {
      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Not allowed to mirror games.");
        break;
      }

      if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("A map must first be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID, GAMEKEY expected hex.");
        break;
      }

      if (!m_Aura->GetNewGameIsInQuotaConservative()) {
        ErrorReply("Already hosting a game.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 6);

      uint16_t gamePort = 6112;
      uint32_t gameHostCounter = 1;

      CRealm* excludedServer = m_Aura->GetRealmByInputId(Args[0]);

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(Args[1], ACCEPT_IPV4);
      if (!maybeAddress.has_value()) {
        ErrorReply("Not a IPv4 address.");
        break;
      }

      try {
        gamePort = static_cast<uint16_t>(stoul(Args[2]));
        size_t posId;
        gameHostCounter = stoul(Args[3], &posId, 16);
        if (posId != Args[3].length()) {
          ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID expected hex.");
          break;
        }
      } catch (...) {
        ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID expected hex.");
        break;
      }

      string gameName = Args[5];
      SetAddressPort(&(maybeAddress.value()), gamePort);
      m_Aura->m_GameSetup->SetContext(shared_from_this());
      m_Aura->m_GameSetup->SetMirrorSource(maybeAddress.value(), gameHostCounter);
      m_Aura->m_GameSetup->SetBaseName(gameName);
      if (excludedServer) m_Aura->m_GameSetup->AddIgnoredRealm(excludedServer);
      m_Aura->m_GameSetup->RunHost();
      for (auto& realm : m_Aura->m_Realms) {
        if (realm != excludedServer && !realm->GetIsMirror()) {
          realm->ResetConnection(false);
          realm->SetReconnectNextTick(true);
        }
      }
      break;
    }

    case HashCode("nick"): {
      if (!m_IRC) {
        ErrorReply("This is an IRC-exclusive command.");
        break;
      }
      if (0 == (m_Permissions & USER_PERMISSIONS_CHANNEL_ADMIN)) {
        ErrorReply("You are not allowed to change my nickname.");
        break;
      }
      m_IRC->Send("NICK :" + Payload);
      m_IRC->m_NickName = Payload;
      break;
    }

    case HashCode("discord"): {
      if (!m_Aura->m_Discord.m_Client) {
        ErrorReply("Not connected to Discord.");
        break;
      }

      if (m_Aura->m_Discord.m_Config.m_InviteUrl.empty()) {
        ErrorReply("This bot is invite-only. Ask the owner for an invitation link.");
        break;
      }

      switch (m_Aura->m_Discord.m_Config.m_FilterJoinServersMode) {
        case FILTER_DENY_ALL:
          SendReply("Install me to your user (DM-only) at <" + m_Aura->m_Discord.m_Config.m_InviteUrl + ">");
          break;
        case FILTER_ALLOW_LIST:
          SendReply("Install me to your server (requires approval) at <" + m_Aura->m_Discord.m_Config.m_InviteUrl + ">");
          break;
        default:
          SendReply("Install me to your server at <" + m_Aura->m_Discord.m_Config.m_InviteUrl + ">");
      }
      break;
    }

    case HashCode("v"): {
      SendReply("v" + cmdToken);
      break;
    }

    case HashCode("afk"):
    case HashCode("unready"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || !m_GameUser)
        break;

      if (m_TargetGame->GetCountDownStarted()/* && !m_TargetGame->GetCountDownUserInitiated()*/) {
        // Stopping the countdown here MAY be sensible behavior,
        // but only if it's not a manually initiated countdown, and if ...
        break;
      }

      uint8_t readyMode = m_TargetGame->GetPlayersReadyMode();
      bool isAlwaysReadyMode = readyMode == READY_MODE_FAST;
      if (readyMode == READY_MODE_EXPECT_RACE) {
        if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
          isAlwaysReadyMode = true;
        } else if (m_TargetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          isAlwaysReadyMode = true;
        }
      }
      if (isAlwaysReadyMode) {
        ErrorReply("You are always assumed to be ready. Please don't go AFK.");
        break;
      }
      if (m_GameUser->GetIsObserver()) {
        ErrorReply("Observers are always assumed to be ready.");
        break;
      }
      if (!m_GameUser->GetIsReady()) {
        ErrorReply("You are already flagged as not ready.");
        break;
      }
      m_GameUser->SetUserReady(false);
      if (m_GameUser->UpdateReady()) {
         // Should never happen
        ErrorReply("Failed to set not ready.");
        m_GameUser->ClearUserReady();
        break;
      }
      --m_TargetGame->m_ControllersReadyCount;
      ++m_TargetGame->m_ControllersNotReadyCount;
      SendAll("Player [" + m_FromName + "] no longer ready to start the game. When you are, use " + cmdToken + "ready");
      break;
    }

    case HashCode("ready"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict() || !m_GameUser) {
        break;
      }

      if (m_TargetGame->GetCountDownStarted()) {
        break;
      }

      uint8_t readyMode = m_TargetGame->GetPlayersReadyMode();
      bool isAlwaysReadyMode = readyMode == READY_MODE_FAST;
      if (readyMode == READY_MODE_EXPECT_RACE) {
        if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
          isAlwaysReadyMode = true;
        } else if (m_TargetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          isAlwaysReadyMode = true;
        }
      }
      if (isAlwaysReadyMode) {
        ErrorReply("You are always assumed to be ready. Please don't go AFK.");
        break;
      }
      if (m_GameUser->GetIsObserver()) {
        ErrorReply("Observers are always assumed to be ready.");
        break;
      }
      if (m_GameUser->GetIsReady()) {
        ErrorReply("You are already flagged as ready.");
        break;
      }
      if (!m_GameUser->GetMapReady()) {
        ErrorReply("Map not downloaded yet.");
        break;
      }
      m_GameUser->SetUserReady(true);
      if (!m_GameUser->UpdateReady()) {
         // Should never happen
        ErrorReply("Failed to set ready.");
        m_GameUser->ClearUserReady();
        break;
      }
      ++m_TargetGame->m_ControllersReadyCount;
      --m_TargetGame->m_ControllersNotReadyCount;
      SendAll("Player [" + m_FromName + "] ready to start the game. Please don't go AFK.");
      break;
    }

    case HashCode("checkready"):
    case HashCode("askready"):
    case HashCode("readystatus"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobbyStrict()) {
        break;
      }
      if (m_TargetGame->GetCountDownStarted()) {
        break;
      }
      SendReply(m_TargetGame->GetReadyStatusText());

      vector<const GameUser::CGameUser*> unreadyPlayers = m_TargetGame->GetUnreadyPlayers();
      if (!unreadyPlayers.empty()) {
        SendReply("Waiting for: " + ToNameListSentence(unreadyPlayers));
      }
      break;
    }

    case HashCode("pin"): {
      if (!m_GameUser || !m_TargetGame->GetIsLobbyStrict()) {
        break;
      }
      if (m_TargetGame->GetCountDownStarted()) {
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "pin <MESSAGE>");
        break;
      }
      if (Payload.size() > 140) {
        ErrorReply("Message cannot exceed 140 characters.");
        break;
      }
      m_GameUser->SetPinnedMessage(Payload);
      SendReply("Message pinned. It will be shown to every user that joins the game.");
      break;
    }

    case HashCode("unpin"): {
      if (!m_GameUser || !m_TargetGame->GetIsLobbyStrict()) {
        break;
      }
      if (m_TargetGame->GetCountDownStarted()) {
        break;
      }
      
      if (!m_GameUser->GetHasPinnedMessage()) {
        ErrorReply("You don't have a pinned message.");
        break;
      }
      m_GameUser->ClearPinnedMessage();
      SendReply("Pinned message removed.");
      break;
    }

    default: {
      bool hasLetter = false;
      for (const auto& c : command) {
        if (97 <= c && c <= 122) {
          hasLetter = true;
          break;
        }
      }
      if (hasLetter) {
        ErrorReply("Unrecognized command [" + command + "].");
      }
      break;
    }
  }
}


uint8_t CCommandContext::TryDeferred(CAura* nAura, const LazyCommandContext& lazyCtx)
{
  string cmdToken;
  CGame* targetGame = nullptr;
  CCommandConfig* commandCFG = nAura->m_Config.m_LANCommandCFG;
  shared_ptr<CCommandContext> ctx = nullptr;

  void* servicePtr = nullptr;
  uint8_t serviceType = nAura->FindServiceFromHostName(lazyCtx.identityLoc, servicePtr);
  if (serviceType == SERVICE_TYPE_INVALID) {
    Print("[AURA] --exec parsed user at service invalid.");
    return APP_ACTION_ERROR;
  }

  if (!lazyCtx.targetGame.empty()) {
    targetGame = nAura->GetGameByString(lazyCtx.targetGame);
    if (!targetGame) {
      return APP_ACTION_WAIT;
    }
  }

  switch (serviceType) {
    case SERVICE_TYPE_GAME:
    case SERVICE_TYPE_DISCORD:
      Print("[AURA] --exec-as: @service not supported [" + lazyCtx.identityLoc + "]");
      return APP_ACTION_ERROR;
    case SERVICE_TYPE_NONE:
      try {
        if (targetGame) {
          ctx = make_shared<CCommandContext>(
            nAura, commandCFG, targetGame, lazyCtx.identityName, lazyCtx.broadcast, &std::cout
          );
        } else {
          ctx = make_shared<CCommandContext>(
            nAura, lazyCtx.identityName, lazyCtx.broadcast, &std::cout
          );
        }
      } catch (...) {
      }
      break;
    case SERVICE_TYPE_IRC:
      if (!nAura->m_IRC.GetIsEnabled()) return APP_ACTION_ERROR;
      if (nAura->m_IRC.m_Config.m_Channels.empty()) return APP_ACTION_ERROR;
      if (!nAura->m_IRC.GetIsLoggedIn()) return APP_ACTION_WAIT;

      try {
         if (targetGame) {
          ctx = make_shared<CCommandContext>(
            nAura, commandCFG, &nAura->m_IRC,
            nAura->m_IRC.m_Config.m_Channels[0], lazyCtx.identityName,
            false, lazyCtx.identityName + nAura->m_IRC.m_Config.m_VerifiedDomain,
            lazyCtx.broadcast, &std::cout
          );
         } else {
           ctx = make_shared<CCommandContext>(
            nAura, commandCFG, targetGame, &nAura->m_IRC,
            nAura->m_IRC.m_Config.m_Channels[0], lazyCtx.identityName,
            false, lazyCtx.identityName + nAura->m_IRC.m_Config.m_VerifiedDomain,
            lazyCtx.broadcast, &std::cout
          );
         }
      } catch (...) {
      }
      break;
    case SERVICE_TYPE_REALM:
      Print("[AURA] --exec parsed user at service is realm.");
      CRealm* sourceRealm = reinterpret_cast<CRealm*>(servicePtr);
      Print("[AURA] --exec parsed user at service is realm " + sourceRealm->GetCanonicalDisplayName() + ".");
      if (!sourceRealm->GetLoggedIn()) {
        Print("[AURA] --exec realm not logged in yet...");
        return APP_ACTION_WAIT;
      }

      commandCFG = sourceRealm->GetCommandConfig();
      try {
         if (targetGame) {
          ctx = make_shared<CCommandContext>(
            nAura, commandCFG, targetGame, sourceRealm,
            lazyCtx.identityName, true, lazyCtx.broadcast, &std::cout
          );
         } else {
           ctx = make_shared<CCommandContext>(
            nAura, commandCFG, sourceRealm,
            lazyCtx.identityName, true, lazyCtx.broadcast, &std::cout
          );
         }
      } catch (...) {
      }
      break;
  }

  if (!ctx) {
    return APP_ACTION_ERROR;
  }

  switch (lazyCtx.auth) {
    case CommandAuth::kAuto:
      break;
    case CommandAuth::kSpoofed:
      ctx->SetPermissions(0u);
      break;
    case CommandAuth::kVerified:
      ctx->SetPermissions(USER_PERMISSIONS_CHANNEL_VERIFIED);
      break;
    case CommandAuth::kAdmin:
      ctx->SetPermissions(USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_CHANNEL_VERIFIED);
      break;
    case CommandAuth::kRootAdmin:
      ctx->SetPermissions(USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_CHANNEL_VERIFIED);
      break;
    case CommandAuth::kSudo:
      ctx->SetPermissions(SET_USER_PERMISSIONS_ALL);
      break;
  }
  ctx->Run(cmdToken, lazyCtx.command, lazyCtx.payload);
  return APP_ACTION_DONE;
}

CCommandContext::~CCommandContext()
{
  m_Aura = nullptr;
  m_SourceGame = nullptr;
  m_TargetGame = nullptr;
  m_GameUser = nullptr;
  m_SourceRealm = nullptr;
  m_TargetRealm = nullptr;
  m_IRC = nullptr;
#ifndef DISABLE_DPP
  delete m_DiscordAPI;
  m_DiscordAPI = nullptr;
#endif
  m_Output = nullptr;
}
