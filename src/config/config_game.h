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

#ifndef AURA_CONFIG_GAME_H_
#define AURA_CONFIG_GAME_H_

#include "../includes.h"
#include "config.h"
#include "../game_setup.h"
#include "../map.h"

//
// CGameConfig
//

// Override order
// Default game config
// Default map config (*) map.flags_locked = yes/no
// Game setup

struct CGameConfig
{
  uint8_t                  m_VoteKickPercentage;         // percentage of players required to vote yes for a votekick to pass
  uint8_t                  m_NumPlayersToStartGameOver;  // when this player count is reached, the game over timer will start
  uint8_t                  m_MaxPlayersLoopback;
  uint8_t                  m_MaxPlayersSameIP;
  uint8_t                  m_PlayersReadyMode;
  bool                     m_AutoStartRequiresBalance;
  bool                     m_SaveStats;
  
  uint32_t                 m_SyncLimit;                  // the maximum number of packets a user can fall out of sync before starting the lag screen (by default)
  uint32_t                 m_SyncLimitSafe;              // the maximum number of packets a user can fall out of sync before starting the lag screen (by default)
  bool                     m_SyncNormalize;              // before 3-minute mark, try to keep players in the game
  uint32_t                 m_AutoKickPing;               // auto kick players with ping higher than this
  uint32_t                 m_WarnHighPing;               // announce on chat when players have a ping higher than this value
  uint32_t                 m_SafeHighPing;               // when players ping drops below this value, announce they no longer have high ping

  uint8_t                  m_LobbyTimeoutMode;
  uint8_t                  m_LobbyOwnerTimeoutMode;
  uint8_t                  m_LoadingTimeoutMode;
  uint8_t                  m_PlayingTimeoutMode;

  uint32_t                 m_LobbyTimeout;               // auto close the game lobby after this many minutes without any owner
  uint32_t                 m_LobbyOwnerTimeout;          // relinquish game ownership after this many minutes
  uint32_t                 m_LoadingTimeout;             // relinquish game ownership after this many minutes
  uint32_t                 m_PlayingTimeout;             // relinquish game ownership after this many minutes

  uint8_t                  m_PlayingTimeoutWarningShortCountDown;
  uint32_t                 m_PlayingTimeoutWarningShortInterval;
  uint8_t                  m_PlayingTimeoutWarningLargeCountDown;
  uint32_t                 m_PlayingTimeoutWarningLargeInterval;

  bool                     m_LobbyOwnerReleaseLANLeaver;

  uint32_t                 m_LobbyCountDownInterval;     // ms between each number count down when !start is issued
  uint32_t                 m_LobbyCountDownStartValue;   // number at which !start count down begins

  uint16_t                 m_Latency;                    // the game refresh latency (by default)
  bool                     m_LatencyEqualizerEnabled;    // whether to add a minimum delay proportional to m_Latency to all actions sent by players
  uint8_t                  m_LatencyEqualizerFrames;     // how many frames should the latency equalizer use

  uint32_t                 m_PerfThreshold;              // the max expected delay between updates - if exceeded it means performance is suffering
  uint32_t                 m_LacksMapKickDelay;
  uint32_t                 m_LogDelay;

  bool                          m_CheckJoinable;
  std::vector<sockaddr_storage> m_ExtraDiscoveryAddresses;    // list of addresses Aura announces hosted games to through UDP unicast.
  uint8_t                       m_ReconnectionMode;

  std::string              m_PrivateCmdToken;            // a symbol prefix to identify commands and send a private reply
  std::string              m_BroadcastCmdToken;          // a symbol prefix to identify commands and send the reply to everyone
  bool                     m_EnableBroadcast;
  std::string              m_IndexVirtualHostName;       // index virtual host name
  std::string              m_LobbyVirtualHostName;       // lobby virtual host name
  bool                     m_NotifyJoins;                // whether the bot should beep when a user joins a hosted game
  std::set<std::string>    m_IgnoredNotifyJoinPlayers;
  bool                     m_HideLobbyNames;
  uint8_t                  m_HideInGameNames;
  bool                     m_LoadInGame;
  bool                     m_EnableJoinObserversInProgress;
  bool                     m_EnableJoinPlayersInProgress;

  std::set<std::string>    m_LoggedWords;
  bool                     m_LogCommands;
  uint8_t                  m_DesyncHandler;
  uint8_t                  m_IPFloodHandler;
  uint8_t                  m_UnsafeNameHandler;         // whether to mutilate user names when they contain unsafe characters, or deny entry, or not to care
  uint8_t                  m_BroadcastErrorHandler;     // under which circumstances should a game lobby be closed given that we failed to announce the game in one or more realms
  bool                     m_PipeConsideredHarmful;

  bool                     m_UDPEnabled;                 // whether this game should be listed in "Local Area Network"
  std::vector<uint8_t>     m_SupportedGameVersions;

  explicit CGameConfig(CConfig& CFG);
  explicit CGameConfig(CGameConfig* nRootConfig, std::shared_ptr<CMap> nMap, std::shared_ptr<CGameSetup> nGameSetup);
  ~CGameConfig();
};

#endif
