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

#include <algorithm>
#include <utility>

#include "game_protocol.h"
#include <crc32/crc32.h>
#include "../util.h"
#include "../file_util.h"
#include "../game_user.h"
#include "../game_slot.h"
#include "../game.h"

using namespace std;

namespace GameProtocol
{
  const vector<uint8_t>& GetEmptyAction()
  {
    return EmptyAction;
  }

  ///////////////////////
  // RECEIVE FUNCTIONS //
  ///////////////////////

  CIncomingJoinRequest* RECEIVE_W3GS_REQJOIN(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_REQJOIN" );
    // DEBUG_Print( data );

    // 2 bytes                    -> Header
    // 2 bytes                    -> Length
    // 4 bytes                    -> Host Counter (Game ID)
    // 4 bytes                    -> Entry Key (used in LAN)
    // 1 byte                     -> ???
    // 2 bytes                    -> Listen Port
    // 4 bytes                    -> Peer Key
    // null terminated string			-> Name
    // 4 bytes                    -> ???
    // 2 bytes                    -> InternalPort (???)
    // 4 bytes                    -> InternalIP

    if (ValidateLength(data) && data.size() >= 20) {
      const uint32_t             HostCounter = ByteArrayToUInt32(data, false, 4);
      const uint32_t             EntryKey    = ByteArrayToUInt32(data, false, 8);
      const std::vector<uint8_t> RawName     = ExtractCString(data, 19);

      if (!RawName.empty() && data.size() >= RawName.size() + 30) {
        std::array<uint8_t, 4> InternalIP = {0, 0, 0, 0};
        copy_n(data.begin() + RawName.size() + 26, 4, InternalIP.begin());
        CIncomingJoinRequest* joinRequest = new CIncomingJoinRequest(HostCounter, EntryKey, string(begin(RawName), end(RawName)), InternalIP);
        return joinRequest;
      }
    }

    return nullptr;
  }

  uint32_t RECEIVE_W3GS_LEAVEGAME(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_LEAVEGAME" );
    // DEBUG_Print( data );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Reason

    if (ValidateLength(data) && data.size() >= 8)
      return ByteArrayToUInt32(data, false, 4);

    Print("W3GS_LEAVEGAME: " + ByteArrayToDecString(data));
    return 0;
  }

  bool RECEIVE_W3GS_GAMELOADED_SELF(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_GAMELOADED_SELF" );
    // DEBUG_Print( data );

    // 2 bytes					-> Header
    // 2 bytes					-> Length

    if (ValidateLength(data))
      return true;

    return false;
  }

  CIncomingAction RECEIVE_W3GS_OUTGOING_ACTION(const std::vector<uint8_t>& data, uint8_t UID)
  {
    // DEBUG_Print( "RECEIVED W3GS_OUTGOING_ACTION" );
    // DEBUG_Print( data );

    // 2 bytes                -> Header
    // 2 bytes                -> Length
    // 4 bytes                -> CRC
    // remainder of packet		-> Action

    /*const std::array<uint8_t, 4> CRC;
    copy_n(data.begin() + 4, 4, CRC.begin());*/
    std::vector<uint8_t> action = std::vector<uint8_t>(begin(data) + 8, end(data));
    return CIncomingAction(UID, action);
  }

  uint32_t RECEIVE_W3GS_OUTGOING_KEEPALIVE(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_OUTGOING_KEEPALIVE" );
    // DEBUG_Print( data );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 1 byte           -> ???
    // 4 bytes					-> CheckSum

    if (ValidateLength(data) && data.size() == 9)
      return ByteArrayToUInt32(data, false, 5);

    return 0;
  }

  CIncomingChatPlayer* RECEIVE_W3GS_CHAT_TO_HOST(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_CHAT_TO_HOST" );
    // DEBUG_Print( data );

    // 2 bytes              -> Header
    // 2 bytes              -> Length
    // 1 byte               -> Total
    // for( 1 .. Total )
    //		1 byte            -> ToUID
    // 1 byte               -> FromUID
    // 1 byte               -> Flag
    // if( Flag == 16 )
    //		null term string	-> Message
    // elseif( Flag == 17 )
    //		1 byte            -> Team
    // elseif( Flag == 18 )
    //		1 byte            -> Color
    // elseif( Flag == 19 )
    //		1 byte            -> Race
    // elseif( Flag == 20 )
    //		1 byte            -> Handicap
    // elseif( Flag == 32 )
    //		4 bytes           -> ExtraFlags
    //		null term string	-> Message

    if (ValidateLength(data))
    {
      uint32_t      i     = 5;
      const uint8_t Total = data[4];

      if (Total > 0 && data.size() >= i + Total)
      {
        const std::vector<uint8_t> ToUIDs = std::vector<uint8_t>(begin(data) + i, begin(data) + i + Total);
        i += Total;
        const uint8_t FromUID = data[i];
        const uint8_t Flag    = data[i + 1];
        i += 2;

        if (Flag == 16 && data.size() >= i + 1)
        {
          // chat message

          const std::vector<uint8_t> Message = ExtractCString(data, i);
          return new CIncomingChatPlayer(FromUID, ToUIDs, Flag, string(begin(Message), end(Message)));
        }
        else if ((Flag >= 17 && Flag <= 20) && data.size() >= i + 1)
        {
          // team/colour/race/handicap change request

          const uint8_t Byte = data[i];
          return new CIncomingChatPlayer(FromUID, ToUIDs, Flag, Byte);
        }
        else if (Flag == 32 && data.size() >= i + 5)
        {
          // chat message with extra flags

          const std::vector<uint8_t> ExtraFlags = std::vector<uint8_t>(begin(data) + i, begin(data) + i + 4);
          const std::vector<uint8_t> Message    = ExtractCString(data, i + 4);
          return new CIncomingChatPlayer(FromUID, ToUIDs, Flag, string(begin(Message), end(Message)), ExtraFlags);
        }
      }
    }

    return nullptr;
  }

  CIncomingMapSize* RECEIVE_W3GS_MAPSIZE(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_MAPSIZE" );
    // DEBUG_Print( data );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> ???
    // 1 byte           -> SizeFlag (1 = have map, 3 = continue download)
    // 4 bytes					-> MapSize

    if (ValidateLength(data) && data.size() >= 13)
      return new CIncomingMapSize(data[8], ByteArrayToUInt32(data, false, 9));

    return nullptr;
  }

  uint32_t RECEIVE_W3GS_PONG_TO_HOST(const std::vector<uint8_t>& data)
  {
    // DEBUG_Print( "RECEIVED W3GS_PONG_TO_HOST" );
    // DEBUG_Print( data );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Pong

    // the pong value is just a copy of whatever was sent in SEND_W3GS_PING_FROM_HOST which was GetTicks( ) at the time of sending
    // so as long as we trust that the client isn't trying to fake us out and mess with the pong value we can find the round trip time by simple subtraction
    // (the subtraction is done elsewhere because the very first pong value seems to be 1 and we want to discard that one)

    if (ValidateLength(data) && data.size() >= 8)
      return ByteArrayToUInt32(data, false, 4);

    return 1;
  }

  ////////////////////
  // SEND FUNCTIONS //
  ////////////////////

  std::vector<uint8_t> SEND_W3GS_PING_FROM_HOST()
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::PING_FROM_HOST, 8, 0};
    AppendByteArray(packet, GetTicks(), false); // ping value
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_REQJOIN(const uint32_t HostCounter, const uint32_t EntryKey, const std::string& Name)
  {
    const uint8_t              Zeros[]  = {0, 0, 0, 0};
    std::vector<uint8_t> packet;
    packet.push_back(GameProtocol::Magic::W3GS_HEADER);                    // W3GS header constant
    packet.push_back(GameProtocol::Magic::REQJOIN);            // W3GS_REQJOIN
    packet.push_back(0);                                       // packet length will be assigned later
    packet.push_back(0);                                       // packet length will be assigned later
    AppendByteArray(packet, HostCounter, false);               // game host counter
    AppendByteArray(packet, EntryKey, false);                  // game entry key
    packet.push_back(0);                                       //
    AppendByteArray(packet, static_cast<uint16_t>(6112), false);  
    AppendByteArray(packet, Zeros, 4);
    AppendByteArrayFast(packet, Name, true);
    AppendByteArray(packet, Zeros, 4);                          // ???
    AppendByteArray(packet, static_cast<uint16_t>(6112), true); //  
    AppendByteArray(packet, Zeros, 4);                          // ???
    AppendByteArray(packet, Zeros, 4);                          // ???
    AppendByteArray(packet, Zeros, 4);                          // ???
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_SLOTINFOJOIN(uint8_t UID, const std::array<uint8_t, 2>& port, const std::array<uint8_t, 4>& externalIP, const vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots)
  {
    std::vector<uint8_t> packet;

    const uint8_t              Zeros[]  = {0, 0, 0, 0};
    const std::vector<uint8_t> SlotInfo = EncodeSlotInfo(slots, randomSeed, layoutStyle, playerSlots);
    packet.push_back(GameProtocol::Magic::W3GS_HEADER);                    // W3GS header constant
    packet.push_back(GameProtocol::Magic::SLOTINFOJOIN);       // W3GS_SLOTINFOJOIN
    packet.push_back(0);                                       // packet length will be assigned later
    packet.push_back(0);                                       // packet length will be assigned later
    AppendByteArray(packet, static_cast<uint16_t>(SlotInfo.size()), false); // SlotInfo length
    AppendByteArrayFast(packet, SlotInfo);                     // SlotInfo
    packet.push_back(UID);                                     // UID
    packet.push_back(2);                                       // AF_INET
    packet.push_back(0);                                       // AF_INET continued...
    AppendByteArray(packet, port);                             // port
    AppendByteArrayFast(packet, externalIP);                   // external IP
    AppendByteArray(packet, Zeros, 4);                         // ???
    AppendByteArray(packet, Zeros, 4);                         // ???
    AssignLength(packet);

    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_REJECTJOIN(uint32_t reason)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::REJECTJOIN, 8, 0};
    AppendByteArray(packet, reason, false); // reason
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_PLAYERINFO(uint8_t UID, const string& name, const std::array<uint8_t, 4>& externalIP, const std::array<uint8_t, 4>& internalIP)
  {
    if (name.empty() || name.size() > MAX_PLAYER_NAME_SIZE) {
      Print("[GAMEPROTO] Invalid player name");
      return std::vector<uint8_t>();
    }

    std::vector<uint8_t> packet;

    const uint8_t PlayerJoinCounter[] = {2, 0, 0, 0};
    const uint8_t Zeros[]             = {0, 0, 0, 0};

    packet.push_back(GameProtocol::Magic::W3GS_HEADER);                         // W3GS header constant
    packet.push_back(GameProtocol::Magic::PLAYERINFO);              // W3GS_PLAYERINFO
    packet.push_back(0);                                            // packet length will be assigned later
    packet.push_back(0);                                            // packet length will be assigned later
    AppendByteArray(packet, PlayerJoinCounter, 4);                  // player join counter
    packet.push_back(UID);                                          // UID
    AppendByteArrayFast(packet, name);                              // player name
    packet.push_back(1);                                            // ???
    packet.push_back(0);                                            // ???
    packet.push_back(2);                                            // AF_INET
    packet.push_back(0);                                            // AF_INET continued...
    packet.push_back(0);                                            // port
    packet.push_back(0);                                            // port continued...
    AppendByteArrayFast(packet, externalIP);                        // external IP
    AppendByteArray(packet, Zeros, 4);                              // ???
    AppendByteArray(packet, Zeros, 4);                              // ???
    packet.push_back(2);                                            // AF_INET
    packet.push_back(0);                                            // AF_INET continued...
    packet.push_back(0);                                            // port
    packet.push_back(0);                                            // port continued...
    AppendByteArrayFast(packet, internalIP);                        // internal IP
    AppendByteArray(packet, Zeros, 4);                              // ???
    AppendByteArray(packet, Zeros, 4);                              // ???
    AssignLength(packet);

    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_PLAYERINFO_EXCLUDE_IP(uint8_t UID, const string& name)
  {
    if (name.empty() || name.size() > MAX_PLAYER_NAME_SIZE) {
      Print("[GAMEPROTO] Invalid player name");
      return std::vector<uint8_t>();
    }

    std::vector<uint8_t> packet;

    const uint8_t PlayerJoinCounter[] = {2, 0, 0, 0};
    const uint8_t Zeros[]             = {0, 0, 0, 0};

    packet.push_back(GameProtocol::Magic::W3GS_HEADER);                         // W3GS header constant
    packet.push_back(GameProtocol::Magic::PLAYERINFO);              // W3GS_PLAYERINFO
    packet.push_back(0);                                            // packet length will be assigned later
    packet.push_back(0);                                            // packet length will be assigned later
    AppendByteArray(packet, PlayerJoinCounter, 4);                  // player join counter
    packet.push_back(UID);                                          // UID
    AppendByteArrayFast(packet, name);                              // player name
    packet.push_back(1);                                            // ???
    packet.push_back(0);                                            // ???
    packet.push_back(2);                                            // AF_INET
    packet.push_back(0);                                            // AF_INET continued...
    packet.push_back(0);                                            // port
    packet.push_back(0);                                            // port continued...
    AppendByteArray(packet, Zeros, 4);	                            // external IP hidden
    AppendByteArray(packet, Zeros, 4);                              // ???
    AppendByteArray(packet, Zeros, 4);                              // ???
    packet.push_back(2);                                            // AF_INET
    packet.push_back(0);                                            // AF_INET continued...
    packet.push_back(0);                                            // port
    packet.push_back(0);                                            // port continued...
    AppendByteArray(packet, Zeros, 4);	                            // internal IP hidden
    AppendByteArray(packet, Zeros, 4);                              // ???
    AppendByteArray(packet, Zeros, 4);                              // ???
    AssignLength(packet);

    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_PLAYERLEAVE_OTHERS(uint8_t UID, uint32_t leftCode)
  {
    if (UID != 255)
    {
      std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::PLAYERLEAVE_OTHERS, 9, 0, UID};
      AppendByteArray(packet, leftCode, false); // left code (see PLAYERLEAVE_ constants in game_protocol.h)
      return packet;
    }

    Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_PLAYERLEAVE_OTHERS");
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> SEND_W3GS_GAMELOADED_OTHERS(uint8_t UID)
  {
    if (UID != 255)
      return std::vector<uint8_t>{GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::GAMELOADED_OTHERS, 5, 0, UID};

    Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_GAMELOADED_OTHERS");

    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> SEND_W3GS_SLOTINFO(vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots)
  {
    const std::vector<uint8_t> SlotInfo     = EncodeSlotInfo(slots, randomSeed, layoutStyle, playerSlots);
    const uint16_t             SlotInfoSize = static_cast<uint16_t>(SlotInfo.size());

    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::SLOTINFO, 0, 0};
    AppendByteArray(packet, SlotInfoSize, false); // SlotInfo length
    AppendByteArrayFast(packet, SlotInfo);        // SlotInfo
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_COUNTDOWN_START()
  {
    return std::vector<uint8_t>{GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::COUNTDOWN_START, 4, 0};
  }

  std::vector<uint8_t> SEND_W3GS_COUNTDOWN_END()
  {
    return std::vector<uint8_t>{GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::COUNTDOWN_END, 4, 0};
  }

  std::vector<uint8_t> SEND_W3GS_EMPTY_ACTIONS(uint32_t count)
  {
    std::vector<uint8_t> actions;
    actions.reserve(6 * count);
    while (count--) {
      AppendByteArrayFast(actions, GetEmptyAction());
    }
    return actions;
  }

  std::vector<uint8_t> SEND_W3GS_INCOMING_ACTION(const ActionQueue& actions, uint16_t sendInterval)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::INCOMING_ACTION, 0, 0};
    AppendByteArray(packet, sendInterval, false); // send interval

    // create subpacket

    if (!actions.empty())
    {
      std::vector<uint8_t> subpacket;
      auto it = actions.begin();
      while (it != actions.end()) {
        subpacket.push_back(it->GetUID());
        AppendByteArray(subpacket, static_cast<uint16_t>(it->GetImmutableAction().size()), false);
        AppendByteArrayFast(subpacket, it->GetImmutableAction());
        ++it;
      }

      // calculate crc (we only care about the lower 2 bytes though)
      uint32_t crc32 = CRC32::CalculateCRC((uint8_t*)string(begin(subpacket), end(subpacket)).c_str(), subpacket.size());

      // finish subpacket
      AppendByteArray(packet, static_cast<uint16_t>(crc32 & 0xFFFF), false);      // crc
      AppendByteArrayFast(packet, subpacket);                                     // subpacket
    }

    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_CHAT_FROM_HOST(uint8_t fromUID, const std::vector<uint8_t>& toUIDs, uint8_t flag, const std::vector<uint8_t>& flagExtra, const string& message)
  {
    if (!toUIDs.empty() && !message.empty() && message.size() < 255)
    {
      std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::CHAT_FROM_HOST, 0, 0, static_cast<uint8_t>(toUIDs.size())};
      AppendByteArrayFast(packet, toUIDs);    // receivers
      packet.push_back(fromUID);              // sender
      packet.push_back(flag);                 // flag
      AppendByteArrayFast(packet, flagExtra); // extra flag
      AppendByteArrayFast(packet, message);   // message
      AssignLength(packet);
      return packet;
    }

    Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_CHAT_FROM_HOST: \"" + message + "\"");
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> SEND_W3GS_START_LAG(vector<GameUser::CGameUser*> users)
  {
    if (users.empty()) {
      Print("[GAMEPROTO] no laggers passed to SEND_W3GS_START_LAG");
      return std::vector<uint8_t>();
    }

    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::START_LAG, 0u, 0u, static_cast<uint8_t>(users.size())};
    for (auto& player : users) {
      packet.push_back((player)->GetUID());
      AppendByteArray(packet, GetTicks() - player->GetStartedLaggingTicks(), false);
    }

    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_STOP_LAG(GameUser::CGameUser* user)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::STOP_LAG, 9, 0, user->GetUID()};
    AppendByteArray(packet, GetTicks() - user->GetStartedLaggingTicks(), false);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_GAMEINFO(const uint8_t war3Version, const uint32_t mapGameType, const uint32_t mapFlags, const std::vector<uint8_t>& mapWidth, const std::vector<uint8_t>& mapHeight, const string& gameName, const string& hostName, uint32_t upTime, const string& mapPath, const std::vector<uint8_t>& mapHash, uint32_t slotsTotal, uint32_t slotsAvailableOff, uint16_t port, uint32_t hostCounter, uint32_t entryKey)
  {
    if (mapWidth.size() != 2 || mapHeight.size() != 2) {
      Print("[GAMEPROTO] invalid dimensions passed to SEND_W3GS_GAMEINFO");
      return std::vector<uint8_t>();
    }
    if (gameName.empty() || hostName.empty() || mapPath.empty()) {
      Print("[GAMEPROTO] name/path not passed to SEND_W3GS_GAMEINFO");
      return std::vector<uint8_t>();
    }
    if (mapHash.size() != 4) {
      Print("[GAMEPROTO] invalid map hash passed to SEND_W3GS_GAMEINFO: " + ByteArrayToDecString(mapHash));
      return std::vector<uint8_t>();
    }

    const uint8_t Unknown2[] = {1, 0, 0, 0};

    // make the stat string

    std::vector<uint8_t> StatString;
    AppendByteArray(StatString, mapFlags, false);
    StatString.push_back(0);
    AppendByteArrayFast(StatString, mapWidth);
    AppendByteArrayFast(StatString, mapHeight);
    AppendByteArrayFast(StatString, mapHash);
    AppendByteArrayFast(StatString, mapPath);
    AppendByteArrayFast(StatString, hostName);
    StatString.push_back(0);
    StatString = EncodeStatString(StatString);

    // make the rest of the packet

    std::vector<uint8_t> packet = {
      GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::GAMEINFO, 0, 0,
      80, 88, 51, 87,
      war3Version, 0, 0, 0
    };
    AppendByteArray(packet, hostCounter, false);             // Host Counter
    AppendByteArray(packet, entryKey, false);                // Entry Key
    AppendByteArrayFast(packet, gameName);                   // Game Name
    packet.push_back(0);                                     // ??? (maybe game password)
    AppendByteArrayFast(packet, StatString);                 // Stat String
    packet.push_back(0);                                     // Stat String null terminator (the stat string is encoded to remove all even numbers i.e. zeros)
    AppendByteArray(packet, slotsTotal, false);              // Slots Total
    AppendByteArray(packet, mapGameType, false);             // Game Type
    AppendByteArray(packet, Unknown2, 4);                    // ???
    //AppendByteArray(packet, slotsTaken, false);            // Slots Taken again??
    AppendByteArray(packet, slotsAvailableOff, false);       // Slots Available off-by-one
    AppendByteArray(packet, upTime, false);                  // time since creation
    AppendByteArray(packet, port, false);                    // port
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_GAMEINFO_TEMPLATE(uint16_t* gameVersionOffset, uint16_t* dynamicInfoOffset, const uint32_t mapGameType, const uint32_t mapFlags, const std::array<uint8_t, 2>& mapWidth, const std::array<uint8_t, 2>& mapHeight, const string& gameName, const string& hostName, const string& mapPath, const std::array<uint8_t, 4>& mapHash, uint32_t slotsTotal, uint32_t hostCounter, uint32_t entryKey)
  {
    if (gameName.empty() || hostName.empty() || mapPath.empty()) {
      Print("[GAMEPROTO] name/path not passed to SEND_W3GS_GAMEINFO");
      return std::vector<uint8_t>();
    }

    const uint8_t Unknown2[] = {1, 0, 0, 0};
    const uint8_t Zeros[] = {0, 0, 0, 0};

    // make the stat string

    std::vector<uint8_t> StatString;
    AppendByteArray(StatString, mapFlags, false);
    StatString.push_back(0);
    AppendByteArrayFast(StatString, mapWidth);
    AppendByteArrayFast(StatString, mapHeight);
    AppendByteArrayFast(StatString, mapHash);
    AppendByteArrayFast(StatString, mapPath);
    AppendByteArrayFast(StatString, hostName);
    StatString.push_back(0);
    StatString = EncodeStatString(StatString);

    // make the rest of the packet

    std::vector<uint8_t> packet = {
      GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::GAMEINFO, 0, 0,
      80, 88, 51, 87
    };
    *gameVersionOffset = static_cast<uint16_t>(packet.size());       // Game version
    AppendByteArray(packet, Zeros, 4);
    AppendByteArray(packet, hostCounter, false);                     // Host Counter
    AppendByteArray(packet, entryKey, false);                        // Entry Key
    AppendByteArrayFast(packet, gameName);                           // Game Name
    packet.push_back(0);                                             // ??? (maybe game password)
    AppendByteArrayFast(packet, StatString);                         // Stat String
    packet.push_back(0);                                             // Stat String null terminator (the stat string is encoded to remove all even numbers i.e. zeros)
    AppendByteArray(packet, slotsTotal, false);                      // Slots Total
    AppendByteArray(packet, mapGameType, false);                     // Game Type
    AppendByteArray(packet, Unknown2, 4);                            // ???
    *dynamicInfoOffset = static_cast<uint16_t>(packet.size());       // TCP port
    AppendByteArray(packet, Zeros, 4);                               // Slots Available off-by-one
    AppendByteArray(packet, Zeros, 4);                               // time since creation
    packet.push_back(0);
    packet.push_back(0);
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_CREATEGAME(const uint8_t war3Version, const uint32_t hostCounter)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::CREATEGAME, 16, 0, 80, 88, 51, 87, war3Version, 0, 0, 0};
    AppendByteArray(packet, hostCounter, false); // Host Counter
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_REFRESHGAME(const uint32_t hostCounter, const uint32_t players, const uint32_t playerSlots)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::REFRESHGAME, 16, 0};
    AppendByteArray(packet, hostCounter, false); // Host Counter
    AppendByteArray(packet, players, false);     // Players
    AppendByteArray(packet, playerSlots, false); // Player Slots
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_DECREATEGAME(const uint32_t hostCounter)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::DECREATEGAME, 8, 0};
    AppendByteArray(packet, hostCounter, false); // Host Counter
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_MAPCHECK(const string& mapPath, const std::array<uint8_t, 4>& mapSize, const std::array<uint8_t, 4>& mapCRC32, const std::array<uint8_t, 4>& mapHash)
  {
    if (mapPath.empty()) {
      Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_MAPCHECK");
      return std::vector<uint8_t>();  
    }

    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::MAPCHECK, 0, 0, 1, 0, 0, 0};
    AppendByteArrayFast(packet, mapPath); // map path
    AppendByteArrayFast(packet, mapSize); // map size
    AppendByteArrayFast(packet, mapCRC32); // map info
    AppendByteArrayFast(packet, mapHash);  // map crc
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_MAPCHECK(const string& mapPath, const std::array<uint8_t, 4>& mapSize, const std::array<uint8_t, 4>& mapCRC32, const std::array<uint8_t, 4>& mapHash, const std::array<uint8_t, 20>& mapSHA1)
  {
    if (mapPath.empty()) {
      Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_MAPCHECK");
      return std::vector<uint8_t>();  
    }

    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::MAPCHECK, 0, 0, 1, 0, 0, 0};
    AppendByteArrayFast(packet, mapPath); // map path
    AppendByteArrayFast(packet, mapSize); // map size
    AppendByteArrayFast(packet, mapCRC32); // map info
    AppendByteArrayFast(packet, mapHash);  // map crc
    AppendByteArrayFast(packet, mapSHA1); // map sha1
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_STARTDOWNLOAD(uint8_t fromUID)
  {
    return std::vector<uint8_t>{GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::STARTDOWNLOAD, 9, 0, 1, 0, 0, 0, fromUID};
  }

  std::vector<uint8_t> SEND_W3GS_MAPPART(uint8_t fromUID, uint8_t toUID, size_t start_abs, const FileChunkTransient& mapFileChunk)
  {
    if (mapFileChunk.start > start_abs || !mapFileChunk.bytes) {
      Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_MAPPART (L707)");
      return std::vector<uint8_t>();
    }

    size_t max_end_abs = mapFileChunk.start + mapFileChunk.bytes->size();
    if (max_end_abs <= start_abs) {
      Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_MAPPART (L707)");
      return std::vector<uint8_t>();
    }

    // calculate end position (don't send more than 1442 map bytes in one packet)
    size_t end_abs = start_abs + 1442;
    if (max_end_abs < end_abs) {
      end_abs = max_end_abs;
    }

    if (end_abs < start_abs) {
      Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_MAPPART (L718)");
      return std::vector<uint8_t>();
    }

    size_t start_rel = start_abs - mapFileChunk.start;
    size_t end_rel = end_abs - mapFileChunk.start;

    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::MAPPART, 0, 0, toUID, fromUID, 1, 0, 0, 0}; // 10 bytes
    AppendByteArray(packet, static_cast<uint32_t>(start_abs), false); // start position, 4 bytes

    // calculate crc

    const std::vector<uint8_t> crc32 = CreateByteArray(CRC32::CalculateCRC(mapFileChunk.bytes->data() + start_rel, (uint32_t)(end_rel - start_rel)), false);
    AppendByteArrayFast(packet, crc32);

    // map data

    const std::vector<uint8_t> data = CreateByteArray(mapFileChunk.bytes->data() + start_rel, (uint32_t)(end_rel - start_rel));
    AppendByteArrayFast(packet, data);
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_MAPPART(uint8_t fromUID, uint8_t toUID, size_t start, const SharedByteArray& mapFileContents)
  {
    if (!mapFileContents || mapFileContents->size() < start) {
      Print("[GAMEPROTO] invalid parameters passed to SEND_W3GS_MAPPART");
      return std::vector<uint8_t>();
    }

    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::MAPPART, 0, 0, toUID, fromUID, 1, 0, 0, 0};
    AppendByteArray(packet, static_cast<uint32_t>(start), false); // start position

    // calculate end position (don't send more than 1442 map bytes in one packet)

    uint32_t end = start + 1442;

    if (end > static_cast<uint32_t>(mapFileContents->size()) || end < start)
      end = static_cast<uint32_t>(mapFileContents->size());

    // calculate crc

    const std::vector<uint8_t> crc32 = CreateByteArray(CRC32::CalculateCRC(mapFileContents->data() + (size_t)(start), (uint32_t)(end - start)), false);
    AppendByteArrayFast(packet, crc32);

    // map data

    const std::vector<uint8_t> data = CreateByteArray(mapFileContents->data() + (size_t)(start), (uint32_t)(end - start));
    AppendByteArrayFast(packet, data);
    AssignLength(packet);
    return packet;
  }

  std::vector<uint8_t> SEND_W3GS_INCOMING_ACTION2(const ActionQueue& actions)
  {
    std::vector<uint8_t> packet = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::INCOMING_ACTION2, 0, 0, 0, 0};

    // create subpacket

    if (!actions.empty())
    {
      std::vector<uint8_t> subpacket;
      auto it = actions.begin();
      while (it != actions.end()) {
        subpacket.push_back(it->GetUID());
        AppendByteArray(subpacket, static_cast<uint16_t>(it->GetImmutableAction().size()), false);
        AppendByteArrayFast(subpacket, it->GetImmutableAction());
        ++it;
      }

      // calculate crc (we only care about the lower 2 bytes though)
      uint32_t crc32 = CRC32::CalculateCRC((uint8_t*)string(begin(subpacket), end(subpacket)).c_str(), subpacket.size());

      // finish subpacket
      AppendByteArray(packet, static_cast<uint16_t>(crc32 & 0xFFFF), false);      // crc
      AppendByteArrayFast(packet, subpacket);                                     // subpacket
    }

    AssignLength(packet);
    return packet;
  }

  /////////////////////
  // OTHER FUNCTIONS //
  /////////////////////

  std::vector<uint8_t> EncodeSlotInfo(const vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots)
  {
    std::vector<uint8_t> SlotInfo;
    SlotInfo.push_back(static_cast<uint8_t>(slots.size())); // number of slots

    for (auto& slot : slots)
      AppendByteArray(SlotInfo, slot.GetProtocolArray());

    AppendByteArray(SlotInfo, randomSeed, false); // random seed
    SlotInfo.push_back(layoutStyle);              // LayoutStyle (0 = melee, 1 = custom forces, 3 = custom forces + fixed player settings)
    SlotInfo.push_back(playerSlots);              // number of player slots (non observer)
    return SlotInfo;
  }

  vector<uint8_t> EmptyAction = {GameProtocol::Magic::W3GS_HEADER, GameProtocol::Magic::INCOMING_ACTION, 6, 0, 0, 0};
}

//
// CIncomingJoinRequest
//

CIncomingJoinRequest::CIncomingJoinRequest(uint32_t nHostCounter, uint32_t nEntryKey, string nName, std::array<uint8_t, 4> nIPv4Internal)
  : m_Censored(false),
    m_OriginalName(std::move(nName)),
    m_IPv4Internal(std::move(nIPv4Internal)),
    m_HostCounter(nHostCounter),
    m_EntryKey(nEntryKey)
{
  m_Name = m_OriginalName;
}

CIncomingJoinRequest::~CIncomingJoinRequest() = default;

void CIncomingJoinRequest::UpdateCensored(uint8_t unsafeNameHandler, const bool pipeConsideredHarmful)
{
  if (unsafeNameHandler == ON_UNSAFE_NAME_NONE) {
    return;
  }

  m_Name = CIncomingJoinRequest::CensorName(m_Name, pipeConsideredHarmful);
  m_Censored = m_Name.size() != m_OriginalName.size();
}

string CIncomingJoinRequest::CensorName(const std::string& originalName, const bool pipeConsideredHarmful)
{
  string name = originalName;

  // Note: Do not always ban |, since it's used for so-called barcode names in Battle.net
  unordered_set<char> charsToRemoveAnyWhere = {
    // Characters used in commands
    ',', '@',

    // TAB, LF, CR, FF
    '\t', '\n', '\r', '\f',

    // NULL, beep, BS, ESC, DEL, 
    '\x00', '\x07', '\x08', '\x1B', '\x7F'
  };
  if (pipeConsideredHarmful) {
    charsToRemoveAnyWhere.insert('|');
  }
  unordered_set<char> charsToRemoveConditional = {
    '[', ']'
  };

  // # only needs to be banned from names' start
  // In particular, it must NOT be banned in trailing #\d+ patterns,
  // because they represent Battle Tags
  unordered_set<char> charsToRemoveStart = {'#', ' '};
  unordered_set<char> charsToRemoveEnd = {' ', '.'};

  name.erase(
    std::remove_if(
      name.begin(), name.end(), 
      [&charsToRemoveAnyWhere](const char& c) {
         return charsToRemoveAnyWhere.find(c) != charsToRemoveAnyWhere.end();
      }
    ),
    name.end()
  );

  // Ensure brackets are balanced
  // I'd rather blanket ban them, but they may be in use as clan markers
  {
    bool balancedBrackets = true;
    uint8_t bracketDepth = 0;
    for (char ch : name) {
      if (ch == '[') {
        ++bracketDepth;
      } else if (ch == ']') {
        if (bracketDepth <= 0) {
          balancedBrackets = false;
          break;
        } else {
          --bracketDepth;
        }
      }
    }
    if (!balancedBrackets) {
      name.erase(
        std::remove_if(
          name.begin(), name.end(), 
          [&charsToRemoveConditional](const char& c) {
             return charsToRemoveConditional.find(c) != charsToRemoveConditional.end();
          }
        ),
        name.end()
      );
    }
  }

  // Remove bad leading characters (operators, and whitespace)
  {
    auto it = std::find_if(
     name.begin(), name.end(), 
     [&charsToRemoveStart](const char& c) {
         return charsToRemoveStart.find(c) == charsToRemoveStart.end();
     }
    );
    name.erase(name.begin(), it);
  }

  // Remove bad trailing characters (mainly whitespace)
  {
    auto it = std::find_if(
      name.rbegin(), name.rend(), 
      [&charsToRemoveEnd](const char& c) {
        return charsToRemoveEnd.find(c) == charsToRemoveEnd.end();
      }
    );
    name.erase(it.base(), name.end());
  }

  if (name == "Open" || name == "Closed" || name == "Abrir" || name == "Cerrado") {
    name.clear();
  }

  return name;
}

//
// CIncomingAction
//

CIncomingAction::CIncomingAction()
  : m_Action(vector<uint8_t>{0}),
    m_UID(0xFF)
{
}

CIncomingAction::CIncomingAction(uint8_t nUID, vector<uint8_t>& nAction)
  : m_Action(std::move(nAction)),
    m_UID(nUID)
{
}

CIncomingAction::CIncomingAction(uint8_t nUID, uint8_t nActionType)
  : m_Action(vector<uint8_t>{nActionType}),
    m_UID(nUID)
{
}

CIncomingAction::CIncomingAction(const CIncomingAction& other)
  : m_Action(other.m_Action),
    m_UID(other.m_UID)
{
}

CIncomingAction::CIncomingAction(CIncomingAction&& other) noexcept
  : m_Action(move(other.m_Action)),
    m_UID(other.m_UID)
{
}

CIncomingAction& CIncomingAction::operator=(CIncomingAction&& other) noexcept
{
  if (this != &other) {
    m_Action = std::move(other.m_Action);
    m_UID = other.m_UID;
  }
  return *this;
}

CIncomingAction::~CIncomingAction() = default;

//
// CIncomingChatPlayer
//

CIncomingChatPlayer::CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, string nMessage)
  : m_Message(std::move(nMessage)),
    m_Type(GameProtocol::ChatToHostType::CTH_MESSAGE),
    m_Byte(255),
    m_FromUID(nFromUID),
    m_Flag(nFlag),
    m_ToUIDs(std::move(nToUIDs))
{
}

CIncomingChatPlayer::CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, string nMessage, std::vector<uint8_t> nExtraFlags)
  : m_Message(std::move(nMessage)),
    m_Type(GameProtocol::ChatToHostType::CTH_MESSAGE),
    m_Byte(255),
    m_FromUID(nFromUID),
    m_Flag(nFlag),
    m_ToUIDs(std::move(nToUIDs)),
    m_ExtraFlags(std::move(nExtraFlags))
{
}

CIncomingChatPlayer::CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, uint8_t nByte)
  : m_Type(GameProtocol::ChatToHostType::CTH_TEAMCHANGE),
    m_Byte(nByte),
    m_FromUID(nFromUID),
    m_Flag(nFlag),
    m_ToUIDs(std::move(nToUIDs))
{
  if (nFlag == 17)
    m_Type = GameProtocol::ChatToHostType::CTH_TEAMCHANGE;
  else if (nFlag == 18)
    m_Type = GameProtocol::ChatToHostType::CTH_COLOURCHANGE;
  else if (nFlag == 19)
    m_Type = GameProtocol::ChatToHostType::CTH_RACECHANGE;
  else if (nFlag == 20)
    m_Type = GameProtocol::ChatToHostType::CTH_HANDICAPCHANGE;
}

CIncomingChatPlayer::~CIncomingChatPlayer() = default;

//
// CIncomingMapSize
//

CIncomingMapSize::CIncomingMapSize(uint8_t nSizeFlag, uint32_t nMapSize)
  : m_MapSize(nMapSize),
    m_SizeFlag(nSizeFlag)
{
}

CIncomingMapSize::~CIncomingMapSize() = default;
