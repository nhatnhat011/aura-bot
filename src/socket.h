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

#ifndef AURA_SOCKET_H_
#define AURA_SOCKET_H_

#include "includes.h"
#include "util.h"

#ifdef _WIN32
#pragma once
#include <windows.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#include <mstcpip.h>
#include <errno.h>

#undef EBADF /* override definition in errno.h */
#define EBADF WSAEBADF
#undef EINTR /* override definition in errno.h */
#define EINTR WSAEINTR
#undef EINVAL /* override definition in errno.h */
#define EINVAL WSAEINVAL
#undef EWOULDBLOCK /* override definition in errno.h */
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef EINPROGRESS /* override definition in errno.h */
#define EINPROGRESS WSAEINPROGRESS
#undef EALREADY /* override definition in errno.h */
#define EALREADY WSAEALREADY
#undef ENOTSOCK /* override definition in errno.h */
#define ENOTSOCK WSAENOTSOCK
#undef EDESTADDRREQ /* override definition in errno.h */
#define EDESTADDRREQ WSAEDESTADDRREQ
#undef EMSGSIZE /* override definition in errno.h */
#define EMSGSIZE WSAEMSGSIZE
#undef EPROTOTYPE /* override definition in errno.h */
#define EPROTOTYPE WSAEPROTOTYPE
#undef ENOPROTOOPT /* override definition in errno.h */
#define ENOPROTOOPT WSAENOPROTOOPT
#undef EPROTONOSUPPORT /* override definition in errno.h */
#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#undef EOPNOTSUPP /* override definition in errno.h */
#define EOPNOTSUPP WSAEOPNOTSUPP
#define EPFNOSUPPORT WSAEPFNOSUPPORT
#undef EAFNOSUPPORT /* override definition in errno.h */
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#undef EADDRINUSE /* override definition in errno.h */
#define EADDRINUSE WSAEADDRINUSE
#undef EADDRNOTAVAIL /* override definition in errno.h */
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#undef ENETDOWN /* override definition in errno.h */
#define ENETDOWN WSAENETDOWN
#undef ENETUNREACH /* override definition in errno.h */
#define ENETUNREACH WSAENETUNREACH
#undef ENETRESET /* override definition in errno.h */
#define ENETRESET WSAENETRESET
#undef ECONNABORTED /* override definition in errno.h */
#define ECONNABORTED WSAECONNABORTED
#undef ECONNRESET /* override definition in errno.h */
#define ECONNRESET WSAECONNRESET
#undef ENOBUFS /* override definition in errno.h */
#define ENOBUFS WSAENOBUFS
#undef EISCONN /* override definition in errno.h */
#define EISCONN WSAEISCONN
#undef ENOTCONN /* override definition in errno.h */
#define ENOTCONN WSAENOTCONN
#define ESHUTDOWN WSAESHUTDOWN
#define ETOOMANYREFS WSAETOOMANYREFS
#undef ETIMEDOUT /* override definition in errno.h */
#define ETIMEDOUT WSAETIMEDOUT
#undef ECONNREFUSED /* override definition in errno.h */
#define ECONNREFUSED WSAECONNREFUSED
#undef ELOOP /* override definition in errno.h */
#define ELOOP WSAELOOP
#ifndef ENAMETOOLONG /* possible previous definition in errno.h */
#define ENAMETOOLONG WSAENAMETOOLONG
#endif
#define EHOSTDOWN WSAEHOSTDOWN
#undef EHOSTUNREACH /* override definition in errno.h */
#define EHOSTUNREACH WSAEHOSTUNREACH
#ifndef ENOTEMPTY /* possible previous definition in errno.h */
#define ENOTEMPTY WSAENOTEMPTY
#endif
#define EPROCLIM WSAEPROCLIM
#define EUSERS WSAEUSERS
#define EDQUOT WSAEDQUOT
#define ESTALE WSAESTALE
#define EREMOTE WSAEREMOTE
// end _WIN32
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef int32_t SOCKET;

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define closesocket close

#endif

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifdef _WIN32
#define SHUT_RDWR 2
#endif

#ifdef _WIN32
#define ADDRESS_LENGTH_TYPE int
#else
#define ADDRESS_LENGTH_TYPE socklen_t
#endif

#define MIN_UDP_PACKET_SIZE 4
#define INET_ADDRSTRLEN_IPV4 16

#ifndef INADDR_MULTICAST_START
#define INADDR_MULTICAST_START 0xE0000000 
#endif

#ifndef INADDR_MULTICAST_END
#define INADDR_MULTICAST_END 0xEFFFFFFF 
#endif

#ifdef _WIN32
[[nodiscard]] inline int32_t GetLastOSError()
{
  return GetLastError();
}
#else
[[nodiscard]] inline int32_t GetLastOSError()
{
  return errno;
}
#endif

struct UDPPkt
{
  sockaddr_storage* sender;
  int length;
  unsigned char buf[1024];
  CSocket* socket;
};

[[nodiscard]] inline bool isIPv4MappedAddress(const sockaddr_in6* addr6) {
  const uint16_t* words = reinterpret_cast<const uint16_t*>(addr6->sin6_addr.s6_addr);
  // Make sure that the reference words are endian-invariant (s6_addr is network-byte order).
  return ((words[0] | words[1] | words[2] | words[3] | words[4]) == 0) && (words[5] == 0xFFFF);
}

[[nodiscard]] inline bool isIPv4MappedAddress(const sockaddr_storage* address) {
  return isIPv4MappedAddress(reinterpret_cast<const sockaddr_in6*>(address));
}

[[nodiscard]] inline bool isLoopbackAddress(const sockaddr_storage* address) {
  if (address->ss_family == AF_INET) {
    const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(address);
    return (addr4->sin_addr.s_addr & htonl(0xFF000000)) == (htonl(INADDR_LOOPBACK) & htonl(0xFF000000));
  } else if (address->ss_family == AF_INET6) {
    const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(address);
    if (isIPv4MappedAddress(addr6)) {
      const in_addr* addr4 = reinterpret_cast<const in_addr*>(addr6->sin6_addr.s6_addr + 12);
      return (addr4->s_addr & htonl(0xFF000000)) == (htonl(INADDR_LOOPBACK) & htonl(0xFF000000));
    } else {
      return (memcmp(&(addr6->sin6_addr), &in6addr_loopback, sizeof(in6_addr)) == 0);
    }
  }
  return false;
}

[[nodiscard]] inline bool isSpecialIPv6Address(const sockaddr_in6* address) {
  const in6_addr* addr6 = &(address->sin6_addr);
  return IN6_IS_ADDR_UNSPECIFIED(addr6) || IN6_IS_ADDR_LOOPBACK(addr6) || IN6_IS_ADDR_MULTICAST(addr6) ||
    IN6_IS_ADDR_LINKLOCAL(addr6) || IN6_IS_ADDR_SITELOCAL(addr6) || IN6_IS_ADDR_V4MAPPED(addr6) ||
    IN6_IS_ADDR_V4COMPAT(addr6);
}

[[nodiscard]] inline bool isSpecialIPv4Address(const sockaddr_in* address) {
  uint32_t addr = address->sin_addr.s_addr;
  if ((addr & htonl(0xFF000000)) == htonl(INADDR_LOOPBACK)) return true;
  if (addr == htonl(INADDR_BROADCAST)) return true;
  if (htonl(INADDR_MULTICAST_START) <= addr && addr <= htonl(INADDR_MULTICAST_END)) return true;
  return false;
}

[[nodiscard]] inline sockaddr_storage IPv4ToIPv6(const sockaddr_storage* inputAddress) {
  sockaddr_storage outputAddress;
  std::memset(&outputAddress, 0, sizeof(outputAddress));

  sockaddr_in6* addr6 = reinterpret_cast<struct sockaddr_in6*>(&outputAddress);
  addr6->sin6_family = AF_INET6;
  addr6->sin6_addr.s6_addr[10] = 0xff;
  addr6->sin6_addr.s6_addr[11] = 0xff;

  const sockaddr_in* addr4 = reinterpret_cast<const struct sockaddr_in*>(inputAddress);
  std::memcpy(&(addr6->sin6_addr.s6_addr[12]), &(addr4->sin_addr), sizeof(in_addr));
  addr6->sin6_port = addr4->sin_port;

  return outputAddress;
}

[[nodiscard]] inline uint8_t GetInnerIPVersion(const sockaddr_storage* inputAddress) {
  if (inputAddress->ss_family == AF_INET) return AF_INET;
  if (inputAddress->ss_family == AF_INET6) {
    const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(inputAddress);
    if (isIPv4MappedAddress(addr6)) return AF_INET;
    return AF_INET6;
  }
  return static_cast<uint8_t>(inputAddress->ss_family);
}

[[nodiscard]] inline std::string AddressToString(const sockaddr_storage& address)
{
  char ipString[INET6_ADDRSTRLEN];
  if (address.ss_family == AF_INET) { // IPv4
    const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(&address);
    inet_ntop(AF_INET, &(addr4->sin_addr), ipString, INET_ADDRSTRLEN);
  } else if (address.ss_family == AF_INET6) { // IPv6
    const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(&address);
    if (isIPv4MappedAddress(addr6)) {
      const in_addr* addr4 = reinterpret_cast<const in_addr*>(addr6->sin6_addr.s6_addr + 12);
      inet_ntop(AF_INET, addr4, ipString, INET_ADDRSTRLEN);
    } else {
      inet_ntop(AF_INET6, &(addr6->sin6_addr), ipString, INET6_ADDRSTRLEN);
    }
  } else {
    return std::string();
  }
  return std::string(ipString);
}

[[nodiscard]] inline std::string AddressToStringStrict(const sockaddr_storage& address)
{
  char ipString[INET6_ADDRSTRLEN];
  if (address.ss_family == AF_INET) { // IPv4
    const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(&address);
    inet_ntop(AF_INET, &(addr4->sin_addr), ipString, INET_ADDRSTRLEN);
  } else if (address.ss_family == AF_INET6) { // IPv6
    const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(&address);
    inet_ntop(AF_INET6, &(addr6->sin6_addr), ipString, INET6_ADDRSTRLEN);
  } else {
    return std::string();
  }
  return std::string(ipString);
}

inline void SetAddressPort(sockaddr_storage* address, const uint16_t port)
{
  if (address->ss_family == AF_INET6) {
    reinterpret_cast<struct sockaddr_in6*>(address)->sin6_port = htons(port);
  } else {
    reinterpret_cast<struct sockaddr_in*>(address)->sin_port = htons(port);
  }
}

[[nodiscard]] inline uint16_t GetAddressPort(const sockaddr_storage* address)
{
  if (address->ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const struct sockaddr_in6*>(address)->sin6_port);
  } else {
    return ntohs(reinterpret_cast<const struct sockaddr_in*>(address)->sin_port);
  }
}

[[nodiscard]] inline bool GetSameAddresses(const sockaddr_storage* reference, const sockaddr_storage* subject)
{
  if (isLoopbackAddress(reference) && isLoopbackAddress(subject)) {
    return true;
  }
	if (reference->ss_family != subject->ss_family) {
		return false;
	}
  if (subject->ss_family == AF_INET6) {
    return memcmp(&(reinterpret_cast<const sockaddr_in6*>(reference)->sin6_addr), &(reinterpret_cast<const sockaddr_in6*>(subject)->sin6_addr), sizeof(in6_addr)) == 0;
  } else {
    return memcmp(&(reinterpret_cast<const sockaddr_in*>(reference)->sin_addr), &(reinterpret_cast<const sockaddr_in*>(subject)->sin_addr), sizeof(in_addr)) == 0;
  }
}

[[nodiscard]] inline bool GetSameAddressesAndPorts(const sockaddr_storage* reference, const sockaddr_storage* subject)
{
  if (!GetSameAddresses(reference, subject)) {
    return false;
  }
  if (subject->ss_family == AF_INET6) {
    if (reinterpret_cast<const sockaddr_in6*>(reference)->sin6_port != reinterpret_cast<const sockaddr_in6*>(subject)->sin6_port)
      return false;      
  } else {
    if (reinterpret_cast<const sockaddr_in*>(reference)->sin_port != reinterpret_cast<const sockaddr_in*>(subject)->sin_port)
      return false;
  }
  return true;
}

[[nodiscard]] inline std::array<uint8_t, 4> AddressToIPv4Array(const sockaddr_storage* address) {
  if (address->ss_family == AF_INET) {
    std::array<uint8_t, 4> ipBytes;
    const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(address);
    memcpy(ipBytes.data(), &(addr4->sin_addr.s_addr), 4);
    return ipBytes;
  } else if (address->ss_family == AF_INET6) {
    const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(address);
    if (isIPv4MappedAddress(addr6)) {
      std::array<uint8_t, 4> ipBytes;
      const in_addr* addr4 = reinterpret_cast<const in_addr*>(addr6->sin6_addr.s6_addr + 12);
      memcpy(ipBytes.data(), &(addr4->s_addr), 4);
      return ipBytes;
    }
  }
  return {0, 0, 0, 0};
}

//
// CSocket
//

class CSocket
{
public:
  SOCKET             m_Socket;
  uint8_t            m_Family;
  int                m_Type;
  uint16_t           m_Port;
  bool               m_HasError;
  bool               m_HasFin;
  int                m_Error;
  std::string        m_Name;

  CSocket(const uint8_t nFamily);
  CSocket(const uint8_t nFamily, SOCKET nSocket);
  CSocket(const uint8_t nFamily, std::string nName);
  virtual ~CSocket();

  [[nodiscard]] std::string                     GetErrorString() const;
  [[nodiscard]] std::string                     GetName() const;
  [[nodiscard]] inline uint16_t                 GetPort() const { return m_Port; }
  [[nodiscard]] inline std::array<uint8_t, 2>   GetPortLE() const { return CreateFixedByteArray(m_Port, false); }
  [[nodiscard]] inline std::array<uint8_t, 2>   GetPortBE() const { return CreateFixedByteArray(m_Port, true); } // Network-byte-order
  [[nodiscard]] inline int32_t                  GetError() const { return m_Error; }
  [[nodiscard]] inline bool                     HasError() const { return m_HasError; }
  [[nodiscard]] inline bool                     HasFin() const { return m_HasFin; }

  [[nodiscard]] inline ADDRESS_LENGTH_TYPE  GetAddressLength() const {
    if (m_Family == AF_INET6)
      return sizeof(sockaddr_in6);
    return sizeof(sockaddr_in);
  }

  void SetFD(fd_set* fd, fd_set* send_fd, int32_t* nfds);
  void Reset();
  void Allocate(const uint8_t family, int type);

  virtual void SendReply(const sockaddr_storage* address, const std::vector<uint8_t>& packet);
};

//
// CStreamIOSocket
//

class CStreamIOSocket : public CSocket
{
public:
  std::string                m_RecvBuffer;
  std::string                m_SendBuffer;
  uint32_t                   m_RemoteSocketCounter;
  int64_t                    m_LastRecv;
  bool                       m_Connected;

  sockaddr_storage           m_RemoteHost;
  CTCPServer*                m_Server;
  uint16_t                   m_Counter;
  bool                       m_LogErrors;

  CStreamIOSocket(uint8_t nFamily, std::string nName);
  CStreamIOSocket(SOCKET nSocket, sockaddr_storage& remoteAddress, CTCPServer* nServer, const uint16_t nCounter);
  virtual ~CStreamIOSocket();

  [[nodiscard]] inline int64_t                    GetLastRecv() const { return m_LastRecv; }
  [[nodiscard]] std::string                       GetName() const;

  [[nodiscard]] inline bool                       GetIsInnerIPv4() const { return GetInnerIPVersion(&m_RemoteHost) == AF_INET; }
  [[nodiscard]] inline bool                       GetIsInnerIPv6() const { return GetInnerIPVersion(&m_RemoteHost) == AF_INET6; }

  [[nodiscard]] inline std::array<uint8_t, 4>     GetIPv4() const { return AddressToIPv4Array(&m_RemoteHost); }
  [[nodiscard]] inline std::string                GetIPString() const { return AddressToString(m_RemoteHost); }
  [[nodiscard]] inline std::string                GetIPStringStrict() const { return AddressToStringStrict(m_RemoteHost); }
  [[nodiscard]] inline const sockaddr_storage*    GetRemoteAddress() const { return static_cast<const sockaddr_storage*>(&m_RemoteHost); }
  [[nodiscard]] inline bool                       GetIsLoopback() const { return isLoopbackAddress(&m_RemoteHost); }
  [[nodiscard]] inline uint16_t                   GetRemotePort() const { return GetAddressPort(&m_RemoteHost); }

  [[nodiscard]] inline bool                       GetConnected() const { return m_Connected; }
  [[nodiscard]] inline bool                       GetLogErrors() const { return m_LogErrors; }
  void Disconnect();

  [[nodiscard]] inline std::string*               GetBytes() { return &m_RecvBuffer; }
  inline void ClearRecvBuffer() { m_RecvBuffer.clear(); }
  inline void SubstrRecvBuffer(uint32_t i) { m_RecvBuffer = m_RecvBuffer.substr(i); }
  bool DoRecv(fd_set* fd);
  void Discard(fd_set* fd);

  inline size_t                                 PutBytes(const std::string& bytes) {
    m_SendBuffer += bytes;
    return bytes.size();
  }
  inline size_t                                 PutBytes(const std::vector<uint8_t>& bytes) {
    m_SendBuffer += std::string(begin(bytes), end(bytes));
    return bytes.size();
  }
  inline void                                   ClearSendBuffer() { m_SendBuffer.clear(); }
  inline void                                   SubstrSendBuffer(uint32_t i) { m_SendBuffer = m_SendBuffer.substr(i); }
  [[nodiscard]] inline bool                     GetIsSendPending() { return !m_SendBuffer.empty(); }
  [[nodiscard]] std::optional<uint32_t>         GetRTT() const;
  void DoSend(fd_set* send_fd);
  void Flush();

  void Close();
  void Reset();
  void SendReply(const sockaddr_storage* address, const std::vector<uint8_t>& packet) override final;
  void SetNoDelay(const bool noDelay);
  void SetQuickAck(const bool quickAck);
  void SetKeepAlive(const bool keepAlive, const uint32_t seconds);
  inline void SetLogErrors(const bool nLogErrors) { m_LogErrors = nLogErrors; }
};

//
// CTCPClient
//

class CTCPClient final : public CStreamIOSocket
{
public:
  bool                      m_Connecting;

  CTCPClient(uint8_t nFamily, std::string nName);
  ~CTCPClient() final;

  [[nodiscard]] inline bool         GetConnecting() const { return m_Connecting; }
  [[nodiscard]] bool                CheckConnect();
  void                              Connect(const std::optional<sockaddr_storage>& localAddress, const sockaddr_storage& remoteHost);

  // Overrides
  void                Reset();
  void                Disconnect();
};

//
// CTCPServer
//

class CTCPServer final : public CSocket
{
public:
  uint16_t                        m_AcceptCounter;

  CTCPServer(uint8_t nFamily);
  ~CTCPServer() final;

  [[nodiscard]] std::string       GetName() const;
  bool                            Listen(sockaddr_storage& address, const uint16_t port, bool retry);
  [[nodiscard]] CStreamIOSocket*  Accept(fd_set* fd);
  void                            Discard(fd_set* fd);
};

//
// CUDPSocket
//

class CUDPSocket : public CSocket
{
public:
  CUDPSocket(uint8_t nFamily);
  ~CUDPSocket();

  bool                        SendTo(const sockaddr_storage* address, const std::vector<uint8_t>& message);
  bool                        SendTo(const std::string& addressLiteral, uint16_t port, const std::vector<uint8_t>& message);
  bool                        Broadcast(const sockaddr_storage* addr4, const std::vector<uint8_t>& message);

  void                        Reset();
  void                        SetBroadcastEnabled(const bool nEnable);
  void                        SetDontRoute(bool dontRoute);
  void                        SendReply(const sockaddr_storage* address, const std::vector<uint8_t>& packet) override final;
};

class CUDPServer final : public CUDPSocket
{
public:
  CUDPServer(uint8_t nFamily);
  ~CUDPServer() final;

  [[nodiscard]] std::string   GetName() const;
  bool                        Listen(sockaddr_storage& address, const uint16_t port, bool retry);
  [[nodiscard]] UDPPkt*       Accept(fd_set* fd);
  void                        Discard(fd_set* fd);
};

#endif // AURA_SOCKET_H_
