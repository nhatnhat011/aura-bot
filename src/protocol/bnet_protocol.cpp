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

#include "bnet_protocol.h"

#include "../util.h"

#include <algorithm>
#include <utility>

using namespace std;

namespace BNETProtocol
{
  ///////////////////////
  // RECEIVE FUNCTIONS //
  ///////////////////////

  bool RECEIVE_SID_ZERO(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_ZERO" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length

    return ValidateLength(packet);
  }

  CIncomingGameHost* RECEIVE_SID_GETADVLISTEX(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_GETADVLISTEX" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> GamesFound
    // if( GamesFound > 0 )
    //		10 bytes			-> ???
    //		2 bytes				-> Port
    //		4 bytes				-> IP
    //		null term string		-> GameName
    //		2 bytes				-> ???
    //		8 bytes				-> HostCounter

    if (ValidateLength(packet) && packet.size() >= 8)
    {
      if (ByteArrayToUInt32(packet, false, 4) > 0 && packet.size() >= 25)
      {
        uint16_t Port = ByteArrayToUInt16(packet, false, 18);

        array<uint8_t, 4> IP;
        copy_n(packet.begin() + 20, 4, IP.begin());

        const vector<uint8_t> GameName = ExtractCString(packet, 24);
        if (GameName.size() > 0xFF) return nullptr;

        if (packet.size() >= GameName.size() + 35) {
          array<uint8_t, 4> HostCounter = {
            ExtractHex(packet, static_cast<uint32_t>(GameName.size()) + 27, true),
            ExtractHex(packet, static_cast<uint32_t>(GameName.size()) + 29, true),
            ExtractHex(packet, static_cast<uint32_t>(GameName.size()) + 31, true),
            ExtractHex(packet, static_cast<uint32_t>(GameName.size()) + 33, true)
          };
          return new CIncomingGameHost(IP, Port, GameName, HostCounter);
        }
      }
    }

    return nullptr;
  }

  EnterChatResult RECEIVE_SID_ENTERCHAT(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_ENTERCHAT" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // null terminated string	-> UniqueName

    if (ValidateLength(packet) && packet.size() >= 5) {
      return EnterChatResult(true, (packet.data() + 4), (packet.data() + FindNullDelimiterOrEnd(packet, 4)));
    }
    return EnterChatResult(false, nullptr, nullptr);
  }

  IncomingChatResult RECEIVE_SID_CHATEVENT(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_CHATEVENT" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> EventID
    // 4 bytes					-> ???
    // 4 bytes					-> Ping
    // 12 bytes					-> ???
    // null terminated string	-> User
    // null terminated string	-> Message

    if (!ValidateLength(packet) || packet.size() < 29) {
      return IncomingChatResult(false, 0, nullptr, nullptr, nullptr, nullptr);
    }
    const uint32_t eventID = ByteArrayToUInt32(packet, false, 4);
    const size_t userEnd = FindNullDelimiterOrEnd(packet, 28);
    if (userEnd > 0xFF || packet.size() <= userEnd + 1) {
      return IncomingChatResult(false, 0, nullptr, nullptr, nullptr, nullptr);
    }
    const size_t messageStart = userEnd + 1;
    const size_t messageEnd = FindNullDelimiterOrEnd(packet, messageStart);
    return IncomingChatResult(
      true, eventID,
      (packet.data() + 28), (packet.data() + userEnd),
      (packet.data() + messageStart), (packet.data() + messageEnd)
    );
  }

  bool RECEIVE_SID_CHECKAD(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_CHECKAD" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length

    return ValidateLength(packet);
  }

  bool RECEIVE_SID_STARTADVEX3(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_STARTADVEX3" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Status

    if (ValidateLength(packet) && packet.size() >= 8)
    {
      const vector<uint8_t> Status = vector<uint8_t>(begin(packet) + 4, begin(packet) + 8);

      if (ByteArrayToUInt32(Status, false) == 0)
        return true;
    }

    return false;
  }

  array<uint8_t, 4> RECEIVE_SID_PING(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_PING" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Ping

    array<uint8_t, 4> value;
    if (ValidateLength(packet) && packet.size() >= 8) {
      copy_n(packet.begin() + 4, 4, value.begin());
    } else {
      value.fill(0);
    }
    return value;
  }

  AuthInfoResult RECEIVE_SID_AUTH_INFO(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_AUTH_INFO" );
    // DEBUG_Print( packet );

    // 2 bytes				    -> Header
    // 2 bytes				    -> Length
    // 4 bytes				    -> LogonType
    // 4 bytes				    -> ServerToken
    // 4 bytes				    -> ???
    // 8 bytes				    -> MPQFileTime
    // null terminated string	    -> IX86VerFileName
    // null terminated string	    -> ValueStringFormula

    if (!ValidateLength(packet) || packet.size() < 25) {
      return AuthInfoResult(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    size_t fileNameEndPos = FindNullDelimiterOrEnd(packet, 24);
    if (fileNameEndPos >= packet.size() || fileNameEndPos > 0xFFFFFF18) {
      return AuthInfoResult(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    return AuthInfoResult(true, packet.data() + 4 /* 4 bytes */, packet.data() + 8 /* 4 bytes */, packet.data() + 16 /* 8 bytes */, packet.data() + 24, (packet.data() + fileNameEndPos), (packet.data() + fileNameEndPos + 1), (packet.data() + FindNullDelimiterOrEnd(packet, fileNameEndPos + 1)));
  }

  AuthCheckResult RECEIVE_SID_AUTH_CHECK(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_AUTH_CHECK" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> KeyState
    // null terminated string	    -> KeyStateDescription

    if (ValidateLength(packet) && packet.size() >= 9) {
      return AuthCheckResult(ByteArrayToUInt32(packet, false, 4), (packet.data() + 8), (packet.data() + FindNullDelimiterOrEnd(packet, 8)));
    }
    return AuthCheckResult(BNETProtocol::KeyResult::BAD, nullptr, nullptr);
  }

  AuthLoginResult RECEIVE_SID_AUTH_ACCOUNTLOGON(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_AUTH_ACCOUNTLOGON" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Status
    // if( Status == 0 )
    //		32 bytes			-> Salt
    //		32 bytes			-> ServerPublicKey

    if (ValidateLength(packet) && packet.size() >= 8) {
      if (ByteArrayToUInt32(packet, false, 4) == 0 && packet.size() >= 72) {
        return AuthLoginResult(true, packet.data() + 8, packet.data() + 40);
      }
    }

    return AuthLoginResult(false, nullptr, nullptr);
  }

  bool RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_AUTH_ACCOUNTLOGONPROOF" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Status

    if (ValidateLength(packet) && packet.size() >= 8)
    {
      uint32_t Status = ByteArrayToUInt32(vector<uint8_t>(begin(packet) + 4, begin(packet) + 8), false);

      if (Status == 0 || Status == 0xE) {
        // OK: 0x0, EMAIL: 0xE
        return true;
      }
    }

    return false;
  }

  bool RECEIVE_SID_AUTH_ACCOUNTSIGNUP(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_AUTH_ACCOUNTSIGNUP" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> Status

    if (ValidateLength(packet) && packet.size() >= 8)
    {
      uint32_t Status = ByteArrayToUInt32(vector<uint8_t>(begin(packet) + 4, begin(packet) + 8), false);

      if (Status == 0x1) {
        return true;
      }
    }

    return false;
  }

  vector<string> RECEIVE_SID_FRIENDLIST(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_FRIENDSLIST" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 1 byte					    -> Total
    // for( 1 .. Total )
    //		null term string	-> Account
    //		1 byte				-> Status
    //		1 byte				-> Area
    //		4 bytes				-> ???
    //		null term string	-> Location

    vector<string> Friends;

    if (ValidateLength(packet) && packet.size() >= 5)
    {
      size_t   i     = 5;
      uint8_t  Total = packet[4];

      while (Total > 0)
      {
        --Total;

        if (packet.size() < i + 1)
          break;

        const vector<uint8_t> Account = ExtractCString(packet, i);
        i += Account.size() + 1;

        if (packet.size() < i + 7)
          break;

        i += 6;
        i += ExtractCString(packet, i).size() + 1;

        Friends.emplace_back(begin(Account), end(Account));
      }
    }

    return Friends;
  }

  vector<string> RECEIVE_SID_CLANMEMBERLIST(const vector<uint8_t>& packet)
  {
    // DEBUG_Print( "RECEIVED SID_CLANMEMBERLIST" );
    // DEBUG_Print( packet );

    // 2 bytes					-> Header
    // 2 bytes					-> Length
    // 4 bytes					-> ???
    // 1 byte					    -> Total
    // for( 1 .. Total )
    //		null term string	-> Name
    //		1 byte				-> Rank
    //		1 byte				-> Status
    //		null term string	-> Location

    vector<string> ClanList;

    if (ValidateLength(packet) && packet.size() >= 9)
    {
      size_t   i     = 9;
      uint8_t  Total = packet[8];

      while (Total > 0)
      {
        --Total;

        if (packet.size() < i + 1)
          break;

        const vector<uint8_t> Name = ExtractCString(packet, i);
        i += Name.size() + 1;

        if (packet.size() < i + 3)
          break;

        i += 2;

        // in the original VB source the location string is read but discarded, so that's what I do here

        i += ExtractCString(packet, i).size() + 1;
        ClanList.emplace_back(begin(Name), end(Name));
      }
    }

    return ClanList;
  }

  optional<CConfig> RECEIVE_HOSTED_GAME_CONFIG(const vector<uint8_t>& packet)
  {
    optional<CConfig> gameConfig;
    if (packet.size() < 64 || !ValidateLength(packet)) {
      Print("[BNETPROTO] RECEIVE_HOSTED_GAME_CONFIG bad packet size");
      return gameConfig;
    }
    const uint16_t slotInfoSize = ByteArrayToUInt16(packet, false, 44);
    if (static_cast<uint16_t>(packet.size()) < 44u + slotInfoSize) {
      Print("[BNETPROTO] RECEIVE_HOSTED_GAME_CONFIG bad slot packet size");
      return gameConfig;
    }
    const uint8_t maxSlots = packet[46];
    if (slotInfoSize != static_cast<uint16_t>(maxSlots) * 9 + 7) {
      Print("[BNETPROTO] RECEIVE_HOSTED_GAME_CONFIG bad slot count");
      return gameConfig;
    }
    gameConfig.emplace();
    gameConfig->SetUint32("rehost.unknown_1", ByteArrayToUInt32(packet, false, 4));
    gameConfig->SetUint32("rehost.unknown_2", ByteArrayToUInt32(packet, false, 8));

    vector<uint8_t> mapSize = vector<uint8_t>(4, 0);
    vector<uint8_t> mapCRC32 = vector<uint8_t>(4, 0);
    vector<uint8_t> mapWeakHash = vector<uint8_t>(4, 0);
    vector<uint8_t> rehostSeed = vector<uint8_t>(4, 0);
    vector<uint8_t> mapSHA1 = vector<uint8_t>(20, 0);

    copy_n(packet.begin() + 12, 4, mapSize.begin());
    gameConfig->SetUint8Vector("map.size", mapSize);

    copy_n(packet.begin() + 16, 4, mapCRC32.begin());
    gameConfig->SetUint8Vector("map.crc32", mapCRC32);

    copy_n(packet.begin() + 20, 4, mapWeakHash.begin());
    gameConfig->SetUint8Vector("map.weak_hash", mapWeakHash);

    copy_n(packet.begin() + 24, 20, mapSHA1.begin());
    gameConfig->SetUint8Vector("map.sha1", mapSHA1);

    uint16_t cursor = 44u;

    // skip 3 bytes - slot info size, max slots
    cursor += 3u;

    uint8_t slotIndex = 0;
    while (slotIndex < maxSlots) {
      vector<uint8_t> slotInfo = vector<uint8_t>(9, 0);
      copy_n(packet.begin() + cursor, 9, slotInfo.begin());
      gameConfig->SetUint8Vector("map.slot_" + ToDecString(slotIndex + 1), slotInfo);
      ++slotIndex;
      cursor += 9;
    }

    copy_n(packet.begin() + cursor, 4, rehostSeed.begin());
    cursor += 4;
    gameConfig->SetUint8Vector("rehost.game.seed", rehostSeed);

    const uint8_t mapLayout = packet[cursor];
    if (mapLayout > 3) {
      // It seems like this is sometimes 4?
      Print("[BNETPROTO] Map layout is unexpectedly " + ToDecString(mapLayout));
    }
    gameConfig->SetUint8("rehost.game.layout", mapLayout);
    cursor++;

    const uint8_t mapNumPlayers = packet[cursor];
    gameConfig->SetUint8("map.num_players", mapNumPlayers);
    cursor++;

    vector<uint8_t> mapClientPath = ExtractCString(packet, cursor);
    gameConfig->SetString("map.path", mapClientPath);

    cursor += static_cast<uint16_t>(mapClientPath.size()) + 1u;
    if (cursor >= packet.size()) {
      Print("[BNETPROTO] Missing hosted game name, etc.");
      return gameConfig;
    }

    gameConfig->SetBool("rehost.game.private", packet[cursor]);
    cursor += 1;

    if (cursor < packet.size()) {
      vector<uint8_t> gameName = ExtractCString(packet, cursor);
      gameConfig->SetString("rehost.game.name", gameName);
    }

    // TODO: RECEIVE_HOSTED_GAME_CONFIG game flags
    // Game flags are the first 4 bytes of the decoded stat string
    // In a PvPGN server, the stat string is stored (encoded) at t_game.info + 9
    // See src/bnetd/game_conv.cpp for decoding routines.
    //
    // Meanwhile, this should be enough as a PoC.
    if (maxSlots > mapNumPlayers) {
      gameConfig->SetUint8("map.observers", MAPOBS_REFEREES);
    } else {
      gameConfig->SetUint8("map.observers", MAPOBS_NONE);
    }

    // In a PVPGN server, stored at t_game.mapsize_x
    // Meanwhile, use GProxy values as placeholder
    vector<uint8_t> placeholderSize = {192, 7};
    gameConfig->SetUint8Vector("map.width", placeholderSize);

    // In a PVPGN server, stored at t_game.mapsize_y
    // Meanwhile, use GProxy values as placeholder
    gameConfig->SetUint8Vector("map.height", placeholderSize);
    return gameConfig;
  }

  ////////////////////
  // SEND FUNCTIONS //
  ////////////////////

  vector<uint8_t> SEND_PROTOCOL_INITIALIZE_SELECTOR()
  {
    return vector<uint8_t>{1};
  }

  vector<uint8_t> SEND_SID_ZERO()
  {
    return vector<uint8_t>{BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::ZERO, 4, 0};
  }

  vector<uint8_t> SEND_SID_STOPADV()
  {
    return vector<uint8_t>{BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::STOPADV, 4, 0};
  }

  vector<uint8_t> SEND_SID_GETADVLISTEX()
  {
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::GETADVLISTEX, 0, 0, /* short */ 0, 0, /* short */ 0, 0, /* unknown */ 0, 0, 0, 0, /* unknown */  0, 0, 0, 0};
    const uint8_t MaxGames[] = {255, 255, 255, 255};
    //const uint8_t GameName[] = {};
    //const uint8_t GamePassword[] = {};
    //const uint8_t GameStats[] = {};
    AppendByteArray(packet, MaxGames, 4);
    //AppendByteArray(packet, GameName, 0);     // Game Name
    packet.push_back(0);                        // Null terminator
    //AppendByteArray(packet, GamePassword, 0); // Game Password
    packet.push_back(0);                        // Null terminator
    //AppendByteArray(packet, GameStats, 0);    // Game Stats
    packet.push_back(0);                        // Null terminator
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_ENTERCHAT()
  {
    return vector<uint8_t>{BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::ENTERCHAT, 6, 0, 0, 0};
  }

  vector<uint8_t> SEND_SID_JOINCHANNEL(const string& channel)
  {
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::JOINCHANNEL, 0, 0};

    if (channel.size() > 0)
    {
      const uint8_t NoCreateJoin[] = {2, 0, 0, 0};
      AppendByteArray(packet, NoCreateJoin, 4); // flags for no create join
    }
    else
    {
      const uint8_t FirstJoin[] = {1, 0, 0, 0};
      AppendByteArray(packet, FirstJoin, 4); // flags for first join
    }

    AppendByteArrayFast(packet, channel);
    AssignLength(packet);

    return packet;
  }

  vector<uint8_t> SEND_SID_CHAT_PUBLIC(const string& message)
  {
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::CHATMESSAGE, 0, 0};
    AppendByteArrayFast(packet, message, true); // null-terminator
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_CHAT_PUBLIC(const vector<uint8_t>& message)
  {
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::CHATMESSAGE, 0, 0};
    AppendByteArrayFast(packet, message);
    packet.push_back(0);
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_CHAT_WHISPER(const string& message, const string& user)
  {
    // /w USER MESSAGE
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::CHATMESSAGE, 0, 0, 0x2f, 0x77, 0x20};
    AppendByteArrayFast(packet, user, false);
    packet.push_back(0x20);
    AppendByteArrayFast(packet, message, true); // With null terminator
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_CHAT_WHISPER(const vector<uint8_t>& message, const vector<uint8_t>& user)
  {
    // /w USER MESSAGE
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::CHATMESSAGE, 0, 0, 0x2f, 0x77, 0x20};
    AppendByteArrayFast(packet, user);
    packet.push_back(0x20);
    AppendByteArrayFast(packet, message);
    packet.push_back(0);
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_CHECKAD()
  {
    return vector<uint8_t>{BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::CHECKAD, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  }

  vector<uint8_t> SEND_SID_PUBLICHOST(const array<uint8_t, 4> address, uint16_t port)
  {
    vector<uint8_t> packet;

    const uint8_t Unknown[] = {2, 0};
    const uint8_t Unknown2[] = {0, 0, 0, 0};
    const uint8_t Unknown3[] = {0, 0, 0, 0};

    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                // BNET header constant
    packet.push_back(BNETProtocol::Magic::PUBLICHOST);// SID_PUBLICHOST
    packet.push_back(0);                                   //
    packet.push_back(0);                                   //
    AppendByteArray(packet, Unknown, 2);                   //
    AppendByteArray(packet, port, true);                   // Custom port
    AppendByteArrayFast(packet, address);                  // Custom IP
    AppendByteArray(packet, Unknown2, 4);                  //
    AppendByteArray(packet, Unknown3, 4);                  //
    AssignLength(packet);

    return packet;
  }

  vector<uint8_t> SEND_SID_STARTADVEX3(uint8_t state, const uint32_t mapGameType, const uint32_t mapFlags, const array<uint8_t, 2>& mapWidth, const array<uint8_t, 2>& mapHeight, const string& gameName, const string& hostName, uint32_t upTime, const string& mapPath, const array<uint8_t, 4>& mapCRC, const array<uint8_t, 20>& mapSHA1, uint32_t hostCounter, uint8_t maxSupportedSlots)
  {
    string HostCounterString = ToHexString(hostCounter);

    if (HostCounterString.size() < 8)
      HostCounterString.insert(static_cast<size_t>(0), static_cast<size_t>(8) - HostCounterString.size(), '0');

    HostCounterString = string(HostCounterString.rbegin(), HostCounterString.rend());

    vector<uint8_t> packet;

    // make the stat string

    vector<uint8_t> StatString;
    AppendByteArray(StatString, mapFlags, false);
    StatString.push_back(0);
    AppendByteArrayFast(StatString, mapWidth);
    AppendByteArrayFast(StatString, mapHeight);
    AppendByteArrayFast(StatString, mapCRC);
    AppendByteArrayFast(StatString, mapPath);
    AppendByteArrayFast(StatString, hostName);
    StatString.push_back(0);
    AppendByteArrayFast(StatString, mapSHA1);
    StatString = EncodeStatString(StatString);

    if (!gameName.empty() && !hostName.empty() && !mapPath.empty() && StatString.size() < 128 && HostCounterString.size() == 8)
    {
      // make the rest of the packet

      const uint8_t Unknown[]    = {255, 3, 0, 0};
      const uint8_t CustomGame[] = {0, 0, 0, 0};

      packet.push_back(BNETProtocol::Magic::BNET_HEADER);                // BNET header constant
      packet.push_back(BNETProtocol::Magic::STARTADVEX3);   // SID_STARTADVEX3
      packet.push_back(0);                                   // packet length will be assigned later
      packet.push_back(0);                                   // packet length will be assigned later
      packet.push_back(state);                               // State (16 = public, 17 = private, 18 = close)
      packet.push_back(0);                                   // State continued...
      packet.push_back(0);                                   // State continued...
      packet.push_back(0);                                   // State continued...
      AppendByteArray(packet, upTime, false);                // time since creation
      AppendByteArray(packet, mapGameType, false);           // Game Type, Parameter
      AppendByteArray(packet, Unknown, 4);                   // ???
      AppendByteArray(packet, CustomGame, 4);                // Custom Game
      AppendByteArrayFast(packet, gameName);                 // Game Name
      packet.push_back(0);                                   // Game Password is NULL
      packet.push_back(86 + maxSupportedSlots);              // Slots Free (ascii 98/110 = char b/n = 11/23 slots free) - note: do not reduce this as this is the # of UID's Warcraft III will allocate
      AppendByteArrayFast(packet, HostCounterString, false); // Host Counter - exclude null terminator
      AppendByteArrayFast(packet, StatString);               // Stat String
      packet.push_back(0);                                   // Stat String null terminator (the stat string is encoded to remove all even numbers i.e. zeros)
      AssignLength(packet);
    } else {
      Print("[BNETPROTO] invalid parameters passed to SEND_SID_STARTADVEX3");
    }

    return packet;
  }

  vector<uint8_t> SEND_SID_NOTIFYJOIN(const string& gameName)
  {
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::NOTIFYJOIN, 0, 0, 0, 0, 0, 0, 14, 0, 0, 0};
    AppendByteArrayFast(packet, gameName);                    // Game Name
    packet.push_back(0);                                      // Game Password is NULL
    AssignLength(packet);

    return packet;
  }

  vector<uint8_t> SEND_SID_PING(const array<uint8_t, 4>& pingValue)
  {
    vector<uint8_t> packet = {BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::PING, 0, 0};
    AppendByteArrayFast(packet, pingValue); // Ping Value
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_LOGONRESPONSE(const vector<uint8_t>& clientToken, const vector<uint8_t>& serverToken, const vector<uint8_t>& passwordHash, const string& accountName)
  {
    vector<uint8_t> packet;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                       // BNET header constant
    packet.push_back(BNETProtocol::Magic::LOGONRESPONSE);    // SID_LOGONRESPONSE
    packet.push_back(0);                                          // packet length will be assigned later
    packet.push_back(0);                                          // packet length will be assigned later
    AppendByteArrayFast(packet, clientToken);                     // Client Token
    AppendByteArrayFast(packet, serverToken);                     // Server Token
    AppendByteArrayFast(packet, passwordHash);                    // Password Hash
    AppendByteArrayFast(packet, accountName);                     // Account Name
    AssignLength(packet);

    return packet;
  }

  vector<uint8_t> SEND_SID_NETGAMEPORT(uint16_t serverPort)
  {
    vector<uint8_t> packet;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                       // BNET header constant
    packet.push_back(BNETProtocol::Magic::NETGAMEPORT);      // SID_NETGAMEPORT
    packet.push_back(0);                                          // packet length will be assigned later
    packet.push_back(0);                                          // packet length will be assigned later
    AppendByteArray(packet, serverPort, false);                   // local game server port
    AssignLength(packet);

    return packet;
  }

  vector<uint8_t> SEND_SID_AUTH_INFO(uint8_t ver, uint32_t localeID, const string& CountryShort, const string& country)
  {
    const uint8_t ProtocolID[]    = {0, 0, 0, 0};
    const uint8_t PlatformID[]    = {54, 56, 88, 73};              // "IX86"
    const uint8_t ProductID_TFT[] = {80, 88, 51, 87};              // "W3XP"
    const uint8_t Version[]       = {ver, 0, 0, 0};
    const uint8_t Language[]      = {83, 85, 110, 101};            // "enUS"
    const uint8_t LocalIP[]       = {127, 0, 0, 1};
    const uint8_t TimeZoneBias[]  = {60, 0, 0, 0};                 // 60 minutes (GMT +0100) but this is probably -0100

    vector<uint8_t> packet;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                        // BNET header constant
    packet.push_back(BNETProtocol::Magic::AUTH_INFO);         // SID_AUTH_INFO
    packet.push_back(0);                                           // packet length will be assigned later
    packet.push_back(0);                                           // packet length will be assigned later
    AppendByteArray(packet, ProtocolID, 4);                        // Protocol ID
    AppendByteArray(packet, PlatformID, 4);                        // Platform ID
    AppendByteArray(packet, ProductID_TFT, 4);                     // Product ID (TFT)
    AppendByteArray(packet, Version, 4);                           // Version
    AppendByteArray(packet, Language, 4);                          // Language (hardcoded as enUS to ensure battle.net sends the bot messages in English)
    AppendByteArray(packet, LocalIP, 4);                           // Local IP for NAT compatibility
    AppendByteArray(packet, TimeZoneBias, 4);                      // Time Zone Bias
    AppendByteArray(packet, localeID, false);                      // Locale ID
    AppendByteArray(packet, localeID, false);                      // Language ID (copying the locale ID should be sufficient since we don't care about sublanguages)
    AppendByteArrayFast(packet, CountryShort);                     // Country Abbreviation
    AppendByteArrayFast(packet, country);                          // Country
    AssignLength(packet);

    return packet;
  }

  vector<uint8_t> SEND_SID_AUTH_CHECK(const array<uint8_t, 4>& clientToken, const array<uint8_t, 4>& exeVersion, const array<uint8_t, 4>& exeVersionHash, const vector<uint8_t>& keyInfoROC, const vector<uint8_t>& keyInfoTFT, const string& exeInfo, const string& keyOwnerName)
  {
    vector<uint8_t> packet;
   uint32_t NumKeys = 2;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                         // BNET header constant
    packet.push_back(BNETProtocol::Magic::AUTH_CHECK);         // SID_AUTH_CHECK
    packet.push_back(0);                                            // packet length will be assigned later
    packet.push_back(0);                                            // packet length will be assigned later
    AppendByteArrayFast(packet, clientToken);                       // Client Token
    AppendByteArrayFast(packet, exeVersion);                        // EXE Version
    AppendByteArrayFast(packet, exeVersionHash);                    // EXE Version Hash
    AppendByteArray(packet, NumKeys, false);                        // number of keys in this packet
    AppendByteArray(packet, static_cast<uint32_t>(0), false);       // boolean Using Spawn (32 bit)
    AppendByteArrayFast(packet, keyInfoROC);                        // ROC Key Info
    AppendByteArrayFast(packet, keyInfoTFT);                        // TFT Key Info
    AppendByteArrayFast(packet, exeInfo);                           // EXE Info
    AppendByteArrayFast(packet, keyOwnerName);                      // CD Key Owner Name
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_AUTH_ACCOUNTLOGON(const array<uint8_t, 32>& clientPublicKey, const string& accountName)
  {
    vector<uint8_t> packet;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                          // BNET header constant
    packet.push_back(BNETProtocol::Magic::AUTH_ACCOUNTLOGON);   // SID_AUTH_ACCOUNTLOGON
    packet.push_back(0);                                             // packet length will be assigned later
    packet.push_back(0);                                             // packet length will be assigned later
    AppendByteArrayFast(packet, clientPublicKey);                    // Client Key
    AppendByteArrayFast(packet, accountName);                        // Account Name
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_AUTH_ACCOUNTLOGONPROOF(const array<uint8_t, 20>& clientPasswordProof)
  {
    vector<uint8_t> packet;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                              // BNET header constant
    packet.push_back(BNETProtocol::Magic::AUTH_ACCOUNTLOGONPROOF);  // SID_AUTH_ACCOUNTLOGONPROOF
    packet.push_back(0);                                                 // packet length will be assigned later
    packet.push_back(0);                                                 // packet length will be assigned later
    AppendByteArrayFast(packet, clientPasswordProof);                    // Client Password Proof
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_AUTH_ACCOUNTSIGNUP(const string& userName, const array<uint8_t, 20>& clientPasswordProof)
  {
    vector<uint8_t> packet;
    packet.push_back(BNETProtocol::Magic::BNET_HEADER);                              // BNET header constant
    packet.push_back(BNETProtocol::Magic::AUTH_ACCOUNTSIGNUP);      // SID_AUTH_ACCOUNTSIGNUP
    packet.push_back(0);                                                 // packet length will be assigned later
    packet.push_back(0);                                                 // packet length will be assigned later
    AppendByteArrayFast(packet, clientPasswordProof);                    // Client Password Proof
    AppendByteArrayFast(packet, userName);
    AssignLength(packet);
    return packet;
  }

  vector<uint8_t> SEND_SID_FRIENDLIST()
  {
    return vector<uint8_t>{BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::FRIENDLIST, 4, 0};
  }

  vector<uint8_t> SEND_SID_CLANMEMBERLIST()
  {
    return vector<uint8_t>{BNETProtocol::Magic::BNET_HEADER, BNETProtocol::Magic::CLANMEMBERLIST, 8, 0, 0, 0, 0, 0};
  }
};

//
// CIncomingGameHost
//

CIncomingGameHost::CIncomingGameHost(array<uint8_t, 4>& nIP, uint16_t nPort, const std::vector<uint8_t>& nGameName, array<uint8_t, 4>& nHostCounter)
  : m_GameName(string(begin(nGameName), end(nGameName))),
    m_IP(move(nIP)),
    m_HostCounter(move(nHostCounter)),
    m_Port(nPort)
{
}

CIncomingGameHost::~CIncomingGameHost() = default;

string CIncomingGameHost::GetIPString() const
{
  string Result;

  if (m_IP.size() >= 4)
  {
    for (uint32_t i = 0; i < 4; ++i)
    {
      Result += to_string(static_cast<uint32_t>(m_IP[i]));

      if (i < 3)
        Result += ".";
    }
  }

  return Result;
}
