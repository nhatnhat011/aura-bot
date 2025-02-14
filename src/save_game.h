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

#ifndef AURA_SAVEGAME_H
#define AURA_SAVEGAME_H

#include "includes.h"

#include <filesystem>

#include "game_slot.h"

//
// CSaveGame
//

class CSaveGame
{
private:
  CAura* m_Aura;
  CPacked* m_Packed;
  bool m_Valid;
	uint8_t m_NumSlots;
  uint32_t m_RandomSeed;
	std::string m_ClientMapPath;
	std::string m_GameName;
	std::string m_ClientPath;
	std::filesystem::path m_ServerPath;
	std::vector<CGameSlot> m_Slots;
	std::array<uint8_t, 4> m_SaveHash;

public:
	CSaveGame(CAura* nAura, const std::filesystem::path& fromPath);
	~CSaveGame();

	[[nodiscard]] const std::filesystem::path& GetServerPath()const { return m_ServerPath; }
	[[nodiscard]] const std::string& GetClientPath() const { return m_ClientPath; }
  [[nodiscard]] const std::string& GetClientMapPath() const { return m_ClientMapPath; }
	[[nodiscard]] const std::string& GetGameName()	const { return m_GameName; }
	[[nodiscard]] uint8_t GetNumSlots() const { return m_NumSlots; }
  [[nodiscard]] uint8_t GetNumHumanSlots() const;
	[[nodiscard]] const std::vector<CGameSlot>& GetSlots() const { return m_Slots; }
	[[nodiscard]] uint32_t GetRandomSeed() const { return m_RandomSeed; }
	[[nodiscard]] const std::array<uint8_t, 4>& GetSaveHash()	const { return m_SaveHash; }
  [[nodiscard]] const CGameSlot* InspectSlot(const uint8_t SID) const;
  bool Load();
  void Unload();
	bool Parse();
};

#endif // AURA_SAVEGAME_H
