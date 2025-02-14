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

#include "realm_chat.h"

#include <algorithm>
#include <cmath>

#include "protocol/bnet_protocol.h"
#include "command.h"
#include "config/config.h"
#include "config/config_realm.h"
#include "file_util.h"
#include "game.h"
#include "includes.h"
#include "realm.h"
#include "util.h"

#include "aura.h"

using namespace std;

//
// CQueuedChatMessage
//

CQueuedChatMessage::CQueuedChatMessage(CRealm* nRealm, shared_ptr<CCommandContext> nCtx, const bool isProxy)
  : m_Realm(nRealm),
    m_QueuedTime(0),
    m_ReceiverSelector(0),
    m_MessageValue(0),

    m_ProxySenderCtx(nullptr),
    m_Callback(CHAT_CALLBACK_NONE),
    m_CallbackData(0),
    m_WasThrottled(false)
{
  if (nCtx && nCtx->m_SourceRealm == nRealm) {
    m_Channel = nCtx->GetChannelName();
  }
  if (m_Channel.empty()) {
    m_Channel = nRealm->GetCurrentChannel();
  }
  if (isProxy) {
    m_ProxySenderCtx = nCtx;
    const string& fromName = nCtx->GetSender();
    m_ProxySenderName = vector<uint8_t>(fromName.begin(), fromName.end());
  }
}

CQueuedChatMessage::~CQueuedChatMessage()
{
  if (m_ProxySenderCtx) {
    m_ProxySenderCtx.reset();
  }
}

void CQueuedChatMessage::SetValidator(const uint8_t validatorType, const uint32_t validatorData)
{
  switch (validatorType) {
    case CHAT_VALIDATOR_LOBBY_JOINABLE:
      m_Validator = vector<uint8_t>();
      m_Validator.reserve(5);
      m_Validator.push_back(validatorType);
      m_Validator.push_back(static_cast<uint8_t>(validatorData));
      m_Validator.push_back(static_cast<uint8_t>(validatorData >> 8));
      m_Validator.push_back(static_cast<uint8_t>(validatorData >> 16));
      m_Validator.push_back(static_cast<uint8_t>(validatorData >> 24));
      break;
    default:
      break;
  }
}

void CQueuedChatMessage::SetCallback(const uint8_t type, const uint32_t data)
{
  m_Callback = type;
  m_CallbackData = data;
}

int64_t CQueuedChatMessage::GetQueuedDuration() const
{
  return GetTime() - m_QueuedTime;
}

bool CQueuedChatMessage::GetIsStale() const
{
  if (m_Validator.empty()) return false;
  switch (m_Validator[0]) {
    case CHAT_VALIDATOR_LOBBY_JOINABLE: {
      if (m_Realm->GetIsGameBroadcastErrored()) return true;
      CGame* refLobby = m_Realm->m_Aura->GetLobbyByHostCounterExact(ByteArrayToUInt32(m_Validator, false, 1));
      if (!refLobby || !refLobby->GetIsSupportedGameVersion(m_Realm->GetGameVersion())) return true;
    }
    default:
      return false;
  }
}

vector<uint8_t> CQueuedChatMessage::GetMessageBytes() const
{
  return BNETProtocol::SEND_SID_CHAT_PUBLIC(m_Message);
}

vector<uint8_t> CQueuedChatMessage::GetWhisperBytes() const
{
  return BNETProtocol::SEND_SID_CHAT_WHISPER(m_Message, m_ReceiverName);
}

uint8_t CQueuedChatMessage::QuerySelection(const std::string& currentChannel) const
{
  switch (m_ReceiverSelector) {
    case RECV_SELECTOR_SYSTEM:
      return CHAT_RECV_SELECTED_SYSTEM;
    case RECV_SELECTOR_ONLY_WHISPER:
      return CHAT_RECV_SELECTED_WHISPER;
    case RECV_SELECTOR_ONLY_PUBLIC:
      if (currentChannel.empty()) return CHAT_RECV_SELECTED_NONE;
      return CHAT_RECV_SELECTED_PUBLIC;
    case RECV_SELECTOR_ONLY_PUBLIC_OR_DROP:
      if (currentChannel.empty()) return CHAT_RECV_SELECTED_DROP;
      return CHAT_RECV_SELECTED_PUBLIC;
    case RECV_SELECTOR_PREFER_PUBLIC:
      if (currentChannel.empty()) return CHAT_RECV_SELECTED_WHISPER;
      return CHAT_RECV_SELECTED_PUBLIC;
    default:
      // Should never happen
      return CHAT_RECV_SELECTED_DROP;
  }
}

vector<uint8_t> CQueuedChatMessage::SelectBytes(const std::string& currentChannel, uint8_t& selectType) const
{
  selectType = QuerySelection(currentChannel);
  switch (selectType) {
    case CHAT_RECV_SELECTED_WHISPER:
      return GetWhisperBytes();
    case CHAT_RECV_SELECTED_PUBLIC:
    case CHAT_RECV_SELECTED_SYSTEM:
      return GetMessageBytes();
    default:
      return vector<uint8_t>();
  }
}

uint8_t CQueuedChatMessage::SelectSize(const size_t wrapSize, const std::string& currentChannel) const
{
  uint8_t selectType = QuerySelection(currentChannel);
  switch (selectType) {
    case CHAT_RECV_SELECTED_WHISPER:
    case CHAT_RECV_SELECTED_PUBLIC:
    case CHAT_RECV_SELECTED_SYSTEM:
      return GetVirtualSize(wrapSize, selectType);
    default:
      // m_Message.size() > 0 => GetVirtualSize(...) > 0
      return 0;
  }
}

bool CQueuedChatMessage::GetSendsEarlyFeedback() const
{
  if (m_EarlyFeedback.empty() || !m_ProxySenderCtx || m_ProxySenderCtx->GetPartiallyDestroyed()) {
    return false;
  }
  return true;
}

void CQueuedChatMessage::SendEarlyFeedback() const {
  m_ProxySenderCtx->SendReply(m_EarlyFeedback);
}

uint8_t CQueuedChatMessage::GetVirtualSize(const size_t wrapSize, const uint8_t selectType) const
{
  // according to realm's antiflood parameters
  size_t result;
  if (selectType == CHAT_RECV_SELECTED_WHISPER) {
    result = (BNETProtocol::GetWhisperSize(m_Message, m_ReceiverName) + wrapSize - 1) / wrapSize;
  } else {
    // public or system
    result = (BNETProtocol::GetMessageSize(m_Message) + wrapSize - 1) / wrapSize;
  }
  // Note that PvPGN antiflood accepts no more than 100 virtual lines in any given message.
  if (result > 0xFF) return 0xFF;
  return static_cast<uint8_t>(result);
}

std::pair<bool, uint8_t> CQueuedChatMessage::OptimizeVirtualSize(const size_t wrapSize) const
{
  // m_ReceiverSelector must be accounted externally
  const uint8_t minSize = GetVirtualSize(wrapSize, CHAT_RECV_SELECTED_PUBLIC);
  return make_pair(GetVirtualSize(wrapSize, CHAT_RECV_SELECTED_WHISPER) > minSize, minSize);
}
