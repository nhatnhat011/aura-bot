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

#include "config_net.h"
#include "../util.h"
#include "../net.h"

#include <utility>
#include <algorithm>
#include <fstream>

using namespace std;

//
// CNetConfig
//

CNetConfig::CNetConfig(CConfig& CFG)
{
  // == SECTION START: Cannot be reloaded ==
  m_UDPMainServerEnabled = CFG.GetBool("net.udp_server.enabled", false);
  m_SupportTCPOverIPv6 = CFG.GetBool("net.ipv6.tcp.enabled", true);
  m_SupportUDPOverIPv6 = CFG.GetBool("net.udp_ipv6.enabled", true);
  m_UDPFallbackPort = CFG.GetUint16("net.udp_fallback.outbound_port", 6113);
  m_UDPIPv6Port = CFG.GetUint16("net.udp_ipv6.port", 6110);
  // == SECTION END ==

  if (m_SupportUDPOverIPv6) {
    if (m_UDPMainServerEnabled) {
      if (m_UDPIPv6Port == 6112) {
        Print("[CONFIG] <net.udp_ipv6.port> must NOT be 6112.");
        CFG.SetFailed();
      }
    } else {
      if (m_UDPFallbackPort != 0 && m_UDPIPv6Port == m_UDPFallbackPort) {
        Print("[CONFIG] <net.udp_ipv6.port> must NOT be the same as <net.udp_fallback.outbound_port>.");
        CFG.SetFailed();
      }
    }
  }

  bool isAnyReconnect = CFG.GetBool("net.tcp_extensions.gproxy.basic.enabled", true);
  bool isMGNReconnect = CFG.GetBool("net.tcp_extensions.gproxy.long.enabled", true);
  m_ProxyReconnect = 0;
  if (isAnyReconnect) {
    m_ProxyReconnect |= RECONNECT_ENABLED_GPROXY_BASIC;
    if (isMGNReconnect) m_ProxyReconnect |= RECONNECT_ENABLED_GPROXY_EXTENDED;
  } else if (isMGNReconnect) {
    Print("[CONFIG] <net.tcp_extensions.gproxy.basic.enabled = yes> is required for <net.tcp_extensions.gproxy.long.enabled = yes>.");
    CFG.SetFailed();
  }

  m_BindAddress4                 = CFG.GetAddressIPv4("net.bind_address", "0.0.0.0");
  CFG.FailIfErrorLast();
  m_BindAddress6                 = CFG.GetAddressIPv6("net.bind_address6", "::");
  CFG.FailIfErrorLast();

  optional<uint16_t> onlyHostPort = CFG.GetMaybeUint16("net.host_port.only");
  if (onlyHostPort.has_value()) {
    m_MinHostPort                 = onlyHostPort.value();
    m_MaxHostPort                 = onlyHostPort.value();
  } else {
    m_MinHostPort                 = CFG.GetUint16("net.host_port.min", 6112);
    m_MaxHostPort                 = CFG.GetUint16("net.host_port.max", m_MinHostPort);
  }

  m_UDPBlockedIPs                = CFG.GetIPStringSet("net.udp_server.block_list", ',', {});
  m_UDPEnableCustomPortTCP4      = CFG.GetBool("net.game_discovery.udp.tcp4_custom_port.enabled", false);
  m_UDPCustomPortTCP4            = CFG.GetUint16("net.game_discovery.udp.tcp4_custom_port.value", 6112);
  m_UDPEnableCustomPortTCP6      = CFG.GetBool("net.game_discovery.udp.tcp6_custom_port.enabled", false);
  m_UDPCustomPortTCP6            = CFG.GetUint16("net.game_discovery.udp.tcp6_custom_port.value", 5678); // Actually TCP4 port, but IPv6 clients connect to it
  m_UDP6TargetPort               = CFG.GetUint16("net.game_discovery.udp.ipv6.target_port", 5678); // UDP port we send information to over IPv6

  // UDP Redirect
  m_UDPForwardTraffic            = CFG.GetBool("net.udp_redirect.enabled", false);
  if (m_SupportUDPOverIPv6) {
    m_UDPForwardAddress          = CFG.GetAddress("net.udp_redirect.ip_address", "127.0.0.1");
    if (m_UDPForwardTraffic) CFG.FailIfErrorLast();
  } else {
    m_UDPForwardAddress          = CFG.GetAddressIPv4("net.udp_redirect.ip_address", "127.0.0.1");
    if (m_UDPForwardTraffic) CFG.FailIfErrorLast();
  }
  uint16_t udpForwardPort        = CFG.GetUint16("net.udp_redirect.port", 6110);
  if (m_UDPForwardTraffic) CFG.FailIfErrorLast();
  SetAddressPort(&m_UDPForwardAddress, udpForwardPort);

  m_UDPForwardGameLists          = CFG.GetBool("net.udp_redirect.realm_game_lists.enabled", false);

  // SO_DONTROUTE
  m_UDPDoNotRouteEnabled         = CFG.GetBool("net.game_discovery.udp.do_not_route", false);

  // SO_BROADCAST
  m_UDPBroadcastEnabled          = CFG.GetBool("net.game_discovery.udp.broadcast.enabled", true);
  m_UDPBroadcastTarget           = CFG.GetAddressIPv4("net.game_discovery.udp.broadcast.address", "255.255.255.255");
  if (m_UDPBroadcastEnabled) CFG.FailIfErrorLast();
  m_UDPBroadcastStrictMode       = CFG.GetBool("net.game_discovery.udp.broadcast.strict", true);

#ifdef DISABLE_MINIUPNP
  m_EnableUPnP                   = CFG.GetBool("net.port_forwarding.upnp.enabled", false);
  if (m_EnableUPnP) {
    Print("[CONFIG] warning - <net.port_forwarding.upnp.enabled = yes> unsupported in this Aura distribution");
    Print("[CONFIG] warning - <net.port_forwarding.upnp.enabled = yes> requires compilation without #define DISABLE_MINIUPNP");
    m_EnableUPnP = false;
  }
#else
  m_EnableUPnP                   = CFG.GetBool("net.port_forwarding.upnp.enabled", true);
#endif

  m_EnableTCPWrapUDP             = CFG.GetBool("net.tcp_extensions.udp_tunnel.enabled", true);
  m_VLANEnabled                  = CFG.GetBool("net.tcp_extensions.gproxy.vlan.enabled", false);
  m_VLANPort                     = CFG.GetUint16("net.tcp_extensions.gproxy.vlan.port", onlyHostPort.value_or(6112u));

  m_ReconnectWaitTicks           = CFG.GetUint16("net.tcp_extensions.gproxy.reconnect_wait", 5);
  m_ReconnectWaitTicksLegacy     = CFG.GetUint16("net.tcp_extensions.gproxy_legacy.reconnect_wait", 3);

  m_ReconnectWaitTicks = 60000 * m_ReconnectWaitTicks;
  m_ReconnectWaitTicksLegacy = 60000 * m_ReconnectWaitTicksLegacy;

  string ipv4Algorithm           = CFG.GetString("net.ipv4.public_address.algorithm", "api");
  if (ipv4Algorithm == "manual") {
    m_PublicIPv4Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL;
    if (!CFG.Exists("net.ipv4.public_address.value")) {
      Print("[CONFIG] <net.ipv4.public_address.value> is missing. Set <net.ipv4.public_address.algorithm = none> if this is intended.");
      CFG.SetFailed();
    } else {
      sockaddr_storage inputIPv4 = CFG.GetAddressIPv4("net.ipv4.public_address.value", "0.0.0.0");
      CFG.FailIfErrorLast();
      m_PublicIPv4Value = AddressToString(inputIPv4);
    }
  } else if (ipv4Algorithm == "api") {
    m_PublicIPv4Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_API;
    m_PublicIPv4Value = CFG.GetString("net.ipv4.public_address.value", "http://api.ipify.org");
#ifdef DISABLE_CPR
    if (m_PublicIPv4Value.length() >= 6 && m_PublicIPv4Value.substr(0, 6) == "https:") {
      Print("[CONFIG] warning - <net.ipv4.public_address.value = HTTPS> unsupported in this Aura distribution");
      Print("[CONFIG] warning - <net.ipv4.public_address.value = HTTPS> requires compilation without #define DISABLE_CPR");
      Print("[CONFIG] hint: try <net.ipv4.public_address.value = http:" + m_PublicIPv4Value.substr(5) + "> instead");
      m_PublicIPv4Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID;
    }
    CFG.Accept("net.ipv4.public_address.value");
#endif
  } else if (ipv4Algorithm == "none") {
    m_PublicIPv4Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE;
    CFG.Accept("net.ipv4.public_address.value");
  } else {
    m_PublicIPv4Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE;
    CFG.Accept("net.ipv4.public_address.value");
  }

  string ipv6Algorithm             = CFG.GetString("net.ipv6.public_address.algorithm", "api");
  if (ipv6Algorithm == "manual") {
    m_PublicIPv6Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL;
    if (!CFG.Exists("net.ipv6.public_address.value")) {
      Print("[CONFIG] <net.ipv6.public_address.value> is missing. Set <net.ipv6.public_address.algorithm = none> if this is intended.");
      CFG.SetFailed();
    } else {
      sockaddr_storage inputIPv6 = CFG.GetAddressIPv6("net.ipv6.public_address.value", "::");
      CFG.FailIfErrorLast();
      m_PublicIPv6Value = AddressToString(inputIPv6);
    }
  } else if (ipv6Algorithm == "api") {
    m_PublicIPv6Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_API;
    m_PublicIPv6Value = CFG.GetString("net.ipv6.public_address.value", "http://api6.ipify.org");
#ifdef DISABLE_CPR
    if (m_PublicIPv6Value.length() >= 6 && m_PublicIPv6Value.substr(0, 6) == "https:") {
      Print("[CONFIG] warning - <net.ipv6.public_address.value = HTTPS> unsupported in this Aura distribution");
      Print("[CONFIG] warning - <net.ipv6.public_address.value = HTTPS> requires compilation without #define DISABLE_CPR");
      Print("[CONFIG] hint: try <net.ipv6.public_address.value = http:" + m_PublicIPv6Value.substr(5) + "> instead");
      m_PublicIPv6Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID;
    }
    CFG.Accept("net.ipv6.public_address.value");
#endif
  } else if (ipv6Algorithm == "none") {
    m_PublicIPv6Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE;
    CFG.Accept("net.ipv6.public_address.value");
  } else {
    m_PublicIPv6Algorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE;
    CFG.Accept("net.ipv6.public_address.value");
  }

  m_EnableGeoLocalization        = CFG.GetBool("hosting.geolocalization.enabled", true);

  m_AllowDownloads               = CFG.GetBool("hosting.map_downloads.enabled", false);
#ifdef DISABLE_CPR
  if (m_AllowDownloads) {
    Print("[CONFIG] warning - <hosting.map_downloads.enabled = yes> unsupported in this Aura distribution");
    Print("[CONFIG] warning - <hosting.map_downloads.enabled = yes> requires compilation without #define DISABLE_CPR");
    m_AllowDownloads = false;
  }
#endif
  m_DownloadTimeout              = CFG.GetUint32("hosting.map_downloads.timeout", 15000);
  m_MapRepositories              = CFG.GetSetInsensitive("hosting.map_downloads.repositories", ',', {"epicwar", "wc3maps"});
  m_AllowTransfers               = CFG.GetStringIndex("hosting.map_transfers.mode", {"never", "auto", "manual"}, MAP_TRANSFERS_AUTOMATIC);
  m_MaxDownloaders               = CFG.GetUint32("hosting.map_transfers.max_players", 3);
  m_MaxUploadSize                = CFG.GetUint32("hosting.map_transfers.max_size", 8192);
  m_MaxUploadSpeed               = CFG.GetUint32("hosting.map_transfers.max_speed", 1024);
  m_MaxParallelMapPackets        = CFG.GetUint32("hosting.map_transfers.max_parallel_packets", 1000);
  m_HasBufferBloat               = CFG.GetBool("net.has_buffer_bloat", false);

  m_AnnounceGProxy               = CFG.GetBool("net.tcp_extensions.gproxy.announce_chat", true);
  m_AnnounceGProxySite           = CFG.GetString("net.tcp_extensions.gproxy.site", "https://www.mymgn.com/gproxy/");
  m_AnnounceIPv6                 = CFG.GetBool("net.ipv6.tcp.announce_chat", true);

  m_LiteralRTT                   = CFG.GetBool("metrics.ping.use_rtt", false);
#ifdef _WIN32
  m_UseSystemRTT                 = CFG.GetBool("metrics.ping.use_tcpinfo", false);
#else
  m_UseSystemRTT                 = CFG.GetBool("metrics.ping.use_tcpinfo", true);
#endif
}

CNetConfig::~CNetConfig() = default;
