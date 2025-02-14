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

#ifndef AURA_GAMESLOT_H_
#define AURA_GAMESLOT_H_

#include "includes.h"
#include <iterator>

#include "hash.h"

//
// CGameSlot
//

class CGameSlot
{
private:
  uint8_t m_Type;
  uint8_t m_UID;            // player id
  uint8_t m_DownloadStatus; // download status (0% to 100%)
  uint8_t m_SlotStatus;     // slot status (0 = open, 1 = closed, 2 = occupied)
  uint8_t m_Computer;       // computer (0 = no, 1 = yes)
  uint8_t m_Team;           // team
  uint8_t m_Color;          // colour
  uint8_t m_Race;           // race (1 = human, 2 = orc, 4 = night elf, 8 = undead, 32 = random, 64 = selectable)
  uint8_t m_ComputerType;   // computer type (0 = easy, 1 = human or normal comp, 2 = hard comp)
  uint8_t m_Handicap;       // handicap
  uint8_t m_Unselectable;  

public:
  explicit CGameSlot(const std::vector<uint8_t>& n);
  CGameSlot(const uint8_t nType, const uint8_t nUID, const uint8_t nDownloadStatus, const uint8_t nSlotStatus, const uint8_t nComputer, const uint8_t nTeam, const uint8_t nColor, const uint8_t nRace, const uint8_t nComputerType = 1, const uint8_t nHandicap = 100);
  ~CGameSlot();

  inline uint8_t              GetUID() const { return m_UID; }
  inline uint8_t              GetDownloadStatus() const { return m_DownloadStatus; }
  inline uint8_t              GetSlotStatus() const { return m_SlotStatus; }
  inline uint8_t              GetComputer() const { return m_Computer; } // computer bit
  inline uint8_t              GetTeam() const { return m_Team; }
  inline uint8_t              GetColor() const { return m_Color; }
  inline uint8_t              GetRace() const { return m_Race; }
  inline uint8_t              GetRaceFixed() const { return m_Race &~ SLOTRACE_SELECTABLE; }
  inline uint8_t              GetRaceSelectable() const { return (m_Race & SLOTRACE_SELECTABLE) ? SLOTRACE_RANDOM | SLOTRACE_SELECTABLE : m_Race; }
  inline uint8_t              GetComputerType() const { return m_ComputerType; }
  inline uint8_t              GetHandicap() const { return m_Handicap; }
  inline uint8_t              GetType() const { return m_Type; }
  inline bool                 GetIsPlayerOrFake() const { return m_SlotStatus == SLOTSTATUS_OCCUPIED && m_Computer == SLOTCOMP_NO; }
  inline bool                 GetIsComputer() const { return m_SlotStatus == SLOTSTATUS_OCCUPIED && m_Computer == SLOTCOMP_YES; }
  inline bool                 GetIsSelectable() const { return m_Type <= SLOTTYPE_USER; }
  inline std::vector<uint8_t> GetProtocolArray() const { return std::vector<uint8_t>{m_UID, m_DownloadStatus, m_SlotStatus, m_Computer, m_Team, m_Color, m_Race, m_ComputerType, m_Handicap}; }
  inline std::vector<uint8_t> GetByteArray() const { return std::vector<uint8_t>{m_UID, m_DownloadStatus, m_SlotStatus, m_Computer, m_Team, m_Color, m_Race, m_ComputerType, m_Handicap, m_Type}; }

  inline void SetUID(uint8_t nUID) { m_UID = nUID; }
  inline void SetDownloadStatus(uint8_t nDownloadStatus) { m_DownloadStatus = nDownloadStatus; }
  inline void SetSlotStatus(uint8_t nSlotStatus) { m_SlotStatus = nSlotStatus; }
  inline void SetComputer(uint8_t nComputer) { m_Computer = nComputer; }
  inline void SetTeam(uint8_t nTeam) { m_Team = nTeam; }
  inline void SetColor(uint8_t nColor) { m_Color = nColor; }
  inline void SetRace(uint8_t nRace) { m_Race = nRace; }
  inline void SetComputerType(uint8_t nComputerType) { m_ComputerType = nComputerType; }
  inline void SetHandicap(uint8_t nHandicap) { m_Handicap = nHandicap; }
  inline void SetType(uint8_t nType) { m_Type = nType; }
};

inline uint8_t ParseSID(const std::string& input)
{
  int32_t SID = 0xFF;
  if (input.length() > 2) {
    return 0xFF;
  }
  try {
    SID = std::stoi(input);
  } catch (...) {
  }
  if (SID <= 0 || 0xFF <= SID) {
    return static_cast<uint8_t>(0xFF);
  }
  return static_cast<uint8_t>(SID - 1);
}

inline uint8_t ParseComputerSkill(const std::string& skill)
{
  std::string inputLower = skill;
  std::transform(std::begin(inputLower), std::end(inputLower), std::begin(inputLower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  switch (HashCode(inputLower)) {
    case HashCode("easy"):
    case HashCode("0"):
      return SLOTCOMP_EASY;
    case HashCode("normal"):
    case HashCode("1"):
      return SLOTCOMP_NORMAL;
    case HashCode("hard"):
    case HashCode("insane"):
    case HashCode("2"):
      return SLOTCOMP_HARD;
    default:
      return SLOTCOMP_INVALID;
  }
}

inline uint8_t ParseRace(const std::string& race)
{
  std::string inputLower = race;
  std::transform(std::begin(inputLower), std::end(inputLower), std::begin(inputLower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  switch (HashCode(inputLower)) {
    case HashCode("human"):
      return SLOTRACE_HUMAN;
    case HashCode("undead"):
      return SLOTRACE_UNDEAD;
    case HashCode("orc"):
      return SLOTRACE_ORC;
    case HashCode("elf"):
    case HashCode("nightelf"):
    case HashCode("night elf"):
      return SLOTRACE_NIGHTELF;
    case HashCode("random"):
      return SLOTRACE_RANDOM;
    case HashCode("roll"):
    case HashCode("pickrandom"):
      return SLOTRACE_PICKRANDOM;
    default:
      return SLOTRACE_INVALID;
  }
}

inline uint8_t ParseColor(const std::string& color)
{
  std::string inputLower = color;
  std::transform(std::begin(inputLower), std::end(inputLower), std::begin(inputLower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  switch (HashCode(inputLower)) {
    case HashCode("red"):
      return 0;
    case HashCode("blue"):
    case HashCode("bleu"):
      return 1;
    case HashCode("teal"):
      return 2;
    case HashCode("purple"):
      return 3;
    case HashCode("yellow"):
      return 4;
    case HashCode("orange"):
      return 5;
    case HashCode("green"):
      return 6;
    case HashCode("pink"):
      return 7;
    case HashCode("gray"):
    case HashCode("grey"):
      return 8;
    case HashCode("light blue"):
    case HashCode("lightblue"):
      return 9;
    case HashCode("dark green"):
    case HashCode("darkgreen"):
      return 10;
    case HashCode("brown"):
      return 11;
    case HashCode("maroon"):
      return 12;
    case HashCode("navy"):
      return 13;
    case HashCode("turquoise"):
      return 14;
    case HashCode("violet"):
      return 15;
    case HashCode("wheat"):
      return 16;
    case HashCode("peach"):
      return 17;
    case HashCode("mint"):
      return 18;
    case HashCode("lavender"):
      return 19;
    case HashCode("coal"):
      return 20;
    case HashCode("snow"):
      return 21;
    case HashCode("emerald"):
      return 22;
    case HashCode("peanut"):
      return 23;
    default:
      return 0xFF;
  }
}

inline std::string GetRaceName(const uint8_t race) {
  switch (race) {
  case SLOTRACE_HUMAN:
    return "Human";
  case SLOTRACE_ORC:
    return "Orc";
  case SLOTRACE_NIGHTELF:
    return "Night Elf";
  case SLOTRACE_UNDEAD:
    return "Undead";
  case SLOTRACE_RANDOM:
    return "Random";
  default:
    return "Unknown";
  }
}

inline std::string GetColorName(const uint8_t color)
{
  switch (color) {
    case 0:
      return "Red";
    case 1:
      return "Blue";
    case 2:
      return "Teal";
    case 3:
      return "Purple";
    case 4:
      return "Yellow";
    case 5:
      return "Orange";
    case 6:
      return "Green";
    case 7:
      return "Pink";
    case 8:
      return "Grey";
    case 9:
      return "Light Blue";
    case 10:
      return "Dark Green";
    case 11:
      return "Brown";
    case 12:
      return "Maroon";
    case 13:
      return "Navy";
    case 14:
      return "Turquoise";
    case 15:
      return "Violet";
    case 16:
      return "Wheat";
    case 17:
      return "Peach";
    case 18:
      return "Mint";
    case 19:
      return "Lavender";
    case 20:
      return "Coal";
    case 21:
      return "Snow";
    case 22:
      return "Emerald";
    case 23:
      return "Peanut";
    case 24:
      return "ObserverDoNotUse";
    default:
      return "Unknown";
  }
}

#endif // AURA_GAMESLOT_H_
