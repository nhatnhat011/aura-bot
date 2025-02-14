Features
============

[Aura][1] has a revamped core, and has had features not only added, but also removed.

# Setup:
- Config is much more readable
- Config keys have been renamed
- Config now uses its correct extension (.ini)
- Config is validated
- Config can be hot-reloaded, including realms settings
- Blizzard.j, common.j moved to their own folder (jass/)
- Games can be hosted from the command line
- Games can be hosted from Windows Explorer
- Connects to and can be controlled from Discord, or IRC
- Automatically performs port-forwarding (UPnP)
- Automatically checks network connectivity
- Can automatically sign up accounts in PvPGN realms if unregistered
- Can configure TCP tunneling to host games in PvPGN realms
- Supports Warcraft III versions 1.21 to 1.29.2
- Automatically detects Warcraft III location and version
- Runs simultaneously with the Warcraft III game client
- May connect to some PvPGN servers with unknown exe version hashes
- Implements inheritance in PvPGN realms settings ("global_realm")
- Reconnectable games (GProxy) do not require exclusive ports
- Reconnectable games can wait for players for as much time as configured
- Permissions required by many commands are configurable.
- Maps can be assigned aliases
- Maps are automatically downloaded from EpicWar and WC3Maps repositories
- Maps and configs have their names fuzzy-matched

# Hosting:
- Automatically identifies maps larger than the vanilla map size limit
- Automatically verifies identity ("spoofcheck") in private games in PvPGNs
- Supports custom lobby layouts: FFA, One-VS-All, Humans-VS-AI
- Supports assigning team captains and drafting players in the game lobby
- Supports assigning exclusive referees: demotes other referees, preventing them
from chatting with players.
- Improved game lobby commands to modify not only AI properties, but also other players.
- Commands accept player names in addition to game slots
- Player races, color, and AI difficulty can be typed in English
- Shorthand commands for adding or removing computers (defaults to Insane)
- Unlimited virtual players can be added.
- Games can be hosted through IPv6
- Beeps when a player joins your game
- Games are reconnectable
- Can remake games
- Can define a timer and/or an amount of players for starting the game
- Game ownership is revoked after leaving games for too long
- Game owners that joined over LAN are revoked as soon as they leave

# Playing:
- Automatically sends intro messages to players
- Automatically links the map download URL in the game lobby
- Issues high-ping warnings, and eventually autokicks players
- Supports rolling dice with D&D notation
- Supports flipping coins, and randomly picking items from a list
- Users can send whispers cross-realm
- Can send cross-realm game invitations to other players
- Optionally equalizes players' latencies for competitive settings

# Permissions:
- Staff from some PvPGN realms no longer have access to 
privileged bot commands nor can affect other realms.
- "Admins" and "root admins" renamed to "moderators" and "admins", respectively.
- Privileged bot commands are gated behind a new category: "sudo users". Sudo users 
are identified by their names/IDs from each realm, IRC server, or Discord.
- Privileged bot commands must be confirmed with a randomly generated key displayed 
in Aura's console.
- Sudo users may use any bot command anywhere by upgrading them to privileged commands.

# Advanced:
- Can enable per-game autosave on disconnection
- Saved games can be loaded without a replay
- Can mirror the game lobby of a third party cross-realms
- Can listen to port 6112 and forward network traffic to another IP:PORT
- Can use UDP strict mode to more accurately simulate game clients
- Can forward PvPGN realms' game lists to another IP:PORT

# Other:
- Uses DotA stats automatically according to maps' file names
- Identifies Evergreen maps automatically according to maps' file names
- Many commands were implemented. See the full list at [3]

# Technical changes:
- Uses C++17
- Has a Windows 64-bit build
- Has an Ubuntu 24 CI build
- Implements a command-line interface (CLI)
- Implements integration with Windows Explorer
- Higher modularization
- Statically analyzed with clang-analyzer and cppcheck
- Using aggressive optimizations
- IPv6 supported
- Unified commands system
- Stricter chat queue system
- Maps are more strictly parsed and sanitized
- Loaded map metadata is cached
- Supports various log levels
- Docs automatically generated
- Updated libraries: StormLib, SQLite, zlib
- Boost is no longer required
- MySQL is no longer required
- Uses SQLite and a simpler database schema.

# Reworked features from [GHost++][2]:
- Admin game:
  There is no specific setting to host a game that grants unlimited permissions to
  anyone who knows or guesses the password transmitted through insecure TCP.

  Instead, the CLI flag `--replaceable` can be provided when hosting a game. 
  This allows players that join the game to use the `!host` command inside to create 
  another game lobby, thereafter replacing the original lobby.

  When the new game lobby starts, if the CLI flag `--auto-rehost` was provided,
  the replaceable lobby will be created again.

# Removed features from [GHost++][2]:
- CASC:
  Bots owners are encouraged to extract the good old MPQ files (i.e., `.w3x`.) and place
  them in the `<bot.maps_path>` folder.

  Much of the same applies for necessary `Blizzard.j` and `common.j`, which should be placed 
  in the `<bot.jass_path>` folder.

- Localization:
  I like localization, but it's quite a bit of work. PRs are welcome.

- Replays:
  Please open an issue if this is important to you.

[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://github.com/uakfdotb/ghostpp
[3]: https://gitlab.com/ivojulca/aura-bot/COMMANDS.md
