/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
7
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

#ifndef AURA_GAMEPROTOCOL_H_
#define AURA_GAMEPROTOCOL_H_

#include "../includes.h"
#include "../game_slot.h"

namespace GameProtocol
{
  namespace Magic
  {
    constexpr uint8_t ZERO               = 0u;  // 0x00
    constexpr uint8_t PING_FROM_HOST     = 1u;  // 0x01
    constexpr uint8_t SLOTINFOJOIN       = 4u;  // 0x04
    constexpr uint8_t REJECTJOIN         = 5u;  // 0x05
    constexpr uint8_t PLAYERINFO         = 6u;  // 0x06
    constexpr uint8_t PLAYERLEAVE_OTHERS = 7u;  // 0x07
    constexpr uint8_t GAMELOADED_OTHERS  = 8u;  // 0x08
    constexpr uint8_t SLOTINFO           = 9u;  // 0x09
    constexpr uint8_t COUNTDOWN_START    = 10u; // 0x0A
    constexpr uint8_t COUNTDOWN_END      = 11u; // 0x0B
    constexpr uint8_t INCOMING_ACTION    = 12u; // 0x0C
    constexpr uint8_t CHAT_FROM_HOST     = 15u; // 0x0F
    constexpr uint8_t START_LAG          = 16u; // 0x10
    constexpr uint8_t STOP_LAG           = 17u; // 0x11
    constexpr uint8_t HOST_KICK_PLAYER   = 28u; // 0x1C
    constexpr uint8_t REQJOIN            = 30u; // 0x1E
    constexpr uint8_t LEAVEGAME          = 33u; // 0x21
    constexpr uint8_t GAMELOADED_SELF    = 35u; // 0x23
    constexpr uint8_t OUTGOING_ACTION    = 38u; // 0x26
    constexpr uint8_t OUTGOING_KEEPALIVE = 39u; // 0x27
    constexpr uint8_t CHAT_TO_HOST       = 40u; // 0x28
    constexpr uint8_t DROPREQ            = 41u; // 0x29
    constexpr uint8_t SEARCHGAME         = 47u; // 0x2F (UDP/LAN)
    constexpr uint8_t GAMEINFO           = 48u; // 0x30 (UDP/LAN)
    constexpr uint8_t CREATEGAME         = 49u; // 0x31 (UDP/LAN)
    constexpr uint8_t REFRESHGAME        = 50u; // 0x32 (UDP/LAN)
    constexpr uint8_t DECREATEGAME       = 51u; // 0x33 (UDP/LAN)
    constexpr uint8_t CHAT_OTHERS        = 52u; // 0x34
    constexpr uint8_t PING_FROM_OTHERS   = 53u; // 0x35
    constexpr uint8_t PONG_TO_OTHERS     = 54u; // 0x36
    constexpr uint8_t MAPCHECK           = 61u; // 0x3D
    constexpr uint8_t STARTDOWNLOAD      = 63u; // 0x3F
    constexpr uint8_t MAPSIZE            = 66u; // 0x42
    constexpr uint8_t MAPPART            = 67u; // 0x43
    constexpr uint8_t MAPPARTNOTOK       = 69u; // 0x45 - just a guess, received this packet after forgetting to send a crc in MAPPART (f7 45 0a 00 01 02 01 00 00 00)
    constexpr uint8_t PONG_TO_HOST       = 70u; // 0x46
    constexpr uint8_t INCOMING_ACTION2   = 72u; // 0x48 - received this packet when there are too many actions to fit in W3GS_INCOMING_ACTION

    // Orthogonal to above
    constexpr uint8_t W3GS_HEADER        = 247u;// 0xF7
    constexpr uint8_t W3FW_HEADER        = 249u;// 0xF9
  };

  enum class ChatToHostType : uint8_t
  {
    CTH_MESSAGE        = 0u, // a chat message
    CTH_MESSAGEEXTRA   = 1u, // a chat message with extra flags
    CTH_TEAMCHANGE     = 2u, // a team change request
    CTH_COLOURCHANGE   = 3u, // a colour change request
    CTH_RACECHANGE     = 4u, // a race change request
    CTH_HANDICAPCHANGE = 5u, // a handicap change request
  };

  extern std::vector<uint8_t> EmptyAction;

  // utility functions

  [[nodiscard]] const std::vector<uint8_t>& GetEmptyAction();

  // receive functions

  [[nodiscard]] CIncomingJoinRequest* RECEIVE_W3GS_REQJOIN(const std::vector<uint8_t>& data);
  [[nodiscard]] uint32_t RECEIVE_W3GS_LEAVEGAME(const std::vector<uint8_t>& data);
  [[nodiscard]] bool RECEIVE_W3GS_GAMELOADED_SELF(const std::vector<uint8_t>& data);
  [[nodiscard]] CIncomingAction RECEIVE_W3GS_OUTGOING_ACTION(const std::vector<uint8_t>& data, uint8_t UID);
  [[nodiscard]] uint32_t RECEIVE_W3GS_OUTGOING_KEEPALIVE(const std::vector<uint8_t>& data);
  [[nodiscard]] CIncomingChatPlayer* RECEIVE_W3GS_CHAT_TO_HOST(const std::vector<uint8_t>& data);
  [[nodiscard]] CIncomingMapSize* RECEIVE_W3GS_MAPSIZE(const std::vector<uint8_t>& data);
  [[nodiscard]] uint32_t RECEIVE_W3GS_PONG_TO_HOST(const std::vector<uint8_t>& data);

  // send functions

  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_PING_FROM_HOST();
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_REQJOIN(const uint32_t HostCounter, const uint32_t EntryKey, const std::string& Name);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_SLOTINFOJOIN(uint8_t UID, const std::array<uint8_t, 2>& port, const std::array<uint8_t, 4>& externalIP, const std::vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_REJECTJOIN(uint32_t reason);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_PLAYERINFO(uint8_t UID, const std::string& name, const std::array<uint8_t, 4>& externalIP, const std::array<uint8_t, 4>& internalIP);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_PLAYERINFO_EXCLUDE_IP(uint8_t UID, const std::string& name);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_PLAYERLEAVE_OTHERS(uint8_t UID, uint32_t leftCode);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_GAMELOADED_OTHERS(uint8_t UID);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_SLOTINFO(std::vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_COUNTDOWN_START();
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_COUNTDOWN_END();
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_EMPTY_ACTIONS(uint32_t count);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_INCOMING_ACTION(const ActionQueue& actions, uint16_t sendInterval);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_INCOMING_ACTION2(const ActionQueue& actions);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_CHAT_FROM_HOST(uint8_t fromUID, const std::vector<uint8_t>& toUIDs, uint8_t flag, const std::vector<uint8_t>& flagExtra, const std::string& message);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_START_LAG(std::vector<GameUser::CGameUser*> users);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_STOP_LAG(GameUser::CGameUser* user);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_GAMEINFO(const uint8_t war3Version, const uint32_t mapGameType, const uint32_t mapFlags, const std::vector<uint8_t>& mapWidth, const std::vector<uint8_t>& mapHeight, const std::string& gameName, const std::string& hostName, uint32_t upTime, const std::string& mapPath, const std::vector<uint8_t>& mapHash, uint32_t slotsTotal, uint32_t slotsAvailableOff, uint16_t port, uint32_t hostCounter, uint32_t entryKey);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_GAMEINFO_TEMPLATE(uint16_t* gameVersionOffset, uint16_t* dynamicInfoOffset, const uint32_t mapGameType, const uint32_t mapFlags, const std::array<uint8_t, 2>& mapWidth, const std::array<uint8_t, 2>& mapHeight, const std::string& gameName, const std::string& hostName, const std::string& mapPath, const std::array<uint8_t, 4>& mapHash, uint32_t slotsTotal, uint32_t hostCounter, uint32_t entryKey);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_CREATEGAME(const uint8_t war3Version, const uint32_t hostCounter);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_REFRESHGAME(const uint32_t hostCounter, const uint32_t players, const uint32_t playerSlots);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_DECREATEGAME(const uint32_t hostCounter);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_MAPCHECK(const std::string& mapPath, const std::array<uint8_t, 4>& mapSize, const std::array<uint8_t, 4>& mapCRC32, const std::array<uint8_t, 4>& mapHash);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_MAPCHECK(const std::string& mapPath, const std::array<uint8_t, 4>& mapSize, const std::array<uint8_t, 4>& mapCRC32, const std::array<uint8_t, 4>& mapHash, const std::array<uint8_t, 20>& mapSHA1);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_STARTDOWNLOAD(uint8_t fromUID);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_MAPPART(uint8_t fromUID, uint8_t toUID, size_t start, const FileChunkTransient& mapFileChunk);
  [[nodiscard]] std::vector<uint8_t> SEND_W3GS_MAPPART(uint8_t fromUID, uint8_t toUID, size_t start, const SharedByteArray& mapFileContents);

  // other functions

  [[nodiscard]] std::vector<uint8_t> EncodeSlotInfo(const std::vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots);
  [[nodiscard]] inline std::string LeftCodeToString(const uint32_t leftCode)
  {
    switch (leftCode) {
      case PLAYERLEAVE_DISCONNECT:
        return "disconnect";
      case PLAYERLEAVE_LOST:
        return "lost";
      case PLAYERLEAVE_LOSTBUILDINGS:
        return "lost buildings";
      case PLAYERLEAVE_WON:
        return "won";
      case PLAYERLEAVE_DRAW:
        return "drew";
      case PLAYERLEAVE_OBSERVER:
        return "observer";
      case PLAYERLEAVE_LOBBY:
        return "from lobby";
      case PLAYERLEAVE_GPROXY:
        return "lost connection to their local GProxy instance";
      default:
        return "code " + std::to_string(leftCode);
    }
  }
};

//
// CIncomingJoinRequest
//

class CIncomingJoinRequest
{
private:
  bool                      m_Censored;
  std::string               m_Name;
  std::string               m_OriginalName;
  std::array<uint8_t, 4>    m_IPv4Internal;
  uint32_t                  m_HostCounter;
  uint32_t                  m_EntryKey;

public:
  CIncomingJoinRequest(uint32_t nHostCounter, uint32_t nEntryKey, std::string nName, std::array<uint8_t, 4> nIPv4Internal);
  ~CIncomingJoinRequest();

  [[nodiscard]] inline bool                   GetIsCensored() const { return m_Censored; }
  [[nodiscard]] inline uint32_t               GetHostCounter() const { return m_HostCounter; }
  [[nodiscard]] inline uint32_t               GetEntryKey() const { return m_EntryKey; }
  [[nodiscard]] inline std::string            GetName() const { return m_Name; }
  [[nodiscard]] inline std::string            GetOriginalName() const { return m_OriginalName; }
  [[nodiscard]] inline std::array<uint8_t, 4> GetIPv4Internal() const { return m_IPv4Internal; }

  void                                        UpdateCensored(uint8_t unsafeNameHandler, const bool pipeConsideredHarmful);
  [[nodiscard]] static std::string            CensorName(const std::string& originalName, const bool pipeConsideredHarmful);
};

//
// CIncomingAction
//

class CIncomingAction
{
private:
  std::vector<uint8_t>   m_Action;
  uint8_t                m_UID;

public:
  CIncomingAction();
  CIncomingAction(uint8_t nUID, std::vector<uint8_t>& nAction);
  CIncomingAction(uint8_t nUID, const uint8_t actionType);
  CIncomingAction(const CIncomingAction&);
  CIncomingAction(CIncomingAction&&) noexcept;
  CIncomingAction& operator=(const CIncomingAction&) = delete;
  CIncomingAction& operator=(CIncomingAction&&) noexcept;
  ~CIncomingAction();

  [[nodiscard]] inline uint8_t                       GetUID() const { return m_UID; }
  [[nodiscard]] inline const std::vector<uint8_t>&   GetImmutableAction() const { return m_Action; }
  [[nodiscard]] inline std::vector<uint8_t>&         GetAction() { return m_Action; }
  [[nodiscard]] inline uint8_t                       GetSniffedType() const { return m_Action.empty() ? GameProtocol::Magic::ZERO : m_Action[0]; }
  [[nodiscard]] inline size_t                        GetLength() const {
    size_t result = m_Action.size() + 3;
    return result < 3 ? m_Action.size() : result;
  }
};

//
// CIncomingChatPlayer
//

class CIncomingChatPlayer
{
public:
private:
  std::string                         m_Message;
  GameProtocol::ChatToHostType        m_Type;
  uint8_t                             m_Byte;
  uint8_t                             m_FromUID;
  uint8_t                             m_Flag;
  std::vector<uint8_t>                m_ToUIDs;
  std::vector<uint8_t>                m_ExtraFlags;

public:
  CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, std::string nMessage);
  CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, std::string nMessage, std::vector<uint8_t> nExtraFlags);
  CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, uint8_t nByte);
  ~CIncomingChatPlayer();

  [[nodiscard]] inline GameProtocol::ChatToHostType       GetType() const { return m_Type; }
  [[nodiscard]] inline uint8_t                            GetFromUID() const { return m_FromUID; }
  [[nodiscard]] inline const std::vector<uint8_t>&        GetToUIDs() const { return m_ToUIDs; }
  [[nodiscard]] inline uint8_t                            GetFlag() const { return m_Flag; }
  [[nodiscard]] inline std::string                        GetMessage() const { return m_Message; }
  [[nodiscard]] inline uint8_t                            GetByte() const { return m_Byte; }
  [[nodiscard]] inline const std::vector<uint8_t>&        GetExtraFlags() const { return m_ExtraFlags; }
};

class CIncomingMapSize
{
private:
  uint32_t m_MapSize;
  uint8_t  m_SizeFlag;

public:
  CIncomingMapSize(uint8_t nSizeFlag, uint32_t nMapSize);
  ~CIncomingMapSize();

  [[nodiscard]] inline uint8_t  GetSizeFlag() const { return m_SizeFlag; }
  [[nodiscard]] inline uint32_t GetMapSize() const { return m_MapSize; }
};

#endif // AURA_GAMEPROTOCOL_H_
