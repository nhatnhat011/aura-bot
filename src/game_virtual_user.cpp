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

#include "game_virtual_user.h"

#include "protocol/game_protocol.h"
#include "game.h"

using namespace std;

//
// CGameVirtualUser
//

CGameVirtualUser::CGameVirtualUser(CGame* nGame, uint8_t nSID, uint8_t nUID, string nName)
  : m_Game(nGame),
    m_Observer(false),
    m_LeftMessageSent(false),
    m_HasPlayerIntent(false),
    m_Status(USERSTATUS_LOBBY),
    m_SID(nSID),
    m_UID(nUID),
    m_OldUID(0xFF),
    m_PseudonymUID(0xFF),
    m_AllowedActions(VIRTUAL_USER_ALLOW_ACTIONS_ANY),
    m_AllowedConnections(VIRTUAL_USER_ALLOW_CONNECTIONS_NONE),
    m_RemainingSaves(GAME_SAVES_PER_PLAYER),
    m_RemainingPauses(GAME_PAUSES_PER_PLAYER),
    m_LeftCode(PLAYERLEAVE_LOBBY),
    m_Name(std::move(nName))
{
}

string CGameVirtualUser::GetLowerName() const
{
  return ToLowerCase(m_Name);
}

string CGameVirtualUser::GetDisplayName() const
{
  // This information is important for letting hosts know which !open, !close, commands to execute.
  return "User[" + ToDecString(m_SID + 1) + "]";
  /*
  if (m_Game->GetIsHiddenPlayerNames() && !(m_Observer && m_Game->GetGameLoaded())) {
    if (m_PseudonymUID == 0xFF) {
      return "Player " + ToDecString(m_UID);
    } else {
      // After CGame::RunPlayerObfuscation()
      return "Player " + ToDecString(m_PseudonymUID) + "?";
    }
  }
  return m_Name;
  */
}

bool CGameVirtualUser::GetCanPause() const
{
  if (!(m_AllowedActions & VIRTUAL_USER_ALLOW_ACTIONS_PAUSE)) return false;
  if (m_RemainingPauses == 0) return false;

  // Referees can pause the game without limit.
  // Full observers can never pause the game.
  return !m_Observer || m_Game->GetHasReferees();
}

bool CGameVirtualUser::GetCanResume() const
{
  if (!(m_AllowedActions & VIRTUAL_USER_ALLOW_ACTIONS_RESUME)) return false;

  // Referees can unpause the game, but full observers cannot.
  return !m_Observer || m_Game->GetHasReferees();
}

bool CGameVirtualUser::GetCanSave() const
{
  if (!(m_AllowedActions & VIRTUAL_USER_ALLOW_ACTIONS_SAVE)) return false;
  if (m_RemainingSaves == 0) return false;

  // Referees can save the game without limit.
  // Full observers can never save the game.
  return !m_Observer || m_Game->GetHasReferees();
}

vector<uint8_t> CGameVirtualUser::GetPlayerInfoBytes() const
{
  const array<uint8_t, 4> IP = {0, 0, 0, 0};
  return GameProtocol::SEND_W3GS_PLAYERINFO(m_UID, GetDisplayName(), IP, IP);
}

vector<uint8_t> CGameVirtualUser::GetGameLoadedBytes() const
{
  return GameProtocol::SEND_W3GS_GAMELOADED_OTHERS(m_UID);
}

vector<uint8_t> CGameVirtualUser::GetGameQuitBytes(const uint8_t leftCode) const
{
  return GameProtocol::SEND_W3GS_PLAYERLEAVE_OTHERS(m_UID, leftCode);
}

void CGameVirtualUser::RefreshUID()
{
  m_OldUID = m_UID;
  m_UID = m_Game->GetNewUID();
}
