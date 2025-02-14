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

#ifndef AURA_VIRTUAL_USER_H_
#define AURA_VIRTUAL_USER_H_

#include "includes.h"
#include "connection.h"

//
// CGameVirtualUser
//

class CGameVirtualUser
{
public:
  CGame*                           m_Game;
  bool                             m_Observer;                     // if the virtual player is an observer
  bool                             m_LeftMessageSent;              // if the playerleave message has been sent or not
  bool                             m_HasPlayerIntent;
  uint8_t                          m_Status;
  uint8_t                          m_SID;
  uint8_t                          m_UID;                          // the virtual player's UID
  uint8_t                          m_OldUID;
  uint8_t                          m_PseudonymUID;
  uint8_t                          m_AllowedActions;
  uint8_t                          m_AllowedConnections;

  // Actions
  uint8_t                          m_RemainingSaves;
  uint8_t                          m_RemainingPauses;

  uint32_t                         m_LeftCode;                     // the code to be sent in W3GS_PLAYERLEAVE_OTHERS for why this virtual player left the game

  std::string                      m_LeftReason;                   // the reason the virtual player left the game
  std::string                      m_Name;                         // the virtual player's name

  CGameVirtualUser(CGame* game, uint8_t nSID, uint8_t nUID, std::string nName);
  ~CGameVirtualUser() = default;

  [[nodiscard]] inline bool                     GetIsObserver() const { return m_Observer; }
  [[nodiscard]] inline bool                     GetLeftMessageSent() const { return m_LeftMessageSent; }
  [[nodiscard]] inline bool                     GetHasPlayerIntent() const { return m_HasPlayerIntent; }

  [[nodiscard]] inline uint8_t                  GetStatus() const { return m_Status; }
  [[nodiscard]] inline uint8_t                  GetSID() const { return m_SID; }
  [[nodiscard]] inline uint8_t                  GetUID() const { return m_UID; }
  [[nodiscard]] inline uint8_t                  GetOldUID() const { return m_OldUID; }
  [[nodiscard]] inline uint8_t                  GetPseudonymUID() const { return m_PseudonymUID; }

  [[nodiscard]] inline bool                     GetIsInLoadingScreen() const { return m_Status == USERSTATUS_LOADING_SCREEN; }
  [[nodiscard]] inline bool                     GetIsEnding() const { return m_Status == USERSTATUS_ENDING; }
  [[nodiscard]] inline bool                     GetIsEnded() const { return m_Status == USERSTATUS_ENDED; }
  [[nodiscard]] inline bool                     GetIsEndingOrEnded() const { return m_Status == USERSTATUS_ENDING || m_Status == USERSTATUS_ENDED; }

  [[nodiscard]] bool                            GetCanPause() const;
  [[nodiscard]] bool                            GetCanResume() const;
  [[nodiscard]] bool                            GetCanSave() const;

  [[nodiscard]] inline uint32_t                 GetLeftCode() const { return m_LeftCode; }
  [[nodiscard]] inline bool                     HasLeftReason() const { return !m_LeftReason.empty(); }
  [[nodiscard]] inline std::string              GetLeftReason() const { return m_LeftReason; }
  [[nodiscard]] inline std::string              GetName() const { return m_Name; }
  [[nodiscard]] std::string                     GetLowerName() const;
  [[nodiscard]] std::string                     GetDisplayName() const;
  [[nodiscard]] inline std::string              GetExtendedName() const { return m_Name + "@@@Virtual"; }

  [[nodiscard]] std::vector<uint8_t>            GetPlayerInfoBytes() const;
  [[nodiscard]] std::vector<uint8_t>            GetGameLoadedBytes() const;
  [[nodiscard]] std::vector<uint8_t>            GetGameQuitBytes(const uint8_t leftCode) const;

  inline void SetObserver(bool nObserver) { m_Observer = nObserver; }
  inline void SetLeftMessageSent(bool nLeftMessageSent) { m_LeftMessageSent = nLeftMessageSent; }
  inline void SetHasPlayerIntent(bool nHasPlayerIntent) { m_HasPlayerIntent = nHasPlayerIntent; }

  inline void SetStatus(uint8_t nStatus) { m_Status = nStatus; }
  inline void SetSID(uint8_t nSID) { m_SID = nSID; }
  inline void SetPseudonymUID(uint8_t nUID) { m_PseudonymUID = nUID; }

  inline void TrySetEnding() {
    if (m_Status != USERSTATUS_ENDED) {
      m_Status = USERSTATUS_ENDING;
    }
  }
  
  inline void ResetLeftReason() { m_LeftReason.clear(); }
  inline void SetLeftReason(const std::string& nLeftReason) { m_LeftReason = nLeftReason; }
  inline void SetLeftCode(uint32_t nLeftCode) { m_LeftCode = nLeftCode; }

  inline void DisableAllActions() { m_AllowedActions = VIRTUAL_USER_ALLOW_ACTIONS_NONE; }
  inline void DropRemainingSaves() { --m_RemainingSaves; }
  inline void SetRemainingSaves(uint8_t nCount) { m_RemainingSaves = nCount; }
  inline void SetCannotSave() { m_RemainingSaves = 0; }
  inline void DropRemainingPauses() { --m_RemainingPauses; }
  inline void SetCannotPause() { m_RemainingPauses = 0; }

  void RefreshUID();
};

#endif // AURA_VIRTUAL_USER_H_
