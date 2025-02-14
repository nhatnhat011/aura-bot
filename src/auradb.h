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

/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT

 */

#ifndef AURA_AURADB_H_
#define AURA_AURADB_H_

#include "includes.h"

#include <filesystem>

#include <sqlite3/sqlite3.h>

#define MAP_TYPE_NONE 0
#define MAP_TYPE_MELEE 1
#define MAP_TYPE_DOTA 2
#define MAP_TYPE_TWRPG 3

#define MAP_DATA_TYPE_NONE 0u
#define MAP_DATA_TYPE_UNIT 1u
#define MAP_DATA_TYPE_ITEM 2u
#define MAP_DATA_TYPE_DESTRUCTABLE 3u
// ... Others
#define MAP_DATA_TYPE_ANY 255u

#define FROM_ADD_IDX 0u
#define FROM_CHECK_IDX 1u
#define LATEST_GAME_IDX 2u
#define ALIAS_ADD_IDX 3u
#define ALIAS_CHECK_IDX 4u
#define USER_BAN_CHECK_IDX 5u
#define IP_BAN_CHECK_IDX 6u
#define MODERATOR_CHECK_IDX 7u
#define GAME_ADD_IDX 8u
#define PLAYER_SUMMARY_IDX 9u
#define UPDATE_PLAYER_START_IDX 10u
#define UPDATE_PLAYER_END_IDX 11u
#define STMT_CACHE_SIZE 12u

/**************
 *** SCHEMA ***
 **************

CREATE TABLE moderators (
    name TEXT NOT NULL,
    server TEXT NOT NULL DEFAULT "",
    PRIMARY KEY ( name, server )
)

CREATE TABLE bans (
    name TEXT NOT NULL,
    server TEXT NOT NULL,
    authserver TEXT NOT NULL,
    ip TEXT NOT NULL,
    date TEXT NOT NULL,
    expiry TEXT NOT NULL,
    permanent INTEGER DEFAULT 0,
    moderator TEXT NOT NULL,
    reason TEXT,
    PRIMARY KEY ( name, server, authserver )
)

CREATE TABLE players (
    name TEXT NOT NULL,
    server TEXT NOT NULL,
    initialip TEXT NOT NULL,
    latestip TEXT NOT NULL,
    initialreport TEXT,
    reports INTEGER,
    latestgame INTEGER,
    games INTEGER,
    dotas INTEGER,
    loadingtime INTEGER,
    duration INTEGER,
    left INTEGER,
    wins INTEGER,
    losses INTEGER,
    kills INTEGER,
    deaths INTEGER,
    creepkills INTEGER,
    creepdenies INTEGER,
    assists INTEGER,
    neutralkills INTEGER,
    towerkills INTEGER,
    raxkills INTEGER,
    courierkills INTEGER,
    PRIMARY KEY ( name, server )
)

CREATE TABLE config (
    name TEXT NOT NULL PRIMARY KEY,
    value TEXT NOT NULL
)

CREATE TABLE iptocountry (
    ip1 INTEGER NOT NULL,
    ip2 INTEGER NOT NULL,
    country TEXT NOT NULL,
    PRIMARY KEY ( ip1, ip2 )
)

CREATE TABLE aliases (
    alias TEXT NOT NULL PRIMARY KEY,
    value TEXT NOT NULL
)

CREATE TABLE games (
    id INTEGER PRIMARY KEY,
    creator TEXT,
    mapcpath TEXT NOT NULL,
    mapspath TEXT NOT NULL,
    crc32 TEXT NOT NULL,
    replay TEXT,
    playernames TEXT NOT NULL, // comma-separated list of players-names; fake player is empty name
    playerids TEXT NOT NULL, // space-separated bytelist (UIDs, then SIDs, then colors)
    saveids TEXT // space-separated bytelist (UIDs at time of recentmost save)
)

CREATE TABLE commands (
    command TEXT NOT NULL,
    scope TEXT NOT NULL,
    type TEXT NOT NULL,
    action TEXT NOT NULL,
    PRIMARY KEY ( command, scope )
)

 **************
 *** SCHEMA ***
 **************/

//
// CSQLITE3 (wrapper class)
//

struct sqlite3;
struct sqlite3_stmt;

class CSQLITE3
{
private:
  void* m_DB;
  bool  m_Ready;

public:
  explicit CSQLITE3(const std::filesystem::path& filename);
  ~CSQLITE3();
  CSQLITE3(CSQLITE3&) = delete;

  [[nodiscard]] inline bool        GetReady() const { return m_Ready; }
  [[nodiscard]] inline std::string GetError() const { return sqlite3_errmsg(static_cast<sqlite3*>(m_DB)); }

  [[nodiscard]] inline int32_t Step(void* Statement) { return sqlite3_step(static_cast<sqlite3_stmt*>(Statement)); }
  inline int32_t Prepare(const std::string& query, void** Statement, bool forCache = false) {
    if (forCache) {
      return sqlite3_prepare_v3(static_cast<sqlite3*>(m_DB), query.c_str(), -1, SQLITE_PREPARE_PERSISTENT, reinterpret_cast<sqlite3_stmt**>(Statement), nullptr);
    } else {
      return sqlite3_prepare_v2(static_cast<sqlite3*>(m_DB), query.c_str(), -1, reinterpret_cast<sqlite3_stmt**>(Statement), nullptr);
    }
   }
  inline int32_t Prepare(const std::string& query, sqlite3_stmt** Statement, bool forCache = false) {
    if (forCache) {
      return sqlite3_prepare_v3(static_cast<sqlite3*>(m_DB), query.c_str(), -1, SQLITE_PREPARE_PERSISTENT, Statement, nullptr);
    } else {
      return sqlite3_prepare_v2(static_cast<sqlite3*>(m_DB), query.c_str(), -1, Statement, nullptr);
    }
  }
  inline int32_t Finalize(void* Statement) { return sqlite3_finalize(static_cast<sqlite3_stmt*>(Statement)); }
  inline int32_t Reset(void* Statement) { return sqlite3_reset(static_cast<sqlite3_stmt*>(Statement)); }
  inline int32_t Exec(const std::string& query) { return sqlite3_exec(static_cast<sqlite3*>(m_DB), query.c_str(), nullptr, nullptr, nullptr); }
  [[nodiscard]] inline const unsigned char* Column(void* Statement, const uint8_t index) { return sqlite3_column_text(static_cast<sqlite3_stmt*>(Statement), index); }
};

//
// CSearchableMapData
//

class CSearchableMapData
{
public:
  uint8_t m_MapType;
  std::map<uint8_t, std::map<std::string, std::vector<std::string>>> m_Data;
  std::map<std::string, std::pair<uint8_t, std::string>> m_Aliases;
  std::vector<std::string> m_Units;
  std::vector<std::string> m_Items;
  std::vector<std::string> m_Abilities;
  std::vector<std::string> m_Buffs;

  CSearchableMapData(uint8_t nMapType);
  ~CSearchableMapData();

  [[nodiscard]] uint8_t Search(std::string& rwSearchName, const uint8_t searchDataType, const bool exactMatch);
  void LoadData(std::filesystem::path sourcePath);
};

//
// CAuraDB
//

constexpr int64_t SchemaNumber = 3;

#define SCHEMA_CHECK_OK 0u
#define SCHEMA_CHECK_NONE 1u
#define SCHEMA_CHECK_ERRORED 2u
#define SCHEMA_CHECK_INCOMPATIBLE 3u
#define SCHEMA_CHECK_LEGACY_INCOMPATIBLE 4u
#define SCHEMA_CHECK_LEGACY_UPGRADEABLE 5u

#define JOURNAL_MODE_DELETE 0u
#define JOURNAL_MODE_TRUNCATE 1u
#define JOURNAL_MODE_PERSIST 2u
#define JOURNAL_MODE_MEMORY 3u
#define JOURNAL_MODE_WAL 4u
#define JOURNAL_MODE_OFF 5u
#define JOURNAL_MODE_LAST 6u
#define JOURNAL_MODE_INVALID 0xFFu

#define SYNCHRONOUS_OFF 0u
#define SYNCHRONOUS_NORMAL 1u
#define SYNCHRONOUS_FULL 2u
#define SYNCHRONOUS_EXTRA 3u
#define SYNCHRONOUS_LAST 4u
#define SYNCHRONOUS_INVALID 0xFFu

class CAuraDB
{
public:
  enum class SchemaStatus : uint8_t
  {
    OK                          = SCHEMA_CHECK_OK,
    INCOMPATIBLE                = SCHEMA_CHECK_INCOMPATIBLE,
    LEGACY_INCOMPATIBLE         = SCHEMA_CHECK_LEGACY_INCOMPATIBLE,
    LEGACY_UPGRADEABLE          = SCHEMA_CHECK_LEGACY_UPGRADEABLE,
    NONE                        = SCHEMA_CHECK_NONE,
    ERRORED                     = SCHEMA_CHECK_ERRORED
  };

  enum class JournalMode : uint8_t
  {
    DEL                         = JOURNAL_MODE_DELETE,
    TRUNCATE                    = JOURNAL_MODE_TRUNCATE,
    PERSIST                     = JOURNAL_MODE_PERSIST,
    MEMORY                      = JOURNAL_MODE_MEMORY,
    WAL                         = JOURNAL_MODE_WAL,
    OFF                         = JOURNAL_MODE_OFF,
    LAST                        = JOURNAL_MODE_LAST,
    INVALID                     = JOURNAL_MODE_INVALID
  };

  enum class SynchronousMode : uint8_t
  {
    OFF                         = SYNCHRONOUS_OFF,
    NORMAL                      = SYNCHRONOUS_NORMAL,
    FULL                        = SYNCHRONOUS_FULL,
    EXTRA                       = SYNCHRONOUS_EXTRA,
    LAST                        = SYNCHRONOUS_LAST,
    INVALID                     = SYNCHRONOUS_INVALID,
  };

private:
  CSQLITE3*                       m_DB;
  CAuraDB::JournalMode            m_JournalMode;
  CAuraDB::SynchronousMode        m_Synchronous;
  std::filesystem::path           m_File;
  std::filesystem::path           m_TWRPGFile;
  bool                            m_FirstRun;
  bool                            m_HasError;
  std::string                     m_Error;
  uint64_t                        m_LatestGameId;

  // we keep some prepared statements in memory rather than recreating them each function call
  // this is an optimization because preparing statements takes time
  // however it only pays off if you're going to be using the statement extremely often
  std::vector<sqlite3_stmt*> m_StmtCache;

  std::map<uint8_t, CSearchableMapData*> m_SearchableMapData;

public:
  explicit CAuraDB(CConfig& CFG);
  ~CAuraDB();
  CAuraDB(CAuraDB&) = delete;

  [[nodiscard]] inline bool                   GetIsFirstRun() const { return m_FirstRun; }
  [[nodiscard]] CAuraDB::SchemaStatus         GetSchemaStatus(int64_t& schemaNumber);
  void                          UpdateSchema(int64_t oldSchemaNumber);
  void                          Initialize();
  void                          PreCompileStatements();
  [[nodiscard]] inline bool                   HasError() const { return m_HasError; }
  [[nodiscard]] inline std::string            GetError() const { return m_Error; }
  [[nodiscard]] inline std::filesystem::path  GetFile() const { return m_File; }
  [[nodiscard]] uint64_t                      GetLatestHistoryGameId();
  void                          UpdateLatestHistoryGameId(uint64_t gameId);

  [[nodiscard]] inline bool                   Begin() const { return m_DB->Exec("BEGIN TRANSACTION") == SQLITE_OK; }
  inline bool                   Commit() const { return m_DB->Exec("COMMIT TRANSACTION") == SQLITE_OK; }

  // Geolocalization
  [[nodiscard]] std::string                   FromCheck(uint32_t ip);
  [[nodiscard]] bool                          FromAdd(uint32_t ip1, uint32_t ip2, const std::string& country);

  // Map aliases
  [[nodiscard]] bool                          AliasAdd(const std::string& alias, const std::string& target);
  [[nodiscard]] std::string                   AliasCheck(const std::string& alias);

  // Server moderators
  [[nodiscard]] uint32_t                      ModeratorCount(const std::string& server);
  [[nodiscard]] bool                          ModeratorCheck(const std::string& server, const std::string& user);
  [[nodiscard]] bool                          ModeratorAdd(const std::string& server, const std::string& user);
  [[nodiscard]] bool                          ModeratorRemove(const std::string& server, const std::string& user);
  [[nodiscard]] std::vector<std::string>      ListModerators(const std::string& server);

  // Bans
  [[nodiscard]] uint32_t                      BanCount(const std::string& authserver);
  [[nodiscard]] CDBBan*                       UserBanCheck(const std::string& user, const std::string& server, const std::string& authserver);
  [[nodiscard]] CDBBan*                       IPBanCheck(std::string ip, const std::string& authserver);
  [[nodiscard]] bool                          GetIsUserBanned(const std::string& user, const std::string& server, const std::string& authserver);
  [[nodiscard]] bool                          GetIsIPBanned(std::string ip, const std::string& authserver);
  bool                                        BanAdd(const std::string& user, const std::string& server, const std::string& authserver, const std::string& ip, const std::string& moderator, const std::string& reason);
  /*
  bool                                        BanAdd(const std::string& user, const std::string& server, const std::string& authserver, const std::string& ip, const std::string& moderator, const std::string& reason, const std::string& expiry);
  bool                                        BanAddPermanent(const std::string& user, const std::string& server, const std::string& authserver, const std::string& ip, const std::string& moderator, const std::string& reason);
  */
  [[nodiscard]] bool                          BanRemove(const std::string& user, const std::string& server, const std::string& authserver);
  [[nodiscard]] std::vector<std::string>      ListBans(const std::string& authserver);

  // Players
  void                                        UpdateGamePlayerOnStart(const std::string& name, const std::string& server, const std::string& ip, uint64_t gameId);
  void                                        UpdateGamePlayerOnEnd(const std::string& name, const std::string& server, const std::string& ip, uint64_t loadingtime, uint64_t duration, uint64_t left);
  [[nodiscard]] CDBGamePlayerSummary*         GamePlayerSummaryCheck(const std::string& name, const std::string& server);
  void                                        UpdateDotAPlayerOnEnd(const std::string& name, const std::string& server, uint32_t winner, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t neutralkills, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills);
  [[nodiscard]] CDBDotAPlayerSummary*         DotAPlayerSummaryCheck(const std::string& name, const std::string& server);
  [[nodiscard]] std::string                   GetInitialIP(const std::string& name, const std::string& server);
  [[nodiscard]] std::string                   GetLatestIP(const std::string& name, const std::string& server);
  [[nodiscard]] std::vector<std::string>      GetIPs(const std::string& name, const std::string& server);
  [[nodiscard]] std::vector<std::string>      GetAlts(const std::string& addressLiteral);

  // Games
  bool                          GameAdd(const uint64_t gameId, const std::string& creator, const std::string& mapClientPath, const std::string& mapServerPath, const std::array<uint8_t, 4>& mapCRC32, const std::vector<std::string>& playerNames, const std::vector<uint8_t>& playerIDs, const std::vector<uint8_t>& slotIDs, const std::vector<uint8_t>& colorIDs);
  [[nodiscard]] CDBGameSummary*               GameCheck(const uint64_t gameId);

  void                          InitMapData();
  [[nodiscard]] CSearchableMapData*           GetMapData(uint8_t mapType) const;
  [[nodiscard]] uint8_t                       FindData(const uint8_t mapType, const uint8_t searchDataType, std::string& objectName, const bool exactMatch) const;
  [[nodiscard]] std::vector<std::string>      GetDescription(const uint8_t mapType, const uint8_t searchDataType, const std::string& objectName) const;
};

#undef SCHEMA_CHECK_OK
#undef SCHEMA_CHECK_NONE
#undef SCHEMA_CHECK_ERRORED
#undef SCHEMA_CHECK_INCOMPATIBLE
#undef SCHEMA_CHECK_LEGACY_INCOMPATIBLE
#undef SCHEMA_CHECK_LEGACY_UPGRADEABLE

#undef JOURNAL_MODE_DELETE
#undef JOURNAL_MODE_TRUNCATE
#undef JOURNAL_MODE_PERSIST
#undef JOURNAL_MODE_MEMORY
#undef JOURNAL_MODE_WAL
#undef JOURNAL_MODE_OFF
#undef JOURNAL_MODE_LAST
#undef JOURNAL_MODE_INVALID

#undef SYNCHRONOUS_OFF
#undef SYNCHRONOUS_NORMAL
#undef SYNCHRONOUS_FULL
#undef SYNCHRONOUS_EXTRA
#undef SYNCHRONOUS_LAST
#undef SYNCHRONOUS_INVALID

//
// CDBBan
//

class CDBBan
{
private:
  std::string m_Name;
  std::string m_Server;
  std::string m_AuthServer;
  std::string m_IP;
  std::string m_Date;
  std::string m_Expiry;
  bool m_Permanent;
  std::string m_Moderator;
  std::string m_Reason;
  bool m_Suspect;           // When issuing bans with ambiguous commands, this flag is used to confirm ban target.
  
public:
  CDBBan(std::string nName, std::string nServer, std::string nAuthServer, std::string nIP, std::string nDate, std::string nExpiry, bool nPermanent, std::string nModerator, std::string nReason);
  ~CDBBan();

  [[nodiscard]] inline std::string GetName() const { return m_Name; }
  [[nodiscard]] inline std::string GetServer() const { return m_Server; }
  [[nodiscard]] inline std::string GetAuthServer() const { return m_AuthServer; }
  [[nodiscard]] inline std::string GetIP() const { return m_IP; }
  [[nodiscard]] inline std::string GetDate() const { return m_Date; }
  [[nodiscard]] inline std::string GetExpiry() const { return m_Expiry; }
  [[nodiscard]] inline std::string GetModerator() const { return m_Moderator; }
  [[nodiscard]] inline std::string GetReason() const { return m_Reason; }
  [[nodiscard]] inline bool GetSuspect() const { return m_Suspect; }
  inline void SetSuspect(bool nSuspect) { m_Suspect = nSuspect; }
};

//
// CDBGamePlayer
//

class CDBGamePlayer
{
private:
  std::string m_Name;
  std::string m_Server;
  std::string m_IP;
  uint64_t    m_LoadingTime;
  uint64_t    m_LeftTime;
  /*uint8_t     m_UID;
  uint8_t     m_SID;*/
  uint8_t     m_Color;

public:
  CDBGamePlayer(std::string name, std::string server, std::string ip, /*uint8_t nUID, uint8_t nSID,*/ uint8_t nColor);
  ~CDBGamePlayer();

  [[nodiscard]] inline std::string GetName() const { return m_Name; }
  [[nodiscard]] inline std::string GetServer() const { return m_Server; }
  [[nodiscard]] inline std::string GetIP() const { return m_IP; }
  [[nodiscard]] inline uint64_t    GetLoadingTime() const { return m_LoadingTime; }
  [[nodiscard]] inline uint64_t    GetLeftTime() const { return m_LeftTime; }
  /*[[nodiscard]] inline uint8_t     GetUID() const { return m_UID; }
  [[nodiscard]] inline uint8_t     GetSID() const { return m_SID; }*/
  [[nodiscard]] inline uint8_t     GetColor() const { return m_Color; }

  inline void SetLoadingTime(uint64_t nLoadingTime) { m_LoadingTime = nLoadingTime; }
  inline void SetLeftTime(uint64_t nLeftTime) { m_LeftTime = nLeftTime; }
};

//
// CDBGameSummary
//

class CDBGameSummary
{
private:
  uint64_t m_ID;
  std::vector<uint8_t> m_UIDs;
  std::vector<uint8_t> m_SIDs;
  std::vector<uint8_t> m_Colors;
  std::vector<std::string> m_PlayerNames;

public:
  CDBGameSummary(uint64_t nID, std::string playerNames, std::string playerIDs);
  ~CDBGameSummary();

  [[nodiscard]] inline uint64_t GetID() const { return m_ID; }
  [[nodiscard]] inline const std::vector<uint8_t>& GetUIDs() const { return m_UIDs; }
  [[nodiscard]] inline const std::vector<uint8_t>& GetSIDs() const { return m_SIDs; }
  [[nodiscard]] inline const std::vector<uint8_t>& GetColors() const { return m_Colors; }
  [[nodiscard]] inline const std::vector<std::string>& GetPlayerNames() const { return m_PlayerNames; }
};

//
// CDBGamePlayerSummary
//

class CDBGamePlayerSummary
{
private:
  uint32_t m_TotalGames;     // total number of games played
  float    m_AvgLoadingTime; // average loading time in milliseconds (this could be skewed because different maps have different load times)
  uint32_t m_AvgLeftPercent; // average time at which the player left the game expressed as a percentage of the game duration (0-100)

public:
  CDBGamePlayerSummary(uint32_t nTotalGames, float nAvgLoadingTime, uint32_t nAvgLeftPercent);
  ~CDBGamePlayerSummary();

  [[nodiscard]] inline uint32_t GetTotalGames() const { return m_TotalGames; }
  [[nodiscard]] inline float    GetAvgLoadingTime() const { return m_AvgLoadingTime; }
  [[nodiscard]] inline uint32_t GetAvgLeftPercent() const { return m_AvgLeftPercent; }
};

//
// CDBDotAPlayer
//

class CDBDotAPlayer
{
private:
  uint8_t  m_Color;
  uint8_t  m_NewColor;
  uint32_t m_Kills;
  uint32_t m_Deaths;
  uint32_t m_CreepKills;
  uint32_t m_CreepDenies;
  uint32_t m_Assists;
  uint32_t m_NeutralKills;
  uint32_t m_TowerKills;
  uint32_t m_RaxKills;
  uint32_t m_CourierKills;

public:
  CDBDotAPlayer();
  CDBDotAPlayer(uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nNeutralKills, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills);
  ~CDBDotAPlayer();

  [[nodiscard]] inline uint8_t GetColor() const { return m_Color; }
  [[nodiscard]] inline uint8_t GetNewColor() const { return m_NewColor; }
  [[nodiscard]] inline uint32_t GetKills() const { return m_Kills; }
  [[nodiscard]] inline uint32_t GetDeaths() const { return m_Deaths; }
  [[nodiscard]] inline uint32_t GetCreepKills() const { return m_CreepKills; }
  [[nodiscard]] inline uint32_t GetCreepDenies() const { return m_CreepDenies; }
  [[nodiscard]] inline uint32_t GetAssists() const { return m_Assists; }
  [[nodiscard]] inline uint32_t GetNeutralKills() const { return m_NeutralKills; }
  [[nodiscard]] inline uint32_t GetTowerKills() const { return m_TowerKills; }
  [[nodiscard]] inline uint32_t GetRaxKills() const { return m_RaxKills; }
  [[nodiscard]] inline uint32_t GetCourierKills() const { return m_CourierKills; }

  inline void IncKills() { ++m_Kills; }
  inline void IncDeaths() { ++m_Deaths; }
  inline void IncAssists() { ++m_Assists; }
  inline void IncTowerKills() { ++m_TowerKills; }
  inline void IncRaxKills() { ++m_RaxKills; }
  inline void IncCourierKills() { ++m_CourierKills; }

  inline void SetColor(uint8_t nColor) { m_Color = nColor; }
  inline void SetNewColor(uint8_t nNewColor) { m_NewColor = nNewColor; }
  inline void SetCreepKills(uint32_t nCreepKills) { m_CreepKills = nCreepKills; }
  inline void SetCreepDenies(uint32_t nCreepDenies) { m_CreepDenies = nCreepDenies; }
  inline void SetNeutralKills(uint32_t nNeutralKills) { m_NeutralKills = nNeutralKills; }
};

//
// CDBDotAPlayerSummary
//

class CDBDotAPlayerSummary
{
private:
  uint32_t m_TotalGames;        // total number of dota games played
  uint32_t m_TotalWins;         // total number of dota games won
  uint32_t m_TotalLosses;       // total number of dota games lost
  uint32_t m_TotalKills;        // total number of hero kills
  uint32_t m_TotalDeaths;       // total number of deaths
  uint32_t m_TotalCreepKills;   // total number of creep kills
  uint32_t m_TotalCreepDenies;  // total number of creep denies
  uint32_t m_TotalAssists;      // total number of assists
  uint32_t m_TotalNeutralKills; // total number of neutral kills
  uint32_t m_TotalTowerKills;   // total number of tower kills
  uint32_t m_TotalRaxKills;     // total number of rax kills
  uint32_t m_TotalCourierKills; // total number of courier kills

public:
  CDBDotAPlayerSummary(uint32_t nTotalGames, uint32_t nTotalWins, uint32_t nTotalLosses, uint32_t nTotalKills, uint32_t nTotalDeaths, uint32_t nTotalCreepKills, uint32_t nTotalCreepDenies, uint32_t nTotalAssists, uint32_t nTotalNeutralKills, uint32_t nTotalTowerKills, uint32_t nTotalRaxKills, uint32_t nTotalCourierKills);
  ~CDBDotAPlayerSummary();

  [[nodiscard]] inline uint32_t GetTotalGames() const { return m_TotalGames; }
  [[nodiscard]] inline uint32_t GetTotalWins() const { return m_TotalWins; }
  [[nodiscard]] inline uint32_t GetTotalLosses() const { return m_TotalLosses; }
  [[nodiscard]] inline uint32_t GetTotalKills() const { return m_TotalKills; }
  [[nodiscard]] inline uint32_t GetTotalDeaths() const { return m_TotalDeaths; }
  [[nodiscard]] inline uint32_t GetTotalCreepKills() const { return m_TotalCreepKills; }
  [[nodiscard]] inline uint32_t GetTotalCreepDenies() const { return m_TotalCreepDenies; }
  [[nodiscard]] inline uint32_t GetTotalAssists() const { return m_TotalAssists; }
  [[nodiscard]] inline uint32_t GetTotalNeutralKills() const { return m_TotalNeutralKills; }
  [[nodiscard]] inline uint32_t GetTotalTowerKills() const { return m_TotalTowerKills; }
  [[nodiscard]] inline uint32_t GetTotalRaxKills() const { return m_TotalRaxKills; }
  [[nodiscard]] inline uint32_t GetTotalCourierKills() const { return m_TotalCourierKills; }
  [[nodiscard]] inline float    GetAvgKills() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalKills) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgDeaths() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalDeaths) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgCreepKills() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalCreepKills) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgCreepDenies() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalCreepDenies) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgAssists() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalAssists) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgNeutralKills() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalNeutralKills) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgTowerKills() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalTowerKills) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgRaxKills() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalRaxKills) / m_TotalGames : 0.f; }
  [[nodiscard]] inline float    GetAvgCourierKills() const { return m_TotalGames > 0 ? static_cast<float>(m_TotalCourierKills) / m_TotalGames : 0.f; }
};

[[nodiscard]] inline uint32_t signed_to_unsigned_32(const int32_t value)
{
  // Add 128 to preserve ordering.
  return static_cast<uint32_t>(static_cast<int64_t>(value) - static_cast<int64_t>(INT32_MIN));
}

[[nodiscard]] inline int32_t unsigned_to_signed_32(const uint32_t value)
{
  // Subtract 128 to preserve ordering.
  return static_cast<int32_t>(static_cast<int64_t>(value) + static_cast<int64_t>(INT32_MIN));
}

[[nodiscard]] inline uint64_t signed_to_unsigned_64(const int64_t value)
{
  // Add 128 to preserve ordering.
  return static_cast<uint64_t>(static_cast<int64_t>(value) - static_cast<int64_t>(INT64_MIN));
}

[[nodiscard]] inline int64_t unsigned_to_signed_64(const uint64_t value)
{
  // Subtract 128 to preserve ordering.
  return static_cast<int64_t>(static_cast<int64_t>(value) + static_cast<int64_t>(INT64_MIN));
}

#endif // AURA_AURADB_H_
