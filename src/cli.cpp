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

#include "auradb.h"
#include "cli.h"
#include "config/config_bot.h"
#include "config/config_game.h"
#include "config/config_net.h"
#include "net.h"
#include "file_util.h"
#include "os_util.h"
#include "protocol/game_protocol.h"
#include "game_setup.h"
#include "realm.h"
#include "util.h"

#include "aura.h"

using namespace std;

//
// CCLI
//

CCLI::CCLI()
 : m_UseStandardPaths(false),
   m_InfoAction(0),
   m_Verbose(false),
   m_ExecAuth(CommandAuth::kAuto)
{
}

CCLI::~CCLI() = default;

/*
CLI::Validator CCLI::GetIsFullyQualifiedUserValidator()
{
  return CLI::Validator(
    [](string &input) -> string {
      if (input.find('@') == string::npos) {
        return "Username must contain '@' to specify realm (trailing @ means no realm)";
      }
      return string{}; // Return an empty string for valid input
    },
    "Username must contain '@' to specify realm (trailing @ means no realm)",
    "IsFullyQualifiedUser"
  );
}
*/

CLIResult CCLI::Parse(const int argc, char** argv)
{
  CLI::App app{AURA_APP_NAME};
  //CLI::Validator IsFullyQualifiedUser = GetIsFullyQualifiedUserValidator();
  argv = app.ensure_utf8(argv);

  bool examples = false;
  bool about = false;

  app.option_defaults()->ignore_case();

  app.add_option("MAPFILE", m_SearchTarget, "Map, config or URI from which to host a game from the CLI. File names resolve from maps directory, unless --stdpaths.");
  app.add_option("GAMENAME", m_GameName, "Name assigned to a game hosted from the CLI.");

  app.add_flag("--stdpaths", m_UseStandardPaths, "Makes file names always resolve from CWD when input through the CLI. Commutative.");
#ifdef _WIN32
  app.add_flag("--init-system,--no-init-system{false}", m_InitSystem, "Adds Aura to the PATH environment variable, as well as to Windows Explorer context menu.");
#else
  app.add_flag("--init-system,--no-init-system{false}", m_InitSystem, "Adds Aura to the PATH environment variable.");
#endif
#ifndef DISABLE_MINIUPNP
  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, "Enable automatic port-forwarding, using Universal Plug-and-Play.");
#else
  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, "Enable automatic port-forwarding, using Universal Plug-and-Play. (This distribution of Aura does not support this feature.)");
#endif
  app.add_flag("--lan,--no-lan{false}", m_LAN, "Show hosted games on Local Area Network.");
  app.add_flag("--bnet,--no-bnet{false}", m_BNET, "Switch to enable or disable every defined realm.");
  app.add_flag("--irc,--no-irc{false}", m_IRC, "Switch to enable or disable IRC integration.");
  app.add_flag("--discord,--no-discord{false}", m_Discord, "Switch to enable or disable Discord integration.");
  app.add_flag("--exit,--no-exit{false}", m_ExitOnStandby, "Terminates the process when idle.");
  app.add_flag("--cache,--no-cache{false}", m_UseMapCFGCache, "Caches loaded map files into the map configs folder.");
  app.add_flag("--about,--version", about, "Display software information.");
  app.add_flag("--example,--examples", examples, "Display CLI hosting examples.");
  app.add_flag("--verbose", m_Verbose, "Outputs detailed information when running CLI actions.");
  app.add_flag("--extract-jass,--no-extract-jass{false}", m_ExtractJASS, "Automatically extract files from the game install directory.");

#ifdef _WIN32
  app.add_option("--homedir", m_HomePath, "Customizes Aura home dir (%AURA_HOME%).");
#else
  app.add_option("--homedir", m_HomePath, "Customizes Aura home dir ($AURA_HOME).");
#endif
  app.add_option("--config", m_CFGPath, "Customizes the main Aura config file. File names resolve from home dir, unless --stdpaths.");
  app.add_option("--config-adapter", m_CFGAdapterPath, "Customizes an adapter file for migrating legacy Aura config files. File names resolve from home dir, unless --stdpaths.");
  app.add_option("--w3version", m_War3Version, "Customizes the game version.");
  app.add_option("--w3dir", m_War3Path, "Customizes the game install directory.");
  app.add_option("--mapdir", m_MapPath, "Customizes the maps directory.");
  app.add_option("--cfgdir", m_MapCFGPath, "Customizes the map configs directory.");
  app.add_option("--cachedir", m_MapCachePath, "Customizes the map cache directory.");
  app.add_option("--jassdir", m_JASSPath, "Customizes the directory where extracted JASS files are stored.");
  app.add_option("--savedir", m_GameSavePath, "Customizes the game save directory.");
  app.add_option("-s,--search-type", m_SearchType, "Restricts file searches when hosting from the CLI. Values: map, config, local, any")->check(CLI::IsMember({"map", "config", "local", "any"}))->default_val("any");
  app.add_option("--bind-address", m_BindAddress, "Restricts connections to the game server, only allowing the input IPv4 address.")->check(CLI::ValidIPV4);
  app.add_option("--host-port", m_HostPort, "Customizes the game server to only listen in the specified port.");
  app.add_option("--lan-mode", m_LANMode, "Customizes the behavior of the game discovery service. Values: strict, lax, free.")->check(CLI::IsMember({"strict", "lax", "free"}));
#ifdef DEBUG
  app.add_option("--log-level", m_LogLevel, "Customizes how detailed Aura's output should be. Values: notice, info, debug, trace, trace2, trace3.")->check(CLI::IsMember({"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", "trace", "trace2", "trace3"}));
#else
  app.add_option("--log-level", m_LogLevel, "Customizes how detailed Aura's output should be. Values: notice, info, debug.")->check(CLI::IsMember({"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug"}));
#endif

  // Game hosting
  app.add_option("--owner", m_GameOwner, "Customizes the game owner when hosting from the CLI.")/*->check(IsFullyQualifiedUser)*/;
  app.add_flag("--no-owner", m_GameOwnerLess, "Disables the game owner feature when hosting from the CLI.");
  app.add_flag("--lock-teams,--no-lock-teams{false}", m_GameTeamsLocked, "Toggles 'Lock Teams' setting when hosting from the CLI.");
  app.add_flag("--teams-together,--no-teams-together{false}", m_GameTeamsTogether, "Toggles 'Teams Together' setting when hosting from the CLI.");
  app.add_flag("--share-advanced,--no-share-advanced{false}", m_GameAdvancedSharedUnitControl, "Toggles 'Advanced Shared Unit Control' setting when hosting from the CLI.");
  app.add_flag("--random-races,--no-random-races{false}", m_GameRandomRaces, "Toggles 'Random Races' setting when hosting from the CLI.");
  app.add_flag("--random-heroes,--no-random-heroes{false}", m_GameRandomHeroes, "Toggles 'Random Heroes' when hosting from the CLI.");
  app.add_option("--observers", m_GameObservers, "Customizes observers when hosting from the CLI. Values: no, referees, defeat, full")->check(CLI::IsMember({"no", "referees", "defeat", "full"}));
  app.add_option("--visibility", m_GameVisibility, "Customizes visibility when hosting from the CLI. Values: default, hide, explored, visible")->check(CLI::IsMember({"default", "hide", "explored", "visible"}));
  app.add_option("--speed", m_GameSpeed, "Customizes game speed when hosting from the CLI. Values: slow, normal, fast")->check(CLI::IsMember({"slow", "normal", "fast"}));
  app.add_option("--list-visibility", m_GameDisplayMode, "Customizes whether the game is displayed in any realms. Values: public, private, none")->check(CLI::IsMember({"public", "private", "none"}));
  app.add_option("--on-ipflood", m_GameIPFloodHandler, "Customizes how to deal with excessive game connections from the same IP. Values: none, notify, deny")->check(CLI::IsMember({"none", "notify", "deny"}));
  app.add_option("--on-unsafe-name", m_GameUnsafeNameHandler, "Customizes how to deal with users that try to join with confusing, or otherwise problematic names. Values: none, censor, deny")->check(CLI::IsMember({"none", "censor", "deny"}));
  app.add_option("--on-broadcast-error", m_GameBroadcastErrorHandler, "Customizes the judgment of when to close a game that couldn't be announced in a realm. Values: Values: ignore, exit-main-error, exit-empty-main-error, exit-any-error, exit-empty-any-error, exit-max-errors")->check(CLI::IsMember({"ignore", "exit-main-error", "exit-empty-main-error", "exit-any-error", "exit-empty-any-error", "exit-max-errors"}));
  app.add_option("--alias", m_GameMapAlias, "Registers an alias for the map used when hosting from the CLI.");
  app.add_option("--mirror", m_MirrorSource, "Mirrors a game, listing it in the connected realms. Syntax: IP:PORT#ID.");
  app.add_option("--exclude", m_ExcludedRealms, "Hides the game in the listed realm(s). Repeatable.");

  app.add_option("--lobby-timeout-mode", m_GameLobbyTimeoutMode, "Customizes under which circumstances should a game lobby timeout. Values: never, empty, ownerless, strict")->check(CLI::IsMember({"never", "empty", "ownerless", "strict"}));
  app.add_option("--lobby-owner-timeout-mode", m_GameLobbyOwnerTimeoutMode, "Customizes under which circumstances should game ownership expire. Values: never, absent, strict")->check(CLI::IsMember({"never", "absent", "strict"}));
  app.add_option("--loading-timeout-mode", m_GameLoadingTimeoutMode, "Customizes under which circumstances should players taking too long to load the game be kicked. Values: never, strict")->check(CLI::IsMember({"never", "strict"}));
  app.add_option("--playing-timeout-mode", m_GamePlayingTimeoutMode, "Customizes under which circumstances should a started game expire. Values: never, dry, strict")->check(CLI::IsMember({"never", "dry", "strict"}));

  app.add_option("--lobby-timeout", m_GameLobbyTimeout, "Sets the time limit for the game lobby (seconds.)");
  app.add_option("--lobby-owner-timeout", m_GameLobbyOwnerTimeout, "Sets the time limit for an absent game owner to keep their power (seconds.)");
  app.add_option("--loading-timeout", m_GameLoadingTimeout, "Sets the time limit for players to load a started game (seconds.)");
  app.add_option("--playing-timeout", m_GamePlayingTimeout, "Sets the time limit for a started game (seconds.)");

  app.add_option("--playing-timeout-warning-short-interval", m_GamePlayingTimeoutWarningShortInterval, "Sets the interval for the latest and most often game timeout warnings to be displayed.");
  app.add_option("--playing-timeout-warning-short-ticks", m_GamePlayingTimeoutWarningShortCountDown, "Sets the amount of ticks for the latest and most often game timeout warnings to be displayed.");
  app.add_option("--playing-timeout-warning-large-interval", m_GamePlayingTimeoutWarningLargeInterval, "Sets the interval for the earliest and rarest game timeout warnings to be displayed.");
  app.add_option("--playing-timeout-warning-large-ticks", m_GamePlayingTimeoutWarningLargeCountDown, "Sets the amount of ticks for the earliest and rarest timeout warnings to be displayed.");

  app.add_flag(  "--fast-expire-lan-owner,--no-fast-expire-lan-owner{false}", m_GameLobbyOwnerReleaseLANLeaver, "Allows to unsafely turn off the feature that removes game owners as soon as they leave a game lobby they joined from LAN.");

  app.add_option("--start-countdown-interval", m_GameLobbyCountDownInterval, "Sets the interval for the game start countdown to tick down.");
  app.add_option("--start-countdown-ticks", m_GameLobbyCountDownStartValue, "Sets the amount of ticks for the game start countdown.");

  app.add_option("--download-timeout", m_GameMapDownloadTimeout, "Sets the time limit for the map download (seconds.)");

  app.add_option("--players-ready", m_GamePlayersReadyMode, "Customizes when Aura will consider a player to be ready to start the game. Values: fast, race, explicit.")->check(CLI::IsMember({"fast", "race", "explicit"}));
  app.add_option("--auto-start-players", m_GameAutoStartPlayers, "Sets an amount of occupied slots for automatically starting the game.");
  app.add_option("--auto-start-time", m_GameAutoStartSeconds, "Sets a time that should pass before automatically starting the game (seconds.)");
  app.add_flag(  "--auto-start-balanced,--no-auto-start-balanced{false}", m_GameAutoStartRequiresBalance, "Whether to require balanced teams before automatically starting the game.");
  app.add_option("--auto-end-players", m_GameNumPlayersToStartGameOver, "Sets a low amount of players required for Aura to stop hosting a game.");
  app.add_option("--lobby-auto-kick-ping", m_GameAutoKickPing, "Customizes the maximum allowed ping in a game lobby.");
  app.add_option("--lobby-high-ping", m_GameWarnHighPing, "Customizes the ping at which Aura will issue high-ping warnings.");
  app.add_option("--lobby-safe-ping", m_GameSafeHighPing, "Customizes the ping required for Aura to consider a high-ping player as acceptable again.");
  app.add_option("--latency", m_GameLatencyAverage, "Sets the refresh period for the game as a ping equalizer, in milliseconds.");
  app.add_option("--latency-max-frames", m_GameLatencyMaxFrames, "Sets a maximum amount of frames clients may fall behind. When exceeded, the lag screen shows up.");
  app.add_option("--latency-safe-frames", m_GameLatencySafeFrames, "Sets a frame difference clients must catch up to in order for the lag screen to go away.");
  app.add_flag(  "--latency-equalizer,--no-latency-equalizer{false}", m_GameLatencyEqualizerEnabled, "Enables a minimum delay for all actions sent by game players.");
  app.add_option("--latency-equalizer-frames", m_GameLatencyEqualizerFrames, "Sets the amount of frames to be used by the latency equalizer.");
  app.add_flag(  "--latency-normalize,--no-latency-normalize{false}", m_GameSyncNormalize, "Whether Aura tries to automatically fix some game-start lag issues.");
  app.add_option("--reconnection", m_GameReconnectionMode, "Customizes GProxy support for the hosted game. Values: disabled, basic, extended.")->check(CLI::IsMember({"disabled", "basic", "extended"}));
  app.add_option("--load", m_GameSavedPath, "Sets the saved game .w3z file path for the game lobby.");
  app.add_option("--reserve", m_GameReservations, "Adds a player to the reserved list of the game lobby.");
  app.add_option("--crossplay", m_GameCrossplayVersions, "Adds support for game clients on the given version to crossplay. Repeatable.");
  app.add_flag(  "--check-joinable,--no-check-joinable{false}", m_GameCheckJoinable, "Reports whether the game is joinable over the Internet.");
  app.add_flag(  "--notify-joins,--no-notify-joins}", m_GameNotifyJoins, "Reports whether the game is joinable over the Internet.");
  app.add_flag(  "--check-reservation,--no-check-reservation{false}", m_GameCheckReservation, "Enforces only players in the reserved list be able to join the game.");
  app.add_option("--hcl", m_GameHCL, "Customizes a hosted game using the HCL standard.")->check(CheckIsValidHCL);
  app.add_flag(  "--ffa", m_GameFreeForAll, "Sets free-for-all game mode - every player is automatically assigned to a different team.");
  app.add_option("--hide-ign-started", m_GameHideLoadedNames, "Whether to hide player names in various outputs (e.g. commands) after the game starts. Values: never, host, always, auto")->check(CLI::IsMember({"never", "host", "always", "auto"}));
  app.add_flag(  "--hide-ign,--no-hide-ign{false}", m_GameHideLobbyNames, "Whether to hide player names in a hosted game lobby.");
  app.add_flag(  "--load-in-game,--no-load-in-game{false}", m_GameLoadInGame, "Whether to allow players chat in the game while waiting for others to finish loading.");
  app.add_flag(  "--join-in-progress-observers,--no-join-in-progress-observers{false}", m_GameEnableJoinObserversInProgress, "Whether to allow observers to watch the game after it has already started.");
  app.add_flag(  "--join-in-progress-players,--no-join-in-progress-players{false}", m_GameEnableJoinPlayersInProgress, "Whether to allow players to join the game after it has already started.");
  app.add_flag(  "--log-game-commands,--no-log-game-commands{false}", m_GameLogCommands, "Whether to log usage of chat triggers in a hosted game lobby.");

  app.add_flag(  "--replaceable,--no-replaceable{false}", m_GameLobbyReplaceable, "Whether users can use the !host command to replace the lobby.");
  app.add_flag(  "--auto-rehost,--no-auto-rehost{false}", m_GameLobbyAutoRehosted, "Registers the provided game setup, and rehosts it whenever there is no active lobby.");

  // Command execution
  app.add_option("--exec", m_ExecCommands, "Runs a command from the CLI. Repeatable.");
  app.add_option("--exec-as", m_ExecAs, "Customizes the user identity when running commands from the CLI.")/*->check(IsFullyQualifiedUser)*/;

  map<string, CommandAuth> commandAuths{
    {"auto", CommandAuth::kAuto},
    {"spoofed", CommandAuth::kSpoofed},
    {"verified", CommandAuth::kVerified},
    {"admin", CommandAuth::kAdmin},
    {"rootadmin", CommandAuth::kRootAdmin},
    {"sudo", CommandAuth::kSudo}
  };

  app.add_option("--exec-auth", m_ExecAuth, "Customizes the user permissions when running commands from the CLI.")->check(CLI::IsMember(commandAuths))->transform(CLI::Transformer(commandAuths));
  app.add_option("--exec-game", m_ExecGame, "Customizes the channel when running commands from the CLI. Values: lobby, game#IDX");
  app.add_flag(  "--exec-broadcast", m_ExecBroadcast, "Enables broadcasting the command execution to all users in the channel");

  // Port-forwarding
#ifndef DISABLE_MINIUPNP
  app.add_option("--port-forward-tcp", m_PortForwardTCP, "Enable port-forwarding on the given TCP ports. Repeatable.");
  app.add_option("--port-forward-udp", m_PortForwardUDP, "Enable port-forwarding on the given UDP ports. Repeatable.");
#else
  app.add_option("--port-forward-tcp", m_PortForwardTCP, "Enable port-forwarding on the given TCP ports. Repeatable. (This distribution of Aura does not support this feature.)");
  app.add_option("--port-forward-udp", m_PortForwardUDP, "Enable port-forwarding on the given UDP ports. Repeatable. (This distribution of Aura does not support this feature.)");
#endif

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    if (0 == app.exit(e)) {
      return CLIResult::kInfoAndQuit;
    }
    return CLIResult::kError;
  } catch (...) {
    Print("[AURA] CLI unhandled exception");
    return CLIResult::kError;
  }

  if (!m_ExecCommands.empty() && !m_ExecAs.has_value()) {
    Print("[AURA] Option --exec-as is required");
    return CLIResult::kError;
  }

  if (!m_ExecGame.empty() && !CheckTargetGameSyntax(m_ExecGame)) {
    Print("[AURA] Option --exec-game accepts values: lobby, game#IDX");
    return CLIResult::kError;
  }

  if (about || examples) {
    if (about) {
      m_InfoAction = CLI_ACTION_ABOUT;
    } else if (examples) {
      m_InfoAction = CLI_ACTION_EXAMPLES;
    }
    return CLIResult::kInfoAndQuit;
  }

  // Make sure directories have a trailing slash.
  // But m_CFGPath is just a file.
  if (m_HomePath.has_value()) NormalizeDirectory(m_HomePath.value());
  if (m_War3Path.has_value()) NormalizeDirectory(m_War3Path.value());
  if (m_MapPath.has_value()) NormalizeDirectory(m_MapPath.value());
  if (m_MapCFGPath.has_value()) NormalizeDirectory(m_MapCFGPath.value());
  if (m_MapCachePath.has_value()) NormalizeDirectory(m_MapCachePath.value());
  if (m_JASSPath.has_value()) NormalizeDirectory(m_JASSPath.value());
  if (m_GameSavePath.has_value()) NormalizeDirectory(m_GameSavePath.value());

  if (m_SearchTarget.has_value()) {
    if (!m_ExitOnStandby.has_value()) m_ExitOnStandby = true;
    if (!m_UseMapCFGCache.has_value()) m_UseMapCFGCache = !m_UseStandardPaths;
  }

  if (m_CFGAdapterPath.has_value()) {
    return CLIResult::kConfigAndQuit;
  }

  return CLIResult::kOk;
}

uint8_t CCLI::GetGameSearchType() const
{
  uint8_t searchType = SEARCH_TYPE_ANY;
  if (m_SearchType.has_value()) {
    if (m_SearchType.value() == "map") {
      searchType = SEARCH_TYPE_ONLY_MAP;
    } else if (m_SearchType.value() == "config") {
      searchType = SEARCH_TYPE_ONLY_CONFIG;
    } else if (m_SearchType.value() == "local") {
      searchType = SEARCH_TYPE_ONLY_FILE;
    } else {
      searchType = SEARCH_TYPE_ANY;
    }
  } else if (m_UseStandardPaths) {
    searchType = SEARCH_TYPE_ONLY_FILE;
  }
  return searchType;
}

uint8_t CCLI::GetGameLobbyTimeoutMode() const
{
  uint8_t timeoutMode = LOBBY_TIMEOUT_OWNERLESS;
  if (m_GameLobbyTimeoutMode.has_value()) {
    if (m_GameLobbyTimeoutMode.value() == "never") {
      timeoutMode = LOBBY_TIMEOUT_NEVER;
    } else if (m_GameLobbyTimeoutMode.value() == "empty") {
      timeoutMode = LOBBY_TIMEOUT_EMPTY;
    } else if (m_GameLobbyTimeoutMode.value() == "ownerless") {
      timeoutMode = LOBBY_TIMEOUT_OWNERLESS;
    } else if (m_GameLobbyTimeoutMode.value() == "strict") {
      timeoutMode = LOBBY_TIMEOUT_STRICT;
    } else {
      timeoutMode = LOBBY_TIMEOUT_OWNERLESS;
    }
  }
  return timeoutMode;
}

uint8_t CCLI::GetGameLobbyOwnerTimeoutMode() const
{
  uint8_t timeoutMode = LOBBY_OWNER_TIMEOUT_ABSENT;
  if (m_GameLobbyOwnerTimeoutMode.has_value()) {
    if (m_GameLobbyOwnerTimeoutMode.value() == "never") {
      timeoutMode = LOBBY_OWNER_TIMEOUT_NEVER;
    } else if (m_GameLobbyOwnerTimeoutMode.value() == "absent") {
      timeoutMode = LOBBY_OWNER_TIMEOUT_ABSENT;
    } else if (m_GameLobbyOwnerTimeoutMode.value() == "strict") {
      timeoutMode = LOBBY_OWNER_TIMEOUT_STRICT;
    } else {
      timeoutMode = LOBBY_OWNER_TIMEOUT_ABSENT;
    }
  }
  return timeoutMode;
}

uint8_t CCLI::GetGameLoadingTimeoutMode() const
{
  uint8_t timeoutMode = GAME_LOADING_TIMEOUT_STRICT;
  if (m_GameLoadingTimeoutMode.has_value()) {
    if (m_GameLoadingTimeoutMode.value() == "never") {
      timeoutMode = GAME_LOADING_TIMEOUT_NEVER;
    } else if (m_GameLoadingTimeoutMode.value() == "strict") {
      timeoutMode = GAME_LOADING_TIMEOUT_STRICT;
    } else {
      timeoutMode = GAME_LOADING_TIMEOUT_STRICT;
    }
  }
  return timeoutMode;
}

uint8_t CCLI::GetGamePlayingTimeoutMode() const
{
  uint8_t timeoutMode = GAME_PLAYING_TIMEOUT_STRICT;
  if (m_GamePlayingTimeoutMode.has_value()) {
    if (m_GamePlayingTimeoutMode.value() == "never") {
      timeoutMode = GAME_PLAYING_TIMEOUT_NEVER;
    } else if (m_GamePlayingTimeoutMode.value() == "dry") {
      timeoutMode = GAME_PLAYING_TIMEOUT_DRY;
    } else if (m_GamePlayingTimeoutMode.value() == "strict") {
      timeoutMode = GAME_PLAYING_TIMEOUT_STRICT;
    } else {
      timeoutMode = GAME_PLAYING_TIMEOUT_STRICT;
    }
  }
  return timeoutMode;
}

uint8_t CCLI::GetGameReconnectionMode() const
{
  uint8_t reconnectionMode = RECONNECT_DISABLED;
  if (m_GameReconnectionMode.has_value()) {
    if (m_GameReconnectionMode.value() == "disabled") {
      reconnectionMode = RECONNECT_DISABLED;
    } else if (m_GameReconnectionMode.value() == "basic") {
      reconnectionMode = RECONNECT_ENABLED_GPROXY_BASIC;
    } else if (m_GameReconnectionMode.value() == "extended") {
      reconnectionMode = RECONNECT_ENABLED_GPROXY_EXTENDED | RECONNECT_ENABLED_GPROXY_BASIC;
    } else {
      reconnectionMode = RECONNECT_DISABLED;
    }
  }
  return reconnectionMode;
}

uint8_t CCLI::GetGameDisplayType() const
{
  uint8_t displayMode = GAME_PUBLIC;
  if (m_GameDisplayMode.has_value()) {
    if (m_GameDisplayMode.value() == "public") {
      displayMode = GAME_PUBLIC;
    } else if (m_GameDisplayMode.value() == "private") {
      displayMode = GAME_PRIVATE;
    } else {
      displayMode = GAME_NONE;
    }
  }
  return displayMode;
}

uint8_t CCLI::GetGameIPFloodHandler() const
{
  uint8_t floodHandler = ON_IPFLOOD_NONE;
  if (m_GameIPFloodHandler.has_value()) {
    if (m_GameIPFloodHandler.value() == "none") {
      floodHandler = ON_IPFLOOD_NONE;
    } else if (m_GameIPFloodHandler.value() == "notify") {
      floodHandler = ON_IPFLOOD_NOTIFY;
    } else if (m_GameIPFloodHandler.value() == "deny") {
      floodHandler = ON_IPFLOOD_DENY;
    }
  }
  return floodHandler;
}

uint8_t CCLI::GetGameUnsafeNameHandler() const
{
  uint8_t nameHandler = ON_UNSAFE_NAME_NONE;
  if (m_GameUnsafeNameHandler.has_value()) {
    if (m_GameUnsafeNameHandler.value() == "none") {
      nameHandler = ON_UNSAFE_NAME_NONE;
    } else if (m_GameUnsafeNameHandler.value() == "censor") {
      nameHandler = ON_UNSAFE_NAME_CENSOR_MAY_DESYNC;
    } else if (m_GameUnsafeNameHandler.value() == "deny") {
      nameHandler = ON_UNSAFE_NAME_DENY;
    }
  }
  return nameHandler;
}

uint8_t CCLI::GetGameBroadcastErrorHandler() const
{
  uint8_t errorHandler = ON_ADV_ERROR_IGNORE_ERRORS;
  if (m_GameBroadcastErrorHandler.has_value()) {
    if (m_GameBroadcastErrorHandler.value() == "ignore") {
      errorHandler = ON_ADV_ERROR_IGNORE_ERRORS;
    } else if (m_GameBroadcastErrorHandler.value() == "exit-main-error") {
      errorHandler = ON_ADV_ERROR_EXIT_ON_MAIN_ERROR;
    } else if (m_GameBroadcastErrorHandler.value() == "exit-empty-main-error") {
      errorHandler = ON_ADV_ERROR_EXIT_ON_MAIN_ERROR_IF_EMPTY;
    } else if (m_GameBroadcastErrorHandler.value() == "exit-any-error") {
      errorHandler = ON_ADV_ERROR_EXIT_ON_ANY_ERROR;
    } else if (m_GameBroadcastErrorHandler.value() == "exit-empty-any-error") {
      errorHandler = ON_ADV_ERROR_EXIT_ON_ANY_ERROR_IF_EMPTY;
    } else if (m_GameBroadcastErrorHandler.value() == "exit-max-errors") {
      errorHandler = ON_ADV_ERROR_EXIT_ON_MAX_ERRORS;
    }
  }
  return errorHandler;
}

uint8_t CCLI::GetGameHideLoadedNames() const
{
  uint8_t hideNamesMode = HIDE_IGN_AUTO;
  if (m_GameHideLoadedNames.has_value()) {
    if (m_GameHideLoadedNames.value() == "never") {
      hideNamesMode = HIDE_IGN_NEVER;
    } else if (m_GameHideLoadedNames.value() == "host") {
      hideNamesMode = HIDE_IGN_HOST;
    } else if (m_GameHideLoadedNames.value() == "always") {
      hideNamesMode = HIDE_IGN_ALWAYS;
    } else if (m_GameHideLoadedNames.value() == "auto") {
      hideNamesMode = HIDE_IGN_AUTO;
    }
  }
  return hideNamesMode;
}

bool CCLI::CheckGameParameters() const
{
  if (m_GameOwnerLess.value_or(false) && m_GameOwner.has_value()) {
    Print("[AURA] Conflicting --owner and --no-owner flags.");
    return false;
  }
  return true;
}

bool CCLI::CheckGameLoadParameters(shared_ptr<CGameSetup> gameSetup) const
{
  if (!gameSetup->RestoreFromSaveFile()) {
    Print("[AURA] Invalid save file [" + PathToString(gameSetup->m_SaveFile) + "]");
    return false;
  } else if (m_GameCheckReservation.has_value() && !m_GameCheckReservation.value()) {
    Print("[AURA] Resuming a loaded game must always check reservations.");
    return false;
  } else if (m_GameLobbyAutoRehosted.value_or(false)) {
    // Do not allow automatically rehosting loads of the same savefile,
    // Because that would mean keeping the CSaveGame around.
    // Also, what's this? The Battle for Wesnoth?
    Print("[AURA] A loaded game cannot be auto rehosted.");
    return false;
  }
  return true;
}

void CCLI::RunInfoActions() const
{
  switch (m_InfoAction) {
    case CLI_ACTION_ABOUT: {
      Print("Aura " + string(AURA_VERSION));
      Print("Aura is a permissive-licensed open source project.");
      Print("Say hi at <" + string(AURA_ISSUES_URL) + ">");
      break;
    }
    case CLI_ACTION_EXAMPLES: {
      Print("Usage: aura [MAP NAME] [GAME NAME]");
      Print(R"(Example: aura wormwar "worm wars")");
      Print(R"(Example: aura "lost temple" "2v2")");
      Print("See additional options at CLI.md");
      break;
    }
  }
}

void CCLI::OverrideConfig(CAura* nAura) const
{
  if (m_War3Version.has_value())
    nAura->m_Config.m_War3Version = m_War3Version.value();
  if (m_War3Path.has_value())
    nAura->m_Config.m_Warcraft3Path = m_War3Path.value();
  if (m_MapPath.has_value())
    nAura->m_Config.m_MapPath = m_MapPath.value();
  if (m_MapCFGPath.has_value())
    nAura->m_Config.m_MapCFGPath = m_MapCFGPath.value();
  if (m_MapCachePath.has_value())
    nAura->m_Config.m_MapCachePath = m_MapCachePath.value();
  if (m_JASSPath.has_value())
    nAura->m_Config.m_JASSPath = m_JASSPath.value();
  if (m_GameSavePath.has_value())
    nAura->m_Config.m_GameSavePath = m_GameSavePath.value();

  if (m_ExtractJASS.has_value())
    nAura->m_Config.m_ExtractJASS = m_ExtractJASS.value();

  if (m_ExitOnStandby.has_value()) {
    nAura->m_Config.m_ExitOnStandby = m_ExitOnStandby.value();
  }
  if (m_BNET.has_value()) {
    nAura->m_Config.m_EnableBNET = m_BNET.value();
  }
  if (m_IRC.has_value()) {
    nAura->m_IRC.m_Config.m_Enabled = m_IRC.value();
  }
  if (m_Discord.has_value()) {
    nAura->m_Discord.m_Config.m_Enabled = m_Discord.value();
  }
  if (m_LAN.has_value()) {
    nAura->m_GameDefaultConfig->m_UDPEnabled = m_LAN.value();
  }
  if (m_UseMapCFGCache.has_value()) {
    nAura->m_Config.m_EnableCFGCache = m_UseMapCFGCache.value();
  }
  if (m_LogLevel.has_value()) {
    vector<string> logLevels = {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", "trace", "trace2", "trace3"};
    uint8_t maxIndex = static_cast<uint8_t>(logLevels.size());
    for (uint8_t i = 0; i < maxIndex; ++i) {
      if (m_LogLevel.value() == logLevels[i]) {
        nAura->m_Config.m_LogLevel = 1 + i;
        break;
      }
    }
  }

  if (m_LANMode.has_value()) {
    const bool isMainServerEnabled = m_LANMode.value() != "free";
    nAura->m_Net.m_Config.m_UDPMainServerEnabled = isMainServerEnabled;
    if (!isMainServerEnabled) {
      nAura->m_Net.m_Config.m_UDPBroadcastStrictMode = m_LANMode.value() == "strict";
    }
  }

  if (m_BindAddress.has_value()) {
    optional<sockaddr_storage> address = CNet::ParseAddress(m_BindAddress.value(), ACCEPT_IPV4);
    if (address.has_value()) {
      nAura->m_Net.m_Config.m_BindAddress4 = address.value();
    }
  }
  if (m_HostPort.has_value()) {
    nAura->m_Net.m_Config.m_MinHostPort = m_HostPort.value();
    nAura->m_Net.m_Config.m_MaxHostPort = m_HostPort.value();
  }

#ifndef DISABLE_MINIUPNP
  if (m_EnableUPnP.has_value()) {
    nAura->m_Net.m_Config.m_EnableUPnP = m_EnableUPnP.value();
  }
#endif
}

bool CCLI::QueueActions(CAura* nAura) const
{
  for (const auto& port : m_PortForwardTCP) {
    AppAction upnpAction = AppAction(APP_ACTION_TYPE_UPNP, APP_ACTION_MODE_TCP, port, port);
    nAura->m_PendingActions.push(upnpAction);
  }

  for (const auto& port : m_PortForwardUDP) {
    AppAction upnpAction = AppAction(APP_ACTION_TYPE_UPNP, APP_ACTION_MODE_UDP, port, port);
    nAura->m_PendingActions.push(upnpAction);
  }

  if (m_SearchTarget.has_value()) {
    CGameExtraOptions options;
    options.AcquireCLI(this);
    if (!CheckGameParameters()) {
      return false;
    }

    const uint8_t searchType = GetGameSearchType();
    optional<string> userName = GetUserMultiPlayerName();
    shared_ptr<CCommandContext> ctx = nullptr;
    try {
      ctx = make_shared<CCommandContext>(nAura, userName.value_or(string()), false, &cout);
    } catch (...) {
      return false;
    }
    shared_ptr<CGameSetup> gameSetup = nullptr;
    try {
      gameSetup = make_shared<CGameSetup>(nAura, ctx, m_SearchTarget.value(), searchType, true, m_UseStandardPaths, true /* lucky mode */);
    } catch (...) {
      return false;
    }
    if (m_GameSavedPath.has_value()) gameSetup->SetGameSavedFile(m_GameSavedPath.value());
    if (m_GameMapDownloadTimeout.has_value()) gameSetup->SetDownloadTimeout(m_GameMapDownloadTimeout.value());
    if (!gameSetup->LoadMapSync()) {
      if (searchType == SEARCH_TYPE_ANY) {
        ctx->ErrorReply("Input does not refer to a valid map, config, or URL.");
      } else if (searchType == SEARCH_TYPE_ONLY_FILE) {
        ctx->ErrorReply("Input does not refer to a valid file");
      } else if (searchType == SEARCH_TYPE_ONLY_MAP) {
        ctx->ErrorReply("Input does not refer to a valid map (.w3x, .w3m)");
      } else if (searchType == SEARCH_TYPE_ONLY_CONFIG) {
        ctx->ErrorReply("Input does not refer to a valid map config file (.ini)");
      }
      return false;
    }
    if (!gameSetup->ApplyMapModifiers(&options)) {
      ctx->ErrorReply("Invalid map options. Map has fixed player settings.");
      return false;
    }
    if (!gameSetup->m_SaveFile.empty()) {
      if (!CheckGameLoadParameters(gameSetup)) {
        return false;
      }
    }
    for (const auto& id : m_ExcludedRealms) {
      CRealm* excludedRealm = nAura->GetRealmByInputId(id);
      if (excludedRealm) {
        gameSetup->AddIgnoredRealm(excludedRealm);
      } else {
        Print("[AURA] Unrecognized realm [" + id + "] ignored by --exclude");
      }
    }
    if (m_GameMapAlias.has_value()) {
      string normalizedAlias = GetNormalizedAlias(m_GameMapAlias.value());
      string mapFileName = gameSetup->GetMap()->GetServerFileName();
      if (nAura->m_DB->AliasAdd(normalizedAlias, mapFileName)) {
        Print("[AURA] Alias <<" + m_GameMapAlias.value() + ">> added for [" + mapFileName + "]");
      } else {
        Print("Failed to add alias.");
      }
    }
    if (m_MirrorSource.has_value()) {
      if (!gameSetup->SetMirrorSource(m_MirrorSource.value())) {
        Print("[AURA] Invalid mirror source [" + m_MirrorSource.value() + "]. Ensure it has the form IP:PORT#ID");
        return false;
      }
    }
    if (m_GameName.has_value()) {
      gameSetup->SetBaseName(m_GameName.value());
    } else {
      if (userName.has_value()) {
        gameSetup->SetBaseName(userName.value() + "'s game");
      } else {
        gameSetup->SetBaseName("Join and play");
      }
    }
    if (userName.has_value()) {
      gameSetup->SetCreator(userName.value());
    }
    if (m_GameOwner.has_value()) {
      pair<string, string> owner = SplitAddress(m_GameOwner.value());
      gameSetup->SetOwner(ToLowerCase(owner.first), ToLowerCase(owner.second));
    } else if (m_GameOwnerLess.value_or(false)) {
      gameSetup->SetOwnerLess(true);
    }
    gameSetup->AcquireCLISimple(this);
    gameSetup->SetActive();
    AppAction hostAction = AppAction(APP_ACTION_TYPE_HOST, 0);
    nAura->m_PendingActions.push(hostAction);
  }

  for (const auto& execEntry : m_ExecCommands) {
    string cmdToken, command, payload;
    uint8_t tokenMatch = ExtractMessageTokensAny(execEntry, cmdToken, cmdToken, cmdToken, command, payload);
    pair<string, string> identity = SplitAddress(m_ExecAs.value());
    LazyCommandContext lazyCommand = LazyCommandContext(m_ExecBroadcast, command, payload, ToLowerCase(identity.first), ToLowerCase(identity.second), m_ExecGame, m_ExecAuth);
    nAura->m_PendingActions.push(lazyCommand);
  }

  return true;
}
