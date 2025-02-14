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

#include "config/config_discord.h"
#include "discord.h"
#include "command.h"
#include "aura.h"
#include "socket.h"
#include "util.h"
#include "protocol/bnet_protocol.h"
#include "realm.h"
#include "net.h"

#include <utility>
#include <algorithm>
#include <variant>

#define THINKING_PUBLIC false

using namespace std;

//////////////////
//// CDiscord ////
//////////////////

CDiscord::CDiscord(CConfig& nCFG)
  : m_Aura(nullptr),
    m_Client(nullptr),
    m_LastPacketTime(GetTime()),
    m_LastAntiIdleTime(GetTime()),
    m_WaitingToConnect(true),
    m_NickName(string()),
    m_Config(CDiscordConfig(nCFG))
{
}

CDiscord::~CDiscord()
{
  for (const auto& ptr : m_Aura->m_ActiveContexts) {
    auto ctx = ptr.lock();
    if (ctx && ctx->m_DiscordAPI) {
      ctx->m_DiscordAPI = nullptr;
      ctx->SetPartiallyDestroyed();
    }
  }

#ifndef DISABLE_DPP
  // dpp has a tendency to crash on shutdown
  if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
    Print("[DISCORD] shutting down");
  }
  if (m_Client) {
    m_Client->shutdown();
    // Crashes in Debug build only.
    delete m_Client;
    m_Client = nullptr;
  }
#endif
  if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
    // CDiscord deallocation is the last step of CAura deallocation
    Print("[AURA] shutdown finished");
  }
}

#ifndef DISABLE_DPP
bool CDiscord::Init()
{
  m_Client = new dpp::cluster(m_Config.m_Token);
  if (!m_Client) {
    return false;
  }

  m_Client->on_log([](const dpp::log_t& event) {
		if (event.severity > dpp::ll_info) {
      Print("[DISCORD] " + dpp::utility::loglevel(event.severity) + " - " + event.message);
		}
	});
 
  m_Client->on_slashcommand([this](const dpp::slashcommand_t& event) {
    try {
      if (!GetIsServerAllowed(event.command.get_guild().id)) return;
    } catch (...) {
      if (!GetIsUserAllowed(event.command.get_issuing_user().id)) return;
    }
    event.thinking(THINKING_PUBLIC);
    m_CommandQueue.push(new dpp::slashcommand_t(event));
  });

  m_Client->on_ready([this](const dpp::ready_t& event) {
    if (!dpp::run_once<struct register_bot_commands>()) return;
    RegisterCommands();
  });

  m_Client->on_guild_create([this](const dpp::guild_create_t& event) {
    if (!GetIsServerAllowed(event.created->id)) {
      LeaveServer(event.created->id, event.created->name, true);
      return;
    }
    if (m_Aura->MatchLogLevel(LOG_LEVEL_INFO)) {
      Print("[DISCORD] Joined server <<" + event.created->name + ">> (#" + to_string(event.created->id) + ").");
    }
  });

  m_Client->set_websocket_protocol(dpp::websocket_protocol_t::ws_etf);
  try {
    m_Client->start(dpp::st_return);  
  } catch (...) {
    return false;
  }

  return true;
}

void CDiscord::RegisterCommands()
{
  vector<dpp::slashcommand> commands;
  dpp::slashcommand nameSpace(m_Config.m_CommandCFG->m_NameSpace, "Run any of Aura's commands.", m_Client->me.id);
  nameSpace.add_option(
    dpp::command_option(dpp::co_string, "command", "The command to be executed.", true)
  );
  nameSpace.add_option(
    dpp::command_option(dpp::co_string, "payload", "Any comma-separated parameters for the command.", false)
  );
  nameSpace.set_dm_permission(true);
  commands.push_back(nameSpace);

  if (m_Config.m_CommandCFG->m_HostPermissions != COMMAND_PERMISSIONS_DISABLED) {
    dpp::slashcommand hostShortcut("host", "Let Aura host a Warcraft 3 game.", m_Client->me.id);
    hostShortcut.add_option(
      dpp::command_option(dpp::co_string, "map", "Map to be hosted.", true)
    );
    hostShortcut.add_option(
      dpp::command_option(dpp::co_string, "title", "Display title for the hosted game lobby.", true)
    );
    hostShortcut.set_dm_permission(true);
    commands.push_back(hostShortcut);
  }
  m_Client->global_bulk_command_create(commands);
}
#endif

void CDiscord::Update()
{
  if (m_Config.m_Enabled == (m_Client == nullptr)) {
    if (m_Config.m_Enabled) {
#ifndef DISABLE_DPP
      if (!Init()){
        // For example, we ran out of logins today (Discord limits to 1000 logins daily.)
        m_Config.m_Enabled = false;
        return;
      }
#else
      return;
#endif
    } else {
#ifndef DISABLE_DPP
      if (m_Client) {
        m_Client->shutdown();
        delete m_Client;
        m_Client = nullptr;
      }
#endif
    }
  }

#ifndef DISABLE_DPP
  while (!m_CommandQueue.empty()) {
    string cmdToken, command, payload;
    cmdToken = "/" + m_Config.m_CommandCFG->m_NameSpace + " ";
    dpp::slashcommand_t* event = m_CommandQueue.front();
    if (!m_Config.m_Enabled) {
      delete event;
      m_CommandQueue.pop();
      continue;
    }
    if (event->command.get_command_name() == m_Config.m_CommandCFG->m_NameSpace) {
      dpp::command_value maybePayload = event->get_parameter("payload");
      if (holds_alternative<string>(maybePayload)) {
        payload = get<string>(maybePayload);
      }
      command = get<string>(event->get_parameter("command"));
      event->edit_original_response(dpp::message("Command queued!"));
    } else if (event->command.get_command_name() == "host") {
      string mapName = get<string>(event->get_parameter("map"));
      string gameName = get<string>(event->get_parameter("title"));
      command = "host";
      payload = mapName + ", " + gameName;
      event->edit_original_response(dpp::message("Hosting your game briefly!"));
    } else {
      delete event;
      m_CommandQueue.pop();
      continue;
    }
    shared_ptr<CCommandContext> ctx = nullptr;
    try {
      ctx = make_shared<CCommandContext>(m_Aura, m_Aura->m_CommandDefaultConfig, event, &std::cout);
    } catch (...) {
      delete event;
      m_CommandQueue.pop();
      continue;
    }
    ctx->Run(cmdToken, command, payload);
    m_CommandQueue.pop();
  }
#endif
  return;
}

#ifndef DISABLE_DPP
void CDiscord::SendUser(const string& message, const uint64_t target)
{
  m_Client->direct_message_create(target, dpp::message(message), [this](const dpp::confirmation_callback_t& callback){
    if (callback.is_error()) {
#ifdef DEBUG
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
        Print("[DISCORD] Failed to send direct message.");
      }
#endif
      return;
    } else {
#ifdef DEBUG
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
        Print("[DISCORD] Direct message sent OK.");
      }
#endif
    }
  });
}

bool CDiscord::GetIsServerAllowed(const uint64_t target) const
{
  switch (m_Config.m_FilterJoinServersMode) {
  case FILTER_ALLOW_ALL:
    return true;
  case FILTER_DENY_ALL:
    return false;
  case FILTER_ALLOW_LIST:
    return m_Config.m_FilterJoinServersList.find(target) != m_Config.m_FilterJoinServersList.end();
  case FILTER_DENY_LIST:
    return m_Config.m_FilterJoinServersList.find(target) == m_Config.m_FilterJoinServersList.end();
  default:
    return false;
  }
}

bool CDiscord::GetIsUserAllowed(const uint64_t target) const
{
  switch (m_Config.m_FilterInstallUsersMode) {
  case FILTER_ALLOW_ALL:
    return true;
  case FILTER_DENY_ALL:
    return false;
  case FILTER_ALLOW_LIST:
    return m_Config.m_FilterInstallUsersList.find(target) != m_Config.m_FilterInstallUsersList.end();
  case FILTER_DENY_LIST:
    return m_Config.m_FilterInstallUsersList.find(target) == m_Config.m_FilterInstallUsersList.end();
  default:
    return false;
  }
}

void CDiscord::LeaveServer(const uint64_t target, const string& name, const bool isJoining)
{
  m_Client->current_user_leave_guild(target, [this, target, name, isJoining](const dpp::confirmation_callback_t& callback){
    if (m_Aura->MatchLogLevel(LOG_LEVEL_NOTICE)) {
      if (isJoining) {
        Print("[DISCORD] Refused to join server <<" + name + ">> (#" + to_string(target) + ").");
      } else {
        Print("[DISCORD] Left server <<" + name + ">> (#" + to_string(target) + ").");
      }
    }
  });
}
#endif

bool CDiscord::GetIsSudoer(const uint64_t nIdentifier)
{
  return m_Config.m_SudoUsers.find(nIdentifier) != m_Config.m_SudoUsers.end();
}

bool CDiscord::GetIsConnected() const
{
#ifndef DISABLE_DPP
  if (!m_Client) return false;
  const auto& shards = m_Client->get_shards();
  for (const auto& shardEntry : shards) {
    if ((shardEntry.second)->is_connected()) {
      return true;
    }
  }
#endif
  return false;
}

bool CDiscord::MatchHostName(const string& hostName) const
{
#ifndef DISABLE_DPP
  if (hostName == m_Config.m_HostName) return true;
  if (hostName == "users." + m_Config.m_HostName) {
    return true;
  }
#endif
  return false;
}