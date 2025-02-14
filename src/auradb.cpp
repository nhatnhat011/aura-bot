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

#include "auradb.h"
#include "net.h"
#include "util.h"
#include "config/config.h"
#ifdef DISABLE_DPP
#include <nlohmann/json.hpp>
#else
#include <dpp/nlohmann/json.hpp>
#endif

#include <fstream>
#include <utility>
#include <algorithm>

using namespace std;

//
// CQSLITE3 (wrapper class)
//

CSQLITE3::CSQLITE3(const filesystem::path& filename)
  : m_Ready(true)
{
  if (sqlite3_open_v2(PathToString(filename).c_str(), reinterpret_cast<sqlite3**>(&m_DB), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    m_Ready = false;
}

CSQLITE3::~CSQLITE3()
{
  sqlite3_close(static_cast<sqlite3*>(m_DB));
}

//
// CSearchableMapData
//

CSearchableMapData::CSearchableMapData(uint8_t nMapType)
 : m_MapType(nMapType)
{
}

CSearchableMapData::~CSearchableMapData()
{
}

uint8_t CSearchableMapData::Search(string& rwSearchName, const uint8_t searchDataType, const bool exactMatch)
{
  auto aliasMatch = m_Aliases.find(rwSearchName);
  if (aliasMatch != m_Aliases.end()) {
    if (searchDataType == MAP_DATA_TYPE_ANY || searchDataType == aliasMatch->second.first) {
      rwSearchName = aliasMatch->second.second;
      return aliasMatch->second.first;
    }
  }

  string fuzzyPattern = PreparePatternForFuzzySearch(rwSearchName);

  string::size_type maxDistance = 0;
  if (!exactMatch) {
    maxDistance = fuzzyPattern.size() / 3;
  }

  bool allowInclusion = !exactMatch && fuzzyPattern.size() >= 5;
  uint8_t bestMatchType = MAP_DATA_TYPE_NONE;
  string::size_type bestDistance = maxDistance + 1;
  string bestMatch;
  vector<string> inclusionMatches;

  if (searchDataType == MAP_DATA_TYPE_ANY || searchDataType == MAP_DATA_TYPE_ITEM) {
    for (const string& element : m_Items) {
      if (element == fuzzyPattern) {
        rwSearchName = element;
        return MAP_DATA_TYPE_ITEM;
      }
      if (allowInclusion && element.find(fuzzyPattern) != string::npos) {
        inclusionMatches.push_back(element);
      }
    }
    if (inclusionMatches.size() == 1) {
      rwSearchName = inclusionMatches[0];
      return MAP_DATA_TYPE_ITEM;
    } else if (!inclusionMatches.empty()) {
      rwSearchName = JoinVector(inclusionMatches, false);
      return MAP_DATA_TYPE_ANY;
    } else if (!exactMatch) {
      for (const string& element : m_Items) {
        string::size_type distance = GetLevenshteinDistanceForSearch(element, fuzzyPattern, bestDistance);
        if (distance < bestDistance) {
          bestDistance = distance;
          bestMatch = element;
          bestMatchType = MAP_DATA_TYPE_ITEM;
        }
      }
    }
  }

  if (!bestMatch.empty()) {
    rwSearchName = bestMatch;
  }
  return bestMatchType;
}

void CSearchableMapData::LoadData(filesystem::path sourceFile)
{
  m_Data[MAP_DATA_TYPE_ITEM] = map<string, vector<string>>();

#ifndef DISABLE_DPP
  ifstream twrpgFile;
  twrpgFile.open(sourceFile.native().c_str(), ios::in);
  if (twrpgFile.fail()) {
    Print("[AURA] warning - [" + PathToString(sourceFile) + "] not found");
  } else {
    try {
      nlohmann::json data = nlohmann::json::parse(twrpgFile);
      for (const auto& element : data["items"].items()) {
        m_Items.push_back(string(element.key()));
        m_Data[MAP_DATA_TYPE_ITEM][string(element.key())] = vector<string>(element.value().begin(), element.value().end());
      }
      for (const auto& element : data["aliases"].items()) {
        m_Aliases[string(element.key())] = make_pair((uint8_t)(element.value()[0]), string(element.value()[1]));
      }
    } catch (nlohmann::json::exception& e) {
      Print("[AURA] error loading [" + PathToString(sourceFile) + "] - " + string(e.what()));
    }
    twrpgFile.close();
  }
#endif
}

//
// CAuraDB
//

CAuraDB::CAuraDB(CConfig& CFG)
  : m_FirstRun(false),
    m_HasError(false),
    m_LatestGameId(0)
{
  m_StmtCache.reserve(STMT_CACHE_SIZE);
  uint8_t i = STMT_CACHE_SIZE;
  while (i--) m_StmtCache[i] = nullptr;

  m_TWRPGFile = CFG.GetPath("game_data.twrpg_path", CFG.GetHomeDir() / filesystem::path("twrpg.json"));
  InitMapData();

  m_File = CFG.GetPath("db.storage_file", CFG.GetHomeDir() / filesystem::path("aura.db"));

  m_JournalMode = CFG.GetEnum<JournalMode>("db.journal_mode", TO_ARRAY("delete", "truncate", "persist", "memory", "wal", "off"), JournalMode::DEL);
  if (CFG.GetErrorLast()) {
    m_JournalMode = JournalMode::INVALID;
    Print("[SQLITE3] invalid <db.journal_mode> (delete, truncate, persist, memory, wal, off are allowed - case sensitive)");
  }

  m_Synchronous = CFG.GetEnum<SynchronousMode>("db.synchronous", TO_ARRAY("off", "normal", "full", "extra"), SynchronousMode::FULL);
  if (CFG.GetErrorLast()) {
    m_Synchronous = SynchronousMode::INVALID;
    Print("[SQLITE3] invalid <db.synchronous> (off, normal, full, extra are allowed - case sensitive)");
  }

  uint16_t journalWALInterval = CFG.GetUint16("db.wal_autocheckpoint", 1000);

  Print("[SQLITE3] opening database [" + PathToString(m_File) + "]");
  m_DB = new CSQLITE3(m_File);

  if (!m_DB->GetReady()) {
    // setting m_HasError to true indicates there's been a critical error and we want Aura to shutdown
    // this is okay here because we're in the constructor so we're not dropping any games or players

    Print(string("[SQLITE3] error opening database [" + PathToString(m_File) + "] - ") + m_DB->GetError());
    m_HasError = true;
    m_Error    = "error opening database";
    return;
  }

  int64_t schemaNumber = 0;
  switch (GetSchemaStatus(schemaNumber))
  {
    case SchemaStatus::OK:
      // do nothing
      break;
    case SchemaStatus::INCOMPATIBLE:
    case SchemaStatus::LEGACY_INCOMPATIBLE:
      Print("[SQLITE3] legacy database format found ([aura.db] schema_number is " + to_string(schemaNumber) + ", expected " + to_string(SchemaNumber) + ")");
      Print("[SQLITE3] please start over with a clean [aura.db] file to run this Aura version");
      Print("[SQLITE3] you SHOULD backup your old [aura.db] file to another folder");
      m_HasError = true;
      m_Error    = "incompatible database format";
      break;
    case SchemaStatus::LEGACY_UPGRADEABLE:
      UpdateSchema(schemaNumber);
      break;
    case SchemaStatus::NONE:
      m_FirstRun = true;
      Initialize();
      break;
    case SchemaStatus::ERRORED:
      m_HasError = true;
      if (m_Error.empty()) {
        m_Error = "schema check error";
      }
      break;
  }

  if (!m_HasError) {
    switch (m_JournalMode) {
      case JournalMode::DEL:
        m_DB->Exec("PRAGMA journal_mode = DELETE");
        break;
      case JournalMode::TRUNCATE:
        m_DB->Exec("PRAGMA journal_mode = TRUNCATE");
        break;
      case JournalMode::PERSIST:
        m_DB->Exec("PRAGMA journal_mode = PERSIST");
        break;
      case JournalMode::MEMORY:
        m_DB->Exec("PRAGMA journal_mode = MEMORY");
        break;
      case JournalMode::WAL:
        m_DB->Exec("PRAGMA journal_mode = WAL");
        m_DB->Exec("PRAGMA wal_autocheckpoint = " + to_string(journalWALInterval));
        break;
      case JournalMode::OFF:
        m_DB->Exec("PRAGMA journal_mode = OFF");
        break;
      case JournalMode::INVALID:
        // do nothing
        break;
    }

    switch (m_Synchronous) {
      case SynchronousMode::OFF:
        m_DB->Exec("PRAGMA synchronous = OFF");
        break;
      case SynchronousMode::NORMAL:
        m_DB->Exec("PRAGMA synchronous = NORMAL");
        break;
      case SynchronousMode::FULL:
        m_DB->Exec("PRAGMA synchronous = FULL");
        break;
      case SynchronousMode::EXTRA:
        m_DB->Exec("PRAGMA synchronous = EXTRA");
        break;
      case SynchronousMode::INVALID:
        // do nothing
        break;
    }

    PreCompileStatements();
  }
}

CAuraDB::~CAuraDB()
{
  Print("[SQLITE3] closing database [" + PathToString(m_File.filename()) + "]");

  uint8_t i = STMT_CACHE_SIZE;
  while (i--) {
    if (m_StmtCache[i]) m_DB->Finalize(m_StmtCache[i]);
  }
  delete m_DB;

  delete m_SearchableMapData[MAP_TYPE_TWRPG];
}

CAuraDB::SchemaStatus CAuraDB::GetSchemaStatus(int64_t& schemaNumber)
{
  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare(R"(SELECT value FROM config WHERE name=?)", reinterpret_cast<void**>(&Statement));

  if (!Statement) {
    // no such table: config
    return SchemaStatus::NONE;
  }

  sqlite3_bind_text(Statement, 1, "schema_number", -1, SQLITE_TRANSIENT);

  int32_t RC = m_DB->Step(Statement);

  if (RC == SQLITE_ROW) {
    if (sqlite3_column_count(Statement) == 1) {
      schemaNumber = sqlite3_column_int64(Statement, 0);
    } else {
      m_HasError = true;
      m_Error    = "schema number missing - no columns found";
      return SchemaStatus::ERRORED;
    }
  } else if (RC == SQLITE_ERROR) {
    m_HasError = true;
    m_Error    = m_DB->GetError();
    return SchemaStatus::ERRORED;
  }

  m_DB->Finalize(Statement);

  // I am using 3 as int64.
  if (schemaNumber == SchemaNumber) {
    return SchemaStatus::OK;
  }

  // Other legacy schemas are not supported,
  // including Josko's original schema (1, but text).

  if (schemaNumber != 0) {
    return SchemaStatus::LEGACY_INCOMPATIBLE;
  }

  return SchemaStatus::NONE;
}

void CAuraDB::UpdateSchema(int64_t oldSchemaNumber)
{
  if (oldSchemaNumber > 2) {
    /*
    Print("[AURA] Updating database schema...");

    if (m_DB->Exec(R"(ALTER TABLE bans ADD COLUMN ip TEXT NOT NULL DEFAULT ""; ALTER TABLE bans ADD COLUMN expiry TEXT NOT NULL DEFAULT ""; ALTER TABLE bans RENAME COLUMN moderators moderator; ALTER TABLE bans ADD COLUMN authserver TEXT NOT NULL default ""))") != SQLITE_OK)
      Print("[SQLITE3] error widening bans table - " + m_DB->GetError());

    if (m_DB->Exec(R"(ALTER TABLE players ADD COLUMN server TEXT NOT NULL DEFAULT ""; ALTER TABLE players ADD COLUMN initialip TEXT NOT NULL DEFAULT "::ffff:0:0"; ALTER TABLE players ADD COLUMN latestip TEXT NOT NULL DEFAULT "::ffff:0:0"; ALTER TABLE players ADD COLUMN initialreport TEXT NOT NULL DEFAULT ""; ALTER TABLE players ADD COLUMN reports INTEGER DEFAULT 0; ALTER TABLE players ADD COLUMN latestgame INTEGER DEFAULT 0))") != SQLITE_OK)
      Print("[SQLITE3] error widening players table - " + m_DB->GetError());

    // crc32 here is the true CRC32 hash of the map file (i.e. <map.crc32> in the map ini, NOT <map.weak_hash>, NOR legacy <map_crc>)
    if (m_DB->Exec("CREATE TABLE games ( id INTEGER PRIMARY KEY, creator TEXT, mapcpath TEXT NOT NULL, mapspath TEXT NOT NULL, crc32 TEXT NOT NULL, replay TEXT, players TEXT NOT NULL)") != SQLITE_OK)
      Print("[SQLITE3] error creating games table - " + m_DB->GetError());

    if (m_DB->Exec("CREATE TABLE commands ( command TEXT NOT NULL, scope TEXT NOT NULL, type TEXT NOT NULL, action TEXT NOT NULL, PRIMARY KEY ( command, scope ) )") != SQLITE_OK)
      Print("[SQLITE3] error creating commands table - " + m_DB->GetError());
    */
  }
}

void CAuraDB::Initialize()
{
  Print("[SQLITE3] initializing database");

  if (m_DB->Exec(R"(CREATE TABLE moderators ( name TEXT NOT NULL, server TEXT NOT NULL DEFAULT "", PRIMARY KEY ( name, server ) ))") != SQLITE_OK)
    Print("[SQLITE3] error creating moderators table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE bans ( name TEXT NOT NULL, server TEXT NOT NULL, authserver TEXT NOT NULL, ip TEXT NOT NULL, date TEXT NOT NULL, expiry TEXT NOT NULL, permanent INTEGER DEFAULT 0, moderator TEXT NOT NULL, reason TEXT, PRIMARY KEY ( name, server, authserver ) )") != SQLITE_OK)
    Print("[SQLITE3] error creating bans table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE players ( name TEXT NOT NULL, server TEXT not NULL, initialip TEXT NOT NULL, latestip TEXT NOT NULL, initialreport TEXT, reports INTEGER DEFAULT 0, latestgame INTEGER DEFAULT 0, games INTEGER DEFAULT 0, dotas INTEGER DEFAULT 0, loadingtime INTEGER DEFAULT 0, duration INTEGER DEFAULT 0, left INTEGER DEFAULT 0, wins INTEGER DEFAULT 0, losses INTEGER DEFAULT 0, kills INTEGER DEFAULT 0, deaths INTEGER DEFAULT 0, creepkills INTEGER DEFAULT 0, creepdenies INTEGER DEFAULT 0, assists INTEGER DEFAULT 0, neutralkills INTEGER DEFAULT 0, towerkills INTEGER DEFAULT 0, raxkills INTEGER DEFAULT 0, courierkills INTEGER DEFAULT 0, PRIMARY KEY ( name, server ) )") != SQLITE_OK)
    Print("[SQLITE3] error creating players table - " + m_DB->GetError());

  // crc32 here is the true CRC32 hash of the map file (i.e. <map.crc32> in the map ini, NOT <map.crc>, NOR legacy <map_crc>)
  if (m_DB->Exec("CREATE TABLE games ( id INTEGER PRIMARY KEY, creator TEXT, mapcpath TEXT NOT NULL, mapspath TEXT NOT NULL, crc32 TEXT NOT NULL, replay TEXT, playernames TEXT NOT NULL, playerids TEXT NOT NULL, saveids TEXT )") != SQLITE_OK)
    Print("[SQLITE3] error creating games table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE config ( name TEXT NOT NULL PRIMARY KEY, value INTEGER )") != SQLITE_OK)
    Print("[SQLITE3] error creating config table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE iptocountry ( ip1 INTEGER NOT NULL, ip2 INTEGER NOT NULL, country TEXT NOT NULL, PRIMARY KEY ( ip1, ip2 ) )") != SQLITE_OK)
    Print("[SQLITE3] error creating iptocountry table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE aliases ( alias TEXT NOT NULL PRIMARY KEY, value TEXT NOT NULL )") != SQLITE_OK)
    Print("[SQLITE3] error creating aliases table - " + m_DB->GetError());

  if (m_DB->Exec("CREATE TABLE commands ( command TEXT NOT NULL, scope TEXT NOT NULL, type TEXT NOT NULL, action TEXT NOT NULL, PRIMARY KEY ( command, scope ) )") != SQLITE_OK)
    Print("[SQLITE3] error creating commands table - " + m_DB->GetError());

  // Insert schema number
  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare(R"(INSERT INTO config VALUES ( "schema_number", ? ))", reinterpret_cast<void**>(&Statement));
  if (Statement) {
    sqlite3_bind_int64(Statement, 1, SchemaNumber);
    const int32_t RC = m_DB->Step(Statement);
    if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error inserting schema number [" + to_string(SchemaNumber) + "] - " + m_DB->GetError());
    }
    m_DB->Finalize(Statement);
  }
}

void CAuraDB::PreCompileStatements()
{
  //m_DB->Prepare("INSERT INTO iptocountry VALUES ( ?, ?, ? )", &(m_StmtCache[FROM_ADD_IDX]), true);
  //m_DB->Prepare("SELECT country FROM iptocountry WHERE ip1<=? AND ip2>=?", &(m_StmtCache[FROM_CHECK_IDX]), true);
  m_DB->Prepare("INSERT OR REPLACE INTO config VALUES ( ?, ? )", &(m_StmtCache[LATEST_GAME_IDX]), true);
  //m_DB->Prepare("INSERT OR REPLACE INTO aliases VALUES ( ?, ? )", &(m_StmtCache[ALIAS_ADD_IDX]), true);
  m_DB->Prepare("SELECT value FROM aliases WHERE alias=?", &(m_StmtCache[ALIAS_CHECK_IDX]), true);
  m_DB->Prepare("SELECT name, server, authserver, ip, date, expiry, permanent, moderator, reason FROM bans WHERE name=? AND server=? AND authserver=?", &(m_StmtCache[USER_BAN_CHECK_IDX]), true);
  m_DB->Prepare("SELECT name, server, authserver, ip, date, expiry, permanent, moderator, reason FROM bans WHERE ip=? AND authserver=?", &(m_StmtCache[IP_BAN_CHECK_IDX]), true);
  m_DB->Prepare("SELECT * FROM moderators WHERE server=? AND name=?", &(m_StmtCache[MODERATOR_CHECK_IDX]), true);
  m_DB->Prepare("INSERT OR REPLACE INTO games ( id, creator, mapcpath, mapspath, crc32, playernames, playerids ) VALUES ( ?, ?, ?, ?, ?, ?, ? )", &(m_StmtCache[GAME_ADD_IDX]));
  m_DB->Prepare("SELECT games, loadingtime, duration, left FROM players WHERE name=? AND server=?", &(m_StmtCache[PLAYER_SUMMARY_IDX]), true);
  m_DB->Prepare(
    "INSERT INTO players (name, server, initialip, latestip, latestgame) "
    "VALUES (?, ?, ?, ?, ?) "
    "ON CONFLICT(name, server) "
    "DO UPDATE SET "
    "latestip = excluded.latestip, "
    "latestgame = excluded.latestgame;",
    &(m_StmtCache[UPDATE_PLAYER_START_IDX]), true
  );
  // gotta insert initialip, latestip here too, because those columns are NOT NULL
  m_DB->Prepare(
    "INSERT INTO players (name, server, initialip, latestip, games, loadingtime, duration, left) "
    "VALUES (?, ?, ?, ?, 1, ?, ?, ?) "
    "ON CONFLICT(name, server) "
    "DO UPDATE SET "
    "latestip = excluded.latestip, "
    "games = games + 1, "
    "loadingtime = loadingtime + excluded.loadingtime, "
    "duration = duration + excluded.duration, "
    "left = left + excluded.left;",
    &(m_StmtCache[UPDATE_PLAYER_END_IDX]), true
  );
}

uint64_t CAuraDB::GetLatestHistoryGameId()
{
  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare(R"(SELECT value FROM config WHERE name=?)", reinterpret_cast<void**>(&Statement));

  bool success = false;
  if (!Statement) {
    Print("[SQLITE3] prepare errors GetLatestHistoryGameId()");
    return 0;
  }

  sqlite3_bind_text(Statement, 1, "latest_game_id", -1, SQLITE_TRANSIENT);

  int64_t latestGameId = 0;
  int32_t RC = m_DB->Step(Statement);
  if (RC == SQLITE_ROW) {
    if (sqlite3_column_count(Statement) == 1) {
      latestGameId = sqlite3_column_int64(Statement, 0);
      success = true;
    }
  }
  m_DB->Finalize(Statement);

  if (success) {
    return signed_to_unsigned_64(latestGameId);
  } else {
    return 0;
  }
}

void CAuraDB::UpdateLatestHistoryGameId(uint64_t gameId)
{
  if (gameId < m_LatestGameId) {
    Print("[SQLITE3] game ID " + to_string(gameId) + " skipped (" + to_string(m_LatestGameId) + " already started)");
    return;
  }

  if (!m_StmtCache[LATEST_GAME_IDX]) {
    m_DB->Prepare("INSERT OR REPLACE INTO config VALUES ( ?, ? )", &(m_StmtCache[LATEST_GAME_IDX]), true);
  }

  if (!m_StmtCache[LATEST_GAME_IDX]) {
    Print("[SQLITE3] prepare error updating latest game id [" + to_string(gameId) + "] - " + m_DB->GetError());
    return;
  }

  bool Success = false;
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[LATEST_GAME_IDX]), 1, "latest_game_id", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(static_cast<sqlite3_stmt*>(m_StmtCache[LATEST_GAME_IDX]), 2, unsigned_to_signed_64(gameId));

  int32_t RC = m_DB->Step(m_StmtCache[LATEST_GAME_IDX]);

  if (RC == SQLITE_DONE)
    Success = true;
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error updating latest game id [" + to_string(gameId) + "] - " + m_DB->GetError());

  m_DB->Reset(m_StmtCache[LATEST_GAME_IDX]);

  if (Success) {
    m_LatestGameId = gameId;
  }
}

uint32_t CAuraDB::ModeratorCount(const string& server)
{
  uint32_t      Count = 0;
  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare("SELECT COUNT(*) FROM moderators WHERE server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
      Count = sqlite3_column_int(Statement, 0);
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error counting moderators [" + server + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error counting moderators [" + server + "] - " + m_DB->GetError());

  return Count;
}

bool CAuraDB::ModeratorCheck(const string& server, const string& rawName)
{
  bool IsAdmin = false;
  string user = ToLowerCase(rawName);

  if (!m_StmtCache[MODERATOR_CHECK_IDX]) {
    m_DB->Prepare("SELECT * FROM moderators WHERE server=? AND name=?", &(m_StmtCache[MODERATOR_CHECK_IDX]), true);
  }

  if (m_StmtCache[MODERATOR_CHECK_IDX])
  {
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[MODERATOR_CHECK_IDX]), 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[MODERATOR_CHECK_IDX]), 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(m_StmtCache[MODERATOR_CHECK_IDX]);

    // we're just checking to see if the query returned a row, we don't need to check the row data itself

    if (RC == SQLITE_ROW)
      IsAdmin = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking moderators [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Reset(m_StmtCache[MODERATOR_CHECK_IDX]);
  }
  else
    Print("[SQLITE3] prepare error checking moderators [" + server + " : " + user + "] - " + m_DB->GetError());

  return IsAdmin;
}

bool CAuraDB::ModeratorAdd(const string& server, const string& rawName)
{
  bool Success = false;
  string user = ToLowerCase(rawName);

  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare("INSERT INTO moderators ( server, name ) VALUES ( ?, ? )", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error adding moderators [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error adding moderators [" + server + " : " + user + "] - " + m_DB->GetError());

  return Success;
}

bool CAuraDB::ModeratorRemove(const string& server, const string& rawName)
{
  bool          Success = false;
  sqlite3_stmt* Statement = nullptr;
  string user = ToLowerCase(rawName);
  m_DB->Prepare("DELETE FROM moderators WHERE server=? AND name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, user.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error removing moderators [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error removing moderators [" + server + " : " + user + "] - " + m_DB->GetError());

  return Success;
}

vector<string> CAuraDB::ListModerators(const string& server)
{
  vector<string> admins;

  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare("SELECT * FROM moderators WHERE server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, server.c_str(), -1, SQLITE_TRANSIENT);

    int32_t RC;
    while ((RC = m_DB->Step(Statement)) == SQLITE_ROW) {
      const unsigned char* user = m_DB->Column(Statement, 0);
      const string userWrap = string(reinterpret_cast<const char*>(user));
      admins.push_back(userWrap);
    }
    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error listing moderators [" + server + "] - " + m_DB->GetError());

  return admins;
}

uint32_t CAuraDB::BanCount(const string& authserver)
{
  uint32_t      Count = 0;
  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare("SELECT COUNT(*) FROM bans WHERE authserver=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, authserver.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
      Count = sqlite3_column_int(Statement, 0);
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error counting bans [" + authserver + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error counting bans [" + authserver + "] - " + m_DB->GetError());

  return Count;
}

CDBBan* CAuraDB::UserBanCheck(const string& rawName, const string& server, const string& authserver)
{
  CDBBan* Ban = nullptr;
  const string user = ToLowerCase(rawName);

  if (!m_StmtCache[USER_BAN_CHECK_IDX]) {
    m_DB->Prepare("SELECT name, server, authserver, ip, date, expiry, permanent, moderator, reason FROM bans WHERE name=? AND server=? AND authserver=?", &(m_StmtCache[USER_BAN_CHECK_IDX]), true);
  }

  if (m_StmtCache[USER_BAN_CHECK_IDX])
  {
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 2, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 3, authserver.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(m_StmtCache[USER_BAN_CHECK_IDX]);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX])) == 9)
      {
        string Name       = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 0));
        string Server     = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 1));
        string AuthServer = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 2));
        string IP         = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 3));
        string Date       = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 4));
        string Expiry     = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 5));
        int64_t Permanent = sqlite3_column_int(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 6);
        string Moderator  = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 7));
        string Reason     = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[USER_BAN_CHECK_IDX]), 8));

        Ban = new CDBBan(Name, Server, AuthServer, IP, Date, Expiry, static_cast<bool>(Permanent), Moderator, Reason);
      }
      else
        Print("[SQLITE3] error checking ban [" + server + " : " + user + "] - row doesn't have 9 columns");
    }
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking ban [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Reset(m_StmtCache[USER_BAN_CHECK_IDX]);
  }
  else
    Print("[SQLITE3] prepare error checking ban [" + server + " : " + user + "] - " + m_DB->GetError());

  return Ban;
}

CDBBan* CAuraDB::IPBanCheck(string ip, const string& authserver)
{
  CDBBan* Ban = nullptr;

  if (!m_StmtCache[IP_BAN_CHECK_IDX]) {
    m_DB->Prepare("SELECT name, server, authserver, ip, date, expiry, permanent, moderator, reason FROM bans WHERE ip=? AND authserver=?", &(m_StmtCache[IP_BAN_CHECK_IDX]), true);
  }

  if (m_StmtCache[IP_BAN_CHECK_IDX])
  {
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 1, ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 2, authserver.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(m_StmtCache[IP_BAN_CHECK_IDX]);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX])) == 9)
      {
        string Name       = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 0));
        string Server     = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 1));
        string AuthServer = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 2));
        string IP         = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 3));
        string Date       = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 4));
        string Expiry     = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 5));
        int64_t Permanent = sqlite3_column_int(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 6);
        string Moderator  = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 7));
        string Reason     = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[IP_BAN_CHECK_IDX]), 8));

        Ban = new CDBBan(Name, Server, AuthServer, IP, Date, Expiry, static_cast<bool>(Permanent), Moderator, Reason);
      }
      else
        Print("[SQLITE3] error checking ban [" + ip + "] - row doesn't have 9 columns");
    }
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking ban [" + ip + "] - " + m_DB->GetError());

    m_DB->Reset(m_StmtCache[IP_BAN_CHECK_IDX]);
  }
  else
    Print("[SQLITE3] prepare error checking ban [" + ip + "] - " + m_DB->GetError());

  return Ban;
}

bool CAuraDB::GetIsUserBanned(const string& user, const string& server, const string& authserver)
{
  CDBBan* ban = UserBanCheck(user, server, authserver);
  if (ban) {
    delete ban;
    return true;
  }
  return false;
}

bool CAuraDB::GetIsIPBanned(string ip, const string& authserver)
{
  CDBBan* ban = IPBanCheck(ip, authserver);
  if (ban) {
    delete ban;
    return true;
  }
  return false;
}

bool CAuraDB::BanAdd(const string& rawName, const string& server, const string& authserver, const string& ip, const string& moderator, const string& reason)
{
  bool          Success = false;
  sqlite3_stmt* Statement = nullptr;
  const string user = ToLowerCase(rawName);
  m_DB->Prepare("INSERT INTO bans ( name, server, authserver, ip, date, expiry, permanent, moderator, reason ) VALUES ( ?, ?, ?, ?, date('now'), date('now', '+10 days'), 0, ?, ? )", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 3, authserver.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 4, ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 5, moderator.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 6, reason.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE) {
      Success = true;
      Print("[SQLITE3] new ban added [" + user + "@" + server + " : " + moderator + "@" + authserver + " : " + reason + " : " + ip + "]");
    } else if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error adding ban [" + user + "@" + server + " : " + moderator + "@" + authserver + " : " + reason + " : " + ip + "] - " + m_DB->GetError());
    }

    m_DB->Finalize(Statement);
  } else {
    Print("[SQLITE3] prepare error adding ban [" + user + "@" + server + " : " + moderator + "@" + authserver + " : " + reason + " : " + ip + "] - " + m_DB->GetError());
  }

  return Success;
}

/*
bool CAuraDB::BanAdd(const string& user, const string& server, const string& authserver, const string& ip, const string& moderator, const string& reason, const string& expiry)
{
  // CAuraDB::BanAdd (arbitrary expiry) not implemented
  Print("[SQLITE3] Custom-expiry bans not implemented.");
  return false;
}

bool CAuraDB::BanAddPermanent(const string& user, const string& server, const string& authserver, const string& ip, const string& moderator, const string& reason)
{
  // CAuraDB::BanAddPermanent not implemented
  Print("[SQLITE3] Permanent bans not implemented.");
  return false;
}
*/

bool CAuraDB::BanRemove(const string& rawName, const string& server, const string& authserver)
{
  bool          Success = false;
  sqlite3_stmt* Statement = nullptr;
  const string user = ToLowerCase(rawName);
  m_DB->Prepare("DELETE FROM bans WHERE name=? AND server=? AND authserver=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 3, authserver.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_DONE)
      Success = true;
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error removing ban [" + server + " : " + user + "] - " + m_DB->GetError());

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error removing ban [" + server + " : " + user + "] - " + m_DB->GetError());

  return Success;
}

vector<string> CAuraDB::ListBans(const string& authserver)
{
  vector<string> bans;

  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare("SELECT * FROM bans WHERE authserver=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, authserver.c_str(), -1, SQLITE_TRANSIENT);

    int32_t RC;
    while ((RC = m_DB->Step(Statement)) == SQLITE_ROW) {
      const unsigned char* user = m_DB->Column(Statement, 0);
      bans.push_back(string(reinterpret_cast<const char*>(user)));
    }
    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error listing bans [" + authserver + "] - " + m_DB->GetError());

  return bans;
}

void CAuraDB::UpdateGamePlayerOnStart(const string& name, const string& server, const string& ip, uint64_t gameId)
{
  const string lowerName = ToLowerCase(name);

  if (!m_StmtCache[UPDATE_PLAYER_START_IDX]) {
    m_DB->Prepare(
      "INSERT INTO players (name, server, initialip, latestip, latestgame) "
      "VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT(name, server) "
      "DO UPDATE SET "
      "latestip = excluded.latestip, "
      "latestgame = excluded.latestgame;",
      &(m_StmtCache[UPDATE_PLAYER_START_IDX]), true
    );
  }

  if (m_StmtCache[UPDATE_PLAYER_START_IDX] == nullptr) {
    Print("[SQLITE3] prepare error adding gameuser on start [" + lowerName + "@" + server + "] - " + m_DB->GetError());
    return;
  }

  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_START_IDX]), 1, lowerName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_START_IDX]), 2, server.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_START_IDX]), 3, ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_START_IDX]), 4, ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_START_IDX]), 5, unsigned_to_signed_64(gameId));

  const int32_t RC = m_DB->Step(m_StmtCache[UPDATE_PLAYER_START_IDX]);

  if (RC != SQLITE_DONE) {
    Print("[SQLITE3] error initializing gameuser [" + lowerName + "@" + server + "] - " + m_DB->GetError());
  }

  m_DB->Reset(m_StmtCache[UPDATE_PLAYER_START_IDX]);
}

void CAuraDB::UpdateGamePlayerOnEnd(const string& name, const string& server, const string& ip, uint64_t loadingtime, uint64_t duration, uint64_t left)
{
  const string lowerName = ToLowerCase(name);

  if (!m_StmtCache[UPDATE_PLAYER_END_IDX]) {
    m_DB->Prepare(
      "INSERT INTO players (name, server, initialip, latestip, games, loadingtime, duration, left) "
      "VALUES (?, ?, ?, ?, 1, ?, ?, ?) "
      "ON CONFLICT(name, server) "
      "DO UPDATE SET "
      "latestip = excluded.latestip, "
      "games = games + 1, "
      "loadingtime = loadingtime + excluded.loadingtime, "
      "duration = duration + excluded.duration, "
      "left = left + excluded.left;",
      &(m_StmtCache[UPDATE_PLAYER_END_IDX]), true
    );
  }

  if (m_StmtCache[UPDATE_PLAYER_END_IDX] == nullptr)
  {
    Print("[SQLITE3] prepare error updating gameuser [" + lowerName + "@" + server + "] - " + m_DB->GetError());
    return;
  }

  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 1, lowerName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 2, server.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 3, ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 4, ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 5, loadingtime);
  sqlite3_bind_int64(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 6, duration);
  sqlite3_bind_int64(static_cast<sqlite3_stmt*>(m_StmtCache[UPDATE_PLAYER_END_IDX]), 7, left);

  const int32_t RC = m_DB->Step(m_StmtCache[UPDATE_PLAYER_END_IDX]);

  if (RC != SQLITE_DONE)
    Print("[SQLITE3] error updating gameuser on end [" + lowerName + "@" + server + "] - " + m_DB->GetError());

  m_DB->Reset(m_StmtCache[UPDATE_PLAYER_END_IDX]);
}

CDBGamePlayerSummary* CAuraDB::GamePlayerSummaryCheck(const string& rawName, const string& server)
{
  CDBGamePlayerSummary* GamePlayerSummary = nullptr;
  const string name = ToLowerCase(rawName);

  if (!m_StmtCache[PLAYER_SUMMARY_IDX]) {
    m_DB->Prepare("SELECT games, loadingtime, duration, left FROM players WHERE name=? AND server=?", &(m_StmtCache[PLAYER_SUMMARY_IDX]), true);
  }

  if (m_StmtCache[PLAYER_SUMMARY_IDX])
  {
    sqlite3_bind_text(m_StmtCache[PLAYER_SUMMARY_IDX], 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(m_StmtCache[PLAYER_SUMMARY_IDX], 2, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(m_StmtCache[PLAYER_SUMMARY_IDX]);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(m_StmtCache[PLAYER_SUMMARY_IDX]) == 4)
      {
        const uint32_t TotalGames  = sqlite3_column_int(m_StmtCache[PLAYER_SUMMARY_IDX], 0);
        const uint64_t LoadingTime = sqlite3_column_int64(m_StmtCache[PLAYER_SUMMARY_IDX], 1);
        const uint64_t Left        = sqlite3_column_int64(m_StmtCache[PLAYER_SUMMARY_IDX], 2);
        const uint64_t Duration    = sqlite3_column_int64(m_StmtCache[PLAYER_SUMMARY_IDX], 3);

        float AvgLoadingTime = 0.;
        uint32_t AvgLeftPercent = static_cast<uint32_t>(static_cast<double>(Duration) / Left * 100);
        if (TotalGames > 0) {
          AvgLoadingTime = static_cast<float>(static_cast<double>(LoadingTime) / TotalGames / 1000);
        }

        GamePlayerSummary = new CDBGamePlayerSummary(TotalGames, AvgLoadingTime, AvgLeftPercent);
      }
      else
        Print("[SQLITE3] error checking gameplayersummary [" + name + "@" + server + "] - row doesn't have 4 columns");
    }
    else if (RC == SQLITE_ERROR)
      Print("[SQLITE3] error checking gameplayersummary [" + name + "@" + server + "] - " + m_DB->GetError());

    m_DB->Reset(m_StmtCache[PLAYER_SUMMARY_IDX]);
  }
  else
    Print("[SQLITE3] prepare error checking gameplayersummary [" + name + "@" + server + "] - " + m_DB->GetError());

  return GamePlayerSummary;
}

void CAuraDB::UpdateDotAPlayerOnEnd(const string& name, const string& server, uint32_t winner, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t neutralkills, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills)
{
  bool          Success = false;
  sqlite3_stmt* Statement = nullptr;
  string lowerName = ToLowerCase(name);
  m_DB->Prepare("SELECT dotas, wins, losses, kills, deaths, creepkills, creepdenies, assists, neutralkills, towerkills, raxkills, courierkills FROM players WHERE name=? AND server=?", reinterpret_cast<void**>(&Statement));

  int32_t  RC;
  uint32_t Dotas  = 1;
  uint32_t Wins   = 0;
  uint32_t Losses = 0;

  if (winner == 1)
    ++Wins;
  else if (winner == 2)
    ++Losses;

  if (Statement == nullptr)
  {
    Print("[SQLITE3] prepare error adding dotaplayer [" + lowerName + "@" + server + "] - " + m_DB->GetError());
    return;
  }

  sqlite3_bind_text(Statement, 1, lowerName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);

  RC = m_DB->Step(Statement);

  if (RC == SQLITE_ROW)
  {
    Dotas += sqlite3_column_int(Statement, 0);
    Wins += sqlite3_column_int(Statement, 1);
    Losses += sqlite3_column_int(Statement, 2);
    kills += sqlite3_column_int(Statement, 3);
    deaths += sqlite3_column_int(Statement, 4);
    creepkills += sqlite3_column_int(Statement, 5);
    creepdenies += sqlite3_column_int(Statement, 6);
    assists += sqlite3_column_int(Statement, 7);
    neutralkills += sqlite3_column_int(Statement, 8);
    towerkills += sqlite3_column_int(Statement, 9);
    raxkills += sqlite3_column_int(Statement, 10);
    courierkills += sqlite3_column_int(Statement, 11);

    Success = true;
  }

  m_DB->Finalize(Statement);

  // there must be a row already because we add one, if not present, in UpdateGamePlayerOnStart( ) before the call to UpdateDotAPlayerOnEnd( )

  if (Success == false)
  {
    Print("[SQLITE3] error adding dotaplayer [" + lowerName + "@" + server + "] - no existing row");
    return;
  }

  m_DB->Prepare("UPDATE players SET dotas=?, wins=?, losses=?, kills=?, deaths=?, creepkills=?, creepdenies=?, assists=?, neutralkills=?, towerkills=?, raxkills=?, courierkills=? WHERE name=? AND server=?", reinterpret_cast<void**>(&Statement));

  if (Statement == nullptr)
  {
    Print("[SQLITE3] prepare error updating dotaplayer [" + lowerName + "@" + server + "] - " + m_DB->GetError());
    return;
  }

  sqlite3_bind_int(Statement, 1, Dotas);
  sqlite3_bind_int(Statement, 2, Wins);
  sqlite3_bind_int(Statement, 3, Losses);
  sqlite3_bind_int(Statement, 4, kills);
  sqlite3_bind_int(Statement, 5, deaths);
  sqlite3_bind_int(Statement, 6, creepkills);
  sqlite3_bind_int(Statement, 7, creepdenies);
  sqlite3_bind_int(Statement, 8, assists);
  sqlite3_bind_int(Statement, 9, neutralkills);
  sqlite3_bind_int(Statement, 10, towerkills);
  sqlite3_bind_int(Statement, 11, raxkills);
  sqlite3_bind_int(Statement, 12, courierkills);
  sqlite3_bind_text(Statement, 13, lowerName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(Statement, 14, server.c_str(), -1, SQLITE_TRANSIENT);

  RC = m_DB->Step(Statement);

  if (RC != SQLITE_DONE)
    Print("[SQLITE3] error adding dotaplayer [" + lowerName + "@" + server + "] - " + m_DB->GetError());

  m_DB->Finalize(Statement);
}

CDBDotAPlayerSummary* CAuraDB::DotAPlayerSummaryCheck(const string& rawName, const string& server)
{
  sqlite3_stmt*         Statement;
  CDBDotAPlayerSummary* DotAPlayerSummary = nullptr;
  string name = ToLowerCase(rawName);
  m_DB->Prepare("SELECT dotas, wins, losses, kills, deaths, creepkills, creepdenies, assists, neutralkills, towerkills, raxkills, courierkills FROM players WHERE name=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW)
    {
      if (sqlite3_column_count(Statement) == 12)
      {
        const uint32_t TotalGames = sqlite3_column_int(Statement, 0);

        if (TotalGames != 0)
        {
          const uint32_t TotalWins         = sqlite3_column_int(Statement, 1);
          const uint32_t TotalLosses       = sqlite3_column_int(Statement, 2);
          const uint32_t TotalKills        = sqlite3_column_int(Statement, 3);
          const uint32_t TotalDeaths       = sqlite3_column_int(Statement, 4);
          const uint32_t TotalCreepKills   = sqlite3_column_int(Statement, 5);
          const uint32_t TotalCreepDenies  = sqlite3_column_int(Statement, 6);
          const uint32_t TotalAssists      = sqlite3_column_int(Statement, 7);
          const uint32_t TotalNeutralKills = sqlite3_column_int(Statement, 8);
          const uint32_t TotalTowerKills   = sqlite3_column_int(Statement, 9);
          const uint32_t TotalRaxKills     = sqlite3_column_int(Statement, 10);
          const uint32_t TotalCourierKills = sqlite3_column_int(Statement, 11);

          DotAPlayerSummary = new CDBDotAPlayerSummary(TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills);
        }
      }
      else
        Print("[SQLITE3] error checking dotaplayersummary [" + name + "@" + server + "] - row doesn't have 12 columns");
    }

    m_DB->Finalize(Statement);
  }
  else
    Print("[SQLITE3] prepare error checking dotaplayersummary [" + name + "@" + server + "] - " + m_DB->GetError());

  return DotAPlayerSummary;
}

string CAuraDB::GetInitialIP(const string& rawName, const string& server)
{
  sqlite3_stmt*         Statement;
  string initialIP;
  string name = ToLowerCase(rawName);
  m_DB->Prepare("SELECT initialip FROM players WHERE name=? AND server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW) {
      if (sqlite3_column_count(Statement) == 1) {
        initialIP  = string((char*)sqlite3_column_text(Statement, 0));
      } else {
        Print("[SQLITE3] error checking initial ip [" + name + "@" + server + "] - row doesn't have 1 column");
      }
    } else if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error checking initial ip [" + name + "@" + server + "] - " + m_DB->GetError());
    }
    m_DB->Finalize(Statement);
  } else {
    Print("[SQLITE3] prepare error checking initial ip [" + name + "@" + server + "] - " + m_DB->GetError());
  }

  return initialIP;
}

string CAuraDB::GetLatestIP(const string& rawName, const string& server)
{
  sqlite3_stmt*         Statement;
  string latestIP;
  string name = ToLowerCase(rawName);
  m_DB->Prepare("SELECT latestip FROM players WHERE name=? AND server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW) {
      if (sqlite3_column_count(Statement) == 1) {
        latestIP  = string((char*)sqlite3_column_text(Statement, 0));
      } else {
        Print("[SQLITE3] error checking latest ip [" + name + "@" + server + "] - row doesn't have 1 column");
      }
    } else if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error checking latest ip [" + name + "@" + server + "] - " + m_DB->GetError());
    }
    m_DB->Finalize(Statement);
  } else {
    Print("[SQLITE3] prepare error checking latest ip [" + name + "@" + server + "] - " + m_DB->GetError());
  }

  return latestIP;
}

vector<string> CAuraDB::GetIPs(const string& rawName, const string& server)
{
  vector<string> addresses;

  sqlite3_stmt*         Statement;
  string initialIP, latestIP;
  string name = ToLowerCase(rawName);
  m_DB->Prepare("SELECT initialip, latestip FROM players WHERE name=? AND server=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(Statement, 2, server.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW) {
      if (sqlite3_column_count(Statement) == 2) {
        initialIP  = string((char*)sqlite3_column_text(Statement, 1));
        latestIP  = string((char*)sqlite3_column_text(Statement, 1));
      } else {
        Print("[SQLITE3] error checking ips [" + name + "@" + server + "] - row doesn't have 1 column");
      }
    } else if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error checking ips [" + name + "@" + server + "] - " + m_DB->GetError());
    }
    m_DB->Finalize(Statement);
  } else {
    Print("[SQLITE3] prepare error checking ips [" + name + "@" + server + "] - " + m_DB->GetError());
  }

  if (!initialIP.empty()) {
    addresses.push_back(initialIP);
  }
  if (!latestIP.empty() && initialIP != latestIP) {
    addresses.push_back(latestIP);
  }
  return addresses;
}

vector<string> CAuraDB::GetAlts(const string& addressLiteral)
{
  vector<string> altAccounts;
  if (addressLiteral.empty()) {
    return altAccounts;
  }
  optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(addressLiteral);
  if (!maybeAddress.has_value() || isLoopbackAddress(&(maybeAddress.value()))) {
    return altAccounts;
  }

  sqlite3_stmt*         Statement;
  m_DB->Prepare("SELECT name, server FROM players WHERE initialip=? OR latestip=?", reinterpret_cast<void**>(&Statement));

  if (Statement)
  {
    sqlite3_bind_text(Statement, 1, addressLiteral.c_str(), -1, SQLITE_TRANSIENT);

    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW) {
      if (sqlite3_column_count(Statement) == 2) {
        string altName  = string((char*)sqlite3_column_text(Statement, 0));
        string altServer  = string((char*)sqlite3_column_text(Statement, 1));
        if (altServer.empty()) {
          altAccounts.push_back(altName + "@@@LAN/VPN");
        } else {
          altAccounts.push_back(altName + "@" + altServer);
        }
      } else {
        Print("[SQLITE3] error checking alts [" + addressLiteral + "] - row doesn't have 2 columns");
      }
    } else if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error checking alts [" + addressLiteral + "] - " + m_DB->GetError());
    }
    m_DB->Finalize(Statement);
  } else {
    Print("[SQLITE3] prepare error checking alts [" + addressLiteral + "] - " + m_DB->GetError());
  }

  return altAccounts;
}

bool CAuraDB::GameAdd(const uint64_t gameId, const string& creator, const string& mapClientPath, const string& mapServerPath, const array<uint8_t, 4>& mapCRC32, const vector<string>& playerNames, const vector<uint8_t>& playerIDs, const vector<uint8_t>& slotIDs, const vector<uint8_t>& colorIDs)
{
  string storageCRC32 = ByteArrayToDecString(mapCRC32);
  string storagePlayerNames = JoinVector(playerNames, false);

  vector<uint8_t> storageIDs;
  storageIDs.reserve(playerIDs.size() * 3);
  storageIDs.insert(storageIDs.end(), playerIDs.begin(), playerIDs.end());
  storageIDs.insert(storageIDs.end(), slotIDs.begin(), slotIDs.end());
  storageIDs.insert(storageIDs.end(), colorIDs.begin(), colorIDs.end());
  string storageIDsText = ByteArrayToDecString(storageIDs);

  if (!m_StmtCache[GAME_ADD_IDX]) {
    m_DB->Prepare("INSERT OR REPLACE INTO games ( id, creator, mapcpath, mapspath, crc32, playernames, playerids ) VALUES ( ?, ?, ?, ?, ?, ?, ? )", reinterpret_cast<void**>(&(m_StmtCache[GAME_ADD_IDX])));
  }

  if (!m_StmtCache[GAME_ADD_IDX]) {
    Print("[SQLITE3] prepare error adding game [" + to_string(gameId) + ", created by " + creator + "] - " + m_DB->GetError());
    return false;
  }

  bool Success = false;
  sqlite3_bind_int64(m_StmtCache[GAME_ADD_IDX], 1, unsigned_to_signed_64(gameId));
  sqlite3_bind_text(m_StmtCache[GAME_ADD_IDX], 2, creator.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(m_StmtCache[GAME_ADD_IDX], 3, mapClientPath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(m_StmtCache[GAME_ADD_IDX], 4, mapServerPath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(m_StmtCache[GAME_ADD_IDX], 5, storageCRC32.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(m_StmtCache[GAME_ADD_IDX], 6, storagePlayerNames.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(m_StmtCache[GAME_ADD_IDX], 7, storageIDsText.c_str(), -1, SQLITE_TRANSIENT);

  const int32_t RC = m_DB->Step(m_StmtCache[GAME_ADD_IDX]);

  if (RC == SQLITE_DONE)
    Success = true;
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error adding game [" + to_string(gameId) + ", created by " + creator + "] - " + m_DB->GetError());

  m_DB->Reset(m_StmtCache[GAME_ADD_IDX]);
  return Success;
}

CDBGameSummary* CAuraDB::GameCheck(const uint64_t gameId)
{
  string playerNames = "";
  string playerIDs = "";
  bool success = false;

  sqlite3_stmt* Statement = nullptr;
  m_DB->Prepare(R"(SELECT playernames, playerids FROM games WHERE id=?)", reinterpret_cast<void**>(&Statement));
  if (Statement) {
    sqlite3_bind_int64(Statement, 1, unsigned_to_signed_64(gameId));
    const int32_t RC = m_DB->Step(Statement);

    if (RC == SQLITE_ROW) {
      if (sqlite3_column_count(Statement) == 2) {
        playerNames = string((char*)sqlite3_column_text(Statement, 0));
        playerIDs = string((char*)sqlite3_column_text(Statement, 1));
        success = true;
      } else {
        Print("[SQLITE3] error checking game [" + to_string(gameId) + "] - row doesn't have 2 columns");
      }
    } else if (RC == SQLITE_ERROR) {
      Print("[SQLITE3] error checking game [" + to_string(gameId) + "] " + m_DB->GetError());
    } else {
      Print("[SQLITE3] error checking game return code " + to_string(RC));
    }

    m_DB->Finalize(Statement);
  }

  if (success) {
    return new CDBGameSummary(gameId, playerNames, playerIDs);
  } else {
    return nullptr;
  }
}

string CAuraDB::FromCheck(uint32_t ip)
{
  // a big thank you to tjado for help with the iptocountry feature

  string From = "??";

  if (!m_StmtCache[FROM_CHECK_IDX]) {
    m_DB->Prepare("SELECT country FROM iptocountry WHERE ip1<=? AND ip2>=?", &(m_StmtCache[FROM_CHECK_IDX]), true);
  }

  if (!m_StmtCache[FROM_CHECK_IDX]) {
    Print("[SQLITE3] prepare error checking iptocountry [" + to_string(ip) + "] - " + m_DB->GetError());
    return From;
  }

  sqlite3_bind_int(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_CHECK_IDX]), 1, unsigned_to_signed_32(ip));
  sqlite3_bind_int(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_CHECK_IDX]), 2, unsigned_to_signed_32(ip));

  const int32_t RC = m_DB->Step(m_StmtCache[FROM_CHECK_IDX]);

  if (RC == SQLITE_ROW)
  {
    if (sqlite3_column_count(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_CHECK_IDX])) == 1)
      From = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_CHECK_IDX]), 0));
    else
      Print("[SQLITE3] error checking iptocountry [" + to_string(ip) + "] - row doesn't have 1 column");
  }
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error checking iptocountry [" + to_string(ip) + "] - " + m_DB->GetError());

  m_DB->Reset(m_StmtCache[FROM_CHECK_IDX]);

  return From;
}

bool CAuraDB::FromAdd(uint32_t ip1, uint32_t ip2, const string& country)
{
  // a big thank you to tjado for help with the iptocountry feature

  bool Success = false;

  if (!m_StmtCache[FROM_ADD_IDX]) {
    m_DB->Prepare("INSERT INTO iptocountry VALUES ( ?, ?, ? )", &(m_StmtCache[FROM_ADD_IDX]), true);
  }

  if (!m_StmtCache[FROM_ADD_IDX]) {
    Print("[SQLITE3] prepare error adding iptocountry [" + to_string(ip1) + " : " + to_string(ip2) + " : " + country + "] - " + m_DB->GetError());
    return false;
  }

  // Losslessly converting IPs to signed 32-bits integers rather than to same-value 64-bits integers
  // This saves ~400 KB in initial database size, down from 3.6 MB to just about 3.17 MB
  // (for reference, ip-to-country.csv is 5.92 MB)
  sqlite3_bind_int(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_ADD_IDX]), 1, unsigned_to_signed_32(ip1));
  sqlite3_bind_int(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_ADD_IDX]), 2, unsigned_to_signed_32(ip2));
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[FROM_ADD_IDX]), 3, country.c_str(), -1, SQLITE_TRANSIENT);

  int32_t RC = m_DB->Step(m_StmtCache[FROM_ADD_IDX]);

  if (RC == SQLITE_DONE)
    Success = true;
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error adding iptocountry [" + to_string(ip1) + " : " + to_string(ip2) + " : " + country + "] - " + m_DB->GetError());

  m_DB->Reset(m_StmtCache[FROM_ADD_IDX]);

  return Success;
}

bool CAuraDB::AliasAdd(const string& alias, const string& target)
{
  bool Success = false;
  if (alias.empty() || target.empty()) {
    return Success;
  }

  if (!m_StmtCache[ALIAS_ADD_IDX]) {
    m_DB->Prepare("INSERT OR REPLACE INTO aliases VALUES ( ?, ? )", &(m_StmtCache[ALIAS_ADD_IDX]), true);
  }

  if (!m_StmtCache[ALIAS_ADD_IDX]) {
    Print("[SQLITE3] prepare error adding alias [" + alias + ": " + target + "] - " + m_DB->GetError());
    return false;
  }

  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[ALIAS_ADD_IDX]), 1, alias.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[ALIAS_ADD_IDX]), 2, target.c_str(), -1, SQLITE_TRANSIENT);

  int32_t RC = m_DB->Step(m_StmtCache[ALIAS_ADD_IDX]);

  if (RC == SQLITE_DONE)
    Success = true;
  else if (RC == SQLITE_ERROR)
    Print("[SQLITE3] error adding alias [" + alias + ": " + target + "] - " + m_DB->GetError());

  m_DB->Reset(m_StmtCache[ALIAS_ADD_IDX]);

  return Success;
}

string CAuraDB::AliasCheck(const string& alias)
{
  string value;
  if (alias.empty()) {
    return value;
  }

  if (!m_StmtCache[ALIAS_CHECK_IDX]) {
    m_DB->Prepare("SELECT value FROM aliases WHERE alias=?", &(m_StmtCache[ALIAS_CHECK_IDX]), true);
  }

  if (!m_StmtCache[ALIAS_CHECK_IDX]) {
    Print("[SQLITE3] prepare error checking alias [" + alias + "] - " + m_DB->GetError());
    return value;
  }

  sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_StmtCache[ALIAS_CHECK_IDX]), 1, alias.c_str(), -1, SQLITE_TRANSIENT);

  const int32_t RC = m_DB->Step(m_StmtCache[ALIAS_CHECK_IDX]);

  if (RC == SQLITE_ERROR) {
    Print("[SQLITE3] error checking alias [" + alias + "] - " + m_DB->GetError());
    return value;
  }

  if (RC == SQLITE_ROW) {
    if (sqlite3_column_count(static_cast<sqlite3_stmt*>(m_StmtCache[ALIAS_CHECK_IDX])) == 1) {
      value = string((char*)sqlite3_column_text(static_cast<sqlite3_stmt*>(m_StmtCache[ALIAS_CHECK_IDX]), 0));
    } else {
      Print("[SQLITE3] error checking alias [" + alias + "] - row doesn't have 1 column");
    }
  }

  m_DB->Reset(m_StmtCache[ALIAS_CHECK_IDX]);

  return value;
}

void CAuraDB::InitMapData()
{
  m_SearchableMapData[MAP_TYPE_TWRPG] = new CSearchableMapData(MAP_TYPE_TWRPG);
  m_SearchableMapData[MAP_TYPE_TWRPG]->LoadData(m_TWRPGFile);
}

CSearchableMapData* CAuraDB::GetMapData(uint8_t mapType) const
{
  switch (mapType) {
    case MAP_TYPE_TWRPG: {
      auto it = m_SearchableMapData.find(mapType);
      if (it == m_SearchableMapData.end()) return nullptr;
      return it->second;
    }
    default:
      return nullptr;
  }
}

uint8_t CAuraDB::FindData(const uint8_t mapType, const uint8_t searchDataType, string& objectName, const bool exactMatch) const
{
  CSearchableMapData* index = GetMapData(mapType);
  if (index == nullptr) return MAP_DATA_TYPE_NONE;
  return index->Search(objectName, searchDataType, exactMatch);
}

vector<string> CAuraDB::GetDescription(const uint8_t mapType, const uint8_t searchDataType, const string& objectName) const
{
  CSearchableMapData* index = GetMapData(mapType);
  if (index == nullptr) return vector<string>();
  auto dataIt = index->m_Data.find(searchDataType);
  if (dataIt == index->m_Data.end()) return vector<string>();
  map<string, vector<string>> descriptions = dataIt->second;
  auto descIt = descriptions.find(objectName);
  if (descIt == descriptions.end()) {
    return vector<string>();
  }
  return descIt->second;
}

//
// CDBBan
//

CDBBan::CDBBan(string nName, string nServer, string nAuthServer, string nIP, string nDate, string nExpiry, bool nPermanent, string nModerator, string nReason)
  : m_Name(std::move(nName)),
    m_Server(std::move(nServer)),
    m_AuthServer(std::move(nAuthServer)),
    m_IP(std::move(nIP)),
    m_Date(std::move(nDate)),
    m_Expiry(std::move(nExpiry)),
    m_Permanent(nPermanent),
    m_Moderator(std::move(nModerator)),
    m_Reason(std::move(nReason)),
    m_Suspect(false)
{
}

CDBBan::~CDBBan() = default;

//
// CDBGamePlayer
//

CDBGamePlayer::CDBGamePlayer(string nName, string nServer, string nIP/*, uint8_t nUID, uint8_t nSID*/, uint8_t nColor)
  : m_Name(std::move(nName)),
    m_Server(std::move(nServer)),
    m_IP(std::move(nIP)),
    m_LoadingTime(0),
    m_LeftTime(0),
    /*m_UID(nUID),
    m_SID(nSID),*/
    m_Color(nColor)
{
}

CDBGamePlayer::~CDBGamePlayer() = default;

//
// CDBGameSummary
//

CDBGameSummary::CDBGameSummary(uint64_t nID, string playerNames, string playerIDs)
  : m_ID(nID),
    m_PlayerNames(SplitArgs(playerNames, 1, 24))
{
  uint8_t playerCount = static_cast<uint8_t>(m_PlayerNames.size());
  if (playerCount == 0) {
    return;
  }
  vector<uint8_t> rawIDs = ExtractNumbers(playerIDs, 3 * playerCount);
  if (rawIDs.size() % 3 != 0 || rawIDs.size() != playerCount * 3) {
    return;
  }
  m_UIDs = vector<uint8_t>(rawIDs.begin(), rawIDs.begin() + playerCount);
  m_SIDs = vector<uint8_t>(rawIDs.begin() + playerCount, rawIDs.begin() + 2 * playerCount);
  m_Colors = vector<uint8_t>(rawIDs.begin() + 2 * playerCount, rawIDs.end());
}

CDBGameSummary::~CDBGameSummary() = default;

//
// CDBGamePlayerSummary
//

CDBGamePlayerSummary::CDBGamePlayerSummary(uint32_t nTotalGames, float nAvgLoadingTime, uint32_t nAvgLeftPercent)
  : m_TotalGames(nTotalGames),
    m_AvgLoadingTime(nAvgLoadingTime),
    m_AvgLeftPercent(nAvgLeftPercent)
{
}

CDBGamePlayerSummary::~CDBGamePlayerSummary() = default;

//
// CDBDotAPlayer
//

CDBDotAPlayer::CDBDotAPlayer()
  : m_Color(0),
    m_NewColor(0),
    m_Kills(0),
    m_Deaths(0),
    m_CreepKills(0),
    m_CreepDenies(0),
    m_Assists(0),
    m_NeutralKills(0),
    m_TowerKills(0),
    m_RaxKills(0),
    m_CourierKills(0)
{
}

CDBDotAPlayer::CDBDotAPlayer(uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nNeutralKills, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills)
  : m_Color(0),
    m_NewColor(0),
    m_Kills(nKills),
    m_Deaths(nDeaths),
    m_CreepKills(nCreepKills),
    m_CreepDenies(nCreepDenies),
    m_Assists(nAssists),
    m_NeutralKills(nNeutralKills),
    m_TowerKills(nTowerKills),
    m_RaxKills(nRaxKills),
    m_CourierKills(nCourierKills)
{
}

CDBDotAPlayer::~CDBDotAPlayer() = default;

//
// CDBDotAPlayerSummary
//

CDBDotAPlayerSummary::CDBDotAPlayerSummary(uint32_t nTotalGames, uint32_t nTotalWins, uint32_t nTotalLosses, uint32_t nTotalKills, uint32_t nTotalDeaths, uint32_t nTotalCreepKills, uint32_t nTotalCreepDenies, uint32_t nTotalAssists, uint32_t nTotalNeutralKills, uint32_t nTotalTowerKills, uint32_t nTotalRaxKills, uint32_t nTotalCourierKills)
  : m_TotalGames(nTotalGames),
    m_TotalWins(nTotalWins),
    m_TotalLosses(nTotalLosses),
    m_TotalKills(nTotalKills),
    m_TotalDeaths(nTotalDeaths),
    m_TotalCreepKills(nTotalCreepKills),
    m_TotalCreepDenies(nTotalCreepDenies),
    m_TotalAssists(nTotalAssists),
    m_TotalNeutralKills(nTotalNeutralKills),
    m_TotalTowerKills(nTotalTowerKills),
    m_TotalRaxKills(nTotalRaxKills),
    m_TotalCourierKills(nTotalCourierKills)
{
}

CDBDotAPlayerSummary::~CDBDotAPlayerSummary() = default;
