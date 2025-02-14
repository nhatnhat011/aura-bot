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

#ifndef AURA_CONFIG_REALM_H_
#define AURA_CONFIG_REALM_H_

#include "../includes.h"
#include "config.h"
#include "config_net.h"
#include "config_commands.h"
#include "../socket.h"

//
// CRealmConfig
//

struct CRealmConfig
{
  // Automatically-assigned values
  uint8_t m_ServerIndex;                         // unique server ID to identify players' realms through host counters (may be recycled on !reload)
  std::string m_CFGKeyPrefix;                    // 

  // Inheritable
  uint8_t m_InheritOnlyCommandCommonBasePermissions;
  uint8_t m_InheritOnlyCommandHostingBasePermissions;
  uint8_t m_InheritOnlyCommandModeratorBasePermissions;
  uint8_t m_InheritOnlyCommandAdminBasePermissions;
  uint8_t m_InheritOnlyCommandBotOwnerBasePermissions;

  // Inheritable
  bool m_UnverifiedRejectCommands;
  bool m_UnverifiedCannotStartGame;
  bool m_UnverifiedAutoKickedFromLobby;
  bool m_AlwaysSpoofCheckPlayers;

  CCommandConfig* m_CommandCFG;

  // Inheritable
  bool        m_Enabled;
  std::optional<sockaddr_storage> m_BindAddress; // the local address from which we connect

  // Not inheritable

  std::string m_InputID;                         // for IRC commands
  std::string m_UniqueName;                      // displayed on the console
  std::string m_CanonicalName;                   // displayed on game rooms
  std::string m_DataBaseID;                      // server ID for database queries
  std::string m_CDKeyROC;
  std::string m_CDKeyTFT;

  // Inheritable

  std::string m_CountryShort;                    // 2-3 letter country code for pvpgn servers
  std::string m_Country;                         // country name
  std::string m_Locale;                          // locale used: numeric or "system"
  uint32_t m_LocaleID;                           // see: http://msdn.microsoft.com/en-us/library/0h88fahh%28VS.85%29.aspx

  std::string m_PrivateCmdToken;                 // a symbol prefix to identify commands and send a private reply
  std::string m_BroadcastCmdToken;               // a symbol prefix to identify commands and send the reply to everyone
  bool m_EnableBroadcast;
  bool m_AnnounceHostToChat;
  bool m_IsMain;
  bool m_IsReHoster;
  bool m_IsMirror;
  bool m_IsVPN;

  bool m_IsHostOften;
  bool m_IsHostMulti;

  bool m_EnableCustomAddress;                    // enable to make peers from pvpgn servers connect to m_PublicHostAddress
  sockaddr_storage m_PublicHostAddress;          // the address to broadcast in pvpgn servers

  bool m_EnableCustomPort;                       // enable to make peers from pvpgn servers connect to m_PublicHostPort
  uint16_t m_PublicHostPort;                     // the port to broadcast in pvpgn servers

  std::string m_HostName;                        // server address to connect to
  uint16_t    m_ServerPort;

  bool m_AutoRegister;
  bool m_UserNameCaseSensitive;
  bool m_PassWordCaseSensitive;
  std::string m_UserName;                        //
  std::string m_PassWord;                        //

  bool m_AuthUseCustomVersion;
  uint8_t m_AuthPasswordHashType;                         // pvpgn or battle.net

  std::optional<uint8_t> m_AuthWar3Version;                  // WC3 minor version
  std::optional<std::vector<uint8_t>> m_AuthExeVersion;       // 4 bytes: WC3 version as {patch, minor, major, build}
  std::optional<std::vector<uint8_t>> m_AuthExeVersionHash;   // 4 bytes
  std::string m_AuthExeInfo;                                  // filename.exe dd/MM/yy hh::mm:ss size

  std::string m_FirstChannel;                    //
  std::set<std::string> m_SudoUsers;             //
  std::set<std::string> m_Admins;            //
  std::string m_GamePrefix;                      // string prepended to game names
  uint32_t m_MaxUploadSize;                      // in KB

  bool m_ConsoleLogChat;                         // whether we should print public chat messages
  uint8_t m_FloodQuotaLines;                     // - PvPGN: corresponds to bnetd.conf: quota_lines (default 5)
  uint8_t m_FloodQuotaTime;                      // - PvPGN: corresponds to bnetd.conf: quota_time (default 5)
  uint16_t m_VirtualLineLength;                  // - PvPGN: corresponds to bnetd.conf: quota_wrapline (default 40)
  uint16_t m_MaxLineLength;                      // - PvPGN: corresponds to bnetd.conf: quota_maxline (default 200)
  bool m_FloodImmune;                            // whether we are allowed to send unlimited commands to the server - PvPGN: corresponds to lua/confg.lua: flood_immunity, or ghost_bots

  std::string m_WhisperErrorReply;

  bool m_QueryGameLists;                         // whether we should periodically request a list of hosted games

  CRealmConfig(CConfig& CFG, CNetConfig* nNetConfig);
  CRealmConfig(CConfig& CFG, CRealmConfig* nRootConfig, uint8_t nServerIndex);
  ~CRealmConfig();

  void Reset();
};

#endif
