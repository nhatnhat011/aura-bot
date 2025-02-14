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

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "w3mmd.h"
#include "auradb.h"
#include "config/config_game.h"
#include "game.h"
#include "protocol/game_protocol.h"
#include "game_slot.h"
#include "util.h"

using namespace std;

//
// CW3MMDAction
//

CW3MMDAction::CW3MMDAction(CGame* nGame, uint8_t nFromUID, uint32_t nID, uint8_t nType, uint8_t nSubType, uint8_t nSID)
  : m_Ticks(nGame->GetGameTicks()),
    m_UpdateID(nID),
    m_Type(nType),
    m_SubType(nSubType),
    m_FromUID(nFromUID),
    m_FromColor(nGame->GetColorFromUID(nFromUID)),
    m_SID(nSID)
{
}

CW3MMDAction::~CW3MMDAction()
{
}

//
// CW3MMDDefinition
//

CW3MMDDefinition::CW3MMDDefinition(CGame* nGame, uint8_t nFromUID, uint32_t nID, uint8_t nType, uint8_t nSubType, uint8_t nSID)
  : m_Ticks(nGame->GetGameTicks()),
    m_UpdateID(nID),
    m_Type(nType),
    m_SubType(nSubType),
    m_FromUID(nFromUID),
    m_FromColor(nGame->GetColorFromUID(nFromUID)),
    m_SID(nSID)
{
}

CW3MMDDefinition::~CW3MMDDefinition()
{
}

//
// CW3MMD
//

CW3MMD::CW3MMD(CGame *nGame)
  : m_Game(nGame),
    m_GameOver(false),
    m_Error(false),
    m_Version(0),
    m_LastValueID(0)
    //m_NextCheckID(0)
{
  m_ResultVerbs[MMD_RESULT_LOSER] = "lost";
  m_ResultVerbs[MMD_RESULT_DRAWER] = "drew";
  m_ResultVerbs[MMD_RESULT_WINNER] = "won";
}

CW3MMD::~CW3MMD()
{
}

bool CW3MMD::HandleTokens(uint8_t fromUID, uint32_t valueID, vector<string> Tokens)
{
  if (Tokens.empty()) {
    return false;
  }
  const string& actionType = Tokens[0];
  if (actionType == "init" && Tokens.size() >= 2) {
    if (Tokens[1] == "version" && Tokens.size() == 4) {
      // Tokens[2] = minimum
      // Tokens[3] = current

      optional<uint32_t> version = ToUint32(Tokens[2]);
      if (!version.has_value()) return false;
      optional<uint32_t> minVersion = ToUint32(Tokens[3]);
      if (!minVersion.has_value()) return false;
      if (version.value() > 1) {
        Print(GetLogPrefix() + "error - map requires MMD parser version " + Tokens[2] + " or higher (using version 1)");
        m_Error = true;
      } else {
        Print(GetLogPrefix() + "map is using Warcraft 3 Map Meta Data library version [" + Tokens[3] + "]");
        m_Version = *version;
      }
    } else if (Tokens[1] == "pid" && Tokens.size() == 4) {
      // Tokens[2] = pid
      // Tokens[3] = name
      optional<uint32_t> SID = ToUint32(Tokens[2]);
      if (!SID.has_value()) return false;

      CW3MMDDefinition* def = new CW3MMDDefinition(m_Game, fromUID, valueID, MMD_DEFINITION_TYPE_INIT, MMD_INIT_TYPE_PLAYER, *SID);
      if (m_Game->m_Config.m_UnsafeNameHandler == ON_UNSAFE_NAME_CENSOR_MAY_DESYNC) {
        def->SetName(CIncomingJoinRequest::CensorName(Tokens[3], m_Game->m_Config.m_PipeConsideredHarmful));
      } else {
        def->SetName(Tokens[3]);
      }
      m_DefQueue.push(def);
    }
  } else if (actionType == "DefVarP" && Tokens.size() == 5) {
    // Tokens[1] = name
    // Tokens[2] = value type
    // Tokens[3] = goal type (ignored here)
    // Tokens[4] = suggestion (ignored here)

    uint8_t subType;
    if (Tokens[2] == "int") {
      subType = MMD_VALUE_TYPE_INT;
    } else if (Tokens[2] == "real") {
      subType = MMD_VALUE_TYPE_REAL;
    } else if (Tokens[2] == "string") {
      subType = MMD_VALUE_TYPE_STRING;
    } else {
      Print(GetLogPrefix() + "invalid DefVarP type [" + Tokens[2] + "] found, ignoring");
      return false;
    }
    CW3MMDDefinition* def = new CW3MMDDefinition(m_Game, fromUID, valueID, MMD_DEFINITION_TYPE_VAR, subType);
    def->SetName(Tokens[1]);
    m_DefQueue.push(def);
  } else if (actionType == "VarP" && Tokens.size() == 5) {
    // Tokens[1] = pid
    // Tokens[2] = name
    // Tokens[3] = operation
    // Tokens[4] = value

    optional<uint32_t> SID = ToUint32(Tokens[1]);
    if (!SID.has_value()) {
      Print(GetLogPrefix() + "VarP [" + Tokens[2] + "] has invalid SID [" + Tokens[1] + "], ignoring");
      return false;
    }
    uint8_t subType = 0xFFu;
    if (Tokens[3] == "=") {
      subType = MMD_OPERATOR_SET;
    } else if (Tokens[3] == "+=") {
      subType = MMD_OPERATOR_ADD;
    } else if (Tokens[3] == "-=") {
      subType = MMD_OPERATOR_SUBTRACT;
    } else {
      Print(GetLogPrefix() + "unknown VarP operation [" + Tokens[3] + "] found, ignoring");
      return false;
    }
    CW3MMDAction* action = new CW3MMDAction(m_Game, fromUID, valueID, MMD_ACTION_TYPE_VAR, subType, *SID);
    action->SetName(Tokens[2]);
    action->AddValue(Tokens[4]);
    m_ActionQueue.push(action);
  } else if (actionType == "FlagP" && Tokens.size() == 3) {
    // Tokens[1] = pid
    // Tokens[2] = flag

    optional<uint32_t> SID = ToUint32(Tokens[1]);
    if (!SID.has_value()) {
      Print(GetLogPrefix() + "FlagP [" + Tokens[2] + "] has invalid SID [" + Tokens[1] + "], ignoring");
      return false;
    }

    uint8_t subType = 0xFFu;
    if (Tokens[2] == "leaver") {
      //m_FlagsLeaver[*SID] = true;
      subType = MMD_FLAG_LEAVER;
    } else if (Tokens[2] == "practicing") {
      //m_FlagsPracticing[*SID] = true;
      subType = MMD_FLAG_PRACTICE;
    } else if (Tokens[2] == "drawer") {
      //m_FlagsPracticing[*SID] = true;
      subType = MMD_FLAG_DRAWER;
    } else if (Tokens[2] == "winner") {
      //m_FlagsPracticing[*SID] = true;
      subType = MMD_FLAG_WINNER;
    } else if (Tokens[2] == "loser") {
      //m_FlagsPracticing[*SID] = true;
      subType = MMD_FLAG_LOSER;
    } else {
      Print(GetLogPrefix() + "unknown flag [" + Tokens[2] + "] found, ignoring");
      return false;
    }

    CW3MMDAction* action = new CW3MMDAction(m_Game, fromUID, valueID, MMD_ACTION_TYPE_FLAG, subType, *SID);
    m_ActionQueue.push(action);
  } else if (actionType == "DefEvent" && Tokens.size() >= 4) {
    // Tokens[1] = name
    // Tokens[2] = # of arguments (n)
    // Tokens[3..n+3] = arguments
    // Tokens[n+3] = format

    optional<uint32_t> arity = ToUint32(Tokens[2]);
    if (!arity.has_value()) {
      Print(GetLogPrefix() + "DefEvent invalid arity [" + Tokens[2] + "] found, ignoring");
      return false;
    }
    if (Tokens.size() != arity.value() + 4) {
      Print(GetLogPrefix() + "DefEvent [" + Tokens[2] + "] tokens missing, ignoring");
      return false;
    }
    CW3MMDDefinition* def = new CW3MMDDefinition(m_Game, fromUID, valueID, MMD_DEFINITION_TYPE_EVENT, arity.value());
    def->SetName(Tokens[1]);
    uint8_t i = 2;
    while (++i < Tokens.size()) {
      def->AddValue(Tokens[i]);
    }
    m_DefQueue.push(def);
  } else if (actionType == "Event" && Tokens.size() >= 2) {
    // Tokens[1] = name
    // Tokens[2..n+2] = arguments (where n is the # of arguments in the corresponding DefEvent)
    CW3MMDAction* action = new CW3MMDAction(m_Game, fromUID, valueID, MMD_ACTION_TYPE_EVENT, 0);
    uint8_t i = 1;
    action->SetName(Tokens[i]);    
    while (++i < Tokens.size()) {
      action->AddValue(Tokens[i]);
    }
    m_ActionQueue.push(action);
  } else if (actionType == "Blank") {
    // ignore
  } else if (actionType == "Custom") {
    LogMetaData(m_Game->GetGameTicks(), "custom: " + JoinVector(Tokens, false));
  } else {
    LogMetaData(m_Game->GetGameTicks(), "unknown action type [" + actionType + "] found, ignoring");
  }
  return true;
}

bool CW3MMD::RecvAction(uint8_t fromUID, const CIncomingAction& action)
{
  if (m_Error) {
    return false;
  }

  unsigned int i = 0;
  const vector<uint8_t>& ActionData = action.GetImmutableAction();
  vector<uint8_t> MissionKey;
  vector<uint8_t> Key;
  vector<uint8_t> Value;

  while (ActionData.size() >= i + 9) {
    if (ActionData[i] == ACTION_SYNC_INT &&
      ActionData[i + 1] == 'M' &&
      ActionData[i + 2] == 'M' &&
      ActionData[i + 3] == 'D' &&
      ActionData[i + 4] == '.' &&
      ActionData[i + 5] == 'D' &&
      ActionData[i + 6] == 'a' &&
      ActionData[i + 7] == 't' &&
      ActionData[i + 8] == 0x00)
    {
      if (ActionData.size() >= i + 10) {
        MissionKey = ExtractCString(ActionData, i + 9);

        if (ActionData.size() >= i + 11 + MissionKey.size()) {
          Key = ExtractCString(ActionData, i + 10 + MissionKey.size());

          if (ActionData.size() >= i + 15 + MissionKey.size() + Key.size()) {
            Value = vector<uint8_t>(ActionData.begin() + i + 11 + MissionKey.size() + Key.size(), ActionData.begin() + i + 15 + MissionKey.size() + Key.size());
            string MissionKeyString = string(MissionKey.begin(), MissionKey.end());
            string KeyString = string(Key.begin(), Key.end());
            //uint32_t ValueInt = ByteArrayToUInt32(Value, false);

            // Print("[W3MMD] DEBUG: mkey [" + MissionKeyString + "], key [" + KeyString + "], value [" + to_string(ValueInt) + "]");

            if (MissionKeyString.size() > 4 && MissionKeyString.substr(0, 4) == "val:") {
              string ValueIDString = MissionKeyString.substr(4);
              optional<uint32_t> ValueID = ToUint32(ValueIDString);
              vector<string> Tokens = TokenizeKey(KeyString);
              if (!ValueID.has_value() || !HandleTokens(fromUID, ValueID.value(), Tokens)) {
                Print(GetLogPrefix() + "error parsing [" + KeyString + "]");
              }
            } else if (MissionKeyString.size() > 4 && MissionKeyString.substr(0, 4) == "chk:") {
              /*
              string CheckIDString = MissionKeyString.substr(4);
              optional<uint32_t> CheckID = ToUint32(CheckIDString);

              // todotodo: cheat detection

               ++m_NextCheckID;
               */
            } else {
              Print(GetLogPrefix() + "unknown mission key [" + MissionKeyString + "] found, ignoring");
            }
            i += 15 + MissionKey.size() + Key.size();
          } else {
            ++i;
          }
        } else {
          ++i;
        }
      } else {
        ++i;
      }
    }
    else {
      ++i;
    }
  }

  return !m_Error;
}

bool CW3MMD::ProcessDefinition(CW3MMDDefinition* definition)
{
  if (definition->GetType() == MMD_DEFINITION_TYPE_INIT) {
    if (definition->GetSubType() == MMD_INIT_TYPE_PLAYER) {
      if (definition->GetSID() >= m_Game->GetNumSlots()) {
        Print(GetLogPrefix() + "cannot initialize player slot " + ToDecString(definition->GetSID()));
        return false;
      }
      const bool found = m_SIDToName.find(definition->GetSID()) != m_SIDToName.end();
      if (found) {
        Print(
          GetLogPrefix() + "Player [" + GetSenderName(definition) + "] overrode previous name [" + m_SIDToName[definition->GetSID()] +
          "] with new name [" + definition->GetName() + "] for SID [" + ToDecString(definition->GetSID()) + "]"
        );
      } else {
        Print(
          GetLogPrefix() + "Player [" + GetSenderName(definition) + "] initialized player ID [" + ToDecString(definition->GetSID()) +
          "] as [" + definition->GetName() + "]"
        );
      }
      if (!found && m_SIDToName.size() >= m_Game->GetNumControllers()) {
        Print(GetLogPrefix() + "too many players initialized");
        return false;
      }
      m_SIDToName[definition->GetSID()] = definition->GetName();
    }
    return true;
  } else if (definition->GetType() == MMD_DEFINITION_TYPE_VAR) {
    if (m_DefVarPs.find(definition->GetName()) != m_DefVarPs.end()) {
      Print(GetLogPrefix() + "duplicate DefVarP [" + definition->GetName() + "] found, ignoring");
      return false;
    }
    if (definition->GetSubType() == MMD_VALUE_TYPE_INT) {
      m_DefVarPs[definition->GetName()] = MMD_VALUE_TYPE_INT;
    } else if (definition->GetSubType() == MMD_VALUE_TYPE_REAL) {
      m_DefVarPs[definition->GetName()] = MMD_VALUE_TYPE_REAL;
    } else { // if (definition->GetSubType() == MMD_VALUE_TYPE_STRING)
      m_DefVarPs[definition->GetName()] = MMD_VALUE_TYPE_STRING;
    }
    return true;
  } else { // if (definition->GetType() == MMD_DEFINITION_TYPE_EVENT)
    if (m_DefEvents.find(definition->GetName()) != m_DefEvents.end()) {
      Print(GetLogPrefix() + "duplicate DefEvent [" + definition->GetName() + "] found, ignoring");
      return false;
    }
    m_DefEvents[definition->GetName()] = definition->CopyValues();
    return true;
  }
}

bool CW3MMD::ProcessAction(CW3MMDAction* action)
{
  if (action->GetType() == MMD_ACTION_TYPE_FLAG) {
    if (m_SIDToName.find(action->GetSID()) == m_SIDToName.end()) {
      Print(GetLogPrefix() + "FlagP [" + action->GetName() + "] has undefined SID [" + ToDecString(action->GetSID()) + "], ignoring");
      return false;
    }
    uint8_t result = 0xFFu;
    switch (action->GetSubType()) {
      case MMD_FLAG_LEAVER: {
        m_FlagsLeaver[action->GetSID()] = true;
        break;
      }
      case MMD_FLAG_PRACTICE: {
        m_FlagsPracticing[action->GetSID()] = true;
        break;
      }
      case MMD_FLAG_DRAWER: {
        result = MMD_RESULT_DRAWER;
        break;
      }
      case MMD_FLAG_WINNER: {
        result = MMD_RESULT_WINNER;
        break;
      }
      default: {
        result = MMD_RESULT_LOSER;
        break;
      }
    }
    if (result == 0xFFu) {
      return true;
    }
    auto previousResultIt = m_Flags.find(action->GetSID());
    if (previousResultIt != m_Flags.end()) {
      if (previousResultIt->second == result) {
        return true;
      }
      Print(
        GetLogPrefix() + "previous flag [" + to_string(previousResultIt->second) + "] would be overriden with new flag [" +
        ToDecString(result) + "] for SID [" + ToDecString(action->GetSID()) + "] - ignoring"
      );
      return false;
    }
    m_Flags[action->GetSID()] = result;
    if (result == MMD_RESULT_WINNER) {
      m_GameOver = true;
    }
    LogMetaData(action->GetRecvTicks(), GetStoredPlayerName(action->GetSID()) + " " + m_ResultVerbs[result] + " the game.");
    return true;
  } else if (action->GetType() == MMD_ACTION_TYPE_VAR) {
    if (m_DefVarPs.find(action->GetName()) == m_DefVarPs.end()) {
      Print(GetLogPrefix() + "VarP [" + action->GetName() + "] found without a corresponding DefVarP, ignoring");
      return false;
    }
    uint8_t valueType = m_DefVarPs[action->GetName()];
    if (action->GetSubType() == MMD_OPERATOR_SET) {
      std::string operand = action->GetFirstValue();
      if (valueType == MMD_VALUE_TYPE_REAL) {
        optional<double> realValue = ToDouble(operand);
        if (!realValue.has_value()) {
          Print(GetLogPrefix() + "invalid real VarP [" + action->GetName() + "] value [" + operand + "] found, ignoring");
          return false;
        }
        VarP VP = VarP(action->GetSID(), action->GetName());
        m_VarPReals[VP] = *realValue;
        return true;
      } else if (valueType == MMD_VALUE_TYPE_INT) {
        optional<uint32_t> intValue = ToUint32(operand);
        if (!intValue.has_value()) {
          Print(GetLogPrefix() + "invalid int VarP [" + action->GetName() + "] value [" + operand + "] found, ignoring");
          return false;
        }
        VarP VP = VarP(action->GetSID(), action->GetName());
        m_VarPInts[VP] = *intValue;
        return true;
      } else { // MMD_VALUE_TYPE_STRING
        VarP VP = VarP(action->GetSID(), action->GetName());
        m_VarPStrings[VP] = operand;
        return true;
      }
    } else {
      if (valueType == MMD_VALUE_TYPE_STRING) {
        Print(GetLogPrefix() + "VarP [" + action->GetName() + "] of type string cannot accept +=, -= operators, ignoring");
        return false;
      }
      std::string operand = action->GetFirstValue();
      if (valueType == MMD_VALUE_TYPE_REAL) {
        optional<double> realValue = ToDouble(operand);
        if (!realValue.has_value()) {
          Print(GetLogPrefix() + "invalid real VarP [" + action->GetName() + "] value [" + operand + "] found, ignoring");
          return false;
        }
        VarP VP = VarP(action->GetSID(), action->GetName());
        if (action->GetSubType() == MMD_OPERATOR_ADD) {
          m_VarPReals[VP] += *realValue;
        } else { // MMD_OPERATOR_SUBTRACT
          m_VarPReals[VP] -= *realValue;
        }
      } else { // MMD_VALUE_TYPE_INT
        optional<uint32_t> intValue = ToUint32(operand);
        if (!intValue.has_value()) {
          Print(GetLogPrefix() + "invalid int VarP [" + action->GetName() + "] value [" + operand + "] found, ignoring");
          return false;
        }
        VarP VP = VarP(action->GetSID(), action->GetName());
        if (action->GetSubType() == MMD_OPERATOR_ADD) {
          m_VarPInts[VP] += *intValue;
        } else { // MMD_OPERATOR_SUBTRACT
          m_VarPInts[VP] -= *intValue;
        }
      }
      return true;
    }
  } else { // if (action->GetType() == MMD_ACTION_TYPE_EVENT) 
    auto defEventIt = m_DefEvents.find(action->GetName());
    if (defEventIt == m_DefEvents.end()) {
      Print(GetLogPrefix() + "Event [" + action->GetName() + "] found without a corresponding DefEvent, ignoring");
      return false;
    }
    const std::vector<std::string>& values = action->RefValues();
    const vector<string>& DefEvent = defEventIt->second;
    if (values.size() != DefEvent.size() - 1) {
      Print(GetLogPrefix() + "Event [" + action->GetName() + "] found with " + to_string(values.size()) + " arguments but expected " + to_string(DefEvent.size() - 1) + " arguments, ignoring");
      return false;
    }
    if (DefEvent.empty()) {
      LogMetaData(action->GetRecvTicks(), "Event [" + action->GetName() + "]");
      return true;
    }

    string Format = DefEvent[DefEvent.size() - 1];

    // replace the markers in the format string with the arguments
    for (uint32_t i = 0; i < values.size(); ++i) {
      // check if the marker is a SID marker

      if (DefEvent[i].substr(0, 4) == "pid:") {
        // replace it with the player's name rather than their SID
        optional<uint32_t> SID = ToUint32(values[i]);
        if (!SID.has_value()) {
          Print(GetLogPrefix() + "Event [" + action->GetName() + "] passed invalid PID " + values[i]);
          return false;
        }
        auto it = m_SIDToName.find(*SID);
        if (it == m_SIDToName.end()) {
          Print(GetLogPrefix() + "Event [" + action->GetName() + "] passed undefined PID " + values[i]);
          ReplaceText(Format, "{" + to_string(i) + "}", "SID:" + values[i]);
        } else {
          ReplaceText(Format, "{" + to_string(i) + "}", it->second);
        }
      } else {
        ReplaceText(Format, "{" + to_string(i) + "}", values[i]);
      }
    }
    LogMetaData(action->GetRecvTicks(), "Event [" + action->GetName() + "]: " + Format);
    return true;
  }
}

bool CW3MMD::UpdateQueue()
{
  const int64_t gameTicks = m_Game->GetGameTicks();
  if (m_Game->GetPaused()) return true;
  if (gameTicks < MMD_PROCESSING_INITIAL_DELAY) return true;
  while (!m_DefQueue.empty()) {
    CW3MMDDefinition* def = m_DefQueue.front();
    if (gameTicks < def->GetRecvTicks() + MMD_PROCESSING_STREAM_DEF_DELAY) {
      break;
    }
    ProcessDefinition(def);
    if (def->GetUpdateID() > m_LastValueID) {
      m_LastValueID = def->GetUpdateID();
    }
    delete def;
    m_DefQueue.pop();
  }
  if (!m_DefQueue.empty()) {
    return true;
  }
  while (!m_ActionQueue.empty()) {
    CW3MMDAction* action = m_ActionQueue.front();
    if (gameTicks < action->GetRecvTicks() + MMD_PROCESSING_STREAM_ACTION_DELAY) {
      break;
    }
    ProcessAction(action);
    if (action->GetUpdateID() > m_LastValueID) {
      m_LastValueID = action->GetUpdateID();
    }
    delete action;
    m_ActionQueue.pop();
  }
  return !m_GameOver;
}

bool CW3MMD::FlushQueue()
{
  while (!m_DefQueue.empty()) {
    CW3MMDDefinition* def = m_DefQueue.front();
    ProcessDefinition(def);
    delete def;
    m_DefQueue.pop();
  }
  while (!m_ActionQueue.empty()) {
    CW3MMDAction* action = m_ActionQueue.front();
    ProcessAction(action);
    delete action;
    m_ActionQueue.pop();
  }
  return !m_GameOver;
}

vector<string> CW3MMD::TokenizeKey(string key) const
{
  vector<string> tokens;
  string token;
  bool escaping = false;

  for (string::iterator i = key.begin(); i != key.end(); ++i) {
    if (escaping) {
      if (*i == ' ') {
        token += ' ';
      } else if (*i == '\\') {
        token += '\\';
      } else {
        Print(GetLogPrefix() + "error tokenizing key [" + key + "], invalid escape sequence found, ignoring");
        return vector<string>();
      }
      escaping = false;
    } else {
      if (*i == ' ') {
        if (token.empty()) {
          Print(GetLogPrefix() + "error tokenizing key [" + key + "], empty token found, ignoring");
          return vector<string>();
        }
        tokens.push_back(token);
        token.clear();
      } else if (*i == '\\') {
        escaping = true;
      } else {
        token += *i;
      }
    }
  }

  if (token.empty()) {
    Print(GetLogPrefix() + "error tokenizing key [" + key + "], empty token found, ignoring");
    return vector<string>();
  }

  tokens.push_back(token);
  return tokens;
}

string CW3MMD::GetStoredPlayerName(uint8_t SID) const
{
  auto nameIterator = m_SIDToName.find(SID);
  if (nameIterator == m_SIDToName.end()) {
    return "SID " + to_string(SID);
  } else {
    return nameIterator->second;
  }
}

string CW3MMD::GetTrustedPlayerNameFromColor(uint8_t color) const
{
  string playerName;
  CDBGamePlayer* dbPlayer = m_Game->GetDBPlayerFromColor(color);
  if (dbPlayer) {
    playerName = dbPlayer->GetName();
  } else {
    Print(GetLogPrefix() + "error retrieving name of player color [" + ToDecString(color) + "] (" + GetColorName(color) + ")");
  }
  return playerName;
}

string CW3MMD::GetSenderName(CW3MMDDefinition* definition) const
{
  return GetTrustedPlayerNameFromColor(definition->GetFromColor());
}

string CW3MMD::GetSenderName(CW3MMDAction* action) const
{
  return GetTrustedPlayerNameFromColor(action->GetFromColor());
}

vector<string> CW3MMD::GetWinners() const
{
  vector<string> winners;
  for (const auto& flagEntry : m_Flags) {
    if (flagEntry.second != MMD_RESULT_WINNER) continue;
    winners.push_back(GetStoredPlayerName(flagEntry.first));
  }
  return winners;
}

string CW3MMD::GetLogPrefix() const
{
  return "[W3MMD: " + m_Game->GetGameName() + "] ";
}

void CW3MMD::LogMetaData(int64_t gameTicks, const string& text) const
{
  m_Game->Log(text, gameTicks);
}
