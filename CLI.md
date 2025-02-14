Command Line Interface
======================

# Overview

[Aura][1]'s CLI provides a set of flags and parameters to configure and control the bot's behavior
when hosting games, or to host games themselves.

Below is a detailed explanation of each flag and parameter along with usage examples.

# Positional arguments

## \`\<MAP\> \<NAME\>\`
Hosts a game with the given name, using a given map file.

- `<MAP>`: The name or identifier of the map to be hosted.
- `<NAME>`: The desired name for the hosted game session.

If ``MAP`` has no slashes, it's resolved relative to Aura's map dir by default.
When it has slashes, or ``--stdpaths`` is used, it's resolved relative to the current working directory (CWD).

## \`\<CONFIG\> \<NAME\>\`
Hosts a game with the given name, using a given map config/metadata file.

- `<CONFIG>`: The configuration settings for the game.
- `<NAME>`: The desired name for the hosted game session.

If ``CONFIG`` has no slashes, it's resolved relative to Aura's map config dir by default.
When it has slashes, or ``--stdpaths`` is used, it's resolved relative to the current working directory (CWD).

# Flags

## \`--about\`

Displays the information of Aura, including its version.

## \`--help\`

Displays usage information and a list of available flags and parameters.

## \`--verbose\`

Displays more detailed information when running CLI actions.

## \`--stdpaths\`

When enabled, this flag instructs Aura to utilize standard paths for directories and files 
entered from the CLI. That is, it makes them relative to the current working directory (CWD).

Notably, it also ensures that map and configuration file paths are interpreted exactly as entered, thus 
disabling fuzzy-matching. However, it's important to note that this flag removes protection against 
[arbitrary directory traversal][3]. Therefore, it should only be used for paths that have been thoroughly validated.

Additionally, the following CLI flags are affected by ``--stdpaths``:
- ``--config <FILE>``. When stdpaths is disabled and FILE has no slashes, it resolves relative to Aura's home dir.

This option is commutative.

## \`--init-system\`

When enabled, this flag instructs Aura to install itself to various relevant places.

- User PATH environment variable. This allows easily invoking Aura from anywhere in the CLI.
- Windows Explorer context menu. This allows easily hosting WC3 map files by right-clicking them.

This option is enabled by default when Aura is executed for the first time.

## \`--no-init-system\`

When enabled, this flag instructs Aura NOT to install itself to various relevant places. 
This can help in avoiding cluttering the system GUI and environment variables.

## \`--auto-port-forward\`

This flag enables Universal Plug and Play (UPnP) functionality within the game 
server. UPnP allows devices to discover and communicate with each other, 
facilitating automatic port forwarding on routers that support this protocol.

When the --auto-port-forward flag is enabled, the game server will attempt to 
automatically configure port forwarding on compatible routers using UPnP. This 
feature is particularly useful for hosting multiplayer game sessions, as it 
ensures that incoming connections from players outside the local network can 
reach the server without manual intervention to configure port forwarding on 
the router.

Note:
- UPnP support must be enabled on the router for this feature to work.
- Some routers may require user authentication for UPnP requests.

This flag is enabled by default.

## \`--no-auto-port-forward\`

Conversely, this flag flag is used to explicitly disable Universal Plug and Play 
(UPnP) functionality within the game server. When this flag is set, the game server 
will not attempt to automatically configure port forwarding using UPnP, even if 
UPnP is supported by the router.

This may be desirable in situations where manual port forwarding configurations are 
preferred, or if there are concerns about security risks associated with UPnP.

Disabling UPnP may require manual configuration of port forwarding rules on the router, 
which could be inconvenient for game hosts. Ensure that you can properly handle the 
implications of using this flag.

This option is equivalent to ``<net.port_forwarding.upnp.enabled = no>`` in `config.ini`

## \`--lan\`

This flag indicates that any hosted games should be available in the 
"Local Area Network" game menu. When this flag is used, hosted games 
will be available to players on the same network, as well as players 
sharing the same VPNs or the IPv6 TCP tunneling mechanism. 
See [NETWORKING.md][2] for more information.

This option is equivalent to ``<net.game_discovery.udp.enabled = yes>`` in `config.ini`

This flag is enabled by default.

## \`--no-lan\`

Conversely, this flag indicates that the game should NOT be available in the 
"Local Area Network" game menu. When this flag is used, hosted games 
will NOT be available to players on the same network, nor to players 
sharing the same VPNs or the IPv6 TCP tunneling mechanism. 
See [NETWORKING.md][2] for more information.

This option is equivalent to ``<net.game_discovery.udp.enabled = no>`` in `config.ini`

## \`--bnet\`

Sets all Battle.net/PvPGN realms registered in `config.ini` to enabled. This flag overrides 
any ``realm_N.enabled = no`` entries in the configuration file. This forces hosted games 
to be playable over the internet through Blizzard's Battle.net platform, as well as 
through alternative PvPGN servers.

This option is equivalent to ``<bot.toggle_every_realm = yes>`` in `config.ini`

## \`--no-bnet\`

Sets all Battle.net/PvPGN realms registered in `config.ini` to disabled. This prevents Aura from 
connecting to them. If this flag is enabled, games cannot be played over the internet through 
Blizzard's Battle.net platform, nor through alternative PvPGN servers.

This option is equivalent to ``<bot.toggle_every_realm = no>`` in `config.ini`

## \`--irc\`

When enabled, this flag instructs Aura to connect to the configured IRC server.

This option is equivalent to ``<irc.enabled = yes>`` in `config.ini`

## \`--no-irc\`

When enabled, this flag instructs Aura not to connect to IRC.

This option is equivalent to ``<irc.enabled = no>`` in `config.ini`

## \`--discord\`

When enabled, this flag instructs Aura to connect to Discord using the defined configuration.

This option is equivalent to ``<discord.enabled = yes>`` in `config.ini`

## \`--no-discord\`

When enabled, this flag instructs Aura not to connect to Discord.

This option is equivalent to ``<discord.enabled = no>`` in `config.ini`

## \`--exit\`

Enables the option to automatically exit the bot after hosting all games queued through the CLI. 
When this option is enabled, the bot will terminate itself once the hosting process is completed. 
When any games are created from the CLI, this option is automatically enabled by default.

## \`--no-exit\`

Disables the option to automatically exit the bot after hosting all games queued through the CLI. 
When this option is enabled, the bot will terminate itself once the hosting process is completed.

## \`--cache\`

Enables caching of map metadata for faster loading times and improved performance during gameplay. 
When caching is enabled, certain map data is stored locally to reduce loading times for subsequent 
sessions.

This option is enabled by default when no games are hosted from the CLI.

## \`--no-cache\`

Disables caching of map metadata. When caching is disabled, game data is not stored locally, and 
each session may experience longer loading times as a result. This option is enabled by default 
when hosting games from the CLI.

## \`--extract-jass\`

When you use this flag, Aura will automatically extract necessary files from your game 
installation. These files are crucial for hosting games directly from game map files. Aura 
does all the work for you, making sure everything is set up correctly behind the scenes.

**Why Use `--extract-jass`?**
- **Convenience**: With this flag, you don't need to worry about manually obtaining files. Aura takes care of it for you.
- **Saves Time**: No need to spend extra time finding and extracting files yourself.
- **Ensures Compatibility**: Extracting the necessary files ensures compatibility with a wide range of game versions.

## \`--no-extract-jass\`

By using this flag, you're indicating that you prefer to handle the extraction of necessary files 
manually. Although Aura won't do it for you automatically, you still have the flexibility 
to obtain and manage these files as you see fit.

**Why Use `--no-extract-jass`?**
- **Manual Control**: Some users prefer to have full control over file management.
- **Customization**: You might have specific preferences or procedures for obtaining these files.
- **Already Extracted**: If you've already extracted the files or prefer a different method, this 
flag allows you to bypass automatic extraction.

**Note**: Even if JASS files are unavailable, you can still host games using map config 
files (``.ini``). These flags simply determine how files necessary for hosting from map files 
(``.w3x``, ``.w3m``) are obtained and managed.

# Parameters

## \`--homedir \<DIRECTORY\>\`

Specifies the directory to be used as Aura's home directory. Paths in the config file are resolved relative to the home directory.

- This flag takes precedence over any other method of determining the home directory, including environment variables.
- If `--homedir` is not provided, the environment variable `AURA_HOME` is used to determine the home directory.
- If neither `--homedir` nor the `AURA_HOME` environment variable are set, the home directory defaults to the directory where Aura's executable is located.

## \`--config \<FILE\>\`

Specifies the location of Aura's main configuration file.

- If `<FILE>` does not contain any slashes, it is resolved relative to the home directory by default, unless overridden by the `--stdpaths` flag.
- The presence of any slashes causes `<FILE>` to be resolved relative to the current working directory (CWD).
- When using `--config`, if the configuration file is not located within the home directory, the bot may only start up if the configuration file includes the entry `<bot.home_path.allow_mismatch = yes>`.

**Note**: Paths in any configuration file are resolved relative to Aura's home directory.

Defaults to ``config.ini`` in the home dir.

## \`--config-adapter \<FILE\>\`

Specifies the location of a file to be used to migrate legacy Ghost++ configuration files (e.g. `default.cfg`) into modern Aura configuration files.

When this option is specified, Aura will read the legacy configuration file at the location specified by `--config` (or at the default location). 
Then, the file specified by `--config-adapter` will be used in order to convert the former, creating a modern Aura configuration file with 
the name `config-migrated.ini`.

It's recommended that users check that the contents of a generated config file matches both their expectation and Aura's expectations, by 
contrasting key-value pairs with those documented as allowed in `CONFIG.md`.

After manually fixing any remaining issues, a generated config file should be renamed to `config.ini`, so that Aura automatically uses it.

**Note**: Paths in any configuration file are resolved relative to Aura's home directory.

## \`--w3version \<NUMBER\>\`

Specifies the version that is to be used when hosting Warcraft 3 games. This parameter allows Aura to switch 
versions on the spot.

Note that, while Aura supports some degree of cross-version compatibility, the version specified by this parameter is assumed in 
case Aura cannot figure out the version used by a game client.

This option is equivalent to ``<game.version>`` in `config.ini`

## \`--w3dir \<DIRECTORY\>\` 

Specifies the directory where the Warcraft 3 game files are located. This parameter allows Aura 
to identify and fetch files from alternative Warcraft 3 game installations on the spot.

This option is equivalent to ``<game.install_path>`` in `config.ini`

## \`--mapdir \<DIRECTORY\>\`

Specifies the directory where Warcraft 3 maps are stored. This parameter allows Aura to locate  
and load maps for hosting games.

This option is equivalent to ``<bot.maps_path>`` in `config.ini`

## \`--cfgdir \<DIRECTORY\>\`

Specifies the directory path where Aura reads metadata files for Warcraft 3 maps. 
These metadata files, also known as "map config" files or mapcfg, are essential 
for hosting games, and can be used as the source of truth even if the original map 
files are unavailable.

This option enables users to specify a custom directory for Aura to locate existing map 
metadata. It's particularly useful for organizing and accessing map configurations 
separately from other application data.

This option is equivalent to ``<bot.map_configs_path>`` in `config.ini`

## \`--cachedir \<DIRECTORY\>\`

Specifies the directory path where Aura automatically generates and stores cache files 
for Warcraft 3 maps. These cache files serve the same purpose as user-created metadata 
files, but are generated by the application to optimize performance.

Separating cache files from user-created files allows for better organization and 
maintenance of map metadata.

This option is equivalent to ``<bot.map_cache_path>`` in `config.ini`

## \`--jassdir \<DIRECTORY\>\`

Specifies the directory where Warcraft 3 script files are stored. These files are needed in order 
to host games from map files (``.w3x``, ``.w3m``).

This option is equivalent to ``<bot.jass_path>`` in `config.ini`

## \`--savedir \<DIRECTORY\>\`

Specifies the directory where Warcraft 3 save files (``.w3z``) are stored.

This option is equivalent to ``<bot.save_path>`` in `config.ini`

## \`--bind-address \<IPv4>\`

If specified, Aura's game server will only allow connections from the provided IPv4 address.
Note that \`0.0.0.0\` is a special value that will allow connections from every IPv4 address.

The most interesting value is \`127.0.0.1\`, which will only allow connections from your 
[loopback interface][4] (i.e. connections only from the local machine where the server is running.)

This option is equivalent to ``<net.bind_address>`` in `config.ini`

## \`--host-port \<PORT>\`

If specified, Aura's game server will listen exclusively at the provided port.
Note that \`0.0.0.0\` is a special value that will allow connections from every IPv4 address.

This option is equivalent to ``<net.host_port.only>`` in `config.ini`

## \`--lan-mode \<MODE\>\`

Specifies how hosted games available for "Local Area Network" should be made known to potential players.

**Options:**

- strict: Exclusively uses back-and-forth communication with interested game clients. Aura's server sends 
tiny periodic messages over the network that prompts open Warcraft III clients to request further 
information. Once Aura provides them with that information, game clients may join your hosted game.

This option is equivalent to ``<net.game_discovery.udp.broadcast.strict = yes>`` in `config.ini`

- lax: Aura periodically sends the full information needed to join a hosted game to all machines in the 
same network. Additionally, it will reply to the information request that happens when a game client first 
opens the "Local Area Network" menu.

This option is equivalent to ``<net.game_discovery.udp.broadcast.strict = no>`` in `config.ini`

- free: Aura periodically sends the full information needed to join a hosted game to all machines in the 
same network. It will not reply to any information requests, nor process any UDP traffic. Port 6112 is not 
used by Aura's UDP server. This is the only UDP mode that allows a Warcraft III game client in the host 
machine to connect to the Local Area Network.

This option is equivalent to ``<net.udp_server.enabled = no>`` in `config.ini`

## \`--log-level \<LEVEL\>\`

Specifies the level of detail for logging output.

Values:
 - trace2: Extremely detailed information, typically used for deep debugging purposes.
 - trace: Detailed information, providing a high level of insight into the bot's internal operations.
 - debug: Fine-grained informational events that are most useful for debugging purposes.
 - info: Confirmation that things are working as expected.
 - notice: Normal but significant events that may require attention.
 - warning: An indication that something unexpected happened, or indicative of some problem in the near future.
 - error: Due to a more serious problem, the software has not been able to perform some function.
 - critical: A serious error that may prevent the bot from functioning correctly.

By default, the logging level is set to info.

## \`--port-forward-tcp \<PORT\>\`, \`--port-forward-udp \<PORT\>\`

The ``--port-forward-tcp <PORT>`` and ``--port-forward-udp <PORT>`` flags are used to trigger 
Universal Plug and Play (UPnP) requests from the command line interface (CLI) for TCP and UDP 
port forwarding, respectively. These flags facilitate the hosting and discovery of multiplayer games 
by allowing incoming connections on specific ports.

  TCP (Transmission Control Protocol):

      The ``--port-forward-tcp <PORT>`` flag initiates UPnP requests to forward TCP traffic on the specified port.
      TCP is used for establishing connections between the game server and multiple clients for multiplayer gameplay.
      Enabling TCP port forwarding ensures that incoming TCP connections can reach the game server, allowing players to join multiplayer games seamlessly.

  UDP (User Datagram Protocol):

      The ``--port-forward-udp <PORT>`` flag initiates UPnP requests to forward UDP traffic on the specified port.
      UDP is utilized for server and client communication to discover hosted games and exchange game data efficiently.
      Enabling UDP port forwarding ensures that UDP packets related to game discovery and communication can reach the game server and clients.

**Note:**
- Ensure that the specified ports are not blocked by firewalls and are available for use.
- UPnP support must be enabled on the router for these requests to be successful.
- Some routers may require user authentication for UPnP requests. Aura does not support this.

# Flags for CLI games

## \`--check-joinable\`, \`--no-check-joinable\`

This flag enables automatic connectivity checks to ensure hosted games are joinable from the Internet.

This flag is disabled by default.

## \`--check-reservation\`, \`--no-check-reservation\`

This flag enables reservation checks to ensure only players with reservations may join games.

This flag is disabled by default.

## \`--replaceable\`, \`--no-replaceable\`

This flag enables users to use the !host command to replace the hosted game by another one.

This flag is disabled by default.

## \`--auto-rehost\`, \`--no-auto-rehost\`

This flag enables automatic rehosting of the same game setup so long as Aura is not hosting another lobby.

This flag is disabled by default.

## \`--notify-joins\`, \`--no-notify-joins\`

This flag enables sound notifications when a player joins the hosted game.

This flag is equivalent to to ``<ui.notify_joins.enabled>`` in `config.ini`.
This flag is disabled by default.

## \`--ffa\`

This flag forces players joining the game into different teams.

This flag is disabled by default.

## \`--lock-teams\`, \`--no-lock-teams\`

This flag enforces fair play in team games. When disabled, players are allowed to arbitrarily change their
diplomatic relationship to other players, including.
- Becoming allied or foes.
- Sharing vision or unsharing it, even with foes.
- Sharing units or unsharing them, only with (current) allies.
- Sharing your victory conditions with your allies ('Allied Victory'.)
- Trading resources with foes.

This flag is enabled by default.

## \`--teams-together\`, \`--no-teams-together\`

This flag enables automatic grouping of teammates' starting positions.

This flag is enabled by default.

## \`--share-advanced\`, \`--no-share-advanced\`

This flag enables 'Advanced Shared Unit Control'. When this setting is active, if a player shares their units 
with someone else, they will be allowed to train units, and build structures on their behalf.

This flag is disabled by default.

## \`--random-races\`

This flag enables randomization of player races in the hosted game. When this flag is used, each player's 
race will be randomly assigned at the start of the game session. This adds an element of unpredictability 
and variety to the game, as players may need to adapt their strategies based on their randomly assigned race.

This flag is disabled by default.

## \`--random-heroes\`

This flag enables randomization of player heroes in the hosted game. With this flag, each player's hero will 
be randomly selected at the beginning of the game session. Randomizing heroes adds excitement and diversity 
to the gameplay, as players must adapt their tactics based on the strengths and abilities of their randomly 
assigned hero.

This flag is disabled by default.

## \`--ownerless\`

This flag prevents players from taking ownership of the hosted game.

This flag is disabled by default.

## \`--latency-equalize4\`

This flag adds a minimum delay to all actions performed by players in the hosted game,
evening the field in case players' connection quality differ significatively.

This flag is enabled by default.

## \`--no-latency-equalize4\`

This flag prevents a minimum delay from being added to actions performed by players in the hosted game,
allowing maximum responsiveness during gameplay.

This flag is disabled by default.

## \`--latency-normalize\`

This flag automatically synchronizes the network state of players in the hosted game, in such a game that some 
apparent lag issues at the game start get automatically fixed over time.

This flag is enabled by default.

## \`--no-latency-normalize\`

This flag prevents the automatic network synchronization mechanism. When this flag is used, lag screens with a 
long or even permanent duration may show up when some games start, rendering it unplayable.

This flag is disabled by default.

## \`--hide-ign\`

This flag prevents users joining a hosted game from knowing the nicknames of other players before the game starts.

This option is equivalent to ``<hosting.nicknames.hide_lobby>`` in `config.ini`

This option is equivalent to ``<map.hosting.nicknames.hide_lobby>`` in map configuration

This flag is disabled by default.

## \`--no-hide-ign\`

This flag allows users joining a hosted game to know the nicknames of other players before the game starts.

This option corresponds to ``<hosting.nicknames.hide_lobby>`` in `config.ini`

This option corresponds to ``<map.hosting.nicknames.hide_lobby>`` in map configuration

This flag is enabled by default.

## \`--load-in-game\`

This flag allows players into the game, in a paused state, as soon as each of them finishes loading. 
This allows them to preview the game map and chat together, while waiting for others to finish loading 
the game themselves.

This option is equivalent to ``<hosting.load_in_game.enabled>`` in `config.ini`

This option is equivalent to ``<map.hosting.load_in_game.enabled>`` in map configuration

This flag is disabled by default.

## \`--no-load-in-game\`

This flag ensures the game loading screen stays up before every player has finished loading the game.
This ensures that faster players will not get unfair advantages due to previewing the map too early.

This option corresponds to ``<hosting.load_in_game.enabled>`` in `config.ini`

This option corresponds to ``<map.hosting.load_in_game.enabled>`` in map configuration

This flag is enabled by default.

## \`--join-in-progress-observers\`

This flag allows the game to remain being announced over LAN and PvPGN servers even after it has started. 
Observers may still join the started game, at any of the game slots the map configuration allows.

If the map has Custom Forces enabled, only the designated game slots may be used for observers to watch an 
ongoing game. More than one observer may watch the game concurrently using any game slot.

This option is equivalent to ``<hosting.join_in_progress.observers>`` in `config.ini`

This option is equivalent to ``<map.hosting.join_in_progress.observers>`` in map configuration

This flag is disabled by default.

## \`--no-join-in-progress-observers\`

This flag causes the game to stop being announced over LAN and PvPGN servers as soon as it starts. 
Only observers that were in the lobby by the time the game started will be able to watch the game.

This flag is enabled by default.

## \`--join-in-progress-players\`

This flag allows the game to remain being announced over LAN and PvPGN servers even after it has started. 
Players may still join the started game, at any of the game slots the map configuration allows.

If the map has Custom Forces enabled, only the designated game slots may be used for players to join a game. 
Note that a single game slot cannot be used for more than one actual player, not even if they disconnect.

This option is equivalent to ``<hosting.join_in_progress.players>`` in `config.ini`

This option is equivalent to ``<map.hosting.join_in_progress.players>`` in map configuration

This flag is disabled by default.

## \`--no-join-in-progress-players\`

This flag causes the game to stop being announced over LAN and PvPGN servers as soon as it starts. 
Only players that were in the lobby by the time the game started will be able to play the game.

This option corresponds to ``<hosting.join_in_progress.players>`` in `config.ini`

This option corresponds to ``<map.hosting.join_in_progress.players>`` in map configuration

This flag is enabled by default.

## \`--log-game-commands\`

This flag causes all usage of game commands to be logged persistently.

Note that persistent logs are written to the `aura.log` file.

This option is equivalent to ``<hosting.log_commands>`` in `config.ini`

This option is equivalent to ``<map.hosting.log_commands>`` in map configuration

This flag is disabled by default.

## \`--no-log-game-commands\`

This flag prevents persistent logging of usage of game commands.

This option is equivalent to ``<hosting.log_commands>`` in `config.ini`

This option is equivalent to ``<map.hosting.log_commands>`` in map configuration

This flag is disabled by default.

## \`--auto-start-balanced\`

This flag specifies that the game may only automatically start when the teams are balanced. In order for this 
flag to be effective, other auto start parameters must also be supplied.

This option is equivalent to ``<hosting.autostart.requires_balance>`` in `config.ini`

This option is equivalent to ``<map.hosting.autostart.requires_balance>`` in map configuration

This flag is enabled by default.

## \`--no-auto-start-balanced\`

This flag specifies that the game may automatically start even if the teams are not balanced. In order for this 
flag to be effective, other auto start parameters must also be supplied.

This option is equivalent to ``<hosting.autostart.requires_balance>`` in `config.ini`

This option is equivalent to ``<map.hosting.autostart.requires_balance>`` in map configuration

This flag is disabled by default.

## \`--fast-expire-lan-owner\`

Enables a security measure that instantly removes the lobby owner if they disconnect from a game session joined 
over a Local Area Network (LAN).

Immediate removal minimizes the risk of impersonation, as an unauthenticated player could otherwise attempt to 
reconnect as the lobby owner. By ensuring the owner is removed upon disconnection, the lobby remains secure from 
potential exploits involving owner impersonation.

This option is equivalent to ``<hosting.expiry.owner.lan>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.owner.lan>`` in map configuration

This flag is enabled by default.

## \`--no-fast-expire-lan-owner\`

Disables the automatic removal of the lobby owner when they leave a game session joined over LAN.

This may be useful in environments where temporary disconnections are common, but it significantly increases the
risk of impersonation. Without authentication, another player could potentially rejoin as the lobby owner, 
leading to unauthorized control over the game. Use this flag cautiously, as it trusts the legitimacy of the 
owner’s reconnection without verification, potentially exposing the lobby to security vulnerabilities.

This option is equivalent to ``<hosting.expiry.owner.lan>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.owner.lan>`` in map configuration

This flag is disabled by default.

# Parameters for CLI games

## \`-s \<TYPE>, --search-type \<TYPE\>\`

Specifies the type of hostable being referenced. This parameter helps Aura determine how to resolve 
the input when hosting maps from the CLI, whether they are maps, configuration files, or remote resources.

**Options:**

- map: Indicates that the specified file is a Warcraft 3 map.
- config: Indicates that the specified file is a map config/metadata file.
- local: Indicates that both maps and config files are allowed, but only in the local machine.
- any: Indicates that the specified file may be found in the cloud.

## \`--exclude \<SERVER\>\`

This makes hosted games invisible in the specified server. The server is to be specified through their 
unique `Input ID`, which corresponds to ``realm_N.input_id`` in `config.ini`. Note that this value may be 
missing, and thus defaults to ``realm_N.unique_name`` (which itself defaults to \`realm_N.host_name\`).

## \`--alias \<ALIAS\>\`

This option lets Aura automatically register an alias for the map hosted. Aliases are case-insensitive, and 
normalized according to the rules listed in \`aliases.ini\`.

## \`--mirror \<IP:PORT\#ID\>\`

This option sets Aura to use game mirroring mode. In this mode, the bot won't host games by itself, but 
instead repost a game hosted elsewhere to connected Battle.net/PvPGN realms. The actual host is identified 
by their IPv4 address and PORT. The game ID, also known as "host counter", should be provided in hexadecimal.

When mirroring games, the following parameters are likely to also be useful:
- `--exclude`: In order to avoid duplicate broadcasts in the source PvPGN realm, if any.
- `--reconnection`: In order to properly communicate reconnection support to clients using GProxy.

Aura will remain in game mirroring mode until the process finishes.

## \`--observers \<OBSERVER\>\`

This parameter sets the observer mode for the hosted game. Observer mode determines how spectators are 
allowed to observe the game.

**Options:**

- no: No observers allowed. Spectators cannot join the game.
- full: Allows spectators to exclusively observe the game, and chat among themselves.
- referees: Allows spectators to observe the game, and communicate with the players, either in public chat, or private chat.
- defeat: Players that lose the game are allowed to remain in the game, as spectators.

## \`--visibility \<VISIBILITY\>\`

This parameter sets the visibility mode for the hosted game, determining how much of the map is visible 
to players and observers.

**Options:**

- default: Standard visibility mode where players can only see areas of the map revealed by their units and structures.
- hide terrain: Hides terrain from players and observers, making the entire map appear as black fog of war.
- map explored: Reveals the entire map to players, allowing them to see all terrain but not enemy units or structures unless they have vision of them.
- always visible: Grants players and observers full visibility of the entire map, including enemy units and structures, without the need for vision.

## \`--speed \<SPEED\>\`

This parameter sets the speed for the hosted game, determining how often the game updates.
Slower settings make the gameplay more accessible, but are uncommon in competitive environments.

**Options:**

- slow: This is the most accesible setting for new players.
- normal: This is the recommended setting for players new to real-time strategy games.
- fast: This is the most competitive setting. It's also the default game speed.

## \`--list-visibility \<DISPLAY\>\`

This parameter toggle whether the game is publicly visible in the game lists of
PvPGN realms.

**Options:**

- private: The game can only be joined from PvPGN realms by typing the game name.
- public: The game is visible and clickable in the game lists in PvPGN realms.

## \`--owner \<USER@SERVER>\`

This parameter specifies the owner of the hosted game. The owner is typically the user who has 
administrative control over the game session. Here's the format for specifying the owner:

- USER: The username of the owner.
- SERVER: The server the user is registered in.

## \`--lobby-timeout-mode \<MODE>\`

Sets the conditions that trigger the countdown for the game lobby timeout, based on a pre-configured timeout value.

**Options:**

- never: The timeout countdown will never start, keeping the lobby active indefinitely.
- empty: The countdown starts when the lobby becomes completely empty of players.
- ownerless: The countdown begins if the lobby owner disconnects, even if other players remain in the lobby.
- strict: The countdown starts immediately when the lobby is created and expires solely based on the timeout value,
with no additional conditions.

This option is equivalent to ``<hosting.expiry.lobby.timeout.mode>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.lobby.timeout.mode>`` in map configuration

## \`--lobby-owner-timeout-mode \<MODE>\`

Determines when the timeout countdown for game ownership expiration should begin, based on a specified timeout value.

**Options:**

- never: The timeout countdown for ownership will never start, even if the owner is absent.
- absent: The countdown starts when the lobby owner has left the room. Node that this mechanism is independent from
``--fast-expire-lan-owner``, which doesn't use a timer.
- strict: The countdown starts as a new game owner is assigned, and ownership expires solely based on the timeout value,
with no further conditions.

This option is equivalent to ``<hosting.expiry.owner.timeout.mode>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.owner.timeout.mode>`` in map configuration

## \`--loading-timeout-mode \<MODE>\`

Defines when the timeout countdown for kicking slow-loading players begins, in conjunction with a set timeout value.

**Options:**

- never: The countdown will not begin, allowing players unlimited time to load.
- strict: The countdown starts as soon as players begin loading,
and any player who exceeds the timeout will be kicked, with no other conditions applied.

This option is equivalent to ``<hosting.expiry.loading.timeout.mode>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.loading.timeout.mode>`` in map configuration

## \`--playing-timeout-mode \<MODE>\`

Specifies when the timeout countdown starts for an active game, used together with a predefined timeout duration. Once 
the timeout condition is fulfilled, all players are kicked from the game even if it is still in progress.

**Options:**

- never: The countdown will not be triggered after the game starts, allowing it to continue indefinitely.
- dry: The countdown starts immediately when the game begins, but instead of terminating the game, the
server simulates the expiration process. This mode is primarily used for testing server configuration without 
affecting active sessions or for playfully pressuring players to finish quickly. In dry mode, warnings and 
countdown messages will appear, but the game itself will not be forcibly ended when the timeout is reached.
- strict: The countdown starts as soon as the game begins, and the game will expire purely based on the timeout
value, without any additional conditions.

This option is equivalent to ``<hosting.expiry.playing.timeout.mode>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.playing.timeout.mode>`` in map configuration

## \`--lobby-timeout \<TIME\>\`

This parameter specifies the maximum time a game lobby is allowed to be unattended, that is,
without a game owner. After this time passes, the lobby is unhosted.

- TIME: Provided in seconds.

This option is equivalent to ``<hosting.expiry.lobby.timeout>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.lobby.timeout>`` in map configuration

## \`--lobby-owner-timeout \<TIME\>\`

This parameter specifies the maximum time a game owner is allowed to remain outside 
the game lobby. After this time passes, the game owner authority is removed.

- TIME: Provided in seconds.

This option is equivalent to ``<hosting.expiry.owner.timeout>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.owner.timeout>`` in map configuration

## \`--loading-timeout \<TIME\>\`

This parameter specifies the maximum time a player loading the game is waited for, before they
are automatically kicked from the game.

- TIME: Provided in seconds.

This option is equivalent to ``<hosting.expiry.loading.timeout>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.loading.timeout>`` in map configuration

## \`--playing-timeout \<TIME\>\`

This parameter specifies the maximum time a game is allowed to exist after it is started. When the 
timeout is reached, all players are kicked from the game even if it is still in progress.

- TIME: Provided in seconds.

This option is equivalent to ``<hosting.expiry.playing.timeout>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.playing.timeout>`` in map configuration

## \`--playing-timeout-warning-short-interval \<INTERVAL>\`

Sets the interval for the most frequent game timeout warnings as the time limit nears. 
These warnings will be displayed at regular intervals when the countdown is in its final phase, 
giving players a clear indication that the game is about to end soon.

This option is equivalent to ``<hosting.expiry.playing.timeout.soon_interval>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.playing.timeout.soon_interval>`` in map configuration

## \`--playing-timeout-warning-short-ticks \<TICKS>\`

Determines the number of ticks for which the most frequent game timeout warnings will be displayed 
before the game reaches the time limit. This option specifies how long the final, frequent warnings 
will persist, helping players to prepare for the end of the session.

This option is equivalent to ``<hosting.expiry.playing.timeout.soon_warnings>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.playing.timeout.soon_warnings>`` in map configuration

## \`--playing-timeout-warning-large-interval \<INTERVAL>\`

Sets the interval for the earliest and least frequent game timeout warnings. These warnings will 
appear sporadically when the game is still far from reaching the timeout limit, serving as an early 
reminder without causing too much distraction.

This option is equivalent to ``<hosting.expiry.playing.timeout.eager_interval>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.playing.timeout.eager_interval>`` in map configuration

## \`--playing-timeout-warning-large-ticks \<TICKS>\`

Determines the number of ticks for which the earliest and rarest game timeout warnings will be displayed. 
This option specifies how long the initial, infrequent warnings will persist, gently alerting players 
of the eventual game timeout.

This option is equivalent to ``<hosting.expiry.playing.timeout.eager_warnings>`` in `config.ini`

This option is equivalent to ``<map.hosting.expiry.playing.timeout.eager_warnings>`` in map configuration

## \`--download-timeout \<TIME\>\`

This parameter specifies the maximum time a map download is allowed to take. Once this time is
exceeded, the map download and game hosting are cancelled.

- TIME: Provided in seconds.

## \`--auto-start-players \<COUNT\>\`

This parameter specifies that the game may automatically start when the provided ``COUNT`` of slots 
have been occupied in the game. This includes computer slots, virtual player slots, and actual players that 
have downloaded the map. Note that, unless configured otherwise, balanced teams are mandatory for the game 
to automatically start.

If \`--auto-start-time\` is also supplied, the game will start only when both conditions are met simultaneously.

This option is equivalent to ``<map.auto_start.players>`` in map configuration

## \`--auto-start-time \<SECONDS\>\`

This parameter specifies that the game will automatically start when the provided ``SECONDS`` 
have passed since  the game was created. Note that, unless configured otherwise, balanced teams are 
mandatory for the game to automatically start.

If \`--auto-start-players\` is also supplied, the game will start only when both conditions are met simultaneously.

This option is equivalent to ``<map.auto_start.time>`` in map configuration

## \`--players-ready \<MODE\>\`

This parameter specifies when players will be considered ready for automatic start purposes.

**Options:**

- fast: All players who have already download the map and are in a non-observer team are treated as ready.
- race: In addition to the above, players must have chosen a race in order to be treated as ready.
- explicit: Players must use the command !ready in order to be treated as such.

This option is equivalent to ``<hosting.game_ready.mode>`` in `config.ini`

This option is equivalent to ``<map.hosting.game_ready.mode>`` in map configuration

## \`--auto-end-players \<COUNT\>\`

This parameter specifies that after one or more players left, and only ``COUNT`` players remain in a game,
the game should be ended, and players should be disconnected.

This option is equivalent to ``<hosting.game_over.player_count>`` in `config.ini`

This option is equivalent to ``<map.hosting.game_over.player_count>`` in map configuration

## \`--lobby-auto-kick-ping \<VALUE\>\`

This parameter specifies that players should be automatically kicked from the hosted game lobby, if their 
latency is found to exceed the provided value, in milliseconds.

This option is equivalent to ``<hosting.high_ping.kick_ms>`` in `config.ini`

This option is equivalent to ``<map.hosting.high_ping.kick_ms>`` in map configuration

## \`--lobby-high-ping \<VALUE\>\`

This parameter specifies that warnings should be issued in the hosted game lobby, if a player's latency is
found to exceed the provided value, in milliseconds.

This option is equivalent to ``<hosting.high_ping.warn_ms>`` in `config.ini`

This option is equivalent to ``<map.hosting.high_ping.warn_ms>`` in map configuration

## \`--lobby-safe-ping \<VALUE\>\`

This parameter specifies that an announcement should be displayed in the hosted game lobby, whenever the 
latency of a player whose latency has previously been found to be high, decreases down to the provided value, 
in milliseconds.

This option is equivalent to ``<hosting.high_ping.safe_ms>`` in `config.ini`

This option is equivalent to ``<map.hosting.high_ping.safe_ms>`` in map configuration

## \`--latency \<VALUE\>\`

This parameter specifies the time, in milliseconds, that should pass between each game tick. All data sent by 
players is forwarded to each other at a rate limited by the provided value.

The lower this value, the more fluid the gameplay will be.

This option is restricted to a minimum of 10 ms, and a maximum of 500 ms. The default value is 100 ms.

This option is equivalent to ``<bot.latency>`` in `config.ini`

This option is equivalent to ``<map.bot.latency>`` in map configuration

## \`--latency-equalizer-frames\`

When the latency equalizer feature is enabled, this parameter specifies the maximum delay that can be added
to players' actions, measured in game ticks.

## \`--latency-max-frames \<VALUE\>\`

This parameter specifies that, whenever a player fails to timely send the provided amount of updates, one per game tick, 
the lag screen should be shown, so that everyone waits for them.

This option is equivalent to ``<net.start_lag.sync_limit>`` in `config.ini`

This option is equivalent to ``<map.net.start_lag.sync_limit>`` in map configuration

## \`--latency-safe-frames \<VALUE\>\`

This parameter specifies that, whenever the lag screen is shown, it will end as soon as everyone is missing no more than
the provided amount of updates, one per game tick.

This option is equivalent to ``<net.stop_lag.sync_limit>`` in `config.ini`

This option is equivalent to ``<map.net.stop_lag.sync_limit>`` in map configuration

## \`--reconnection \<MODE\>\`

This parameter customizes the reconnection support, if any, for the hosted game. For this parameter to be effective,
related config.ini entries must also be enabled.

**Options:**

- disabled: GProxy reconnection not supported for the game.
- basic: GProxy classic reconnection supported. Requires ``<net.tcp_extensions.gproxy.basic.enabled = yes>``
- extended: GProxyDLL extended time reconnection supported, additionally to classic reconnection.
Requires ``<net.tcp_extensions.gproxy.long.enabled = yes>``

This option is equivalent to ``<map.reconnection.mode>`` in map configuration.

## \`--start-countdown-interval \<VALUE\>\`

This parameter specifies, in milliseconds, how long should Aura wait between each tick during the game start countdown.

This option is equivalent to ``<hosting.game_start.count_down_interval>`` in `config.ini`

This option is equivalent to ``<map.hosting.game_start.count_down_interval>`` in map configuration

## \`--start-countdown-ticks \<VALUE\>\`

This parameter specifies how many ticks should Aura wait for during game start countdown.

This option is equivalent to ``<hosting.game_start.count_down_ticks>`` in `config.ini`

This option is equivalent to ``<map.hosting.game_start.count_down_ticks>`` in map configuration

## \`--hcl\`

This parameter specifies a custom game mode to be prepicked for maps that implement the HCL standard.
For instance, ``--hcl ap`` sets 'All Pick' mode in DotA games.

This flag should never be used for maps that aren't known to support HCL. Otherwise, it will corrupt 
player handicaps.

This option is equivalent to ``<map.hcl>`` in map configuration

## \`--load \<FILE\>\`

Specifies the location of a saved game a game lobby will resume.

- If `<FILE>` does not contain any slashes, it is resolved relative to the saved games directory by default, unless overridden by the `--stdpaths` flag.
- The presence of any slashes causes `<FILE>` to be resolved relative to the current working directory (CWD).

## \`--reserve \<PLAYER\>\`

Makes a reservation for a player to join the game lobby. This is required for loaded games to properly work.

## \`--crossplay \<VERSION\>\`

Adds support for clients running the given game version to join the game lobby. Note that crossplay has an 
inherent risk of desynchronization between game clients. Usage of this feature is only recommended when hosting 
well-tested maps with custom game data.

Avoid using crossplay for melee or altered melee maps, in order to minimize potential issues. Note, however, that 
maps using SLK optimization, such as those optimized with [W3x2Lni][5], are likely immune against desynchronizations.

Note also that versions 1.29, and 1.23 are not compatible with earlier game versions.

## \`--on-ipflood \<ACTION\>\`

This parameter customizes behavior when an excessive amount of players join the game from the same IP address.

**Options:**

- none: Accepts any amount of players joining from the same IP address.
- notify: Sends a message to the game chat.
- deny: Prevents new players from joining the game.

This option is equivalent to ``<hosting.ip_filter.flood_handler>`` in `config.ini`

This option is equivalent to ``<map.hosting.ip_filter.flood_handler>`` in map configuration

## \`--on-unsafe-name \<ACTION\>\`

This parameter customizes behavior when a player tries to join a game using an unsafe or otherwise problematic name.

**Options:**

- none: Allow them to join.
- censor: Removes unsafe characters from their name. In some maps, usage of this option may cause desynchronization between players.
- deny: Prevents them from joining the game.

This option is equivalent to ``<hosting.name_filter.unsafe_handler>`` in `config.ini`

This option is equivalent to ``<map.hosting.name_filter.unsafe_handler>`` in map configuration

## \`--on-broadcast-error \<ACTION\>\`

This parameter customizes behavior when a game fails to be announced in one of the connected PvPGN realms.

**Options:**

- ignore: No action taken.
- exit-main-error: If a game cannot be announced in one of the main PvPGN realms, the game lobby is closed.
- exit-empty-main-error: If a game with no users cannot be announced in one of the main PvPGN realms, the game lobby is closed.
- exit-any-error: If a game cannot be announced in one of the connected PvPGN realms, the game lobby is closed.
- exit-empty-any-error: If a game with no users cannot be announced in one of the connected PvPGN realms, the game lobby is closed.
- exit-max-errors: If a game cannot be announce in any of the connected PvPGN realms, the game lobby is closed.

This option is equivalent to ``<hosting.realm_broadcast.error_handler>`` in `config.ini`

This option is equivalent to ``<map.hosting.realm_broadcast.error_handler>`` in map configuration

## \`--hide-ign-started\`

This parameter customizes whether Aura will obfuscate players names after the game starts.
By customizing this parameter, players names may effectively be hidden or revealed in some contexts.

**Options:**

- never: Players names are revealed.
- host: Players names are hidden in the output of many commands and status reports.
- always: Players names are hidden in the output of many commands and status reports. Additionally, 
when used together with --hide-ign, any hosted map will also be unaware of the identities of non-local 
players. When used this way, games running certain maps may desync.
- auto: This is the default. Players names are obfuscated in FFA games where there are 3 or more players.

This option is equivalent to ``<hosting.nicknames.hide_in_game>`` in `config.ini`

This option is equivalent to ``<map.hosting.nicknames.hide_in_game>`` in map configuration

# Flags for CLI commands

## \`--exec-broadcast\`

This flag enables broadcasting the command execution to all users or players in the channel. 
When activated, the command specified by --exec will be broadcasted to all users in the same 
realm or game, ensuring transparency and visibility of the executed action.

By using this flag, users or players will be informed about the command being executed, fostering 
a collaborative and informed environment during the game session.

# Parameters for CLI commands

## \`--exec \<COMMAND\>\`

This parameter allows the execution of a specific command after the bot start up, and the CLI game (if any) 
has been created. Here's how it works:

- COMMAND: The command to be executed. The command token is not to be included (e.g. ! or .)

## \`--exec-as \<USER@SERVER>\`

This parameter specifies the user and server on which the command specified by --exec should be executed. 
It allows the execution of commands as a specific user on a particular server. Here's the format:

- USER: The username to whom the command is attributed.
- SERVER: The server the user is registered in.

## \`--exec-auth \<AUTH\>\`

This parameter sets the authentication mode for executing commands specified by --exec.

**Options:**

- spoofed: Treats the user as unverified by their respective server.
- verified: Treats the user as verified by their respective server.
- admin: Treats the user as an admin of their server.
- rootadmin: Treats the user as a root admin of their server.
- sudo: Treats the user as a bot sudoer. These are the highest privileges.

## \`--exec-game \<IDENTIFIER\>\`

This parameter determines where the command specified by --exec will be run.

**Options:**

- lobby: The command runs in the hosted lobby.
- game#\<GAME\>: The command runs in the game with the specified ID.

Set the appropriate game based on where you want the command to be executed, or
leave empty.

[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://gitlab.com/ivojulca/aura-bot/NETWORKING.md
[3]: https://owasp.org/www-community/attacks/Path_Traversal
[4]: https://en.wikipedia.org/wiki/Localhost
[5]: https://www.hiveworkshop.com/threads/w3x2lni-v2-7-2.305201/
