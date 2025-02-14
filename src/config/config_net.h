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

#ifndef AURA_CONFIG_NET_H_
#define AURA_CONFIG_NET_H_

#include "../includes.h"
#include "config.h"

//
// CNetConfig
//

struct CNetConfig
{
  uint8_t                                 m_ProxyReconnect;             // whether to listen to GProxy++ reconnects
  sockaddr_storage                        m_BindAddress4;               // Defaults to 0.0.0.0
  sockaddr_storage                        m_BindAddress6;               // Defaults to ::
  uint16_t                                m_MinHostPort;                // the min port to host games on
  uint16_t                                m_MaxHostPort;                // the max port to host games on

  bool                                    m_UDPEnableCustomPortTCP4;    // enable to make IPv4 peers connect to m_UDPCustomPortTCP4
  uint16_t                                m_UDPCustomPortTCP4;          // the TCP port to broadcast over LAN, or to specific IPv4 clients
  bool                                    m_UDPEnableCustomPortTCP6;    // enable to make IPv6 peers connect to m_UDPCustomPortTCP6
  uint16_t                                m_UDPCustomPortTCP6;          // the TCP port to announce to IPv6 clients

  bool                                    m_EnableTCPWrapUDP;
  bool                                    m_VLANEnabled;
  uint16_t                                m_VLANPort;

  uint16_t                                m_UDP6TargetPort;             // the remote UDP port to which we send unicast game discovery messages over IPv6

  bool                                    m_UDPBroadcastStrictMode;     // set to false to send full game info periodically rather than small refresh packets
  bool                                    m_UDPForwardTraffic;          // whether to forward UDP traffic
  sockaddr_storage                        m_UDPForwardAddress;          // the address to forward UDP traffic to
  bool                                    m_UDPForwardGameLists;        // whether to forward PvPGN game lists through UDP unicast.
  bool                                    m_UDPBroadcastEnabled;        // whether to perform UDP broadcasts to announce hosted games. (unicast is in config_game)
  sockaddr_storage                        m_UDPBroadcastTarget;
  std::set<std::string>                   m_UDPBlockedIPs;              // list of IPs ignored by Aura's UDP server
  bool                                    m_UDPDoNotRouteEnabled;       // whether to enable SO_DONTROUTE for UDP sockets

  bool                                    m_AllowDownloads;             // allow map downloads or not
  uint32_t                                m_DownloadTimeout;
  std::set<std::string>                   m_MapRepositories;            // enabled map repositories
  uint8_t                                 m_AllowTransfers;             // map transfers mode
  uint32_t                                m_MaxDownloaders;             // maximum number of map downloaders at the same time
  uint32_t                                m_MaxUploadSize;              // maximum total map size that we may transfer to players in lobbies
  uint32_t                                m_MaxUploadSpeed;             // maximum total map upload speed in KB/sec
  uint32_t                                m_MaxParallelMapPackets;      // map pieces sent in parallel to downloading users
  bool                                    m_HasBufferBloat;

  int64_t                                 m_ReconnectWaitTicks;         // the maximum number of minutes to wait for a GProxyDLL reconnect
  int64_t                                 m_ReconnectWaitTicksLegacy;   // the maximum number of minutes to wait for a GProxy++ reconnect

  bool                                    m_AnnounceGProxy;
  std::string                             m_AnnounceGProxySite;
  bool                                    m_AnnounceIPv6;

  bool                                    m_EnableUPnP;
  uint8_t                                 m_PublicIPv4Algorithm;
  std::string                             m_PublicIPv4Value;
  uint8_t                                 m_PublicIPv6Algorithm;
  std::string                             m_PublicIPv6Value;

  bool                                    m_EnableGeoLocalization;

  // == SECTION START: Cannot be reloaded ==
  bool                                    m_UDPMainServerEnabled;
  bool                                    m_SupportTCPOverIPv6;
  bool                                    m_SupportUDPOverIPv6;
  uint16_t                                m_UDPFallbackPort;
  uint16_t                                m_UDPIPv6Port;
  bool                                    m_LiteralRTT;                  // legit, or use LC style pings (divides actual pings by two)
  bool                                    m_UseSystemRTT;                // uses getsockopt syscall to measure TCP latency, rather than trusting the game client
  // == SECTION END ==



  explicit CNetConfig(CConfig& CFG);
  ~CNetConfig();
};

#endif
