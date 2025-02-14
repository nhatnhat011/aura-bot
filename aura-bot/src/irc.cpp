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

#include "config/config_irc.h"
#include "irc.h"
#include "command.h"
#include "aura.h"
#include "socket.h"
#include "util.h"
#include "protocol/bnet_protocol.h"
#include "realm.h"
#include "net.h"

#include <utility>
#include <algorithm>

using namespace std;

//////////////
//// CIRC ////
//////////////

CIRC::CIRC(CConfig& nCFG)
  : m_Aura(nullptr),
    m_Socket(new CTCPClient(AF_INET, "IRC")),
    m_LastConnectionAttemptTime(0),
    m_LastPacketTime(GetTime()),
    m_LastAntiIdleTime(GetTime()),
    m_WaitingToConnect(true),
    m_LoggedIn(false),
    m_NickName(string()),
    m_Config(CIRCConfig(nCFG))
{
  //m_Socket->SetKeepAlive(true, IRC_TCP_KEEPALIVE_IDLE_TIME);
}

CIRC::~CIRC()
{
  delete m_Socket;

  for (const auto& ptr : m_Aura->m_ActiveContexts) {
    auto ctx = ptr.lock();
    if (ctx && ctx->m_IRC == this) {
      ctx->m_IRC = nullptr;
      ctx->SetPartiallyDestroyed();
    }
  }
}

bool CIRC::MatchHostName(const string& hostName) const
{
  if (hostName == m_Config.m_HostName) return true;
  if (hostName == m_Config.m_VerifiedDomain) return true;
  return false;
}

uint32_t CIRC::SetFD(void* fd, void* send_fd, int32_t* nfds) const
{
  // irc socket

  if (!m_Socket->HasError() && !m_Socket->HasFin() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 0;
  }

  return 1;
}

void CIRC::ResetConnection()
{
  m_Socket->Reset();
  //m_Socket->SetKeepAlive(true, IRC_TCP_KEEPALIVE_IDLE_TIME);
  m_WaitingToConnect = true;
  m_LoggedIn = false;
}

void CIRC::Update(void* fd, void* send_fd)
{
  if (!m_Config.m_Enabled) {
    if (m_Socket && m_Socket->GetConnected()) {
      Print("[IRC: " + m_Config.m_HostName + "] disconnected");
      ResetConnection();
      m_WaitingToConnect = false;
    }
    return;
  }

  const int64_t Time = GetTime();

  if (m_Socket->HasError() || m_Socket->HasFin())
  {
    if (m_Socket->HasError()) {
      // the socket has an error
      Print("[IRC: " + m_Config.m_HostName + "] disconnected due to socket error");
    } else {
      // remote end terminated the connection
      Print("[IRC: " + m_Config.m_HostName + "] remote terminated the connection");
    }
    Print("[IRC: " + m_Config.m_HostName + "] waiting 60 seconds to reconnect");
    ResetConnection();
    m_LastConnectionAttemptTime = Time;
    return;
  }

  if (m_Socket->GetConnected())
  {
    // the socket is connected and everything appears to be working properly

    if (Time - m_LastPacketTime > 210)
    {
      Print("[IRC: " + m_Config.m_HostName + "] ping timeout, reconnecting...");
      ResetConnection();
      return;
    }

    if (Time - m_LastAntiIdleTime > 60)
    {
      Send("TIME");
      m_LastAntiIdleTime = Time;
    }

    if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
      ExtractPackets();
    }
    if (m_Socket->HasError() || m_Socket->HasFin()) {
      return;
    }
    m_Socket->DoSend(static_cast<fd_set*>(send_fd));
    return;
  }

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && !m_WaitingToConnect)
  {
    // the socket was disconnected

    Print("[IRC: " + m_Config.m_HostName + "] disconnected, waiting 60 seconds to reconnect");
    ResetConnection();
    m_LastConnectionAttemptTime = Time;
    return;
  }

  if (m_Socket->GetConnecting())
  {
    // we are currently attempting to connect to irc

    if (m_Socket->CheckConnect())
    {
      // the connection attempt completed
      m_Socket->SetKeepAlive(true, IRC_TCP_KEEPALIVE_IDLE_TIME);

      m_NickName = m_Config.m_NickName;

      if (m_Config.m_HostName.find("quakenet.org") == string::npos && !m_Config.m_Password.empty())
        Send("PASS " + m_Config.m_Password);

      Send("NICK " + m_Config.m_NickName);
      Send("USER " + m_Config.m_UserName + " " + m_Config.m_NickName + " " + m_Config.m_UserName + " :aura-bot");

      m_Socket->DoSend(static_cast<fd_set*>(send_fd));

      m_LoggedIn = true;
      Print("[IRC: " + m_Config.m_HostName + "] connected");

      m_LastPacketTime = Time;

      return;
    }
    else if (Time - m_LastConnectionAttemptTime > 15)
    {
      // the connection attempt timed out (15 seconds)

      Print("[IRC: " + m_Config.m_HostName + "] connect timed out, waiting 60 seconds to reconnect");
      ResetConnection();
      m_LastConnectionAttemptTime = Time;
      return;
    }
  }

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && (Time - m_LastConnectionAttemptTime > 60)) {
    // attempt to connect to irc

    Print("[IRC: " + m_Config.m_HostName + "] connecting to server [" + m_Config.m_HostName + "] on port " + to_string(m_Config.m_Port));
    optional<sockaddr_storage> emptyBindAddress;
    sockaddr_storage resolvedAddress;
    if (m_Aura->m_Net.ResolveHostName(resolvedAddress, ACCEPT_ANY, m_Config.m_HostName, m_Config.m_Port)) {
      m_Socket->Connect(emptyBindAddress, resolvedAddress);
    } else {
      m_Socket->m_HasError = true;
    }
    m_WaitingToConnect          = false;
    m_LastConnectionAttemptTime = Time;
  }

  return;
}

void CIRC::ExtractPackets()
{
  const int64_t Time = GetTime();
  string*       Recv = m_Socket->GetBytes();

  // separate packets using the CRLF delimiter

  vector<string> Packets = Tokenize(*Recv, '\n');

  for (auto& Packets_Packet : Packets)
  {
    // delete the superflous '\r'

    const string::size_type pos = Packets_Packet.find('\r');

    if (pos != string::npos)
      Packets_Packet.erase(pos, 1);

    // track timeouts

    m_LastPacketTime = Time;

    // ping packet
    // in:  PING :2748459196
    // out: PONG :2748459196
    // respond to the packet sent by the server

    if (Packets_Packet.compare(0, 4, "PING") == 0)
    {
      Send("PONG :" + Packets_Packet.substr(6));
      continue;
    }

    // notice packet
    // in: NOTICE AUTH :*** Checking Ident
    // not actually important

    if (Packets_Packet.compare(0, 6, "NOTICE") == 0)
    {
      //Print("[IRC: " + m_Config.m_HostName + "] " + Packets_Packet);
      continue;
    }

    // now we need to further tokenize each packet
    // the delimiter is space
    // we use a std::vector so we can check its number of tokens

    const vector<string> Tokens = Tokenize(Packets_Packet, ' ');

    // privmsg packet
    // in:  :nickname!~username@hostname PRIVMSG #channel :message
    // print the message, check if it's a command then execute if it is

    if (Tokens.size() > 3 && Tokens[1] == "PRIVMSG" && m_Config.m_CommandCFG->m_Enabled)
    {
      // don't bother parsing if the message is very short (1 character)
      // since it's surely not a command

      if (Tokens[3].size() < 3)
        continue;

      string nickName, hostName;

      // get the nickname

      uint32_t i = 1;

      for (; Tokens[0][i] != '!'; ++i)
        nickName += Tokens[0][i];

      // skip the username

      for (; Tokens[0][i] != '@'; ++i)
        ;

      // get the hostname

      for (++i; i < Tokens[0].size(); ++i)
        hostName += Tokens[0][i];

      // get the channel

      string channel = Tokens[2];

      // get the message

      string message = Packets_Packet.substr(Tokens[0].size() + Tokens[1].size() + Tokens[2].size() + 4);

      if (message.empty() || channel.empty())
        continue;

      string cmdToken, command, payload;
      uint8_t tokenMatch = ExtractMessageTokensAny(message, m_Config.m_PrivateCmdToken, m_Config.m_BroadcastCmdToken, cmdToken, command, payload);
      if (tokenMatch != COMMAND_TOKEN_MATCH_NONE) {
        const bool isWhisper = channel[0] != '#';
        shared_ptr<CCommandContext> ctx = nullptr;
        try {
          ctx = make_shared<CCommandContext>(m_Aura, m_Config.m_CommandCFG, this, channel, nickName, isWhisper, hostName, !isWhisper && tokenMatch == COMMAND_TOKEN_MATCH_BROADCAST, &std::cout);
        } catch (...) {
        }
        if (ctx) {
          ctx->UpdatePermissions();
          ctx->Run(cmdToken, command, payload);
        }
      }

      continue;
    }

    // kick packet
    // in:  :nickname!~username@hostname KICK #channel nickname :reason
    // out: JOIN #channel
    // rejoin the channel if we're the victim

    if (Tokens.size() == 5 && Tokens[1] == "KICK")
    {
      if (Tokens[3] == m_NickName) {
        Send("JOIN " + Tokens[2]);
      }

      continue;
    }

    // message of the day end packet
    // in: :server 376 nickname :End of /MOTD command.
    // out: JOIN #channel
    // join channels and auth and set +x on QuakeNet

    if (Tokens.size() >= 2 && Tokens[1] == "376") {
      // auth if the server is QuakeNet

      if (m_Config.m_HostName.find("quakenet.org") != string::npos && !m_Config.m_Password.empty()) {
        SendUser("AUTH " + m_Config.m_UserName + " " + m_Config.m_Password, "Q@CServe.quakenet.org");
        Send("MODE " + m_Config.m_NickName + " +x");
      }

      // join channels

      for (auto& channel : m_Config.m_Channels)
        Send("JOIN " + channel);

      continue;
    }

    // nick taken packet
    // in:  :server 433 CurrentNickname WantedNickname :Nickname is already in use.
    // out: NICK NewNickname
    // append an underscore and send the new nickname

    if (Tokens.size() >= 2 && Tokens[1] == "433")
    {
      // nick taken, append _

      m_NickName += '_';
      Send("NICK " + m_NickName);
      continue;
    }
  }

  // clear the whole buffer
  m_Socket->ClearRecvBuffer();
}

void CIRC::Send(const string& message)
{
  // max message length is 512 bytes including the trailing CRLF

  if (m_Socket->GetConnected())
    m_Socket->PutBytes(message + LF);
}

void CIRC::SendUser(const string& message, const string& target)
{
  // max message length is 512 bytes including the trailing CRLF

  if (!m_Socket->GetConnected())
    return;

  m_Socket->PutBytes("PRIVMSG " + target + " :" + (message.size() > 450 ? message.substr(0, 450) : message) + LF);
}

void CIRC::SendChannel(const string& message, const string& target)
{
  // Sending messages to channels or to user works exactly the same, except that channels start with #.
  SendUser(message, target);
}

void CIRC::SendAllChannels(const string& message)
{
  for (auto& channel : m_Config.m_Channels) {
    m_Socket->PutBytes("PRIVMSG " + channel + " :" + (message.size() > 450 ? message.substr(0, 450) : message) + LF);
  }
}

CCommandConfig* CIRC::GetCommandConfig() const
{
  return m_Config.m_CommandCFG;
}

bool CIRC::GetIsModerator(const std::string& nHostName)
{
  return m_Config.m_Admins.find(nHostName) != m_Config.m_Admins.end();
}

bool CIRC::GetIsSudoer(const std::string& nHostName)
{
  return m_Config.m_SudoUsers.find(nHostName) != m_Config.m_SudoUsers.end();
}
