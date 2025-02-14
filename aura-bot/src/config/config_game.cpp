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

#include "config_game.h"
#include "../util.h"

#include <utility>
#include <algorithm>

#define INHERIT(gameConfigKey) this->gameConfigKey = nRootConfig->gameConfigKey;

#define INHERIT_MAP(gameConfigKey, mapDataKey) \
  if ((nMap->mapDataKey).has_value()) { \
    this->gameConfigKey = (nMap->mapDataKey).value(); \
  } else { \
    this->gameConfigKey = nRootConfig->gameConfigKey; \
  }

#define INHERIT_CUSTOM(gameConfigKey, gameSetupKey) \
  if ((nGameSetup->gameSetupKey).has_value()) { \
    this->gameConfigKey = (nGameSetup->gameSetupKey).value(); \
  } else { \
    this->gameConfigKey = nRootConfig->gameConfigKey; \
  }

#define INHERIT_MAP_OR_CUSTOM(gameConfigKey, mapDataKey, gameSetupKey) \
  if ((nGameSetup->gameSetupKey).has_value()) { \
    this->gameConfigKey = (nGameSetup->gameSetupKey).value(); \
  } else if ((nMap->mapDataKey).has_value()) { \
    this->gameConfigKey = (nMap->mapDataKey).value(); \
  } else { \
    this->gameConfigKey = nRootConfig->gameConfigKey; \
  }

using namespace std;

//
// CGameConfig
//

CGameConfig::CGameConfig(CConfig& CFG)
{
  m_VoteKickPercentage                     = CFG.GetUint8("hosting.vote_kick.min_percent", 70);
  m_NumPlayersToStartGameOver              = CFG.GetUint8("hosting.game_over.player_count", 1);
  m_MaxPlayersLoopback                     = CFG.GetUint8("hosting.ip_filter.max_loopback", 8);
  m_MaxPlayersSameIP                       = CFG.GetUint8("hosting.ip_filter.max_same_ip", 8);
  m_PlayersReadyMode                       = CFG.GetStringIndex("hosting.game_ready.mode", {"fast", "race", "explicit"}, READY_MODE_EXPECT_RACE);
  m_AutoStartRequiresBalance               = CFG.GetBool("hosting.autostart.requires_balance", true);
  m_SaveStats                              = CFG.GetBool("db.game_stats.enabled", true);

  m_SyncLimit                              = CFG.GetUint32("net.start_lag.sync_limit", 32);
  m_SyncLimitSafe                          = CFG.GetUint32("net.stop_lag.sync_limit", 8);
  m_SyncNormalize                          = CFG.GetBool("net.sync_normalization.enabled", true);
  if (m_SyncLimit <= m_SyncLimitSafe) {
    Print("<net.start_lag.sync_limit> must be larger than <net.stop_lag.sync_limit>");
    CFG.SetFailed();
  }

  m_AutoKickPing                           = CFG.GetUint32("hosting.high_ping.kick_ms", 250);
  m_WarnHighPing                           = CFG.GetUint32("hosting.high_ping.warn_ms", 175);
  m_SafeHighPing                           = CFG.GetUint32("hosting.high_ping.safe_ms", 130);

  m_LobbyTimeoutMode                       = CFG.GetStringIndex("hosting.expiry.lobby.mode", {"never", "empty", "ownerless", "strict"}, LOBBY_TIMEOUT_OWNERLESS);
  m_LobbyOwnerTimeoutMode                  = CFG.GetStringIndex("hosting.expiry.owner.mode", {"never", "absent", "strict"}, LOBBY_OWNER_TIMEOUT_ABSENT);
  m_LoadingTimeoutMode                     = CFG.GetStringIndex("hosting.expiry.loading.mode", {"never", "strict"}, GAME_LOADING_TIMEOUT_STRICT);
  m_PlayingTimeoutMode                     = CFG.GetStringIndex("hosting.expiry.playing.mode", {"never", "dry", "strict"}, GAME_PLAYING_TIMEOUT_STRICT);

  m_LobbyTimeout                           = CFG.GetUint32("hosting.expiry.lobby.timeout", 600);
  m_LobbyOwnerTimeout                      = CFG.GetUint32("hosting.expiry.owner.timeout", 120);
  m_LoadingTimeout                         = CFG.GetUint32("hosting.expiry.loading.timeout", 900);
  m_PlayingTimeout                         = CFG.GetUint32("hosting.expiry.playing.timeout", 18000);

  m_PlayingTimeoutWarningShortCountDown    = CFG.GetUint8("hosting.expiry.playing.timeout.soon_warnings", 10);
  m_PlayingTimeoutWarningShortInterval     = CFG.GetUint32("hosting.expiry.playing.timeout.soon_interval", 60);
  m_PlayingTimeoutWarningLargeCountDown    = CFG.GetUint8("hosting.expiry.playing.timeout.eager_warnings", 3);
  m_PlayingTimeoutWarningLargeInterval     = CFG.GetUint32("hosting.expiry.playing.timeout.eager_interval", 1200);

  m_LobbyOwnerReleaseLANLeaver             = CFG.GetBool("hosting.expiry.owner.lan", true);

  m_LobbyCountDownInterval                 = CFG.GetUint32("hosting.game_start.count_down_interval", 500);
  m_LobbyCountDownStartValue               = CFG.GetUint32("hosting.game_start.count_down_ticks", 5);

  m_Latency                                = CFG.GetUint16("bot.latency", 100);
  m_LatencyEqualizerEnabled                = CFG.GetBool("bot.latency.equalizer.enabled", false);
  m_LatencyEqualizerFrames                 = CFG.GetUint8("bot.latency.equalizer.frames", PING_EQUALIZER_DEFAULT_FRAMES);

  m_PerfThreshold                          = CFG.GetUint32("bot.perf_limit", 150);
  m_LacksMapKickDelay                      = CFG.GetUint32("hosting.map.missing.kick_delay", 60); // default: 1 minute
  m_LogDelay                               = CFG.GetUint32("hosting.log_delay", 180); // default: 3 minutes

  m_CheckJoinable                          = CFG.GetBool("monitor.hosting.on_start.check_connectivity", false);
  m_ExtraDiscoveryAddresses                = CFG.GetHostListWithImplicitPort("net.game_discovery.udp.extra_clients.ip_addresses", GAME_DEFAULT_UDP_PORT, ',');
  m_ReconnectionMode                       = RECONNECT_ENABLED_GPROXY_BASIC | RECONNECT_ENABLED_GPROXY_EXTENDED;

  m_PrivateCmdToken                        = CFG.GetString("hosting.commands.trigger", "!");
  if (!m_PrivateCmdToken.empty() && m_PrivateCmdToken[0] == '/') {
    Print("[CONFIG] Error - invalid value provided for <hosting.commands.trigger> - slash (/) is reserved by Battle.net");
    CFG.SetFailed();
  }
  m_BroadcastCmdToken                      = CFG.GetString("hosting.commands.broadcast.trigger");
  if (!m_BroadcastCmdToken.empty() && m_BroadcastCmdToken[0] == '/') {
    Print("[CONFIG] Error - invalid value provided for <hosting.commands.broadcast.trigger> - slash (/) is reserved by Battle.net");
    CFG.SetFailed();
  }
  m_EnableBroadcast                        = CFG.GetBool("hosting.commands.broadcast.enabled", false);

  if (!m_EnableBroadcast)
    m_BroadcastCmdToken.clear();

  m_IndexVirtualHostName                   = CFG.GetString("hosting.index.creator_name", 1, 15, "");
  m_LobbyVirtualHostName                   = CFG.GetString("hosting.self.virtual_player.name", 1, 15, "|cFF4080C0Aura");

  m_NotifyJoins                            = CFG.GetBool("ui.notify_joins.enabled", false);
  m_IgnoredNotifyJoinPlayers               = CFG.GetSet("ui.notify_joins.exceptions", ',', {});
  m_HideLobbyNames                         = CFG.GetBool("hosting.nicknames.hide_lobby", false);
  m_HideInGameNames                        = CFG.GetStringIndex("hosting.nicknames.hide_in_game", {"never", "host", "always", "auto"}, HIDE_IGN_AUTO);
  m_LoadInGame                             = CFG.GetBool("hosting.load_in_game.enabled", false);
  m_EnableJoinObserversInProgress          = CFG.GetBool("hosting.join_in_progress.observers", false);
  m_EnableJoinPlayersInProgress            = CFG.GetBool("hosting.join_in_progress.players", false);

  m_LoggedWords                            = CFG.GetSetInsensitive("hosting.log_words", ',', {});
  m_LogCommands                            = CFG.GetBool("hosting.log_commands", false);
  m_DesyncHandler                          = CFG.GetStringIndex("hosting.desync.handler", {"none", "notify", "drop"}, ON_DESYNC_NOTIFY);
  m_IPFloodHandler                         = CFG.GetStringIndex("hosting.ip_filter.flood_handler", {"none", "notify", "deny"}, ON_IPFLOOD_DENY);
  m_UnsafeNameHandler                      = CFG.GetStringIndex("hosting.name_filter.unsafe_handler", {"none", "censor", "deny"}, ON_UNSAFE_NAME_DENY);
  m_BroadcastErrorHandler                  = CFG.GetStringIndex("hosting.realm_broadcast.error_handler", {"ignore", "exit_main_error", "exit_empty_main_error", "exit_any_error", "exit_empty_any_error", "exit_max_errors"}, ON_ADV_ERROR_EXIT_ON_MAX_ERRORS);
  m_PipeConsideredHarmful                  = CFG.GetBool("hosting.name_filter.is_pipe_harmful", true);
  m_UDPEnabled                             = CFG.GetBool("net.game_discovery.udp.enabled", true);

  set<uint8_t> supportedGameVersions       = CFG.GetUint8Set("hosting.crossplay.versions", ',');
  m_SupportedGameVersions = vector<uint8_t>(supportedGameVersions.begin(), supportedGameVersions.end());
}

CGameConfig::CGameConfig(CGameConfig* nRootConfig, shared_ptr<CMap> nMap, shared_ptr<CGameSetup> nGameSetup)
{
  INHERIT(m_VoteKickPercentage)

  if (m_VoteKickPercentage > 100)
    m_VoteKickPercentage = 100;

  INHERIT_MAP_OR_CUSTOM(m_NumPlayersToStartGameOver, m_NumPlayersToStartGameOver, m_NumPlayersToStartGameOver)
  INHERIT(m_MaxPlayersLoopback)
  INHERIT(m_MaxPlayersSameIP)
  INHERIT_MAP_OR_CUSTOM(m_PlayersReadyMode, m_PlayersReadyMode, m_PlayersReadyMode)
  INHERIT_MAP_OR_CUSTOM(m_AutoStartRequiresBalance, m_AutoStartRequiresBalance, m_AutoStartRequiresBalance)
  INHERIT(m_SaveStats);

  INHERIT_MAP_OR_CUSTOM(m_SyncLimit, m_LatencyMaxFrames, m_LatencyMaxFrames)
  INHERIT_MAP_OR_CUSTOM(m_SyncLimitSafe, m_LatencySafeFrames, m_LatencySafeFrames)
  INHERIT_CUSTOM(m_SyncNormalize, m_SyncNormalize)

  INHERIT_MAP_OR_CUSTOM(m_AutoKickPing, m_AutoKickPing, m_AutoKickPing)
  INHERIT_MAP_OR_CUSTOM(m_WarnHighPing, m_WarnHighPing, m_WarnHighPing)
  INHERIT_MAP_OR_CUSTOM(m_SafeHighPing, m_SafeHighPing, m_SafeHighPing)

  INHERIT_MAP_OR_CUSTOM(m_LobbyTimeoutMode, m_LobbyTimeoutMode, m_LobbyTimeoutMode);
  INHERIT_MAP_OR_CUSTOM(m_LobbyOwnerTimeoutMode, m_LobbyOwnerTimeoutMode, m_LobbyOwnerTimeoutMode);
  INHERIT_MAP_OR_CUSTOM(m_LoadingTimeoutMode, m_LoadingTimeoutMode, m_LoadingTimeoutMode);
  INHERIT_MAP_OR_CUSTOM(m_PlayingTimeoutMode, m_PlayingTimeoutMode, m_PlayingTimeoutMode);

  INHERIT_MAP_OR_CUSTOM(m_LobbyTimeout, m_LobbyTimeout, m_LobbyTimeout)
  INHERIT_MAP_OR_CUSTOM(m_LobbyOwnerTimeout, m_LobbyOwnerTimeout, m_LobbyOwnerTimeout)
  INHERIT_MAP_OR_CUSTOM(m_LoadingTimeout, m_LoadingTimeout, m_LoadingTimeout)
  INHERIT_MAP_OR_CUSTOM(m_PlayingTimeout, m_PlayingTimeout, m_PlayingTimeout)

  m_LobbyTimeout *= 1000;
  m_LobbyOwnerTimeout *= 1000;
  m_LoadingTimeout *= 1000;
  m_PlayingTimeout *= 1000;

  INHERIT_MAP_OR_CUSTOM(m_PlayingTimeoutWarningShortCountDown, m_PlayingTimeoutWarningShortCountDown, m_PlayingTimeoutWarningShortCountDown)
  INHERIT_MAP_OR_CUSTOM(m_PlayingTimeoutWarningShortInterval, m_PlayingTimeoutWarningShortInterval, m_PlayingTimeoutWarningShortInterval);
  INHERIT_MAP_OR_CUSTOM(m_PlayingTimeoutWarningLargeCountDown, m_PlayingTimeoutWarningLargeCountDown, m_PlayingTimeoutWarningLargeCountDown)
  INHERIT_MAP_OR_CUSTOM(m_PlayingTimeoutWarningLargeInterval, m_PlayingTimeoutWarningLargeInterval, m_PlayingTimeoutWarningLargeInterval);

  INHERIT_MAP_OR_CUSTOM(m_LobbyOwnerReleaseLANLeaver, m_LobbyOwnerReleaseLANLeaver, m_LobbyOwnerReleaseLANLeaver);

  INHERIT_MAP_OR_CUSTOM(m_LobbyCountDownInterval, m_LobbyCountDownInterval, m_LobbyCountDownInterval)
  INHERIT_MAP_OR_CUSTOM(m_LobbyCountDownStartValue, m_LobbyCountDownStartValue, m_LobbyCountDownStartValue)

  INHERIT_MAP_OR_CUSTOM(m_Latency, m_Latency, m_LatencyAverage)
  INHERIT_MAP_OR_CUSTOM(m_LatencyEqualizerEnabled, m_LatencyEqualizerEnabled, m_LatencyEqualizerEnabled)
  INHERIT_MAP_OR_CUSTOM(m_LatencyEqualizerFrames, m_LatencyEqualizerFrames, m_LatencyEqualizerFrames)

  if (m_LatencyEqualizerFrames == 0) {
    m_LatencyEqualizerFrames = 1;
  }

  INHERIT(m_PerfThreshold)
  INHERIT(m_LacksMapKickDelay)
  INHERIT(m_LogDelay)

  m_LacksMapKickDelay *= 1000;
  m_LogDelay *= 1000;

  INHERIT_CUSTOM(m_CheckJoinable, m_CheckJoinable)
  INHERIT(m_ExtraDiscoveryAddresses)
  INHERIT_MAP_OR_CUSTOM(m_ReconnectionMode, m_ReconnectionMode, m_ReconnectionMode)

  INHERIT(m_PrivateCmdToken)
  INHERIT(m_BroadcastCmdToken)
  INHERIT(m_EnableBroadcast)

  INHERIT(m_IndexVirtualHostName)
  if (m_IndexVirtualHostName.empty()) {
    m_IndexVirtualHostName = nGameSetup->m_CreatedBy.empty() ? "Aura Bot" : nGameSetup->m_CreatedBy;
  }

  INHERIT(m_LobbyVirtualHostName)

  INHERIT_CUSTOM(m_NotifyJoins, m_NotifyJoins)
  INHERIT(m_IgnoredNotifyJoinPlayers)
  INHERIT_MAP_OR_CUSTOM(m_HideLobbyNames, m_HideLobbyNames, m_HideLobbyNames)
  INHERIT_MAP_OR_CUSTOM(m_HideInGameNames, m_HideInGameNames, m_HideInGameNames)
  INHERIT_MAP_OR_CUSTOM(m_LoadInGame, m_LoadInGame, m_LoadInGame)
  INHERIT_MAP_OR_CUSTOM(m_EnableJoinObserversInProgress, m_EnableJoinObserversInProgress, m_EnableJoinObserversInProgress)
  INHERIT_MAP_OR_CUSTOM(m_EnableJoinPlayersInProgress, m_EnableJoinPlayersInProgress, m_EnableJoinPlayersInProgress)

  INHERIT(m_LoggedWords)
  INHERIT_MAP_OR_CUSTOM(m_LogCommands, m_LogCommands, m_LogCommands)
  INHERIT(m_DesyncHandler)
  INHERIT_MAP_OR_CUSTOM(m_IPFloodHandler, m_IPFloodHandler, m_IPFloodHandler)
  INHERIT_MAP_OR_CUSTOM(m_UnsafeNameHandler, m_UnsafeNameHandler, m_UnsafeNameHandler)
  INHERIT_MAP_OR_CUSTOM(m_BroadcastErrorHandler, m_BroadcastErrorHandler, m_BroadcastErrorHandler)
  INHERIT_MAP(m_PipeConsideredHarmful, m_PipeConsideredHarmful)

  if (nGameSetup->GetIsMirror()) {
    m_UDPEnabled = false;
  } else {
    INHERIT(m_UDPEnabled)
  }

  INHERIT(m_SupportedGameVersions)
  INHERIT(m_VoteKickPercentage)
}

CGameConfig::~CGameConfig() = default;
