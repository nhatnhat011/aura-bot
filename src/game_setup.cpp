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

#include "game_setup.h"

#include "auradb.h"
#include "command.h"
#include "file_util.h"
#include "game.h"
#include "protocol/game_protocol.h"
#include "hash.h"
#include "irc.h"
#include "map.h"
#include "realm.h"
#include "save_game.h"

#include "aura.h"

#define SEARCH_RESULT(a, b) (make_pair(a, b))

using namespace std;

// CGameExtraOptions

CGameExtraOptions::CGameExtraOptions()
{
}

CGameExtraOptions::CGameExtraOptions(const optional<bool>& nRandomRaces, const optional<bool>& nRandomHeroes, const optional<uint8_t>& nVisibility, const optional<uint8_t>& nSpeed, const optional<uint8_t>& nObservers)
  : m_TeamsLocked(false),
    m_TeamsTogether(false),
    m_RandomRaces(nRandomRaces),
    m_RandomHeroes(nRandomHeroes),
    m_Visibility(nVisibility),
    m_Speed(nSpeed),
    m_Observers(nObservers)
{
}

bool CGameExtraOptions::ParseMapObservers(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "no" || lower == "none" || lower == "no observers" || lower == "no obs" || lower == "sin obs" || lower == "sin observador" || lower == "sin observadores") {
    m_Observers = MAPOBS_NONE;
  } else if (lower == "referee" || lower == "referees" || lower == "arbiter" || lower == "arbitro" || lower == "arbitros" || lower == "árbitros") {
    m_Observers = MAPOBS_REFEREES;
  } else if (lower == "observadores derrotados" || lower == "derrotados" || lower == "obs derrotados" || lower == "obs on defeat" || lower == "observers on defeat" || lower == "on defeat" || lower == "defeat" || lower == "ondefeat") {
    m_Observers = MAPOBS_ONDEFEAT;
  } else if (lower == "full observers" || lower == "solo observadores" || lower == "full") {
    m_Observers = MAPOBS_ALLOWED;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapVisibility(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "no" || lower == "default" || lower == "predeterminado" || lower == "fog" || lower == "fog of war" || lower == "niebla" || lower == "niebla de guerra" || lower == "fow") {
    m_Visibility = MAPVIS_DEFAULT;
  } else if (lower == "hide terrain" || lower == "hide" || lower == "ocultar terreno" || lower == "ocultar" || lower == "hidden") {
    m_Visibility = MAPVIS_HIDETERRAIN;
  } else if (lower == "explored map" || lower == "map explored" || lower == "explored" || lower == "mapa explorado" || lower == "explorado") {
    m_Visibility = MAPVIS_EXPLORED;
  } else if (lower == "always visible" || lower == "always" || lower == "visible" || lower == "todo visible" || lower == "todo" || lower == "revelar" || lower == "todo revelado") {
    m_Visibility = MAPVIS_ALWAYSVISIBLE;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapSpeed(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "slow") {
    m_Speed = MAPSPEED_SLOW;
  } else if (lower == "normal") {
    m_Speed = MAPSPEED_NORMAL;
  } else if (lower == "fast") {
    m_Speed = MAPSPEED_FAST;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapRandomRaces(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "random race" || lower == "rr" || lower == "yes" || lower == "random" || lower == "random races") {
    m_RandomRaces = true;
  } else if (lower == "default" || lower == "no" || lower == "predeterminado") {
    m_RandomRaces = false;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapRandomHeroes(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "random hero" || lower == "rh" || lower == "yes" || lower == "random" || lower == "random heroes") {
    m_RandomHeroes = true;
  } else if (lower == "default" || lower == "no" || lower == "predeterminado") {
    m_RandomHeroes = false;
  } else {
    return false;
  }
  return true;
}

void CGameExtraOptions::AcquireCLI(const CCLI* nCLI) {
  if (nCLI->m_GameObservers.has_value()) ParseMapObservers(nCLI->m_GameObservers.value());
  if (nCLI->m_GameVisibility.has_value()) ParseMapVisibility(nCLI->m_GameVisibility.value());
  if (nCLI->m_GameSpeed.has_value()) ParseMapSpeed(nCLI->m_GameSpeed.value());
  if (nCLI->m_GameTeamsLocked.has_value()) m_TeamsLocked = nCLI->m_GameTeamsLocked.value();
  if (nCLI->m_GameTeamsTogether.has_value()) m_TeamsTogether = nCLI->m_GameTeamsTogether.value();
  if (nCLI->m_GameAdvancedSharedUnitControl.has_value()) m_AdvancedSharedUnitControl = nCLI->m_GameAdvancedSharedUnitControl.value();
  if (nCLI->m_GameRandomRaces.has_value()) m_RandomRaces = nCLI->m_GameRandomRaces.value();
  if (nCLI->m_GameRandomHeroes.has_value()) m_RandomHeroes = nCLI->m_GameRandomHeroes.value();
}

CGameExtraOptions::~CGameExtraOptions() = default;

//
// CGameSetup
//

CGameSetup::CGameSetup(CAura* nAura, shared_ptr<CCommandContext> nCtx, CConfig* nMapCFG)
  : m_Aura(nAura),
    m_RestoredGame(nullptr),
    //m_Map(nullptr),
    m_Ctx(nCtx),

    m_Attribution(nCtx->GetUserAttribution()),
    m_SearchRawTarget(string()),
    m_SearchType(SEARCH_TYPE_ANY),

    m_AllowPaths(false),
    m_StandardPaths(false),
    m_LuckyMode(false),

    m_FoundSuggestions(false),
    m_IsDownloadable(false),
    m_IsStepDownloading(false),
    m_IsStepDownloaded(false),
    m_MapDownloadSize(0),
    m_DownloadFileStream(nullptr),
    m_DownloadTimeout(m_Aura->m_Net.m_Config.m_DownloadTimeout),
    m_SuggestionsTimeout(SUGGESTIONS_TIMEOUT),
    m_AsyncStep(GAMESETUP_STEP_MAIN),

    m_IsMapDownloaded(false),

    m_OwnerLess(false),
    m_IsMirror(false),    
    m_RealmsDisplayMode(GAME_PUBLIC),
    m_LobbyReplaceable(false),
    m_LobbyAutoRehosted(false),
    m_CreationCounter(0),

    m_CreatedFrom(nullptr),
    m_CreatedFromType(SERVICE_TYPE_NONE),

    m_MapExtraOptions(nullptr),
    m_MapReadyCallbackAction(MAP_ONREADY_SET_ACTIVE),

    m_ExitingSoon(false),
    m_DeleteMe(false)
{
  memset(&m_RealmsAddress, 0, sizeof(m_RealmsAddress));
  m_Map = GetBaseMapFromConfig(nMapCFG, false);
}

CGameSetup::CGameSetup(CAura* nAura, shared_ptr<CCommandContext> nCtx, const string nSearchRawTarget, const uint8_t nSearchType, const bool nAllowPaths, const bool nUseStandardPaths, const bool nUseLuckyMode)
  : m_Aura(nAura),
    m_RestoredGame(nullptr),
    //m_Map(nullptr),
    m_Ctx(nCtx),

    m_Attribution(nCtx->GetUserAttribution()),
    m_SearchRawTarget(move(nSearchRawTarget)),
    m_SearchType(nSearchType),

    m_AllowPaths(nAllowPaths),
    m_StandardPaths(nUseStandardPaths),
    m_LuckyMode(nUseLuckyMode),

    m_FoundSuggestions(false),
    m_IsDownloadable(false),
    m_IsStepDownloading(false),
    m_IsStepDownloaded(false),
    m_MapDownloadSize(0),
    m_DownloadFileStream(nullptr),
    m_DownloadTimeout(m_Aura->m_Net.m_Config.m_DownloadTimeout),
    m_SuggestionsTimeout(SUGGESTIONS_TIMEOUT),
    m_AsyncStep(GAMESETUP_STEP_MAIN),

    m_IsMapDownloaded(false),

    m_OwnerLess(false),
    m_IsMirror(false),    
    m_RealmsDisplayMode(GAME_PUBLIC),
    m_LobbyReplaceable(false),
    m_LobbyAutoRehosted(false),
    m_CreationCounter(0),

    m_CreatedFrom(nullptr),
    m_CreatedFromType(SERVICE_TYPE_NONE),

    m_MapExtraOptions(nullptr),
    m_MapReadyCallbackAction(MAP_ONREADY_SET_ACTIVE),

    m_ExitingSoon(false),
    m_DeleteMe(false)
    
{
  memset(&m_RealmsAddress, 0, sizeof(m_RealmsAddress));
}

std::string CGameSetup::GetInspectName() const
{
  return m_Map->GetConfigName();
}

bool CGameSetup::GetIsStale() const
{
  if (m_ActiveTicks.has_value()) return false;
  return m_ActiveTicks.value() + GAMESETUP_STALE_TICKS < GetTicks();
}

void CGameSetup::ParseInputLocal()
{
  m_SearchTarget = make_pair("local", m_SearchRawTarget);
}

void CGameSetup::ParseInput()
{
  if (m_StandardPaths) {
    ParseInputLocal();
    return;
  }

  if (m_SearchType != SEARCH_TYPE_ANY) {
    ParseInputLocal();
    return;
  }

  string lower = m_SearchRawTarget;
  transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });

  string aliasSource = GetNormalizedAlias(lower);
  string aliasTarget;
  if (!aliasSource.empty()) aliasTarget = m_Aura->m_DB->AliasCheck(aliasSource);
  if (!aliasTarget.empty()) {
    m_SearchRawTarget = aliasTarget;
    lower = m_SearchRawTarget;
    transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });
  } else {
    auto it = m_Aura->m_LastMapIdentifiersFromSuggestions.find(lower);
    if (it != m_Aura->m_LastMapIdentifiersFromSuggestions.end()) {
      m_SearchRawTarget = it->second;
      lower = ToLowerCase(m_SearchRawTarget);
    }
  }

  if (lower.length() >= 6 && (lower.substr(0, 6) == "local-" || lower.substr(0, 6) == "local:")) {
    ParseInputLocal();
    return;
  }

  // Custom namespace/protocol
  if (lower.length() >= 8 && (lower.substr(0, 8) == "epicwar-" || lower.substr(0, 8) == "epicwar:")) {
    m_SearchTarget = make_pair("epicwar", MaybeBase10(lower.substr(8)));
    m_IsDownloadable = true;
    return;
  }

  if (lower.length() >= 8 && (lower.substr(0, 8) == "wc3maps-" || lower.substr(0, 8) == "wc3maps:")) {
    m_SearchTarget = make_pair("wc3maps", MaybeBase10(lower.substr(8)));
    m_IsDownloadable = true;
    return;
  }
 

  bool isUri = false;
  if (lower.length() >= 7 && lower.substr(0, 7) == "http://") {
    isUri = true;
    lower = lower.substr(7);
  } else if (lower.length() >= 8 && lower.substr(0, 8) == "https://") {
    isUri = true;
    lower = lower.substr(8);
  }

  if (lower.length() >= 17 && lower.substr(0, 12) == "epicwar.com/") {
    string mapCode = lower.substr(17);
    m_SearchTarget = make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.length() >= 21 && lower.substr(0, 16) == "www.epicwar.com/") {
    string mapCode = lower.substr(21);
    m_SearchTarget = make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.length() >= 16 && lower.substr(0, 12) == "wc3maps.com/") {
    string::size_type mapCodeEnd = lower.find_first_of('/', 16);
    string mapCode = mapCodeEnd == string::npos ? lower.substr(16) : lower.substr(16, mapCodeEnd - 16);
    m_SearchTarget = make_pair("wc3maps", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.length() >= 20 && lower.substr(0, 16) == "www.wc3maps.com/") {
    string::size_type mapCodeEnd = lower.find_first_of('/', 20);
    string mapCode = mapCodeEnd == string::npos ? lower.substr(20) : lower.substr(20, mapCodeEnd - 20);
    m_SearchTarget = make_pair("wc3maps", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (isUri) {
    m_SearchTarget = make_pair("remote", string());
  } else {
    ParseInputLocal();
  }
}


pair<uint8_t, filesystem::path> CGameSetup::SearchInputStandard()
{
  // Error handling: SearchInputStandard() reports the full file path
  // This can be absolute. That's not a problem.
  filesystem::path targetPath = m_SearchTarget.second;
  if (!FileExists(targetPath)) {
    Print("[CLI] File not found: " + PathToString(targetPath));
    if (!targetPath.is_absolute()) {
      try {
        Print("[CLI] (File resolved to: " + PathToString(filesystem::absolute(targetPath)) + ")");
      } catch (...) {}
    }
    return SEARCH_RESULT(MATCH_TYPE_NONE, targetPath);
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP) {
    return SEARCH_RESULT(MATCH_TYPE_MAP, targetPath);
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG) {
    return SEARCH_RESULT(MATCH_TYPE_CONFIG, targetPath);
  }
  if (m_SearchTarget.second.length() < 5) {
    return SEARCH_RESULT(MATCH_TYPE_INVALID, targetPath);
  }

  string targetExt = ParseFileExtension(PathToString(targetPath.filename()));
  if (targetExt == ".w3m" || targetExt == ".w3x") {
    return SEARCH_RESULT(MATCH_TYPE_MAP, targetPath);
  }
  if (targetExt == ".ini") {
    return SEARCH_RESULT(MATCH_TYPE_CONFIG, targetPath);
  }
  return SEARCH_RESULT(MATCH_TYPE_INVALID, targetPath);
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocalExact()
{
  string fileExtension = ParseFileExtension(m_SearchTarget.second);
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config.m_MapPath / filesystem::path(m_SearchTarget.second)).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config.m_MapPath.parent_path()) {
      return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if ((fileExtension == ".w3m" || fileExtension == ".w3x") && FileExists(testPath)) {
      return SEARCH_RESULT(MATCH_TYPE_MAP, testPath);
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config.m_MapCFGPath / filesystem::path(m_SearchTarget.second)).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config.m_MapCFGPath.parent_path()) {
      return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if ((fileExtension == ".ini") && FileExists(testPath)) {
      return SEARCH_RESULT(MATCH_TYPE_CONFIG, testPath);
    }
  }
  return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocalTryExtensions()
{
  string fileExtension = ParseFileExtension(m_SearchTarget.second);
  bool hasExtension = fileExtension == ".w3x" || fileExtension == ".w3m" || fileExtension == ".ini";
  if (hasExtension) {
    return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config.m_MapPath / filesystem::path(m_SearchTarget.second + ".w3x")).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config.m_MapPath.parent_path()) {
      return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if (FileExists(testPath)) {
      return SEARCH_RESULT(MATCH_TYPE_MAP, testPath);
    }
    testPath = (m_Aura->m_Config.m_MapPath / filesystem::path(m_SearchTarget.second + ".w3m")).lexically_normal();
    if (FileExists(testPath)) {
      return SEARCH_RESULT(MATCH_TYPE_MAP, testPath);
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config.m_MapCFGPath / filesystem::path(m_SearchTarget.second + ".ini")).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config.m_MapCFGPath.parent_path()) {
      return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if (FileExists(testPath)) {
      return SEARCH_RESULT(MATCH_TYPE_CONFIG, testPath);
    }
  }
  return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocalFuzzy(vector<string>& fuzzyMatches)
{
  vector<pair<string, int>> allResults;
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    vector<pair<string, int>> mapResults = FuzzySearchFiles(m_Aura->m_Config.m_MapPath, FILE_EXTENSIONS_MAP, m_SearchTarget.second);
    for (const auto& result : mapResults) {
      // Whether 0x80 is set flags the type of result: If it is there, it's a map
      allResults.push_back(make_pair(result.first, result.second | 0x80));
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    vector<pair<string, int>> cfgResults = FuzzySearchFiles(m_Aura->m_Config.m_MapCFGPath, FILE_EXTENSIONS_CONFIG, m_SearchTarget.second);
    allResults.insert(allResults.end(), cfgResults.begin(), cfgResults.end());
  }
  if (allResults.empty()) {
    return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
  }

  size_t resultCount = min(FUZZY_SEARCH_MAX_RESULTS, static_cast<int>(allResults.size()));
  partial_sort(
    allResults.begin(),
    allResults.begin() + resultCount,
    allResults.end(),
    [](const pair<string, int>& a, const pair<string, int>& b) {
        return (a.second &~ 0x80) < (b.second &~ 0x80);
    }
  );

  if (m_LuckyMode || allResults.size() == 1) {
    if (allResults[0].second & 0x80) {
      return SEARCH_RESULT(MATCH_TYPE_MAP, m_Aura->m_Config.m_MapPath / filesystem::path(allResults[0].first));
    } else {
      return SEARCH_RESULT(MATCH_TYPE_CONFIG, m_Aura->m_Config.m_MapCFGPath / filesystem::path(allResults[0].first));
    }
  }

  // Return suggestions through passed argument.
  for (uint8_t i = 0; i < resultCount; i++) {
    fuzzyMatches.push_back(allResults[i].first);
  }

  return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
}

#ifndef DISABLE_CPR
void CGameSetup::SearchInputRemoteFuzzy(vector<string>& fuzzyMatches) {
  if (fuzzyMatches.size() >= 5) return;
  vector<pair<string, string>> remoteSuggestions = GetMapRepositorySuggestions(m_SearchTarget.second, 5 - static_cast<uint8_t>(fuzzyMatches.size()));
  if (remoteSuggestions.empty()) {
    return;
  }
  // Return suggestions through passed argument.
  m_FoundSuggestions = true;
  m_Aura->m_LastMapIdentifiersFromSuggestions.clear();
  for (const auto& suggestion : remoteSuggestions) {
    string lowerName = ToLowerCase(suggestion.first);
    m_Aura->m_LastMapIdentifiersFromSuggestions[lowerName] = suggestion.second;
    fuzzyMatches.push_back(suggestion.first + " (" + suggestion.second + ")");
  }
}
#endif

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocal(vector<string>& fuzzyMatches)
{
  // 1. Try exact match
  pair<uint8_t, filesystem::path> exactResult = SearchInputLocalExact();
  if (exactResult.first == MATCH_TYPE_MAP || exactResult.first == MATCH_TYPE_CONFIG) {
    return exactResult;
  }
  // 2. If no extension, try adding extensions: w3x, w3m, ini
  pair<uint8_t, filesystem::path> plusExtensionResult = SearchInputLocalTryExtensions();
  if (plusExtensionResult.first == MATCH_TYPE_MAP || plusExtensionResult.first == MATCH_TYPE_CONFIG) {
    return plusExtensionResult;
  }
  // 3. Fuzzy search
  if (!m_Aura->m_Config.m_StrictSearch) {
    pair<uint8_t, filesystem::path> fuzzyResult = SearchInputLocalFuzzy(fuzzyMatches);
    if (fuzzyResult.first == MATCH_TYPE_MAP || fuzzyResult.first == MATCH_TYPE_CONFIG) {
      return fuzzyResult;
    }
  }
  return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInput()
{
  if (m_StandardPaths) {
    return SearchInputStandard();
  }

  if (m_SearchTarget.first == "remote") {
    // "remote" means unsupported URL.
    // Supported URLs specify the domain in m_SearchTarget.first, such as "epicwar".
    return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());
  }

  if (m_SearchTarget.first == "local") {
    filesystem::path testSearchPath = filesystem::path(m_SearchTarget.second);
    if (testSearchPath != testSearchPath.filename()) {
      if (m_AllowPaths) {
        // Search target has slashes. Treat as standard path.
        return SearchInputStandard();
      } else {
        // Search target has slashes. Protect against arbitrary directory traversal.
        return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());        
      }
    }

    // Find config or map path
    vector<string> fuzzyMatches;
    pair<uint8_t, filesystem::path> result = SearchInputLocal(fuzzyMatches);
    if (result.first == MATCH_TYPE_MAP || result.first == MATCH_TYPE_CONFIG) {
      return result;
    }

    if (m_Aura->m_Config.m_MapSearchShowSuggestions) {
      if (m_Aura->m_StartedGames.empty()) {
        // Synchronous download, only if there are no ongoing games.
#ifndef DISABLE_CPR
        SearchInputRemoteFuzzy(fuzzyMatches);
#endif
      }
      if (!fuzzyMatches.empty()) {
        m_Ctx->ErrorReply("Suggestions: " + JoinVector(fuzzyMatches, false), CHAT_SEND_SOURCE_ALL);
      }
    }

    return SEARCH_RESULT(MATCH_TYPE_NONE, filesystem::path());
  }

  // Input corresponds to a namespace, such as epicwar.
  // Gotta find a matching config file.
  string resolvedCFGName = m_SearchTarget.first + "-" + m_SearchTarget.second + ".ini";
  filesystem::path resolvedCFGPath = (m_Aura->m_Config.m_MapCFGPath / filesystem::path(resolvedCFGName)).lexically_normal();
  if (PathHasNullBytes(resolvedCFGPath) || resolvedCFGPath.parent_path() != m_Aura->m_Config.m_MapCFGPath.parent_path()) {
    return SEARCH_RESULT(MATCH_TYPE_FORBIDDEN, filesystem::path());
  }
  if (FileExists(resolvedCFGPath)) {
    return SEARCH_RESULT(MATCH_TYPE_CONFIG, resolvedCFGPath);
  }
  return SEARCH_RESULT(MATCH_TYPE_NONE, resolvedCFGPath);
}

shared_ptr<CMap> CGameSetup::GetBaseMapFromConfig(CConfig* mapCFG, const bool silent)
{
  shared_ptr<CMap> map = nullptr;
  try {
    map = make_shared<CMap>(m_Aura, mapCFG);
  } catch (...) {
    return map;
  }
  /*
  if (!map) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map config", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  */
  string errorMessage = map->CheckProblems();
  if (!errorMessage.empty()) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map config: " + errorMessage, CHAT_SEND_SOURCE_ALL);
    return map;
  }
  return map;
}

shared_ptr<CMap> CGameSetup::GetBaseMapFromConfigFile(const filesystem::path& filePath, const bool isCache, const bool silent)
{
  CConfig MapCFG;
  if (!MapCFG.Read(filePath)) {
    if (!silent) m_Ctx->ErrorReply("Map config file [" + PathToString(filePath.filename()) + "] not found.", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  shared_ptr<CMap> map = GetBaseMapFromConfig(&MapCFG, silent);
  if (!map) return nullptr;
  if (isCache) {
    if (MapCFG.GetIsModified()) {
      vector<uint8_t> bytes = MapCFG.Export();
      FileWrite(filePath, bytes.data(), bytes.size());
      Print("[AURA] Updated map cache for [" + PathToString(filePath.filename()) + "] as [" + PathToString(filePath) + "]");
    }
  }
  return map;
}

shared_ptr<CMap> CGameSetup::GetBaseMapFromMapFile(const filesystem::path& filePath, const bool silent)
{
  bool isInMapsFolder = filePath.parent_path() == m_Aura->m_Config.m_MapPath.parent_path();
  string fileName = PathToString(filePath.filename());
  if (fileName.empty()) return nullptr;
  string baseFileName;
  if (fileName.length() > 6 && fileName[fileName.length() - 6] == '~' && isdigit(fileName[fileName.length() - 5])) {
    baseFileName = fileName.substr(0, fileName.length() - 6) + fileName.substr(fileName.length() - 4);
  } else {
    baseFileName = fileName;
  }

  CConfig MapCFG;
  MapCFG.SetBool("map.cfg.partial", true);
  if (m_StandardPaths) MapCFG.SetBool("map.standard_path", true);
  MapCFG.Set("map.path", R"(Maps\Download\)" + baseFileName);
  string localPath = isInMapsFolder && !m_StandardPaths ? fileName : PathToString(filePath);
  MapCFG.Set("map.local_path", localPath);

  if (m_IsMapDownloaded) {
    MapCFG.Set("map.site", m_MapSiteUri);
    MapCFG.Set("map.url", m_MapDownloadUri);
    MapCFG.Set("downloaded_by", m_Attribution);
  }
  if (baseFileName.find("_evrgrn3") != string::npos) {
    if (m_MapSiteUri.empty()) MapCFG.Set("map.site", "https://www.hiveworkshop.com/threads/351924/");
    MapCFG.Set("map.short_desc", "This map uses Warcraft 3: Reforged game mechanics.");
    MapCFG.Set("map.type", "evergreen");
  } else if (baseFileName.find("DotA") != string::npos) {
    MapCFG.Set("map.type", "dota");
  }

  shared_ptr<CMap> baseMap = nullptr;
  try {
    baseMap = make_shared<CMap>(m_Aura, &MapCFG);
  } catch (...) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map.", CHAT_SEND_SOURCE_ALL);
    return baseMap;
  }
  string errorMessage = baseMap->CheckProblems();
  if (!errorMessage.empty()) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map: " + errorMessage, CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }

  if (m_Aura->m_Config.m_EnableCFGCache && isInMapsFolder && !m_StandardPaths) {
    string resolvedCFGName;
    if (m_SearchTarget.first == "local") {
      resolvedCFGName = m_SearchTarget.first + "-" + fileName + ".ini";
    } else {
      resolvedCFGName = m_SearchTarget.first + "-" + m_SearchTarget.second + ".ini";
    }
    filesystem::path resolvedCFGPath = (m_Aura->m_Config.m_MapCachePath / filesystem::path(resolvedCFGName)).lexically_normal();

    vector<uint8_t> bytes = MapCFG.Export();
    FileWrite(resolvedCFGPath, bytes.data(), bytes.size());
    m_Aura->m_CFGCacheNamesByMapNames[fileName] = resolvedCFGName;
    Print("[AURA] Cached map config for [" + fileName + "] as [" + PathToString(resolvedCFGPath) + "]");
  }

  if (!silent) m_Ctx->SendReply("Loaded OK [" + fileName + "]", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
  return baseMap;
}

shared_ptr<CMap> CGameSetup::GetBaseMapFromMapFileOrCache(const filesystem::path& mapPath, const bool silent)
{
  filesystem::path fileName = mapPath.filename();
  if (fileName.empty()) return nullptr;
  if (m_Aura->m_Config.m_EnableCFGCache) {
    bool cacheSuccess = false;
    if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
      Print("[AURA] Searching map in cache [" + PathToString(fileName) + "]");
    }
    if (m_Aura->m_CFGCacheNamesByMapNames.find(fileName) != m_Aura->m_CFGCacheNamesByMapNames.end()) {
      string cfgName = m_Aura->m_CFGCacheNamesByMapNames[fileName];
      filesystem::path cfgPath = m_Aura->m_Config.m_MapCachePath / filesystem::path(cfgName);
      shared_ptr<CMap> cachedResult = GetBaseMapFromConfigFile(cfgPath, true, true);
      if (cachedResult &&
        (
          cachedResult->GetServerPath() == fileName ||
          FileNameEquals(PathToString(cachedResult->GetServerPath()), PathToString(fileName))
        )
      ) {
        cacheSuccess = true;
      }
      if (cacheSuccess) {
        if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
          Print("[AURA] Map cache success");
        }
        if (!silent) m_Ctx->SendReply("Loaded OK [" + PathToString(fileName) + "]", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
        return cachedResult;
      } else {
        m_Aura->m_CFGCacheNamesByMapNames.erase(fileName);
      }
    }
    if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
      Print("[AURA] Map cache miss");
    }
  }
  return GetBaseMapFromMapFile(mapPath, silent);
}

bool CGameSetup::ApplyMapModifiers(CGameExtraOptions* extraOptions)
{
  bool failed = false;
  if (extraOptions->m_TeamsLocked.has_value()) {
    if (!m_Map->SetTeamsLocked(extraOptions->m_TeamsLocked.value()))
      failed = true;
  }
  if (extraOptions->m_TeamsTogether.has_value()) {
    if (!m_Map->SetTeamsTogether(extraOptions->m_TeamsTogether.value()))
      failed = true;
  }
  if (extraOptions->m_AdvancedSharedUnitControl.has_value()) {
    if (!m_Map->SetAdvancedSharedUnitControl(extraOptions->m_AdvancedSharedUnitControl.value()))
      failed = true;
  }
  if (extraOptions->m_RandomRaces.has_value()) {
    if (!m_Map->SetRandomRaces(extraOptions->m_RandomRaces.value()))
      failed = true;
  }
  if (extraOptions->m_RandomHeroes.has_value()) {
    if (!m_Map->SetRandomHeroes(extraOptions->m_RandomHeroes.value()))
      failed = true;
  }
  if (extraOptions->m_Visibility.has_value()) {
    if (!m_Map->SetMapVisibility(extraOptions->m_Visibility.value()))
      failed = true;
  }
  if (extraOptions->m_Speed.has_value()) {
    if (!m_Map->SetMapSpeed(extraOptions->m_Speed.value()))
      failed = true;
  }
  if (extraOptions->m_Observers.has_value()) {
    if (!m_Map->SetMapObservers(extraOptions->m_Observers.value())) {
      failed = true;
    }
  }
  return !failed;
}

#ifndef DISABLE_CPR
uint32_t CGameSetup::ResolveMapRepositoryTask()
{
  if (m_Aura->m_Net.m_Config.m_MapRepositories.find(m_SearchTarget.first) == m_Aura->m_Net.m_Config.m_MapRepositories.end()) {
    m_ErrorMessage = "Downloads from  " + m_SearchTarget.first + " are disabled.";
    return RESOLUTION_ERR;
  }

  string downloadUri, downloadFileName;
  uint64_t SearchTargetType = HashCode(m_SearchTarget.first);

  switch (SearchTargetType) {
    case HashCode("epicwar"): {
      m_MapSiteUri = "https://www.epicwar.com/maps/" + m_SearchTarget.second;
      Print("[NET] GET <" + m_MapSiteUri + ">");
      auto response = cpr::Get(
        cpr::Url{m_MapSiteUri},
        cpr::Timeout{m_DownloadTimeout},
        cpr::ProgressCallback(
          [this](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool
          {
            return !this->m_ExitingSoon;
          }
        )
      );
      if (m_ExitingSoon) {
        m_ErrorMessage = "Shutting down.";
        return RESOLUTION_ERR;
      }
      if (response.status_code == 0) {
        m_ErrorMessage = "Failed to access " + m_SearchTarget.first + " repository (connectivity error).";
        return RESOLUTION_ERR;
      }
      if (response.status_code != 200) {
        m_ErrorMessage = "Failed to access repository (status code " + to_string(response.status_code) + ").";
        return RESOLUTION_ERR;
      }
      Print("[AURA] resolved " + m_SearchTarget.first + " entry in " + to_string(static_cast<float>(response.elapsed * 1000)) + " ms");
      
      size_t downloadUriStartIndex = response.text.find("<a href=\"/maps/download/");
      if (downloadUriStartIndex == string::npos) return RESOLUTION_ERR;
      size_t downloadUriEndIndex = response.text.find("\"", downloadUriStartIndex + 24);
      if (downloadUriEndIndex == string::npos) {
        m_ErrorMessage = "Malformed API response";
        return RESOLUTION_ERR;
      }
      downloadUri = "https://epicwar.com" + response.text.substr(downloadUriStartIndex + 9, (downloadUriEndIndex) - (downloadUriStartIndex + 9));
      size_t lastSlashIndex = downloadUri.rfind("/");
      if (lastSlashIndex == string::npos) {
        m_ErrorMessage = "Malformed download URI";
        return RESOLUTION_ERR;
      }
      string encodedName = downloadUri.substr(lastSlashIndex + 1);
      downloadFileName = DecodeURIComponent(encodedName);
      break;
    }

    case HashCode("wc3maps"): {
      m_MapSiteUri = "https://www.wc3maps.com/api/download/" + m_SearchTarget.second;
      Print("[NET] GET <" + m_MapSiteUri + ">");
      auto response = cpr::Get(
        cpr::Url{m_MapSiteUri},
        cpr::Timeout{m_DownloadTimeout},
        cpr::Redirect{0, false, false, cpr::PostRedirectFlags::POST_ALL},
        cpr::ProgressCallback(
          [this](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool
          {
            return !this->m_ExitingSoon;
          }
        )
      );
      if (m_ExitingSoon) {
        m_ErrorMessage = "Shutting down.";
        return RESOLUTION_ERR;
      }
      if (response.status_code == 0) {
        m_ErrorMessage = "Remote host unavailable (status code " + to_string(response.status_code) + ").";
        return RESOLUTION_ERR;
      }
      if (response.status_code < 300 || 399 < response.status_code) {
        m_ErrorMessage = "Failed to access repository (status code " + to_string(response.status_code) + ").";
        return RESOLUTION_ERR;
      }
      Print("[AURA] Resolved " + m_SearchTarget.first + " entry in " + to_string(static_cast<float>(response.elapsed * 1000)) + " ms");
      downloadUri = response.header["location"];
      size_t lastSlashIndex = downloadUri.rfind("/");
      if (lastSlashIndex == string::npos) {
        m_ErrorMessage = "Malformed download URI.";
        return RESOLUTION_ERR;
      }
      downloadFileName = downloadUri.substr(lastSlashIndex + 1);
      downloadUri = downloadUri.substr(0, lastSlashIndex + 1) + EncodeURIComponent(downloadFileName);
      break;
    }

    default: {
      m_ErrorMessage = "Unsupported remote domain: " + m_SearchTarget.first;
      return RESOLUTION_ERR;
    }
  }

  if (downloadFileName.empty() || downloadFileName[0] == '.' || downloadFileName[0] == '-' || downloadFileName.length() > 80) {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  std::string InvalidChars = "/\\\0\"*?:|<>;,";
  if (downloadFileName.find_first_of(InvalidChars) != string::npos) {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  string fileExtension = ParseFileExtension(downloadFileName);
  if (fileExtension != ".w3m" && fileExtension != ".w3x") {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  string downloadFileNameNoExt = downloadFileName.substr(0, downloadFileName.length() - fileExtension.length());
  downloadFileName = downloadFileNameNoExt + fileExtension; // case-sensitive name, but lowercase extension

  // CON, PRN, AUX, NUL, etc. are case-insensitive
  transform(begin(downloadFileNameNoExt), end(downloadFileNameNoExt), begin(downloadFileNameNoExt), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (downloadFileNameNoExt == "con" || downloadFileNameNoExt == "prn" || downloadFileNameNoExt == "aux" || downloadFileNameNoExt == "nul") {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }
  if (downloadFileNameNoExt.length() == 4 && (downloadFileNameNoExt.substr(0, 3) == "com" || downloadFileNameNoExt.substr(0, 3) == "lpt")) {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  m_IsStepDownloaded = true;
  m_BaseDownloadFileName = downloadFileName;
  m_MapDownloadUri = downloadUri;

  return RESOLUTION_OK;
}

void CGameSetup::RunResolveMapRepository()
{
  m_IsStepDownloading = true;
  m_AsyncStep = GAMESETUP_STEP_RESOLUTION;
  m_DownloadFuture = async(launch::async, &::CGameSetup::ResolveMapRepositoryTask, this);
}

uint32_t CGameSetup::RunResolveMapRepositorySync()
{
  m_IsStepDownloading = true;
  uint32_t result = ResolveMapRepositoryTask();
  m_IsStepDownloading = false;
  if (!m_IsStepDownloaded) {
    m_Ctx->ErrorReply(m_ErrorMessage, CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return 0;
  }
  m_IsStepDownloading = false;
  m_IsStepDownloaded = false;
  return result;
}

void CGameSetup::OnResolveMapSuccess()
{
  if (PrepareDownloadMap()) {
    RunDownloadMap();
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Unable to start map download.")
    OnLoadMapError();
  }  
}

void CGameSetup::SetDownloadFilePath(filesystem::path&& filePath)
{
  m_DownloadFilePath = move(filePath);
}

bool CGameSetup::PrepareDownloadMap()
{
  if (m_SearchTarget.first != "epicwar" && m_SearchTarget.first != "wc3maps") {
    Print("Error!! trying to download from unsupported repository [" + m_SearchTarget.first + "] !!");
    return false;
  }
  if (!m_Aura->m_Net.m_Config.m_AllowDownloads) {
    m_Ctx->ErrorReply("Map downloads are not allowed.", CHAT_SEND_SOURCE_ALL);
    return false;
  }

  string fileNameFragmentPost = ParseFileExtension(m_BaseDownloadFileName);
  string fileNameFragmentPre = m_BaseDownloadFileName.substr(0, m_BaseDownloadFileName.length() - fileNameFragmentPost.length());
  bool nameSuccess = false;
  string mapSuffix;
  for (uint8_t i = 0; i < 10; ++i) {
    if (i != 0) {
      mapSuffix = "~" + to_string(i);
    }
    string testFileName = fileNameFragmentPre + mapSuffix + fileNameFragmentPost;
    filesystem::path testFilePath = m_Aura->m_Config.m_MapPath / filesystem::path(testFileName);
    if (FileExists(testFilePath)) {
      // Map already exists.
      // I'd rather directly open the file with wx flags to avoid racing conditions,
      // but there is no standard C++ way to do this, and cstdio isn't very helpful.
      continue;
    }
    if (m_Aura->m_MapFilesTimedBusyLocks.find(testFileName) != m_Aura->m_MapFilesTimedBusyLocks.end()) {
      // Map already hosted.
      continue;
    }
    SetDownloadFilePath(move(testFilePath));
    nameSuccess = true;
    break;
  }
  if (!nameSuccess) {
    m_Ctx->ErrorReply("Download failed - duplicate map name [" + m_BaseDownloadFileName + "].", CHAT_SEND_SOURCE_ALL);
    return false;
  }
  Print("[NET] GET <" + m_MapDownloadUri + "> as [" + PathToString(m_DownloadFilePath.filename()) + "]...");
  m_DownloadFileStream = new ofstream(m_DownloadFilePath.native().c_str(), std::ios_base::out | std::ios_base::binary);
  return true;
}

uint32_t CGameSetup::DownloadMapTask()
{
  if (!m_DownloadFileStream) return 0;
  if (!m_DownloadFileStream->is_open()) {
    m_DownloadFileStream->close();
    delete m_DownloadFileStream;
    m_DownloadFileStream = nullptr;
    m_ErrorMessage = "Download failed - unable to write to disk.";
    return 0;
  }
  cpr::Response response = cpr::Download(
    *m_DownloadFileStream,
    cpr::Url{m_MapDownloadUri},
    cpr::Header{{"user-agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0"}},
    cpr::Timeout{m_DownloadTimeout},
    cpr::ProgressCallback(
      [this](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool
      {
        return !this->m_ExitingSoon;
      }
    )
  );
  m_DownloadFileStream->close();
  delete m_DownloadFileStream;
  m_DownloadFileStream = nullptr;
  if (m_ExitingSoon) {
    m_ErrorMessage = "Shutting down.";
    FileDelete(m_DownloadFilePath);
    return RESOLUTION_ERR;
  }
  if (response.status_code == 0) {
    m_ErrorMessage = "Failed to access " + m_SearchTarget.first + " repository (connectivity error).";
    FileDelete(m_DownloadFilePath);
    return 0;
  }
  if (response.status_code != 200) {
    m_ErrorMessage = "Map not found in " + m_SearchTarget.first + " repository (code " + to_string(response.status_code) + ").";
    FileDelete(m_DownloadFilePath);
    return 0;
  }
  const bool timedOut = cpr::ErrorCode::OPERATION_TIMEDOUT == response.error.code;
  if (timedOut) {
    m_ErrorMessage = "Map download took too long.";
    FileDelete(m_DownloadFilePath);
    return 0;
  }
  Print("[AURA] download task completed in " + to_string(static_cast<float>(response.elapsed * 1000)) + " ms");
  // Signals completion.
  m_IsStepDownloaded = true;

  return static_cast<uint32_t>(response.downloaded_bytes);
}

void CGameSetup::RunDownloadMap()
{
  if (m_ExitingSoon) return;
  m_IsStepDownloading = true;
  m_AsyncStep = GAMESETUP_STEP_DOWNLOAD;
  m_DownloadFuture = async(launch::async, &::CGameSetup::DownloadMapTask, this);
}

uint32_t CGameSetup::RunDownloadMapSync()
{
  m_IsStepDownloading = true;
  uint32_t byteSize = DownloadMapTask();
  m_IsStepDownloading = false;
  if (!m_IsStepDownloaded) {
    m_Ctx->ErrorReply(m_ErrorMessage, CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return 0;
  }
  m_IsMapDownloaded = true;
  m_IsStepDownloaded = false;
  return byteSize;
}

vector<pair<string, string>> CGameSetup::GetMapRepositorySuggestions(const string& pattern, const uint8_t maxCount)
{
  vector<pair<string, string>> suggestions;
  string searchUri = "https://www.epicwar.com/maps/search/?go=1&n=" + EncodeURIComponent(pattern) + "&a=&c=0&p=0&pf=0&roc=0&tft=0&order=desc&sort=downloads&page=1";
  Print("[AURA] Looking up suggestions...");
  Print("[AURA] GET <" + searchUri + ">");
  auto response = cpr::Get(
    cpr::Url{searchUri},
    cpr::Timeout{m_SuggestionsTimeout},
    cpr::ProgressCallback(
      [this](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool
      {
        return !this->m_ExitingSoon;
      }
    )
  );
  if (m_ExitingSoon || response.status_code != 200) {
    return suggestions;
  }

  vector<pair<string, int>> matchingMaps = ExtractEpicWarMaps(response.text, maxCount);
  for (const auto& element : matchingMaps) {
    suggestions.push_back(make_pair(element.first, "epicwar-" + to_string(element.second)));
  }
  return suggestions;
}

#endif

void CGameSetup::LoadMap()
{
  if (m_Map) {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map is already loaded.")
    OnLoadMapSuccess();
    return;
  }

  ParseInput();
  pair<uint8_t, std::filesystem::path> searchResult = SearchInput();
  if (searchResult.first == MATCH_TYPE_FORBIDDEN) {
    OnLoadMapError();
    return;
  }
  if (searchResult.first == MATCH_TYPE_INVALID) {
    // Exclusive to standard paths mode.
    m_Ctx->ErrorReply("Invalid file extension for [" + PathToString(searchResult.second.filename()) + "]. Please use --search-type");
    OnLoadMapError();
    return;
  }
  if (searchResult.first != MATCH_TYPE_MAP && searchResult.first != MATCH_TYPE_CONFIG) {
    if (m_SearchType != SEARCH_TYPE_ANY || !m_IsDownloadable) {
      PRINT_IF(LOG_LEVEL_DEBUG, "[GAMESETUP] No results found matching search criteria.")
      OnLoadMapError();
      return;
    }
    if (m_Aura->m_Config.m_EnableCFGCache) {
      filesystem::path cachePath = m_Aura->m_Config.m_MapCachePath / filesystem::path(m_SearchTarget.first + "-" + m_SearchTarget.second + ".ini");
      m_Map = GetBaseMapFromConfigFile(cachePath, true, true);
      if (m_Map) {
        DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map loaded from cache.")
        OnLoadMapSuccess();
        return;
      }
    }
#ifndef DISABLE_CPR
    m_Ctx->SendReply("Resolving map repository...");
    RunResolveMapRepository();
    return;
#else
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map downloads not supported in this Aura distribution")
    OnLoadMapError();
    return;
#endif
  }
  if (searchResult.first == MATCH_TYPE_CONFIG) {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Loading config...")
    m_Map = GetBaseMapFromConfigFile(searchResult.second, false, false);
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Loading from map or cache...")
    m_Map = GetBaseMapFromMapFileOrCache(searchResult.second, false);
  }
  if (m_Map) {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map loaded successfully.")
    OnLoadMapSuccess();
  } else {
    PRINT_IF(LOG_LEVEL_DEBUG, "[GAMESETUP] Map failed to load")
    OnLoadMapError();
  }
//
}

void CGameSetup::OnLoadMapSuccess()
{
  if (m_ExitingSoon) {
    m_DeleteMe = true;
    return;
  }
  if (m_Ctx->GetPartiallyDestroyed()) {
    PRINT_IF(LOG_LEVEL_ERROR, "[GAMESETUP] Game setup aborted - context destroyed")
    m_DeleteMe = true;
    return;
  }
  if (m_MapExtraOptions) {
    if (!ApplyMapModifiers(m_MapExtraOptions)) {
      m_Ctx->ErrorReply("Invalid map options. Map has fixed player settings.", CHAT_SEND_SOURCE_ALL);
      m_DeleteMe = true;
      ClearExtraOptions();
      return;
    }
    ClearExtraOptions();
  }
  if (!m_SaveFile.empty()) {
    if (!RestoreFromSaveFile()) {
      m_Ctx->ErrorReply("Invalid save file.", CHAT_SEND_SOURCE_ALL);
      m_DeleteMe = true;
      return;
    }
  }

  if (m_MapReadyCallbackAction == MAP_ONREADY_ALIAS) {
    if (m_Aura->m_DB->AliasAdd(m_MapReadyCallbackData, m_Map->GetServerFileName())) {
      m_Ctx->SendReply("Alias [" + m_MapReadyCallbackData + "] added for [" + m_Map->GetServerFileName() + "]");
    } else {
      m_Ctx->ErrorReply("Failed to add alias.");
    }
  } else if (m_MapReadyCallbackAction == MAP_ONREADY_HOST) {
    /*if (m_Aura->m_StartedGames.size() > m_Aura->m_Config.m_MaxStartedGames || m_Aura->m_StartedGames.size() == m_Aura->m_Config.m_MaxStartedGames && !m_Aura->m_Config.m_DoNotCountReplaceableLobby) {
      m_Ctx->ErrorReply("Games hosted quota reached.", CHAT_SEND_SOURCE_ALL);
      return;
	}*/
    SetBaseName(m_MapReadyCallbackData);
    CGame* sourceGame = m_Ctx->GetSourceGame();
    CGame* targetGame = m_Ctx->GetTargetGame();
    if (targetGame && !targetGame->GetCountDownStarted() && targetGame->GetIsReplaceable() && !targetGame->GetIsBeingReplaced()) {
      targetGame->SendAllChat("Another lobby is being created. This lobby will be closed soon.");
      targetGame->StartGameOverTimer();
      targetGame->SetIsBeingReplaced(true);
      ++m_Aura->m_ReplacingLobbiesCounter;
    }
    CRealm* sourceRealm = m_Ctx->GetSourceRealm();
    if (m_Aura->m_Config.m_AutomaticallySetGameOwner) {
      SetOwner(m_Ctx->GetSender(), sourceRealm);
    }
    if (sourceGame) {
      SetCreator(m_Ctx->GetSender(), sourceGame);
    } else if (sourceRealm) {
      SetCreator(m_Ctx->GetSender(), sourceRealm);
    } else if (m_Ctx->GetSourceIRC()) {
      SetCreator(m_Ctx->GetSender(), m_Ctx->GetSourceIRC());
#ifndef DISABLE_DPP
    } else if (m_Ctx->GetDiscordAPI()) {
      SetCreator(m_Ctx->GetSender(), &m_Aura->m_Discord);
#endif
    } else {
      SetCreator(m_Ctx->GetSender());
    }
    RunHost();
  }
}

void CGameSetup::OnLoadMapError()
{
  if (m_ExitingSoon || m_Ctx->GetPartiallyDestroyed()) {
    m_DeleteMe = true;
    return;
  }
  if (m_FoundSuggestions) {
    m_Ctx->ErrorReply("Not found. Use the epicwar-number identifiers.", CHAT_SEND_SOURCE_ALL);
  } else {
    m_Ctx->ErrorReply("Map not found", CHAT_SEND_SOURCE_ALL);
  }
  m_DeleteMe = true;
}

#ifndef DISABLE_CPR
void CGameSetup::OnDownloadMapSuccess()
{
  if (m_ExitingSoon || !m_Aura) {
    m_DeleteMe = true;
    return;
  }
  m_IsMapDownloaded = true;
  m_Map = GetBaseMapFromMapFileOrCache(m_DownloadFilePath, false);
  if (m_Map) {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Downloaded map loaded successfully.")
    OnLoadMapSuccess();
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Downloaded map failed to load.")
    OnLoadMapError();
  }
}

void CGameSetup::OnFetchSuggestionsEnd()
{
}
#endif

bool CGameSetup::GetMapLoaded() const
{
  return m_Map != nullptr;
}

bool CGameSetup::LoadMapSync()
{
  if (m_Map) return true;

  ParseInput();
  pair<uint8_t, std::filesystem::path> searchResult = SearchInput();
  if (searchResult.first == MATCH_TYPE_FORBIDDEN) {
    return false;
  }
  if (searchResult.first == MATCH_TYPE_INVALID) {
    // Exclusive to standard paths mode.
    m_Ctx->ErrorReply("Invalid file extension for [" + PathToString(searchResult.second.filename()) + "]. Please use --search-type");
    return false;
  }
  if (searchResult.first != MATCH_TYPE_MAP && searchResult.first != MATCH_TYPE_CONFIG) {
    if (m_SearchType != SEARCH_TYPE_ANY || !m_IsDownloadable) {
      return false;
    }
    if (m_Aura->m_Config.m_EnableCFGCache) {
      filesystem::path cachePath = m_Aura->m_Config.m_MapCachePath / filesystem::path(m_SearchTarget.first + "-" + m_SearchTarget.second + ".ini");
      m_Map = GetBaseMapFromConfigFile(cachePath, true, true);
      if (m_Map) return true;
    }
#ifndef DISABLE_CPR
    if (RunResolveMapRepositorySync() != RESOLUTION_OK) {
      Print("[AURA] Failed to resolve remote map.");
      return false;
    }
    if (!PrepareDownloadMap()) {
      return false;
    }
    uint32_t downloadSize = RunDownloadMapSync();
    if (downloadSize == 0) {
      Print("[AURA] Failed to download map.");
      return false;
    }
    m_Map = GetBaseMapFromMapFileOrCache(m_DownloadFilePath, false);
    return true;
#else
    return false;
#endif
  }
  if (searchResult.first == MATCH_TYPE_CONFIG) {
    m_Map = GetBaseMapFromConfigFile(searchResult.second, false, false);
  } else {
    m_Map = GetBaseMapFromMapFileOrCache(searchResult.second, false);
  }
  return m_Map != nullptr;
}

void CGameSetup::SetActive()
{
  if (m_Aura->m_GameSetup) {
    if (!m_Aura->m_AutoRehostGameSetup || m_Aura->m_AutoRehostGameSetup.get() != m_Aura->m_GameSetup.get()) {
      DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Pending game setup destroyed")
    } else if (this != m_Aura->m_AutoRehostGameSetup.get()) {
      DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Auto-rehost game setup deprioritized")
    }
  }
  m_Aura->m_GameSetup = shared_from_this();
  m_ActiveTicks = GetTicks();
}

bool CGameSetup::RestoreFromSaveFile()
{
  m_RestoredGame = new CSaveGame(m_Aura, m_SaveFile);
  if (!m_RestoredGame->Load()) return false;
  bool success = m_RestoredGame->Parse();
  m_RestoredGame->Unload();
  if (!CaseInsensitiveEquals(
    // Not using FileNameEquals because these are client paths
    ParseFileName(m_RestoredGame->GetClientMapPath()),
    ParseFileName(m_Map->GetClientPath())
  )) {
    m_Ctx->ErrorReply("Requires map [" + m_RestoredGame->GetClientMapPath() + "]", CHAT_SEND_SOURCE_ALL);
    return false;
  }
  return success;
}

bool CGameSetup::RunHost()
{
  return m_Aura->CreateGame(shared_from_this());
}

bool CGameSetup::SetMirrorSource(const sockaddr_storage& nSourceAddress, const uint32_t nGameIdentifier)
{
  m_IsMirror = true;
  m_Identifier = nGameIdentifier;
  memcpy(&m_RealmsAddress, &nSourceAddress, sizeof(sockaddr_storage));
  return true;
}

bool CGameSetup::SetMirrorSource(const string& nInput)
{
  string::size_type portStart = nInput.find(":", 0);
  if (portStart == string::npos) return false;
  string::size_type idStart = nInput.find("#", portStart);
  if (idStart == string::npos) return false;
  string rawAddress = nInput.substr(0, portStart);
  if (rawAddress.length() < 7) return false;
  string rawPort = nInput.substr(portStart + 1, idStart - (portStart + 1));
  if (rawPort.empty()) return false;
  string::size_type idEnd = nInput.find(":", idStart);
  string rawId;
  if (idEnd == string::npos) {
    rawId = nInput.substr(idStart + 1);
  } else {
    rawId = nInput.substr(idStart + 1, idEnd - (idStart + 1));
  }
  if (rawId.empty()) return false;
  optional<sockaddr_storage> maybeAddress;
  if (rawAddress[0] == '[' && rawAddress[rawAddress.length() - 1] == ']') {
    maybeAddress = CNet::ParseAddress(rawAddress.substr(1, rawAddress.length() - 2), ACCEPT_IPV4);
  } else {
    maybeAddress = CNet::ParseAddress(rawAddress, ACCEPT_IPV4);
  }
  if (!maybeAddress.has_value()) return false;
  uint16_t gamePort = 0;
  uint32_t gameId = 0;
  try {
    int64_t value = stol(rawPort);
    if (value <= 0 || value > 0xFFFF) return false;
    gamePort = static_cast<uint16_t>(value);
  } catch (...) {
    return false;
  }
  optional<uint32_t> maybeId = ParseUint32Hex(rawId);
  if (!maybeId.has_value()) {
    return false;
  }
  gameId = maybeId.value();
  SetAddressPort(&(maybeAddress.value()), gamePort);
  return SetMirrorSource(maybeAddress.value(), gameId);
}

void CGameSetup::AddIgnoredRealm(const CRealm* nRealm)
{
  m_RealmsExcluded.insert(nRealm->GetServer());
}

void CGameSetup::RemoveIgnoredRealm(const CRealm* nRealm)
{
  m_RealmsExcluded.erase(nRealm->GetServer());
}

void CGameSetup::SetDisplayMode(const uint8_t nDisplayMode)
{
  m_RealmsDisplayMode = nDisplayMode;
}

void CGameSetup::SetOwner(const string& nOwner, const CRealm* nRealm)
{
  if (nRealm == nullptr) {
    m_Owner = make_pair(nOwner, string());
  } else {
    m_Owner = make_pair(nOwner, nRealm->GetServer());
  }
}

void CGameSetup::SetCreator(const string& nCreator)
{
  m_CreatedBy = nCreator;
  m_CreatedFrom = nullptr;
  m_CreatedFromType = SERVICE_TYPE_NONE;
}

void CGameSetup::SetCreator(const string& nCreator, CGame* nGame)
{
  m_CreatedBy = nCreator;
  m_CreatedFrom = reinterpret_cast<void*>(nGame);
  m_CreatedFromType = SERVICE_TYPE_GAME;
}

void CGameSetup::SetCreator(const string& nCreator, CRealm* nRealm)
{
  m_CreatedBy = nCreator;
  m_CreatedFrom = reinterpret_cast<void*>(nRealm);
  m_CreatedFromType = SERVICE_TYPE_REALM;
}

void CGameSetup::SetCreator(const string& nCreator, CIRC* nIRC)
{
  m_CreatedBy = nCreator;
  m_CreatedFrom = reinterpret_cast<void*>(nIRC);
  m_CreatedFromType = SERVICE_TYPE_IRC;
}

void CGameSetup::SetCreator(const string& nCreator, CDiscord* nDiscord)
{
  // TODO: CGameSetup::SetCreator() - Discord case
  m_CreatedBy = nCreator;
  m_CreatedFrom = reinterpret_cast<void*>(nDiscord);
  m_CreatedFromType = SERVICE_TYPE_DISCORD;
}

void CGameSetup::RemoveCreator()
{
  m_CreatedBy.clear();
  m_CreatedFrom = nullptr;
  m_CreatedFromType = SERVICE_TYPE_INVALID;
}

bool CGameSetup::MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const
{
  if (m_CreatedFromType != fromType) return false;
  switch (fromType) {
    case SERVICE_TYPE_REALM:
      return reinterpret_cast<const CRealm*>(m_CreatedFrom) == reinterpret_cast<const CRealm*>(fromThing);
    case SERVICE_TYPE_IRC:
      return reinterpret_cast<const CIRC*>(m_CreatedFrom) == reinterpret_cast<const CIRC*>(fromThing);
    case SERVICE_TYPE_DISCORD:
      return reinterpret_cast<const CDiscord*>(m_CreatedFrom) == reinterpret_cast<const CDiscord*>(fromThing);
  }
  return false;
}

void CGameSetup::OnGameCreate()
{
  // Transferred to CGame. Do not deallocate.
  m_RestoredGame = nullptr;
  if (m_LobbyAutoRehosted) {
    // Base-36 suffix 0123456789abcdefghijklmnopqrstuvwxyz
    if (m_CreationCounter < 10) {
      SetName(m_BaseName + "-" + string(1, 48 + m_CreationCounter));
    } else {
      SetName(m_BaseName + "-" + string(1, 87 + m_CreationCounter));
    }
    m_CreationCounter = (m_CreationCounter + 1) % 36;
  }
}

bool CGameSetup::Update()
{
#ifndef DISABLE_CPR
  if (!m_IsStepDownloading) return m_DeleteMe;
  auto status = m_DownloadFuture.wait_for(chrono::seconds(0));
  if (status != future_status::ready) return m_DeleteMe;
  const bool success = m_IsStepDownloaded;
  const uint8_t finishedStep = m_AsyncStep;
  m_IsStepDownloading = false;
  m_IsStepDownloaded = false;
  m_AsyncStep = GAMESETUP_STEP_MAIN;
  if (!success && finishedStep != GAMESETUP_STEP_SUGGESTIONS) {
    m_Ctx->ErrorReply(m_ErrorMessage, CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    PRINT_IF(LOG_LEVEL_DEBUG, "[GAMESETUP] Task failed. Releasing game setup...")
    m_DeleteMe = true;
    return m_DeleteMe;
  }
  switch (finishedStep) {
    case GAMESETUP_STEP_RESOLUTION:
      DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map resolution completed")
      OnResolveMapSuccess();
      break;
    case GAMESETUP_STEP_DOWNLOAD:
      DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map download completed")
      OnDownloadMapSuccess();
      break;
    case GAMESETUP_STEP_SUGGESTIONS:
      DPRINT_IF(LOG_LEVEL_TRACE, "[GAMESETUP] Map suggestions fetched")
      OnFetchSuggestionsEnd();
      break;
    default:
      Print("[AURA] error - unhandled async task completion");
  }
#endif
  return m_DeleteMe;
}

void CGameSetup::AwaitSettled()
{
#ifndef DISABLE_DPP
  if (m_IsStepDownloading) {
    m_DownloadFuture.wait();
  }
#endif
}

void CGameSetup::SetGameSavedFile(const std::filesystem::path& filePath)
{
  if (m_StandardPaths) {
    m_SaveFile = filePath;
  } else if (filePath != filePath.filename()) {
    m_SaveFile = filePath;
  } else {
    m_SaveFile = m_Aura->m_Config.m_GameSavePath / filePath;
  }
}

void CGameSetup::ClearExtraOptions()
{
  if (m_MapExtraOptions) {
    delete m_MapExtraOptions;
    m_MapExtraOptions = nullptr;
  }
}

void CGameSetup::AcquireCLISimple(const CCLI* nCLI)
{
  if (nCLI->m_GameLobbyTimeoutMode.has_value()) SetLobbyTimeoutMode(nCLI->GetGameLobbyTimeoutMode());
  if (nCLI->m_GameLobbyOwnerTimeoutMode.has_value()) SetLobbyOwnerTimeoutMode(nCLI->GetGameLobbyOwnerTimeoutMode());
  if (nCLI->m_GameLoadingTimeoutMode.has_value()) SetLoadingTimeoutMode(nCLI->GetGameLoadingTimeoutMode());
  if (nCLI->m_GamePlayingTimeoutMode.has_value()) SetPlayingTimeoutMode(nCLI->GetGamePlayingTimeoutMode());

  if (nCLI->m_GameLobbyTimeout.has_value()) SetLobbyTimeout(nCLI->m_GameLobbyTimeout.value());
  if (nCLI->m_GameLobbyOwnerTimeout.has_value()) SetLobbyOwnerTimeout(nCLI->m_GameLobbyOwnerTimeout.value());
  if (nCLI->m_GameLoadingTimeout.has_value()) SetLoadingTimeout(nCLI->m_GameLoadingTimeout.value());
  if (nCLI->m_GamePlayingTimeout.has_value()) SetPlayingTimeout(nCLI->m_GamePlayingTimeout.value());

  if (nCLI->m_GamePlayingTimeoutWarningShortCountDown.has_value()) SetPlayingTimeoutWarningShortCountDown(nCLI->m_GamePlayingTimeoutWarningShortCountDown.value());
  if (nCLI->m_GamePlayingTimeoutWarningShortInterval.has_value()) SetPlayingTimeoutWarningShortInterval(nCLI->m_GamePlayingTimeoutWarningShortInterval.value());
  if (nCLI->m_GamePlayingTimeoutWarningLargeCountDown.has_value()) SetPlayingTimeoutWarningLargeCountDown(nCLI->m_GamePlayingTimeoutWarningLargeCountDown.value());
  if (nCLI->m_GamePlayingTimeoutWarningLargeInterval.has_value()) SetPlayingTimeoutWarningLargeInterval(nCLI->m_GamePlayingTimeoutWarningLargeInterval.value());

  if (nCLI->m_GameLobbyOwnerReleaseLANLeaver.has_value()) SetLobbyOwnerReleaseLANLeaver(nCLI->m_GameLobbyOwnerReleaseLANLeaver.value());

  if (nCLI->m_GameLobbyCountDownInterval.has_value()) SetLobbyCountDownInterval(nCLI->m_GameLobbyCountDownInterval.value());
  if (nCLI->m_GameLobbyCountDownStartValue.has_value()) SetLobbyCountDownStartValue(nCLI->m_GameLobbyCountDownStartValue.value());

  if (nCLI->m_GameCheckJoinable.has_value()) SetIsCheckJoinable(nCLI->m_GameCheckJoinable.value());
  if (nCLI->m_GameNotifyJoins.has_value()) SetNotifyJoins(nCLI->m_GameNotifyJoins.value());
  if (nCLI->m_GameCheckReservation.has_value()) SetCheckReservation(nCLI->m_GameCheckReservation.value());
  if (nCLI->m_GameLobbyReplaceable.has_value()) SetLobbyReplaceable(nCLI->m_GameLobbyReplaceable.value());
  if (nCLI->m_GameLobbyAutoRehosted.has_value()) SetLobbyAutoRehosted(nCLI->m_GameLobbyAutoRehosted.value());

  if (nCLI->m_GameAutoStartPlayers.has_value()) SetAutoStartPlayers(nCLI->m_GameAutoStartPlayers.value());
  if (nCLI->m_GameAutoStartSeconds.has_value()) SetAutoStartSeconds(nCLI->m_GameAutoStartSeconds.value());
  if (nCLI->m_GameAutoStartRequiresBalance.has_value()) SetAutoStartRequiresBalance(nCLI->m_GameAutoStartRequiresBalance.value());

  if (nCLI->m_GameLatencyAverage.has_value()) SetLatencyAverage(nCLI->m_GameLatencyAverage.value());
  if (nCLI->m_GameLatencyMaxFrames.has_value()) SetLatencyMaxFrames(nCLI->m_GameLatencyMaxFrames.value());
  if (nCLI->m_GameLatencySafeFrames.has_value()) SetLatencySafeFrames(nCLI->m_GameLatencySafeFrames.value());
  if (nCLI->m_GameLatencyEqualizerEnabled.has_value()) SetLatencyEqualizerEnabled(nCLI->m_GameLatencyEqualizerEnabled.value());
  if (nCLI->m_GameLatencyEqualizerFrames.has_value()) SetLatencyEqualizerFrames(nCLI->m_GameLatencyEqualizerFrames.value());

  if (nCLI->m_GameHCL.has_value()) SetHCL(nCLI->m_GameHCL.value());
  if (nCLI->m_GameFreeForAll.value_or(false)) SetCustomLayout(CUSTOM_LAYOUT_FFA);

  if (nCLI->m_GameNumPlayersToStartGameOver.has_value()) SetNumPlayersToStartGameOver(nCLI->m_GameNumPlayersToStartGameOver.value());

  if (nCLI->m_GameAutoKickPing.has_value()) SetAutoKickPing(nCLI->m_GameAutoKickPing.value());
  if (nCLI->m_GameWarnHighPing.has_value()) SetWarnKickPing(nCLI->m_GameWarnHighPing.value());
  if (nCLI->m_GameSafeHighPing.has_value()) SetSafeKickPing(nCLI->m_GameSafeHighPing.value());

  if (nCLI->m_GameSyncNormalize.has_value()) SetSyncNormalize(nCLI->m_GameSyncNormalize.value());

  if (nCLI->m_GameHideLobbyNames.has_value()) SetHideLobbyNames(nCLI->m_GameHideLobbyNames.value());
  if (nCLI->m_GameHideLoadedNames.has_value()) SetHideInGameNames(nCLI->GetGameHideLoadedNames());
  if (nCLI->m_GameLoadInGame.has_value()) SetLoadInGame(nCLI->m_GameLoadInGame.value());
  if (nCLI->m_GameEnableJoinObserversInProgress.has_value()) SetEnableJoinObserversInProgress(nCLI->m_GameEnableJoinObserversInProgress.value());
  if (nCLI->m_GameEnableJoinPlayersInProgress.has_value()) SetEnableJoinPlayersInProgress(nCLI->m_GameEnableJoinPlayersInProgress.value());

  if (nCLI->m_GameLogCommands.has_value()) SetLogCommands(nCLI->m_GameLogCommands.value());

  SetReservations(nCLI->m_GameReservations);
  SetSupportedGameVersions(nCLI->m_GameCrossplayVersions);
  SetVerbose(nCLI->m_Verbose);
  SetDisplayMode(nCLI->GetGameDisplayType());
  if (nCLI->m_GameReconnectionMode.has_value()) SetReconnectionMode(nCLI->GetGameReconnectionMode());
  if (nCLI->m_GameIPFloodHandler.has_value()) SetIPFloodHandler(nCLI->GetGameIPFloodHandler());
  if (nCLI->m_GameUnsafeNameHandler.has_value()) SetUnsafeNameHandler(nCLI->GetGameUnsafeNameHandler());
  if (nCLI->m_GameBroadcastErrorHandler.has_value()) SetBroadcastErrorHandler(nCLI->GetGameBroadcastErrorHandler());
}

CGameSetup::~CGameSetup()
{
  ClearExtraOptions();

  m_Ctx.reset();

  delete m_RestoredGame;
  m_RestoredGame = nullptr;

  m_CreatedFrom = nullptr;
  m_Aura = nullptr;

  m_Map.reset();
}
