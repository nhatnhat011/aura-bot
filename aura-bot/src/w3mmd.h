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

#ifndef W3MMD_H
#define W3MMD_H

#include "includes.h"

//
// CW3MMD
//

typedef std::pair<uint32_t, std::string> VarP;

class CW3MMDAction
{
public:
  int64_t                                                       m_Ticks;              // m_Game->GetGameTicks()when this definition was received
  uint32_t                                                      m_UpdateID;
  uint8_t                                                       m_Type;               // FlagP, VarP, Event, Blank, Custom,
  uint8_t                                                       m_SubType;            // (winner, loser, drawer, leaver, practice), (set, add, subtract), (NO), (NO), (NO)
  uint8_t                                                       m_FromUID;
  uint8_t                                                       m_FromColor;
  uint8_t                                                       m_SID;                // (OK), (OK), (NO), (NO), (NO)
  std::string                                                   m_Name;               // (NO), (OK), (OK), (NO), (NO)
  std::vector<std::string>                                      m_Values;             // (NO), (1), (n), (NO), (?)

  CW3MMDAction(CGame* nGame, uint8_t nFromUID, uint32_t nID, uint8_t nType, uint8_t nSubType = 0, uint8_t nSID = 0);
  ~CW3MMDAction();

  inline int64_t GetRecvTicks() { return m_Ticks; }
  inline uint32_t GetUpdateID() { return m_UpdateID; }

  inline uint8_t GetType() const { return m_Type; }
  inline uint8_t GetSubType() const { return m_SubType; }

  inline uint8_t GetFromUID() const { return m_FromUID; }
  inline uint8_t GetFromColor() const { return m_FromColor; }
  inline uint8_t GetSID() const { return m_SID; }

  inline const std::string& GetName() const { return m_Name; }
  void SetName(const std::string& name) { m_Name = name; }

  std::vector<std::string> CopyValues() { return m_Values; }
  const std::vector<std::string>& RefValues() const { return m_Values; }
  void AddValue(const std::string& value) { m_Values.push_back(value); }
  std::string GetFirstValue() const { return m_Values[0]; }
};

class CW3MMDDefinition
{
public:
  int64_t                                                       m_Ticks;              // m_Game->GetGameTicks() when this definition was received
  uint32_t                                                      m_UpdateID;
  uint8_t                                                       m_Type;               // init, DefVarP, DefEvent
  uint8_t                                                       m_SubType;            // (pid, version), (int, real, string), (0-ary, 1-ary, 2-ary, etc.)
  uint8_t                                                       m_FromUID;
  uint8_t                                                       m_FromColor;
  uint8_t                                                       m_SID;                // (SID OK, version NO), (NO), (NO)
  std::string                                                   m_Name;               // (pid OK, version NO), (OK, OK, OK), (OK+)
  std::vector<std::string>                                      m_Values;             // (pid NO, version 2), (NO), (n)

  CW3MMDDefinition(CGame* nGame, uint8_t nFromUID, uint32_t nID, uint8_t nType, uint8_t nSubType = 0, uint8_t nSID = 0);
  ~CW3MMDDefinition();

  inline int64_t GetRecvTicks() const { return m_Ticks; }
  inline uint32_t GetUpdateID() const { return m_UpdateID; }

  inline uint8_t GetType() const { return m_Type; }
  inline uint8_t GetSubType() const { return m_SubType; }

  inline uint8_t GetFromUID() const { return m_FromUID; }
  inline uint8_t GetFromColor() const { return m_FromColor; }
  inline uint8_t GetSID() const { return m_SID; }

  inline const std::string& GetName() const { return m_Name; }
  void SetName(const std::string& name) { m_Name = name; }

  std::vector<std::string> CopyValues() { return m_Values; }
  const std::vector<std::string>& RefValues() const { return m_Values; }
  void AddValue(const std::string& value) { m_Values.push_back(value); }
};

class CW3MMD
{
private:
  CGame*                                          m_Game;
  bool                                            m_GameOver;
  bool                                            m_Error;
  uint32_t                                        m_Version;
  uint32_t                                        m_LastValueID;
  //uint32_t                                      m_NextCheckID;
  std::map<uint8_t, std::string>                  m_SIDToName;           // pid -> player name (e.g. 0 -> "Varlock") --- note: will not be automatically converted to lower case
  std::map<uint8_t, uint8_t>                      m_Flags;               // pid -> flag (e.g. 0 -> MMD_RESULT_WINNER)
  std::map<uint8_t, bool>                         m_FlagsLeaver;         // pid -> leaver flag (e.g. 0 -> true) --- note: will only be present if true
  std::map<uint8_t, bool>                         m_FlagsPracticing;     // pid -> practice flag (e.g. 0 -> true) --- note: will only be present if true
  std::map<std::string, uint8_t>                  m_DefVarPs;            // varname -> value type (e.g. "kills" -> MMD_VALUE_TYPE_INT)
  std::map<VarP, int32_t>                         m_VarPInts;            // pid,varname -> value (e.g. 0,"kills" -> 5)
  std::map<VarP, double>                          m_VarPReals;           // pid,varname -> value (e.g. 0,"x" -> 0.8)
  std::map<VarP, std::string>                     m_VarPStrings;         // pid,varname -> value (e.g. 0,"hero" -> "heroname")
  std::map<std::string, std::vector<std::string>> m_DefEvents;           // event -> vector of format + arguments
  std::array<std::string, 3>                      m_ResultVerbs;

  std::queue<CW3MMDDefinition*>                   m_DefQueue;
  std::queue<CW3MMDAction*>                       m_ActionQueue;

public:
  CW3MMD(CGame* nGame);
  ~CW3MMD();

  [[nodiscard]] inline bool GetIsGameOver() { return m_GameOver; }

  bool HandleTokens(uint8_t fromUID, uint32_t valueID, std::vector<std::string> tokens);
  bool RecvAction(uint8_t fromUID, const CIncomingAction& action);
  bool ProcessDefinition(CW3MMDDefinition* nDef);
  bool ProcessAction(CW3MMDAction* nAction);
  bool UpdateQueue();
  bool FlushQueue();
  [[nodiscard]] std::vector<std::string> TokenizeKey(std::string key) const;
  [[nodiscard]] std::string GetStoredPlayerName(uint8_t SID) const;
  [[nodiscard]] std::string GetTrustedPlayerNameFromColor(uint8_t color) const;
  [[nodiscard]] std::string GetSenderName(CW3MMDAction* action) const;
  [[nodiscard]] std::string GetSenderName(CW3MMDDefinition* definition) const;
  [[nodiscard]] std::vector<std::string> GetWinners() const;
  [[nodiscard]] std::string GetLogPrefix() const;
  void LogMetaData(int64_t recvTicks, const std::string& text) const;
};

#endif
