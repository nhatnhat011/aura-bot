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

#include "socket.h"
#include "net.h"
#include "util.h"

using namespace std;

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) (void)(x)
#endif


//
// CSocket
//

CSocket::CSocket(const uint8_t nFamily)
  : m_Socket(INVALID_SOCKET),
    m_Family(nFamily),
    m_Type(0),
    m_Port(0),
    m_HasError(false),
    m_HasFin(false),
    m_Error(0)
{
}

CSocket::CSocket(const uint8_t nFamily, SOCKET nSocket)
  : m_Socket(nSocket),
    m_Family(nFamily),
    m_Type(0),
    m_Port(0),
    m_HasError(false),
    m_HasFin(false),
    m_Error(0)
{
}

CSocket::CSocket(const uint8_t nFamily, string nName)
  : m_Socket(INVALID_SOCKET),
    m_Family(nFamily),
    m_Type(0),
    m_Port(0),
    m_HasError(false),
    m_HasFin(false),
    m_Error(0),
    m_Name(nName)
{
}

CSocket::~CSocket()
{
  if (m_Socket != INVALID_SOCKET) {
    closesocket(m_Socket);
    m_Socket = INVALID_SOCKET;
  }
}

string CSocket::GetName() const
{
  return m_Name;
}

string CSocket::GetErrorString() const
{
  if (!m_HasError)
    return "NO ERROR";

  switch (m_Error)
  {
    case EWOULDBLOCK:
      return "EWOULDBLOCK";
    case EINPROGRESS:
      return "EINPROGRESS";
    case EALREADY:
      return "EALREADY";
    case ENOTSOCK:
      return "ENOTSOCK";
    case EDESTADDRREQ:
      return "EDESTADDRREQ";
    case EMSGSIZE:
      return "EMSGSIZE";
    case EPROTOTYPE:
      return "EPROTOTYPE";
    case ENOPROTOOPT:
      return "ENOPROTOOPT";
    case EPROTONOSUPPORT:
      return "EPROTONOSUPPORT";
    case ESOCKTNOSUPPORT:
      return "ESOCKTNOSUPPORT";
    case EOPNOTSUPP:
      return "EOPNOTSUPP";
    case EPFNOSUPPORT:
      return "EPFNOSUPPORT";
    case EAFNOSUPPORT:
      return "EAFNOSUPPORT";
    case EADDRINUSE:
      return "EADDRINUSE";
    case EADDRNOTAVAIL:
      return "EADDRNOTAVAIL";
    case ENETDOWN:
      return "ENETDOWN";
    case ENETUNREACH:
      return "ENETUNREACH";
    case ENETRESET:
      return "ENETRESET";
    case ECONNABORTED:
      return "ECONNABORTED";
    case ENOBUFS:
      return "ENOBUFS";
    case EISCONN:
      return "EISCONN";
    case ENOTCONN:
      return "ENOTCONN";
    case ESHUTDOWN:
      return "ESHUTDOWN";
    case ETOOMANYREFS:
      return "ETOOMANYREFS";
    case ETIMEDOUT:
      return "ETIMEDOUT";
    case ECONNREFUSED:
      return "ECONNREFUSED";
    case ELOOP:
      return "ELOOP";
    case ENAMETOOLONG:
      return "ENAMETOOLONG";
    case EHOSTDOWN:
      return "EHOSTDOWN";
    case EHOSTUNREACH:
      return "EHOSTUNREACH";
    case ENOTEMPTY:
      return "ENOTEMPTY";
    case EUSERS:
      return "EUSERS";
    case EDQUOT:
      return "EDQUOT";
    case ESTALE:
      return "ESTALE";
    case EREMOTE:
      return "EREMOTE";
    case ECONNRESET:
      return "Connection reset by peer";
  }

  return "UNKNOWN ERROR (" + to_string(m_Error) + ")";
}

void CSocket::SetFD(fd_set* fd, fd_set* send_fd, int* nfds)
{
  if (m_Socket == INVALID_SOCKET)
    return;

  FD_SET(m_Socket, fd);
  FD_SET(m_Socket, send_fd);

#ifndef _WIN32
  if (m_Socket > *nfds)
    *nfds = m_Socket;
#endif
}

void CSocket::Allocate(const uint8_t family, int type)
{
  m_Socket = socket(family, type, 0);
  m_Type = type;

  if (m_Socket == INVALID_SOCKET)
  {
    m_HasError = true;
    m_Error = GetLastOSError();
    Print("[SOCKET] error (socket) - " + GetErrorString());
    return;
  }
}

void CSocket::Reset()
{
  if (m_Socket != INVALID_SOCKET) {
    closesocket(m_Socket);
  }

  m_Socket = INVALID_SOCKET;
  m_HasError = false;
  m_Error = 0;
  m_HasFin = false;
}

void CSocket::SendReply(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  UNREFERENCED_PARAMETER(address);
  UNREFERENCED_PARAMETER(message);
}

//
// CStreamIOSocket
//

CStreamIOSocket::CStreamIOSocket(uint8_t nFamily, string nName)
  : CSocket(nFamily, nName),
    m_LastRecv(GetTicks()),
    m_Connected(false),
    m_Server(nullptr),
    m_Counter(0),
    m_LogErrors(false)
{
  memset(&m_RemoteHost, 0, sizeof(sockaddr_storage));

  Allocate(nFamily, SOCK_STREAM);

  // make socket non blocking
#ifdef _WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif

  // disable Nagle's algorithm
  SetNoDelay(true);

  // disable delayed acks
  SetQuickAck(true);
}

CStreamIOSocket::CStreamIOSocket(SOCKET nSocket, sockaddr_storage& nAddress, CTCPServer* nServer, const uint16_t nCounter)
  : CSocket(static_cast<uint8_t>(nAddress.ss_family), nSocket),
    m_LastRecv(GetTicks()),
    m_Connected(true),
    m_RemoteHost(move(nAddress)),
    m_Server(nServer),
    m_Counter(nCounter),
    m_LogErrors(false)
{
  // make socket non blocking
#ifdef _WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

string CStreamIOSocket::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty() && m_Server != nullptr) {
    return m_Server->GetName() + "-C" + to_string(m_Counter);
  }
  return name;
}

CStreamIOSocket::~CStreamIOSocket()
{
  m_Server = nullptr;
}

void CStreamIOSocket::SetNoDelay(const bool noDelay)
{
  int32_t OptVal = noDelay;
  setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
}

void CStreamIOSocket::SetQuickAck(const bool quickAck)
{
#ifdef _WIN32
  int32_t OptVal = quickAck;
  DWORD bytesReturned;
	WSAIoctl(m_Socket, SIO_TCP_SET_ACK_FREQUENCY, &OptVal, sizeof(OptVal), nullptr, 0, &bytesReturned, nullptr, nullptr);
#else
  int32_t OptVal = quickAck;
  setsockopt(m_Socket, IPPROTO_TCP, TCP_QUICKACK, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
#endif
}

void CStreamIOSocket::SetKeepAlive(const bool keepAlive, const uint32_t seconds)
{
#ifdef _WIN32
  tcp_keepalive keepAliveSettings;
  keepAliveSettings.onoff = keepAlive;
  keepAliveSettings.keepalivetime = seconds * 1000;
  keepAliveSettings.keepaliveinterval = 30000;

  DWORD bytesReturned;
  WSAIoctl(m_Socket, SIO_KEEPALIVE_VALS, &keepAliveSettings, sizeof(keepAliveSettings), nullptr, 0, &bytesReturned, nullptr, nullptr);
#else
  int32_t OptVal = keepAlive;
  setsockopt(m_Socket, IPPROTO_TCP, SO_KEEPALIVE, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));

  if (keepAlive) {
    setsockopt(m_Socket, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<const char*>(&seconds), sizeof(seconds));
    int32_t keepAliveInterval = 30;
    int32_t keepAliveProbes = 4;
    setsockopt(m_Socket, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const char*>(&keepAliveInterval), sizeof(keepAliveInterval));
    setsockopt(m_Socket, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const char*>(&keepAliveProbes), sizeof(keepAliveProbes));
  }
#endif
}

void CStreamIOSocket::Close()
{
  if (m_Socket != INVALID_SOCKET) {
    closesocket(m_Socket);
  }

  m_Socket = INVALID_SOCKET;
  m_Connected = false;
  m_RecvBuffer.clear();
  m_SendBuffer.clear();

  memset(&m_RemoteHost, 0, sizeof(sockaddr_storage));
}

void CStreamIOSocket::Reset()
{
  CSocket::Reset();

  Allocate(m_Family, SOCK_STREAM);

  m_Connected = false;
  m_RecvBuffer.clear();
  m_SendBuffer.clear();
  m_LastRecv = GetTicks();

  memset(&m_RemoteHost, 0, sizeof(sockaddr_storage));

// make socket non blocking

#ifdef _WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

bool CStreamIOSocket::DoRecv(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connected)
    return false;

  if (!FD_ISSET(m_Socket, fd))
    return false;

  // data is waiting, receive it
  char buffer[1024];
  int32_t c = recv(m_Socket, buffer, 1024, 0);

  if (c > 0) {
    // success! add the received data to the buffer
    m_RecvBuffer += string(buffer, c);
    m_LastRecv = GetTicks();
    return true;
  }

  if (c == SOCKET_ERROR && GetLastOSError() != EWOULDBLOCK) {
    // receive error
    m_HasError = true;
    m_Error = GetLastOSError();
    if (m_LogErrors) {
      Print("[TCPSOCKET] (" + GetName() +") error (recv) - " + GetErrorString());
    }
  } else if (c == 0) {
    // the other end closed the connection
    if (m_LogErrors) {
      Print("[TCPSOCKET] (" + GetName() +") remote terminated the connection");
    }
    m_HasFin = true;
    m_LogErrors = false;
  }
  return false;
}

void CStreamIOSocket::Discard(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connected)
    return;

  if (!FD_ISSET(m_Socket, fd))
    return;

  char buffer[1024];
  recv(m_Socket, buffer, 1024, 0);
}

optional<uint32_t> CStreamIOSocket::GetRTT() const
{
  optional<uint32_t> rtt;

#ifndef _WIN32
  struct tcp_info info;
  socklen_t info_len = sizeof(info);

  if (getsockopt(m_Socket, IPPROTO_TCP, TCP_INFO, &info, &info_len) == 0) {
    rtt = static_cast<uint32_t>(info.tcpi_rtt / 1000);
  }
#endif

  return rtt;
}

void CStreamIOSocket::DoSend(fd_set* send_fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || m_HasFin || !m_Connected || m_SendBuffer.empty())
    return;

  if (FD_ISSET(m_Socket, send_fd))
  {
    // socket is ready, send it

    int32_t s = send(m_Socket, m_SendBuffer.c_str(), static_cast<int32_t>(m_SendBuffer.size()), MSG_NOSIGNAL);

    if (s > 0)
    {
      // success! only some of the data may have been sent, remove it from the buffer

      m_SendBuffer = m_SendBuffer.substr(s);
    }
    else if (s == SOCKET_ERROR && GetLastOSError() != EWOULDBLOCK)
    {
      // send error

      m_HasError = true;
      m_Error = GetLastOSError();
      if (m_LogErrors) {
        Print("[TCPSOCKET] (" + GetName() +") error (send) - " + GetErrorString());
      }
      return;
    }
  }
}

void CStreamIOSocket::Flush()
{
  if (m_Socket == INVALID_SOCKET || m_HasError || m_HasFin || !m_Connected || m_SendBuffer.empty())
    return;

  send(m_Socket, m_SendBuffer.c_str(), static_cast<int32_t>(m_SendBuffer.size()), MSG_NOSIGNAL);
  m_SendBuffer.clear();
}

void CStreamIOSocket::SendReply(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  UNREFERENCED_PARAMETER(address);
  PutBytes(message);
}

void CStreamIOSocket::Disconnect()
{
  if (m_Socket != INVALID_SOCKET)
    shutdown(m_Socket, SHUT_RDWR);

  m_Connected = false;
}

//
// CTCPClient
//

CTCPClient::CTCPClient(uint8_t nFamily, string nName)
  : CStreamIOSocket(nFamily, nName),
    m_Connecting(false)
{
}

CTCPClient::~CTCPClient()
{
}

void CTCPClient::Reset()
{
  CStreamIOSocket::Reset();

  m_Connecting = false;
}

void CTCPClient::Disconnect()
{
  if (m_Socket != INVALID_SOCKET)
    shutdown(m_Socket, SHUT_RDWR);

  m_Connected  = false;
  m_Connecting = false;
}

void CTCPClient::Connect(const optional<sockaddr_storage>& localAddress, const sockaddr_storage& remoteHost)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || m_Connecting || m_Connected)
    return;

  if (localAddress.has_value()) {
    if (localAddress.value().ss_family != remoteHost.ss_family) {
      m_HasError = true;
      Print("[TCP] Cannot connect to " + AddressToString(remoteHost) + " from bind address " + AddressToString(localAddress.value()));
      return;
    }

    if (::bind(m_Socket, reinterpret_cast<const struct sockaddr*>(&localAddress), sizeof(sockaddr_storage)) == SOCKET_ERROR) {
      m_HasError = true;
      m_Error = GetLastOSError();
      Print("[TCPCLIENT] (" + GetName() +") error (bind) - " + GetErrorString());
      return;
    }
  }

  memcpy(&m_RemoteHost, &remoteHost, sizeof(sockaddr_storage));

  // connect
  if (connect(m_Socket, reinterpret_cast<struct sockaddr*>(&m_RemoteHost), sizeof(sockaddr_storage)) == SOCKET_ERROR)
  {
    if (GetLastOSError() != EINPROGRESS && GetLastOSError() != EWOULDBLOCK)
    {
      // connect error

      m_HasError = true;
      m_Error = GetLastOSError();
      Print("[TCPCLIENT] (" + GetName() +") error (connect) - " + GetErrorString());
      return;
    }
  }

  m_Connecting = true;
}

bool CTCPClient::CheckConnect()
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connecting)
    return false;

  fd_set fd;
  FD_ZERO(&fd);
  FD_SET(m_Socket, &fd);

  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = 0;

// check if the socket is connected

#ifdef _WIN32
  if (select(1, nullptr, &fd, nullptr, &tv) == SOCKET_ERROR)
#else
  if (select(m_Socket + 1, nullptr, &fd, nullptr, &tv) == SOCKET_ERROR)
#endif
  {
    m_HasError = true;
    m_Error = GetLastOSError();
    Print("[TCPCLIENT] (" + GetName() +") error (connect) - " + GetErrorString());
    return false;
  }

  if (FD_ISSET(m_Socket, &fd))
  {
    m_Connecting = false;
    m_Connected  = true;
    return true;
  }

  return false;
}

//
// CTCPServer
//

CTCPServer::CTCPServer(uint8_t nFamily)
  : CSocket(nFamily),
    m_AcceptCounter(0)
{
  Allocate(m_Family, SOCK_STREAM);

  // make socket non blocking
#ifdef _WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif

  // set the socket to reuse the address in case it hasn't been released yet
  int32_t optval = 1;
#ifndef _WIN32
  setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int32_t));
#endif

  // accept IPv4 additionally to IPv6
  if (m_Family == AF_INET6) {
    int32_t OptVal = 0;
    setsockopt(m_Socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
  }

  // disable Nagle's algorithm
  {
    int32_t OptVal = 1;
    setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
  }

  // disable Delayed Ack algorithm
  {
    int32_t OptVal = 1;
#ifdef _WIN32
    DWORD bytesReturned;
    WSAIoctl(m_Socket, SIO_TCP_SET_ACK_FREQUENCY, &OptVal, sizeof(OptVal), nullptr, 0, &bytesReturned, nullptr, nullptr);
#else
    setsockopt(m_Socket, IPPROTO_TCP, TCP_QUICKACK, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
#endif
  }
}

CTCPServer::~CTCPServer()
{
  Print("[TCP] Closed " + GetName());
}

string CTCPServer::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty()) {
    return "TCPServer@" + to_string(m_Port);
  }
  return name;
}

bool CTCPServer::Listen(sockaddr_storage& address, const uint16_t port, bool retry)
{
  if (m_Socket == INVALID_SOCKET) {
    Print("[TCP] Socket invalid");
    return false;
  }

  if (m_HasError && !retry) {
    Print("[TCP] Failed to listen TCP at port " + to_string(port) + ". Error " + to_string(m_Error));
    return false;
  }

  if (m_HasError) {
    m_HasError = false;
    m_Error = 0;
  }

  ADDRESS_LENGTH_TYPE addressLength = GetAddressLength();
  SetAddressPort(&address, port);

  if (::bind(m_Socket, reinterpret_cast<const struct sockaddr*>(&address), addressLength) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error = GetLastOSError();
    Print("[TCP] error (bind) - " + GetErrorString());
    return false;
  }

  // listen, queue length 8

  if (listen(m_Socket, 8) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error = GetLastOSError();
    Print("[TCP] error (listen) - " + GetErrorString());
    return false;
  }

  if (port == 0) {
    if (getsockname(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength) == -1) {
      m_HasError = true;
      m_Error = GetLastOSError();
      Print("[TCP] error (getsockname) - " + GetErrorString());
      return false;
    }
    m_Port = GetAddressPort(&address);
  } else {
    m_Port = port;
  }

  if (m_Family == AF_INET6) {
    Print("[TCP] IPv6 listening on port " + to_string(m_Port) + " (IPv4 too)");
  } else {
    Print("[TCP] IPv4 listening on port " + to_string(m_Port));
  }
  return true;
}

CStreamIOSocket* CTCPServer::Accept(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return nullptr;

  if (FD_ISSET(m_Socket, fd)) {
    // a connection is waiting, accept it

    sockaddr_storage         address;
    ADDRESS_LENGTH_TYPE      addressLength = GetAddressLength();
    SOCKET                   NewSocket;
    memset(&address, 0, addressLength);

    if ((NewSocket = accept(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength)) != INVALID_SOCKET) {
      ++m_AcceptCounter;
      CStreamIOSocket* incomingSocket = new CStreamIOSocket(NewSocket, address, this, m_AcceptCounter);
      incomingSocket->SetKeepAlive(true, 180);
      return incomingSocket;
    }
  }

  return nullptr;
}

void CTCPServer::Discard(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return;

  if (FD_ISSET(m_Socket, fd)) {
    // a connection is waiting, accept it

    sockaddr_storage         address;
    ADDRESS_LENGTH_TYPE      addressLength = GetAddressLength();
    SOCKET                   NewSocket;
    memset(&address, 0, addressLength);

    if ((NewSocket = accept(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength)) != INVALID_SOCKET) {
      closesocket(NewSocket);
    }
  }
}

//
// CUDPSocket
//

CUDPSocket::CUDPSocket(uint8_t nFamily)
  : CSocket(nFamily)
{
  Allocate(m_Family, SOCK_DGRAM);

  if (m_Family == AF_INET6) {
    int32_t OptVal = 0;
    setsockopt(m_Socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
  }
}

CUDPSocket::~CUDPSocket()
{
}

bool CUDPSocket::SendTo(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return false;

  if (m_Family == address->ss_family) {
    const string MessageString = string(begin(message), end(message));
    return -1 != sendto(m_Socket, MessageString.c_str(), static_cast<int>(MessageString.size()), 0, reinterpret_cast<const struct sockaddr*>(address), sizeof(sockaddr_storage));
  }
  if (m_Family == AF_INET && address->ss_family == AF_INET6) {
    Print("Error - Attempt to send UDP6 message from UDP4 socket: " + ByteArrayToDecString(message));
    return false;
  }
  if (m_Family == AF_INET6 && address->ss_family == AF_INET) {
    sockaddr_storage addr6 = IPv4ToIPv6(address);
    const string MessageString = string(begin(message), end(message));
    return -1 != sendto(m_Socket, MessageString.c_str(), static_cast<int>(MessageString.size()), 0, reinterpret_cast<const struct sockaddr*>(&addr6), sizeof(addr6));
  }
  return false;
}

bool CUDPSocket::SendTo(const string& addressLiteral, uint16_t port, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    return false;
  }

  optional<sockaddr_storage> address = CNet::ParseAddress(addressLiteral);
  if (!address.has_value()) {
    m_HasError = true;
    // m_Error = h_error;
    Print("[UDP] error (gethostbyname)");
    return false;
  }
  
  sockaddr_storage* targetAddress = &(address.value());
  SetAddressPort(targetAddress, port);
  return SendTo(targetAddress, message);
}

bool CUDPSocket::Broadcast(const sockaddr_storage* addr4, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    Print("Broadcast critical error");
    return false;
  }

  const string MessageString = string(begin(message), end(message));
  int result = sendto(m_Socket, MessageString.c_str(), static_cast<int>(MessageString.size()), 0, reinterpret_cast<const struct sockaddr*>(addr4), sizeof(sockaddr_in));

  if (result == -1) {
    return false;
  }

  return true;
}

void CUDPSocket::SetBroadcastEnabled(const bool nEnable)
{
  // Broadcast is only defined over IPv4, but a subset of IPv6 maps to IPv6.

  int32_t OptVal = nEnable;
#ifdef _WIN32
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&OptVal, sizeof(int32_t));
#else
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (const void*)&OptVal, sizeof(int32_t));
#endif
}

void CUDPSocket::SetDontRoute(bool dontRoute)
{
  // whether to let packets ignore routes set by routing table
  // if DONTROUTE is enabled, packets are sent directly to the interface belonging to the target address
  int32_t OptVal = dontRoute;
  setsockopt(m_Socket, SOL_SOCKET, SO_DONTROUTE, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
}

void CUDPSocket::Reset()
{
  CSocket::Reset();
  Allocate(m_Family, SOCK_DGRAM);
}

void CUDPSocket::SendReply(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  SendTo(address, message);
}

CUDPServer::CUDPServer(uint8_t nFamily)
  : CUDPSocket(nFamily)
{
  // make socket non blocking
#ifdef _WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

CUDPServer::~CUDPServer()
{
  Print("[UDP] Closed " + GetName());
}

string CUDPServer::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty()) {
    return "UDPServer@" + to_string(m_Port);
  }
  return name;
}

bool CUDPServer::Listen(sockaddr_storage& address, const uint16_t port, bool retry)
{
  if (m_Socket == INVALID_SOCKET) {
    Print("[UDPServer] Socket invalid");
    return false;
  }

  if (m_HasError && !retry) {
    Print("[UDPServer] Failed to listen UDP at port " + to_string(port) + ". Error " + to_string(m_Error));
    return false;
  }  

  if (m_HasError) {
    m_HasError = false;
    m_Error = 0;
  }

  ADDRESS_LENGTH_TYPE addressLength = GetAddressLength();
  SetAddressPort(&address, port);

  if (::bind(m_Socket, reinterpret_cast<const struct sockaddr*>(&address), addressLength) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error = GetLastOSError();
    Print("[UDP] error (bind) - " + GetErrorString());
    return false;
  }

  if (port == 0) {
    if (getsockname(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength) == -1) {
      m_HasError = true;
      m_Error = GetLastOSError();
      Print("[UDP] error (getsockname) - " + GetErrorString());
      return false;
    }
    m_Port = GetAddressPort(&address);
  } else {
    m_Port = port;
  }

  if (m_Family == AF_INET6) {
    Print("[UDP] listening IPv4/IPv6 UDP traffic on port " + to_string(m_Port));
  } else {
    Print("[UDP] listening IPv4-only UDP traffic on port " + to_string(m_Port));
  }
  return true;
}

UDPPkt* CUDPServer::Accept(fd_set* fd) {
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    return nullptr;
  }

  if (!FD_ISSET(m_Socket, fd)) {
    return nullptr;
  }

  char buffer[1024];
  sockaddr_storage* address = new sockaddr_storage(); // It's the responsibility of the caller to delete this.
  ADDRESS_LENGTH_TYPE addressLength = sizeof(sockaddr_storage);

  int bytesRead = recvfrom(m_Socket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(address), &addressLength);
#ifdef _WIN32
  if (bytesRead == SOCKET_ERROR) {
    //int error = WSAGetLastError();
#else
  if (bytesRead < 0) {
    //int error = errno;
#endif
    //Print("Error code " + to_string(error) + " receiving data from " + AddressToString(*pkt.sender));
    delete address;
    return nullptr;
  }
  if (bytesRead < MIN_UDP_PACKET_SIZE) {
    delete address;
    return nullptr;
  }

  UDPPkt* pkt = new UDPPkt();
  if (pkt == nullptr)
    return nullptr;

  pkt->socket = this;
  pkt->sender = address;
  pkt->length = bytesRead;
  memcpy(pkt->buf, buffer, bytesRead);
  return pkt;
}

void CUDPServer::Discard(fd_set* fd) {
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    return;
  }

  if (!FD_ISSET(m_Socket, fd)){
    return;
  }
  
  char buffer[1024];
  recv(m_Socket, buffer, sizeof(buffer), 0);
}
