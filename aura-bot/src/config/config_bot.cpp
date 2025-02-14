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

#include "config_bot.h"
#include "../util.h"
#include "../net.h"
#include "../file_util.h"
#include "../map.h"
#include "../command.h"

#include <utility>
#include <algorithm>
#include <fstream>

using namespace std;

//
// CBotConfig
//

CBotConfig::CBotConfig(CConfig& CFG)
{
  m_Enabled                      = CFG.GetBool("hosting.enabled", true);
  m_ExtractJASS                  = CFG.GetBool("game.extract_jass.enabled", true);
  m_War3Version                  = CFG.GetMaybeUint8("game.version");
  CFG.FailIfErrorLast();
  m_Warcraft3Path                = CFG.GetMaybeDirectory("game.install_path");

  m_MapPath                      = CFG.GetDirectory("bot.maps_path", CFG.GetHomeDir() / filesystem::path("maps"));
  m_MapCFGPath                   = CFG.GetDirectory("bot.map_configs_path", CFG.GetHomeDir() / filesystem::path("mapcfgs"));
  m_MapCachePath                 = CFG.GetDirectory("bot.map_cache_path", CFG.GetHomeDir() / filesystem::path("mapcache"));
  m_JASSPath                     = CFG.GetDirectory("bot.jass_path", CFG.GetHomeDir() / filesystem::path("jass"));
  m_GameSavePath                 = CFG.GetDirectory("bot.save_path", CFG.GetHomeDir() / filesystem::path("saves"));

  // Non-configurable?
  m_AliasesPath                  = CFG.GetHomeDir() / filesystem::path("aliases.ini");
  m_LogPath                      = CFG.GetHomeDir() / filesystem::path("aura.log");

  m_MinHostCounter               = CFG.GetInt("hosting.namepace.first_game_id", 100) & 0x00FFFFFF;

  m_MaxLobbies                   = CFG.GetInt("hosting.games_quota.max_lobbies", 1);
  m_MaxStartedGames              = CFG.GetInt("hosting.games_quota.max_started", 20);
  m_MaxJoinInProgressGames       = CFG.GetInt("hosting.games_quota.max_join_in_progress", 0);
  m_MaxTotalGames                = CFG.GetInt("hosting.games_quota.max_total", 20);
  m_AutoRehostQuotaConservative  = CFG.GetBool("hosting.games_quota.auto_rehost.conservative", false);

  m_AutomaticallySetGameOwner    = CFG.GetBool("hosting.game_owner.from_creator", true);
  m_EnableDeleteOversizedMaps    = CFG.GetBool("bot.persistence.delete_huge_maps.enabled", false);
  m_MaxSavedMapSize              = CFG.GetInt("bot.persistence.delete_huge_maps.size", 0x6400); // 25 MiB

  optional<filesystem::path> maybeGreeting = CFG.GetMaybePath("bot.greeting_path");
  if (maybeGreeting.has_value() && !maybeGreeting.value().empty()) {
    m_Greeting = ReadChatTemplate(maybeGreeting.value());
  }

  m_StrictSearch                 = CFG.GetBool("bot.load_maps.strict_search", false);
  m_MapSearchShowSuggestions     = CFG.GetBool("bot.load_maps.show_suggestions", true);
  m_EnableCFGCache               = CFG.GetBool("bot.load_maps.cache.enabled", true);
  m_CFGCacheRevalidateAlgorithm  = CFG.GetStringIndex("bot.load_maps.cache.revalidation.algorithm", {"never", "always", "modified"}, CACHE_REVALIDATION_MODIFIED);

  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};

  m_LANCommandCFG = new CCommandConfig(
    CFG, "lan_realm.", false, false,
    CFG.GetStringIndex("lan_realm.commands.common.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("lan_realm.commands.hosting.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("lan_realm.commands.moderator.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("lan_realm.commands.admin.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO),
    CFG.GetStringIndex("lan_realm.commands.bot_owner.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO)
  );

#ifdef DEBUG
  m_LogLevel                     = 1 + CFG.GetStringIndex("bot.log_level", {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", "trace", "trace2", "trace3"}, LOG_LEVEL_INFO - 1);
#else
  m_LogLevel                     = 1 + CFG.GetStringIndex("bot.log_level", {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"}, LOG_LEVEL_INFO - 1);
#endif
  m_ExitOnStandby                = CFG.GetBool("bot.exit_on_standby", false);

  // Master switch mainly intended for CLI. CFG key provided for completeness.
  m_EnableBNET                   = CFG.GetMaybeBool("bot.toggle_every_realm");

  m_SudoKeyWord                  = CFG.GetString("bot.keywords.sudo", "sudo");

  CFG.Accept("db.storage_file");
}

void CBotConfig::Reset()
{
  m_LANCommandCFG = nullptr;
}

CBotConfig::~CBotConfig()
{
  delete m_LANCommandCFG;
}
