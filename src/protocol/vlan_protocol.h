#ifndef VLANPROTOCOL_H
#define VLANPROTOCOL_H

#include "../includes.h"

//
// CVirtualLanProtocol
//

namespace VLANProtocol
{
  namespace Magic
  {
    constexpr uint8_t SEARCHGAME                                     = 47u;  // 0x2F (UDP/LAN)
    constexpr uint8_t GAMEINFO                                       = 48u;  // 0x30 (UDP/LAN)
    constexpr uint8_t CREATEGAME                                     = 49u;  // 0x31 (UDP/LAN)
    constexpr uint8_t REFRESHGAME                                    = 50u;  // 0x32 (UDP/LAN)
    constexpr uint8_t DECREATEGAME                                   = 51u;  // 0x33 (UDP/LAN)

    constexpr uint8_t VLAN_HEADER                                    = 250u; // 0xFA
  };

  // receive functions

  [[nodiscard]] CIncomingVLanSearchGame RECEIVE_VLAN_SEARCHGAME(const std::vector<uint8_t>& data);
  [[nodiscard]] CIncomingVLanGameInfo* RECEIVE_VLAN_GAMEINFO(const std::vector<uint8_t>& data);

  // send functions

  [[nodiscard]] std::vector<uint8_t> SEND_VLAN_SEARCHGAME(bool TFT, uint32_t war3Version );
  [[nodiscard]] std::vector<uint8_t> SEND_VLAN_GAMEINFO(bool TFT, uint32_t war3Version, uint32_t mapGameType, uint32_t mapFlags, std::array<uint8_t, 2> mapWidth, std::array<uint8_t, 2> mapHeight, std::string gameName, std::string hostName, uint32_t elapsedTime, std::string mapPath, std::array<uint8_t, 4> mapCRC, uint32_t slotsTotal, uint32_t slotsOpen, std::array<uint8_t, 4> ip, uint16_t port, uint32_t hostCounter, uint32_t entryKey);
  [[nodiscard]] std::vector<uint8_t> SEND_VLAN_CREATEGAME(bool TFT, uint32_t war3Version, uint32_t hostCounter, std::array<uint8_t, 4> ip, uint16_t port);
  [[nodiscard]] std::vector<uint8_t> SEND_VLAN_REFRESHGAME(uint32_t hostCounter, uint32_t players, uint32_t playerSlots, std::array<uint8_t, 4> ip, uint16_t port);
  [[nodiscard]] std::vector<uint8_t> SEND_VLAN_DECREATEGAME(uint32_t hostCounter, std::array<uint8_t, 4> ip, uint16_t port);
};

//
// CIncomingVLanSearchGame
//

struct CIncomingVLanSearchGame
{
  bool isValid;
  bool isTFT;
  uint32_t gameVersion;

  CIncomingVLanSearchGame(bool nValid, bool nTFT, uint32_t nVersion)
   : isValid(nValid),
     isTFT(nTFT),
     gameVersion(nVersion)
  {};

  ~CIncomingVLanSearchGame() = default;
};

//
// CIncomingVLanGameInfo
//
struct CIncomingVLanGameInfo
{
private:
  bool m_TFT;
  uint32_t m_Version;
  uint32_t m_MapGameType;
  std::string m_GameName;
  std::vector<uint8_t> m_StatString;
  uint32_t m_ReceivedTime;
  uint32_t m_ElapsedTime;
  uint32_t m_SlotsTotal;
  uint32_t m_SlotsOpen;
  std::array<uint8_t, 4> m_IP;
  uint16_t m_Port;
  uint32_t m_HostCounter;
  uint32_t m_EntryKey;

  // decoded from stat string:

  uint32_t m_MapFlags;
  uint16_t m_MapWidth;
  uint16_t m_MapHeight;
  std::vector<uint8_t> m_MapCRC;
  std::string m_MapPath;
  std::string m_HostName;

public:
  CIncomingVLanGameInfo(bool nTFT, uint32_t nVersion, uint32_t nMapGameType, std::string nGameName, uint32_t nElapsedTime, uint32_t nSlotsTotal, uint32_t nSlotsOpen, const std::array<uint8_t, 4>& nIP, uint16_t nPort, uint32_t nHostCounter, uint32_t nEntryKey, const std::vector<uint8_t>& nStatString);
  ~CIncomingVLanGameInfo();

  [[nodiscard]] bool GetTFT( )                                        { return m_TFT; }
  [[nodiscard]] uint32_t GetVersion( )                                { return m_Version; }
  [[nodiscard]] uint32_t GetMapGameType( )                            { return m_MapGameType; }
  [[nodiscard]] uint32_t GetMapFlags( )                               { return m_MapFlags; }
  [[nodiscard]] uint16_t GetMapWidth( )                               { return m_MapWidth; }
  [[nodiscard]] uint16_t GetMapHeight( )                              { return m_MapHeight; }
  [[nodiscard]] const std::string& GetGameName( )                            { return m_GameName; }
  [[nodiscard]] const std::vector<uint8_t>& GetStatString( )                 { return m_StatString; }
  [[nodiscard]] const std::string& GetHostName( )                            { return m_HostName; }
  [[nodiscard]] uint32_t GetReceivedTime( )                           { return m_ReceivedTime; }
  [[nodiscard]] uint32_t GetElapsedTime( )                            { return m_ElapsedTime; }
  [[nodiscard]] const std::string& GetMapPath( )                             { return m_MapPath; }
  [[nodiscard]] const std::vector<uint8_t>& GetMapCRC( )                     { return m_MapCRC; }
  [[nodiscard]] uint32_t GetSlotsTotal( )                             { return m_SlotsTotal; }
  [[nodiscard]] uint32_t GetSlotsOpen( )                              { return m_SlotsOpen; }
  [[nodiscard]] const std::array<uint8_t, 4>& GetIP( )                         { return m_IP; }
  [[nodiscard]] uint16_t GetPort( )                                   { return m_Port; }
  [[nodiscard]] uint32_t GetHostCounter( )                            { return m_HostCounter; }
  [[nodiscard]] uint32_t GetEntryKey( )                               { return m_EntryKey; }

  void SetElapsedTime( uint32_t nElapsedTime )          { m_ElapsedTime = nElapsedTime; }
  void SetSlotsTotal( uint32_t nSlotsTotal )            { m_SlotsTotal = nSlotsTotal; }
  void SetSlotsOpen( uint32_t nSlotsOpen )              { m_SlotsOpen = nSlotsOpen; }
};
#endif
