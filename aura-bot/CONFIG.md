Config
==========
# Supported config keys
## \`bot.exit_on_standby\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`bot.greeting_path\`
- Type: path
- Error handling: Use default value

## \`bot.home_path.allow_mismatch\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`bot.jass_path\`
- Type: directory
- Default value: Aura home directory
- Error handling: Use default value

## \`bot.keywords.sudo\`
- Type: string
- Default value: sudo
- Error handling: Use default value

## \`bot.latency\`
- Type: uint16
- Default value: 100
- Error handling: Use default value

## \`bot.latency.equalizer.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`bot.latency.equalizer.frames\`
- Type: uint8
- Default value: PING_EQUALIZER_DEFAULT_FRAMES
- Error handling: Use default value

## \`bot.load_maps.cache.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`bot.load_maps.cache.revalidation.algorithm\`
- Type: enum
- Default value: CACHE_REVALIDATION_MODIFIED
- Error handling: Use default value

## \`bot.load_maps.show_suggestions\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`bot.load_maps.strict_search\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`bot.log_level\`
- Type: enum
- Default value: LOG_LEVEL_INFO - 1
- Error handling: Use default value

## \`bot.log_level\`
- Type: enum
- Default value: LOG_LEVEL_INFO - 1
- Error handling: Use default value

## \`bot.map_cache_path\`
- Type: directory
- Default value: Aura home directory
- Error handling: Use default value

## \`bot.map_configs_path\`
- Type: directory
- Default value: Aura home directory
- Error handling: Use default value

## \`bot.maps_path\`
- Type: directory
- Default value: Aura home directory
- Error handling: Use default value

## \`bot.perf_limit\`
- Type: uint32
- Default value: 150
- Error handling: Use default value

## \`bot.persistence.delete_huge_maps.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`bot.persistence.delete_huge_maps.size\`
- Type: int
- Default value: 25600
- Error handling: Use default value

## \`bot.save_path\`
- Type: directory
- Default value: Aura home directory
- Error handling: Use default value

## \`bot.toggle_every_realm\`
- Type: bool
- Error handling: Use default value

## \`db.game_stats.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`db.storage_file\`
- Type: path
- Default value: Aura home directory
- Error handling: Use default value

## \`db.wal_autocheckpoint\`
- Type: uint16
- Default value: 1000
- Error handling: Use default value

## \`discord.commands.admin.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`discord.commands.bot_owner.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`discord.commands.common.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`discord.commands.hosting.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`discord.commands.moderator.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`discord.direct_messages.list\`
- Type: uint64set
- Default value: 
- Error handling: Use default value

## \`discord.direct_messages.mode\`
- Type: enum
- Default value: FILTER_ALLOW_ALL
- Error handling: Use default value

## \`discord.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`discord.host_name\`
- Type: string
- Default value: discord.com
- Error handling: Use default value

## \`discord.invites.list\`
- Type: uint64set
- Default value: 
- Error handling: Use default value

## \`discord.invites.mode\`
- Type: enum
- Default value: FILTER_ALLOW_ALL
- Error handling: Use default value

## \`discord.sudo_users\`
- Type: uint64set
- Default value: 
- Error handling: Use default value

## \`discord.unverified_users.reject_commands\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`game_data.twrpg_path\`
- Type: path
- Default value: Aura home directory
- Error handling: Use default value

## \`game.extract_jass.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`game.install_path\`
- Type: directory
- Error handling: Use default value

## \`game.version\`
- Type: uint8
- Error handling: Abort operation

## \`global_realm.admins\`
- Type: setinsensitive
- Default value: Empty
- Error handling: Use default value

## \`global_realm.admins\`
- Type: setinsensitive
- Default value: Empty
- Error handling: Use default value

## \`global_realm.announce_chat\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`global_realm.announce_chat\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.auth_custom\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.auth_custom\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.auth_exe_version\`
- Type: uint8vector
- Error handling: Abort operation

## \`global_realm.auth_exe_version\`
- Type: uint8vector
- Error handling: Abort operation

## \`global_realm.auth_exe_version_hash\`
- Type: uint8vector
- Error handling: Abort operation

## \`global_realm.auth_exe_version_hash\`
- Type: uint8vector
- Error handling: Abort operation

## \`global_realm.auth_game_version\`
- Type: uint8
- Error handling: Use default value

## \`global_realm.auth_game_version\`
- Type: uint8
- Error handling: Use default value

## \`global_realm.auth_password_hash_type\`
- Type: enum
- Default value: pvpgn
- Error handling: Use default value

## \`global_realm.auth_password_hash_type\`
- Type: enum
- Default value: Empty
- Error handling: Use default value

## \`global_realm.auto_register\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.auto_register\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.bind_address\`
- Type: address
- Error handling: Use default value

## \`global_realm.bind_address\`
- Type: address
- Error handling: Use default value

## \`global_realm.canonical_name\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.cd_key.roc\`
- Type: string
- Constraints: Min length: 26. Max length: 26.
- Default value: FFFFFFFFFFFFFFFFFFFFFFFFFF
- Error handling: Use default value

## \`global_realm.cd_key.tft\`
- Type: string
- Constraints: Min length: 26. Max length: 26.
- Default value: FFFFFFFFFFFFFFFFFFFFFFFFFF
- Error handling: Use default value

## \`global_realm.commands.admin.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`global_realm.commands.admin.permissions\`
- Type: enum
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.bot_owner.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`global_realm.commands.bot_owner.permissions\`
- Type: enum
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.broadcast.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.commands.broadcast.enabled\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.broadcast.trigger\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.common.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`global_realm.commands.common.permissions\`
- Type: enum
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.hosting.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`global_realm.commands.hosting.permissions\`
- Type: enum
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.moderator.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`global_realm.commands.moderator.permissions\`
- Type: enum
- Default value: Empty
- Error handling: Use default value

## \`global_realm.commands.trigger\`
- Type: string
- Default value: !
- Error handling: Use default value

## \`global_realm.commands.trigger\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.country\`
- Type: string
- Default value: Peru
- Error handling: Use default value

## \`global_realm.country\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.country_short\`
- Type: string
- Default value: PER
- Error handling: Use default value

## \`global_realm.country_short\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.custom_ip_address.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.custom_ip_address.enabled\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.custom_ip_address.value\`
- Type: addressipv4
- Default value: 0.0.0.0
- Error handling: Abort operation

## \`global_realm.custom_ip_address.value\`
- Type: addressipv4
- Error handling: Abort operation

## \`global_realm.custom_port.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.custom_port.enabled\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.custom_port.value\`
- Type: uint16
- Default value: 6112
- Error handling: Abort operation

## \`global_realm.custom_port.value\`
- Type: uint16
- Default value: Empty
- Error handling: Abort operation

## \`global_realm.db_id\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`global_realm.enabled\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.first_channel\`
- Type: string
- Default value: The Void
- Error handling: Use default value

## \`global_realm.first_channel\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.flood.immune\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.flood.immune\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.flood.lines\`
- Type: uint8
- Default value: 5
- Error handling: Use default value

## \`global_realm.flood.lines\`
- Type: uint8
- Default value: Empty
- Error handling: Use default value

## \`global_realm.flood.max_size\`
- Type: uint16
- Default value: 200
- Error handling: Use default value

## \`global_realm.flood.max_size\`
- Type: uint16
- Default value: Empty
- Error handling: Use default value

## \`global_realm.flood.time\`
- Type: uint8
- Default value: 5
- Error handling: Use default value

## \`global_realm.flood.time\`
- Type: uint8
- Default value: Empty
- Error handling: Use default value

## \`global_realm.flood.wrap\`
- Type: uint16
- Default value: 40
- Error handling: Use default value

## \`global_realm.flood.wrap\`
- Type: uint16
- Default value: Empty
- Error handling: Use default value

## \`global_realm.game_host.throttle\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`global_realm.game_host.throttle\`
- Type: bool
- Default value: !m_IsHostOften
- Error handling: Use default value

## \`global_realm.game_host.unique\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`global_realm.game_host.unique\`
- Type: bool
- Default value: !m_IsHostMulti
- Error handling: Use default value

## \`global_realm.game_prefix\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.game_prefix\`
- Type: string
- Constraints: Min length: 0. Max length: 16.
- Default value: Empty
- Error handling: Use default value

## \`global_realm.host_name\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.input_id\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.locale\`
- Type: string
- Default value: system
- Error handling: Use default value

## \`global_realm.locale\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.logs.console.chat\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`global_realm.logs.console.chat\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.main\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.main\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.map_transfers.max_size\`
- Type: int
- Default value: Empty
- Error handling: Use default value

## \`global_realm.map_transfers.max_size\`
- Type: int
- Default value: Empty
- Error handling: Use default value

## \`global_realm.mirror\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.mirror\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.password\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.password\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.password.case_sensitive\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.password.case_sensitive\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.protocol.whisper.error_reply\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.protocol.whisper.error_reply\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.queries.games_list.enabled\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.queries.games_list.enabled\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.rehoster\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.rehoster\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.server_port\`
- Type: uint16
- Default value: 6112
- Error handling: Use default value

## \`global_realm.server_port\`
- Type: uint16
- Default value: Empty
- Error handling: Use default value

## \`global_realm.sudo_users\`
- Type: setinsensitive
- Default value: Empty
- Error handling: Use default value

## \`global_realm.sudo_users\`
- Type: setinsensitive
- Default value: Empty
- Error handling: Use default value

## \`global_realm.unique_name\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.unverified_users.always_verify\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.unverified_users.always_verify\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.unverified_users.auto_kick\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.unverified_users.auto_kick\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.unverified_users.reject_commands\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.unverified_users.reject_commands\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.unverified_users.reject_start\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.unverified_users.reject_start\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.username\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.username\`
- Type: string
- Default value: Empty
- Error handling: Use default value

## \`global_realm.username.case_sensitive\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.username.case_sensitive\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`global_realm.vpn\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`global_realm.vpn\`
- Type: bool
- Default value: Empty
- Error handling: Use default value

## \`hosting.autostart.requires_balance\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`hosting.commands.broadcast.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.commands.trigger\`
- Type: string
- Default value: !
- Error handling: Use default value

## \`hosting.crossplay.versions\`
- Type: uint8set
- Default value: '
- Error handling: Use default value

## \`hosting.desync.handler\`
- Type: enum
- Default value: ON_DESYNC_NOTIFY
- Error handling: Use default value

## \`hosting.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`hosting.expiry.loading.mode\`
- Type: enum
- Default value: GAME_LOADING_TIMEOUT_STRICT
- Error handling: Use default value

## \`hosting.expiry.loading.timeout\`
- Type: uint32
- Default value: 900
- Error handling: Use default value

## \`hosting.expiry.lobby.mode\`
- Type: enum
- Default value: LOBBY_TIMEOUT_OWNERLESS
- Error handling: Use default value

## \`hosting.expiry.lobby.timeout\`
- Type: uint32
- Default value: 600
- Error handling: Use default value

## \`hosting.expiry.owner.lan\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`hosting.expiry.owner.mode\`
- Type: enum
- Default value: LOBBY_OWNER_TIMEOUT_ABSENT
- Error handling: Use default value

## \`hosting.expiry.owner.timeout\`
- Type: uint32
- Default value: 120
- Error handling: Use default value

## \`hosting.expiry.playing.mode\`
- Type: enum
- Default value: GAME_PLAYING_TIMEOUT_STRICT
- Error handling: Use default value

## \`hosting.expiry.playing.timeout\`
- Type: uint32
- Default value: 18000
- Error handling: Use default value

## \`hosting.expiry.playing.timeout.eager_interval\`
- Type: uint32
- Default value: 1200
- Error handling: Use default value

## \`hosting.expiry.playing.timeout.eager_warnings\`
- Type: uint8
- Default value: 3
- Error handling: Use default value

## \`hosting.expiry.playing.timeout.soon_interval\`
- Type: uint32
- Default value: 60
- Error handling: Use default value

## \`hosting.expiry.playing.timeout.soon_warnings\`
- Type: uint8
- Default value: 10
- Error handling: Use default value

## \`hosting.game_over.player_count\`
- Type: uint8
- Default value: 1
- Error handling: Use default value

## \`hosting.game_owner.from_creator\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`hosting.game_ready.mode\`
- Type: enum
- Default value: READY_MODE_EXPECT_RACE
- Error handling: Use default value

## \`hosting.game_start.count_down_interval\`
- Type: uint32
- Default value: 500
- Error handling: Use default value

## \`hosting.game_start.count_down_ticks\`
- Type: uint32
- Default value: 5
- Error handling: Use default value

## \`hosting.games_quota.auto_rehost.conservative\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.games_quota.max_join_in_progress\`
- Type: int
- Default value: 0
- Error handling: Use default value

## \`hosting.games_quota.max_lobbies\`
- Type: int
- Default value: 1
- Error handling: Use default value

## \`hosting.games_quota.max_started\`
- Type: int
- Default value: 20
- Error handling: Use default value

## \`hosting.games_quota.max_total\`
- Type: int
- Default value: 20
- Error handling: Use default value

## \`hosting.geolocalization.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`hosting.high_ping.kick_ms\`
- Type: uint32
- Default value: 250
- Error handling: Use default value

## \`hosting.high_ping.safe_ms\`
- Type: uint32
- Default value: 130
- Error handling: Use default value

## \`hosting.high_ping.warn_ms\`
- Type: uint32
- Default value: 175
- Error handling: Use default value

## \`hosting.index.creator_name\`
- Type: string
- Constraints: Min length: 1. Max length: 15.
- Default value: 
- Error handling: Use default value

## \`hosting.ip_filter.flood_handler\`
- Type: enum
- Default value: ON_IPFLOOD_DENY
- Error handling: Use default value

## \`hosting.ip_filter.max_loopback\`
- Type: uint8
- Default value: 8
- Error handling: Use default value

## \`hosting.ip_filter.max_same_ip\`
- Type: uint8
- Default value: 8
- Error handling: Use default value

## \`hosting.join_in_progress.observers\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.join_in_progress.players\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.load_in_game.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.log_commands\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.log_delay\`
- Type: uint32
- Default value: 180
- Error handling: Use default value

## \`hosting.log_words\`
- Type: setinsensitive
- Default value: 
- Error handling: Use default value

## \`hosting.map_downloads.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.map_downloads.repositories\`
- Type: setinsensitive
- Default value: "epicwar" "wc3maps"
- Error handling: Use default value

## \`hosting.map_downloads.timeout\`
- Type: uint32
- Default value: 15000
- Error handling: Use default value

## \`hosting.map_transfers.max_parallel_packets\`
- Type: uint32
- Default value: 1000
- Error handling: Use default value

## \`hosting.map_transfers.max_players\`
- Type: uint32
- Default value: 3
- Error handling: Use default value

## \`hosting.map_transfers.max_size\`
- Type: uint32
- Default value: 8192
- Error handling: Use default value

## \`hosting.map_transfers.max_speed\`
- Type: uint32
- Default value: 1024
- Error handling: Use default value

## \`hosting.map_transfers.mode\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`hosting.map.missing.kick_delay\`
- Type: uint32
- Default value: 60
- Error handling: Use default value

## \`hosting.name_filter.is_pipe_harmful\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`hosting.name_filter.unsafe_handler\`
- Type: enum
- Default value: ON_UNSAFE_NAME_DENY
- Error handling: Use default value

## \`hosting.namepace.first_game_id\`
- Type: int
- Default value: 100
- Error handling: Use default value

## \`hosting.nicknames.hide_in_game\`
- Type: enum
- Default value: HIDE_IGN_AUTO
- Error handling: Use default value

## \`hosting.nicknames.hide_lobby\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`hosting.realm_broadcast.error_handler\`
- Type: enum
- Default value: ON_ADV_ERROR_EXIT_ON_MAX_ERRORS
- Error handling: Use default value

## \`hosting.self.virtual_player.name\`
- Type: string
- Constraints: Min length: 1. Max length: 15.
- Default value: |cFF4080C0Aura
- Error handling: Use default value

## \`hosting.vote_kick.min_percent\`
- Type: uint8
- Default value: 70
- Error handling: Use default value

## \`irc.admins\`
- Type: set
- Default value: Empty
- Error handling: Use default value

## \`irc.channels\`
- Type: list
- Default value: Empty
- Error handling: Use default value

## \`irc.commands.admin.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`irc.commands.bot_owner.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`irc.commands.broadcast.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`irc.commands.common.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`irc.commands.hosting.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`irc.commands.moderator.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`irc.commands.trigger\`
- Type: string
- Default value: !
- Error handling: Use default value

## \`irc.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`irc.port\`
- Type: uint16
- Default value: 6667
- Error handling: Use default value

## \`irc.sudo_users\`
- Type: set
- Default value: Empty
- Error handling: Use default value

## \`irc.unverified_users.reject_commands\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`lan_realm.commands.admin.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`lan_realm.commands.bot_owner.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`lan_realm.commands.common.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`lan_realm.commands.hosting.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`lan_realm.commands.moderator.permissions\`
- Type: enum
- Default value: auto
- Error handling: Use default value

## \`metrics.ping.use_rtt\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`metrics.ping.use_tcpinfo\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`metrics.ping.use_tcpinfo\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`monitor.hosting.on_start.check_connectivity\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.bind_address\`
- Type: addressipv4
- Default value: 0.0.0.0
- Error handling: Abort operation

## \`net.bind_address6\`
- Type: addressipv6
- Default value: ::
- Error handling: Abort operation

## \`net.game_discovery.udp.broadcast.address\`
- Type: addressipv4
- Default value: 255.255.255.255
- Error handling: Abort operation

## \`net.game_discovery.udp.broadcast.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.game_discovery.udp.broadcast.strict\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.game_discovery.udp.do_not_route\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.game_discovery.udp.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.game_discovery.udp.extra_clients.ip_addresses\`
- Type: hostlistwithimplicitport
- Default value: '
- Error handling: Use default value

## \`net.game_discovery.udp.ipv6.target_port\`
- Type: uint16
- Default value: 5678
- Error handling: Use default value

## \`net.game_discovery.udp.tcp4_custom_port.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.game_discovery.udp.tcp4_custom_port.value\`
- Type: uint16
- Default value: 6112
- Error handling: Use default value

## \`net.game_discovery.udp.tcp6_custom_port.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.game_discovery.udp.tcp6_custom_port.value\`
- Type: uint16
- Default value: 5678
- Error handling: Use default value

## \`net.has_buffer_bloat\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.host_port.max\`
- Type: uint16
- Default value: \<net.host_port.min\>
- Error handling: Use default value

## \`net.host_port.min\`
- Type: uint16
- Default value: \<net.host_port.only\>
- Error handling: Use default value

## \`net.host_port.only\`
- Type: uint16
- Error handling: Use default value

## \`net.ipv4.public_address.algorithm\`
- Type: string
- Default value: api
- Error handling: Use default value

## \`net.ipv4.public_address.value\`
- Type: addressipv4
- Default value: 0.0.0.0
- Error handling: Abort operation

## \`net.ipv4.public_address.value\`
- Type: string
- Default value: http://api.ipify.org
- Error handling: Abort operation

## \`net.ipv6.public_address.algorithm\`
- Type: string
- Default value: api
- Error handling: Use default value

## \`net.ipv6.public_address.value\`
- Type: addressipv6
- Default value: ::
- Error handling: Abort operation

## \`net.ipv6.public_address.value\`
- Type: string
- Default value: http://api6.ipify.org
- Error handling: Abort operation

## \`net.ipv6.tcp.announce_chat\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.ipv6.tcp.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.port_forwarding.upnp.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.port_forwarding.upnp.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.start_lag.sync_limit\`
- Type: uint32
- Default value: 32
- Error handling: Use default value

## \`net.stop_lag.sync_limit\`
- Type: uint32
- Default value: 8
- Error handling: Use default value

## \`net.sync_normalization.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.tcp_extensions.gproxy_legacy.reconnect_wait\`
- Type: uint16
- Default value: 3
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.announce_chat\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.basic.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.long.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.reconnect_wait\`
- Type: uint16
- Default value: 5
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.site\`
- Type: string
- Default value: https://www.mymgn.com/gproxy/
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.vlan.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.tcp_extensions.gproxy.vlan.port\`
- Type: uint16
- Default value: onlyHostPort.value_or(6112u
- Error handling: Use default value

## \`net.tcp_extensions.udp_tunnel.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.udp_fallback.outbound_port\`
- Type: uint16
- Default value: 6113
- Error handling: Use default value

## \`net.udp_ipv6.enabled\`
- Type: bool
- Default value: true
- Error handling: Use default value

## \`net.udp_ipv6.port\`
- Type: uint16
- Default value: 6110
- Error handling: Use default value

## \`net.udp_redirect.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.udp_redirect.ip_address\`
- Type: address
- Default value: 127.0.0.1
- Error handling: Abort operation

## \`net.udp_redirect.ip_address\`
- Type: addressipv4
- Default value: 127.0.0.1
- Error handling: Abort operation

## \`net.udp_redirect.port\`
- Type: uint16
- Default value: 6110
- Error handling: Abort operation

## \`net.udp_redirect.realm_game_lists.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`net.udp_server.block_list\`
- Type: ipstringset
- Default value: 
- Error handling: Use default value

## \`net.udp_server.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`ui.notify_joins.enabled\`
- Type: bool
- Default value: false
- Error handling: Use default value

## \`ui.notify_joins.exceptions\`
- Type: set
- Default value: 
- Error handling: Use default value
