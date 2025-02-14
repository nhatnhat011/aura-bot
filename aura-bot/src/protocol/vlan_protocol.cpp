#include "vlan_protocol.h"

#include "../util.h"

using namespace std;

namespace VLANProtocol
{
  ///////////////////////
  // RECEIVE FUNCTIONS //
  ///////////////////////

  CIncomingVLanSearchGame RECEIVE_VLAN_SEARCHGAME(const vector<uint8_t>& data)
  {
    const static uint32_t ProductID_ROC  = 1463898675;  // "WAR3"
    const static uint32_t ProductID_TFT  = 1462982736;  // "W3XP"

    // DEBUG_Print( "RECEIVED VLAN_SEARCHGAME");
    // DEBUG_Print(data);

    // 2 bytes          -> Header
    // 2 bytes          -> Length
    // 4 bytes          -> ProductID
    // 4 bytes          -> Version

    if (ValidateLength(data) && data.size() >= 12) {
      uint32_t ProductID = ByteArrayToUInt32(data, false, 4);
      uint32_t Version = ByteArrayToUInt32(data, false, 8);
      bool TFT = false;

      switch (ProductID)
      {
        case ProductID_TFT:
          TFT = true;
          // fall through
        case ProductID_ROC:
          return CIncomingVLanSearchGame(true, TFT, Version);
      }
    }
    return CIncomingVLanSearchGame(false, false, 0);
  }

  CIncomingVLanGameInfo* RECEIVE_VLAN_GAMEINFO(const vector<uint8_t>& data)
  {
    const static uint32_t ProductID_ROC  = 1463898675;  // "WAR3"
    const static uint32_t ProductID_TFT  = 1462982736;  // "W3XP"

    // DEBUG_Print( "RECEIVED VLAN_GAMEINFO");
    // DEBUG_Print(data);

    // 2 bytes          -> Header
    // 2 bytes          -> Length
    // 4 bytes          -> ProductID
    // 4 bytes          -> Version
    // 4 bytes          -> HostCounter
    // 4 bytes          -> EntryKey
    // null term string      -> GameName
    // null term string      -> StatString
    // 1 byte          -> SlotsTotal
    // 4 bytes          -> GameType
    // 4 bytes          -> SlotsOpen
    // 4 bytes          -> ElapsedTime
    // 4 bytes          -> IP
    // 2 bytes          -> Port

    if (ValidateLength(data) && data.size() >= 16)
    {
      uint32_t ProductID = ByteArrayToUInt32(data, false, 4);
      uint32_t Version = ByteArrayToUInt32(data, false, 8);
      uint32_t HostCounter = ByteArrayToUInt32(data, false, 12);
      uint32_t EntryKey = ByteArrayToUInt32(data, false, 16);
      vector<uint8_t> GameName = ExtractCString(data, 20);
      vector<uint8_t> StatString = ExtractCString(data, 22 + GameName.size());
      int i = 23 + GameName.size() + StatString.size();
      uint32_t SlotsTotal = ByteArrayToUInt16(data, false, i);
      uint32_t MapGameType = ByteArrayToUInt32(data, false, i + 4);
      uint32_t SlotsOpen = ByteArrayToUInt32(data, false, i + 8);
      uint32_t ElapsedTime = ByteArrayToUInt32(data, false, i + 12);
      array<uint8_t, 4> IP;
      copy_n(data.begin() + i + 16, 4, IP.begin());
      uint16_t Port = ByteArrayToUInt16(data, false, i + 20);

      bool TFT;

      if (ProductID == ProductID_TFT )
        TFT = true;
      else if (ProductID == ProductID_ROC)
        TFT = false;
      else
        return nullptr;

      return new CIncomingVLanGameInfo(TFT, Version, MapGameType, string(GameName.begin(), GameName.end()), ElapsedTime, SlotsTotal, SlotsOpen, IP, Port, HostCounter, EntryKey, StatString);
    }

    return nullptr;
  }

  ////////////////////
  // SEND FUNCTIONS //
  ////////////////////

  vector<uint8_t> SEND_VLAN_SEARCHGAME(bool TFT, uint32_t war3Version)
  {
    unsigned char ProductID_ROC[]  = {51, 82, 65, 87};    // "WAR3"
    unsigned char ProductID_TFT[]  = {80, 88, 51, 87};    // "W3XP"

    vector<uint8_t> packet;
    packet.push_back(VLANProtocol::Magic::VLAN_HEADER);               // VLAN header constant
    packet.push_back(VLANProtocol::Magic::SEARCHGAME);    // VLAN_SEARCHGAME
    packet.push_back(0);                                  // packet length will be assigned later
    packet.push_back(0);                                  // packet length will be assigned later

    if (TFT )
      AppendByteArray(packet, ProductID_TFT, 4);          // Product ID (TFT)
    else
      AppendByteArray(packet, ProductID_ROC, 4);          // Product ID (ROC)

    AppendByteArray(packet, war3Version, false);          // Version
    AssignLength(packet);
    // DEBUG_Print("SENT W3GS_SEARCHGAME");
    // DEBUG_Print(packet);
    return packet;
  }

  vector<uint8_t> SEND_VLAN_GAMEINFO(bool TFT, uint32_t war3Version, uint32_t mapGameType, uint32_t mapFlags, array<uint8_t, 2> mapWidth, array<uint8_t, 2> mapHeight, string gameName, string hostName, uint32_t elapsedTime, string mapPath, array<uint8_t, 4> mapCRC, uint32_t slotsTotal, uint32_t slotsOpen, array<uint8_t, 4> ip, uint16_t port, uint32_t hostCounter, uint32_t entryKey)
  {
    unsigned char ProductID_ROC[]  = {51, 82, 65, 87};    // "WAR3"
    unsigned char ProductID_TFT[]  = {80, 88, 51, 87};    // "W3XP"

    vector<uint8_t> packet;

    if (gameName.empty() || hostName.empty() || mapPath.empty() || mapCRC.size() != 4) {
      Print("[VLAN] invalid parameters passed to SEND_VLAN_GAMEINFO");
      return packet;
    }
    // make the stat string

    vector<uint8_t> StatString;
    AppendByteArray(StatString, mapFlags, false);
    StatString.push_back(0);
    AppendByteArray(StatString, mapWidth);
    AppendByteArray(StatString, mapHeight);
    AppendByteArrayFast(StatString, mapCRC);
    AppendByteArrayFast(StatString, mapPath);
    AppendByteArrayFast(StatString, hostName);
    StatString.push_back(0);
    StatString = EncodeStatString(StatString);

    // make the rest of the packet

    packet.push_back(VLANProtocol::Magic::VLAN_HEADER);               // VLAN header constant
    packet.push_back(VLANProtocol::Magic::GAMEINFO);      // VLAN_GAMEINFO
    packet.push_back(0);                                  // packet length will be assigned later
    packet.push_back(0);                                  // packet length will be assigned later

    if (TFT)
      AppendByteArray(packet, ProductID_TFT, 4);          // Product ID (TFT)
    else
      AppendByteArray(packet, ProductID_ROC, 4);          // Product ID (ROC)

    AppendByteArray(packet, war3Version, false);          // Version
    AppendByteArray(packet, hostCounter, false);          // Host Counter
    AppendByteArray(packet, entryKey, false);             // Entry Key
    AppendByteArrayFast(packet, gameName);                // Game Name
    packet.push_back(0);                                  // ??? (maybe game password)
    AppendByteArrayFast(packet, StatString);              // Stat String
    packet.push_back(0);                                  // Stat String null terminator (the stat string is encoded to remove all even numbers i.e. zeros)
    AppendByteArray(packet, slotsTotal, false);           // Slots Total
    AppendByteArray(packet, mapGameType, false);          // Map Game Type
    AppendByteArray(packet, slotsOpen, false);            // Slots Open
    AppendByteArray(packet, elapsedTime, false);          // time since creation
    AppendByteArrayFast(packet, ip);                      // ip
    AppendByteArray(packet, port, false);                 // port
    AssignLength(packet);

    // DEBUG_Print( "SENT VLAN_GAMEINFO");
    // DEBUG_Print(packet);
    return packet;
  }

  vector<uint8_t> SEND_VLAN_CREATEGAME(bool TFT, uint32_t war3Version, uint32_t hostCounter, array<uint8_t, 4> ip, uint16_t port)
  {
    unsigned char ProductID_ROC[]  = {51, 82, 65, 87};     // "WAR3"
    unsigned char ProductID_TFT[]  = {80, 88, 51, 87};     // "W3XP"

    vector<uint8_t> packet;
    packet.push_back(VLANProtocol::Magic::VLAN_HEADER);               // VLAN header constant
    packet.push_back(VLANProtocol::Magic::CREATEGAME);    // VLAN_CREATEGAME
    packet.push_back(0);                                  // packet length will be assigned later
    packet.push_back(0);                                  // packet length will be assigned later

    if (TFT )
      AppendByteArray(packet, ProductID_TFT, 4);          // Product ID (TFT)
    else
      AppendByteArray(packet, ProductID_ROC, 4);          // Product ID (ROC)

    AppendByteArray(packet, war3Version, false);          // Version
    AppendByteArray(packet, hostCounter, false);          // Host Counter
    AppendByteArrayFast(packet, ip);                      // IP - added by h3rmit
    AppendByteArray(packet, port, false);                 // Port - added by h3rmit
    AssignLength(packet);
    // DEBUG_Print("SENT VLAN_CREATEGAME");
    // DEBUG_Print(packet);
    return packet;
  }

  vector<uint8_t> SEND_VLAN_REFRESHGAME(uint32_t hostCounter, uint32_t players, uint32_t playerSlots, array<uint8_t, 4> ip, uint16_t port)
  {
    vector<uint8_t> packet;
    packet.push_back(VLANProtocol::Magic::VLAN_HEADER);               // VLAN header constant
    packet.push_back(VLANProtocol::Magic::REFRESHGAME);   // VLAN_REFRESHGAME
    packet.push_back(0);                                  // packet length will be assigned later
    packet.push_back(0);                                  // packet length will be assigned later
    AppendByteArray(packet, hostCounter, false);          // Host Counter
    AppendByteArray(packet, players, false);              // Players
    AppendByteArray(packet, playerSlots, false);          // Player Slots
    AppendByteArrayFast(packet, ip);                      // IP - added by h3rmit
    AppendByteArray(packet, port, false);                 // Port - added by h3rmit
    AssignLength(packet);
    // DEBUG_Print("SENT VLAN_REFRESHGAME");
    // DEBUG_Print(packet);
    return packet;
  }

  vector<uint8_t> SEND_VLAN_DECREATEGAME(uint32_t hostCounter, array<uint8_t, 4> ip, uint16_t port)
  {
    vector<uint8_t> packet;
    packet.push_back(VLANProtocol::Magic::VLAN_HEADER);               // VLAN header constant
    packet.push_back(VLANProtocol::Magic::DECREATEGAME);  // VLAN_DECREATEGAME
    packet.push_back(0);                                  // packet length will be assigned later
    packet.push_back(0);                                  // packet length will be assigned later
    AppendByteArray(packet, hostCounter, false);          // Host Counter
    AppendByteArrayFast(packet, ip);                      // IP - added by h3rmit
    AppendByteArray(packet, port, false);                 // Port - added by h3rmit
    AssignLength(packet);
    // DEBUG_Print("SENT VLAN_DECREATEGAME");
    // DEBUG_Print(packet);
    return packet;
  }
}

//
// CIncomingVLanGameInfo
//

CIncomingVLanGameInfo::CIncomingVLanGameInfo( bool nTFT, uint32_t nVersion, uint32_t nMapGameType, string nGameName, uint32_t nElapsedTime, uint32_t nSlotsTotal, uint32_t nSlotsOpen, const array<uint8_t, 4>& nIP, uint16_t nPort, uint32_t nHostCounter, uint32_t nEntryKey, const vector<uint8_t>& nStatString )
{
  m_TFT = nTFT;
  m_Version = nVersion;
  m_MapGameType = nMapGameType;
  m_StatString = nStatString;
  m_GameName = nGameName;
  m_ElapsedTime = nElapsedTime;
  m_SlotsTotal = nSlotsTotal;
  m_SlotsOpen = nSlotsOpen;
  m_IP = nIP;
  m_Port = nPort;
  m_HostCounter = nHostCounter;
  m_EntryKey = nEntryKey;
  m_ReceivedTime = GetTime();

  // decode stat string

  vector<uint8_t> StatString = DecodeStatString(m_StatString);
  vector<uint8_t> MapFlags;
  vector<uint8_t> MapWidth;
  vector<uint8_t> MapHeight;
  vector<uint8_t> MapCRC;
  vector<uint8_t> MapPath;
  vector<uint8_t> HostName;

  if (StatString.size() >= 14) {
    unsigned int i = 13;
    MapFlags = vector<uint8_t>(StatString.begin(), StatString.begin() + 4);
    MapWidth = vector<uint8_t>(StatString.begin() + 5, StatString.begin() + 7);
    MapHeight = vector<uint8_t>(StatString.begin() + 7, StatString.begin() + 9);
    MapCRC = vector<uint8_t>(StatString.begin() + 9, StatString.begin() + 13);
    MapPath = ExtractCString(StatString, 13);
    i += MapPath.size() + 1;

    m_MapFlags = ByteArrayToUInt32(MapFlags, false);
    m_MapWidth = ByteArrayToUInt16(MapWidth, false);
    m_MapHeight = ByteArrayToUInt16(MapHeight, false);
    m_MapCRC = MapCRC;
    m_MapPath = string(MapPath.begin(), MapPath.end());

    if (StatString.size() >= i + 1) {
      HostName = ExtractCString(StatString, i);
      m_HostName = string(HostName.begin(), HostName.end());
    }
  }
}

CIncomingVLanGameInfo::~CIncomingVLanGameInfo()
{
}
