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

#ifndef AURA_CONFIG_BOT_H_
#define AURA_CONFIG_BOT_H_

#include "../includes.h"
#include "config.h"
#include "config_commands.h"

//
// CBotConfig
//

struct CBotConfig
{
  bool                                    m_Enabled;                     // set to false to prevent new games from being created
  bool                                    m_ExtractJASS;
  std::optional<uint8_t>                  m_War3Version;                 // warcraft 3 version
  std::optional<std::filesystem::path>    m_Warcraft3Path;               // Warcraft 3 path
  std::filesystem::path                   m_MapPath;                     // map path
  std::filesystem::path                   m_MapCFGPath;                  // map config path
  std::filesystem::path                   m_MapCachePath;                // map cache path
  std::filesystem::path                   m_JASSPath;                    // JASS files path
  std::filesystem::path                   m_GameSavePath;                // save files path

  std::filesystem::path                   m_AliasesPath;                 // aliases path
  std::filesystem::path                   m_LogPath;                     // aliases path

  std::filesystem::path                   m_GreetingPath;                // the path of the greeting the bot sends to all players joining a game
  std::vector<std::string>                m_Greeting;                    // read from m_GreetingPath

  uint32_t                                m_MinHostCounter;              // defines a subspace for game identifiers

  uint32_t                                m_MaxLobbies;                  // maximum number of non-started games
  uint32_t                                m_MaxStartedGames;             // maximum number of games in progress
  uint32_t                                m_MaxJoinInProgressGames;           // maximum number of games in progress that can be watched
  uint32_t                                m_MaxTotalGames;               // maximum sum of all active games
  bool                                    m_AutoRehostQuotaConservative;

  bool                                    m_AutomaticallySetGameOwner;   // whether the game creator should automatically be set as game owner
  bool                                    m_EnableDeleteOversizedMaps;   // may delete maps in m_MapPath exceeding m_MaxSavedMapSize
  uint32_t                                m_MaxSavedMapSize;             // maximum byte size of maps kept persistently in the m_MapPath folder

  bool                                    m_StrictSearch;                // accept only exact paths (no fuzzy searches) for maps, etc.
  bool                                    m_MapSearchShowSuggestions;
  bool                                    m_EnableCFGCache;              // save read map CFGs to disk
  uint8_t                                 m_CFGCacheRevalidateAlgorithm; // always, never, modified

  CCommandConfig*                         m_LANCommandCFG;

  uint8_t                                 m_LogLevel;
  bool                                    m_ExitOnStandby;
  std::optional<bool>                     m_EnableBNET;                  // master switch to enable/disable ALL bnet configs on startup

  std::string                             m_SudoKeyWord;                 // something to send as confirmation for !su commands
  
  explicit CBotConfig(CConfig& CFG);
  ~CBotConfig();

  void Reset();
};

#endif
