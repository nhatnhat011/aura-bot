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

#ifndef AURA_COMMAND_H_
#define AURA_COMMAND_H_

#include "includes.h"

#include <ostream>

//
// CCommandContext
//

class CCommandContext : public std::enable_shared_from_this<CCommandContext>
{
public:
  CAura*                        m_Aura;
  CCommandConfig*               m_Config;
  CRealm*                       m_SourceRealm;
  CRealm*                       m_TargetRealm;
  CGame*                        m_SourceGame;
  CGame*                        m_TargetGame;
  GameUser::CGameUser*          m_GameUser;
  CIRC*                         m_IRC;
#ifndef DISABLE_DPP
  dpp::slashcommand_t*          m_DiscordAPI;
#else
  void*                         m_DiscordAPI;
#endif

protected:
  std::string                   m_FromName;
  uint64_t                      m_FromIdentifier;
  bool                          m_FromWhisper;
  uint8_t                       m_FromType;
  bool                          m_IsBroadcast;
  char                          m_Token;
  uint16_t                      m_Permissions;

  std::string                   m_ServerName;
  std::string                   m_ReverseHostName; // user hostname, reversed from their IP (received from IRC chat)
  std::string                   m_ChannelName;
  std::string                   m_ActionMessage;

  std::ostream*                 m_Output;

  std::optional<bool>           m_OverrideVerified;
  std::optional<uint8_t>        m_OverridePermissions;

  bool                          m_PartiallyDestroyed;

public:
  // Game
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* game, GameUser::CGameUser* user, const bool& nIsBroadcast, std::ostream* outputStream);

  // Realm, Realm->Game
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CRealm* fromRealm, const std::string& fromName, const bool& isWhisper, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CRealm* fromRealm, const std::string& fromName, const bool& isWhisper, const bool& nIsBroadcast, std::ostream* outputStream);

  // IRC, IRC->Game
  CCommandContext(CAura* nAura, CCommandConfig* config, CIRC* ircNetwork, const std::string& channelName, const std::string& userName, const bool& isWhisper, const std::string& reverseHostName, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CIRC* ircNetwork, const std::string& channelName, const std::string& userName, const bool& isWhisper, const std::string& reverseHostName, const bool& nIsBroadcast, std::ostream* outputStream);

#ifndef DISABLE_DPP
  // Discord, Discord->Game
  CCommandContext(CAura* nAura, CCommandConfig* config, dpp::slashcommand_t* discordAPI, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, dpp::slashcommand_t* discordAPI, std::ostream* outputStream);
#endif

  // Arbitrary, Arbitrary->Game
  CCommandContext(CAura* nAura, const std::string& nFromName, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, const std::string& nFromName, const bool& nIsBroadcast, std::ostream* outputStream);

  [[nodiscard]] inline bool GetWritesToStdout() const { return m_FromType == FROM_OTHER; }

  [[nodiscard]] std::string GetUserAttribution();
  [[nodiscard]] std::string GetUserAttributionPreffix();
  [[nodiscard]] std::ostream* GetOutputStream() { return m_Output; }

  [[nodiscard]] inline bool GetIsWhisper() const { return m_FromWhisper; }
  [[nodiscard]] inline const std::string& GetSender() const { return m_FromName; }
  [[nodiscard]] inline std::string GetChannelName() const { return m_ChannelName; }
  [[nodiscard]] inline CRealm* GetSourceRealm() const { return m_SourceRealm; }
  [[nodiscard]] inline CGame* GetSourceGame() const { return m_SourceGame; }
  [[nodiscard]] inline CGame* GetTargetGame() const { return m_TargetGame; }
  [[nodiscard]] inline CIRC* GetSourceIRC() const { return m_IRC; }
#ifndef DISABLE_DPP
  [[nodiscard]] inline dpp::slashcommand_t* GetDiscordAPI() const { return m_DiscordAPI; }
#endif

  void SetIdentity(const std::string& userName);
  void SetAuthenticated(const bool& nAuthenticated);
  void SetPermissions(const uint16_t nPermissions);
  void UpdatePermissions();
  void ClearActionMessage() { m_ActionMessage.clear(); }

  [[nodiscard]] std::optional<bool> CheckPermissions(const uint8_t nPermissionsRequired) const;
  [[nodiscard]] bool CheckPermissions(const uint8_t nPermissionsRequired, const uint8_t nAutoPermissions) const;
  [[nodiscard]] std::optional<std::pair<std::string, std::string>> CheckSudo(const std::string& message);
  [[nodiscard]] bool GetIsSudo() const;
  [[nodiscard]] bool CheckActionMessage(const std::string& nMessage) { return m_ActionMessage == nMessage; }
  [[nodiscard]] bool CheckConfirmation(const std::string& cmdToken, const std::string& cmd, const std::string& payload, const std::string& errorMessage);

  [[nodiscard]] std::vector<std::string> JoinReplyListCompact(const std::vector<std::string>& stringList) const;

  void SendPrivateReply(const std::string& message, const uint8_t ctxFlags = 0);
  void SendReplyCustomFlags(const std::string& message, const uint8_t ctxFlags);
  void SendReply(const std::string& message, const uint8_t ctxFlags = 0);
  void InfoReply(const std::string& message, const uint8_t ctxFlags = 0);
  void DoneReply(const std::string& message, const uint8_t ctxFlags = 0);
  void ErrorReply(const std::string& message, const uint8_t ctxFlags = 0);
  void SendAll(const std::string& message);
  void InfoAll(const std::string& message);
  void DoneAll(const std::string& message);
  void ErrorAll(const std::string& message);
  void SendAllUnlessHidden(const std::string& message);
  [[nodiscard]] GameUser::CGameUser* GetTargetUser(const std::string& target);
  [[nodiscard]] GameUser::CGameUser* RunTargetUser(const std::string& target);
  [[nodiscard]] GameUser::CGameUser* GetTargetUserOrSelf(const std::string& target);
  [[nodiscard]] GameUser::CGameUser* RunTargetPlayerOrSelf(const std::string& target);
  [[nodiscard]] bool GetParsePlayerOrSlot(const std::string& target, uint8_t& SID, GameUser::CGameUser*& user);
  [[nodiscard]] bool RunParsePlayerOrSlot(const std::string& target, uint8_t& SID, GameUser::CGameUser*& user);
  [[nodiscard]] bool GetParseNonPlayerSlot(const std::string& target, uint8_t& SID);
  [[nodiscard]] bool RunParseNonPlayerSlot(const std::string& target, uint8_t& SID);
  [[nodiscard]] CRealm* GetTargetRealmOrCurrent(const std::string& target);
  [[nodiscard]] bool GetParseTargetRealmUser(const std::string& target, std::string& nameFragment, std::string& realmFragment, CRealm*& realm, bool allowNoRealm = false, bool searchHistory = false);
  [[nodiscard]] uint8_t GetParseTargetServiceUser(const std::string& target, std::string& nameFragment, std::string& locationFragment, void*& location);
  [[nodiscard]] CGame* GetTargetGame(const std::string& target);
  void UseImplicitReplaceable();
  void UseImplicitHostedGame();
  void Run(const std::string& token, const std::string& command, const std::string& payload);
  void SetPartiallyDestroyed() { m_PartiallyDestroyed = true; }
  bool GetPartiallyDestroyed() const { return m_PartiallyDestroyed; }

  [[nodiscard]] static uint8_t TryDeferred(CAura* nAura, const LazyCommandContext& lazyCtx);
  
  ~CCommandContext();
};

[[nodiscard]] inline std::string GetTokenName(const std::string& token) {
  if (token.length() != 1) return std::string();
  switch (token[0]) {
    case '.':
      return " (period.)";
    case ',':
      return " (comma.)";
    case '~':
      return " (tilde.)";
    case '-':
      return " (hyphen.)";
    case '#':
      return " (hashtag.)";
    case '@':
      return " (at.)";
    case '$':
      return " (dollar.)";
    case '%':
      return " (percent.)";
  }
  return std::string();
}

[[nodiscard]] inline std::string HelpMissingComma(const std::string& payload) {
  if (payload.find(',') == std::string::npos) return " - did you miss the comma?";
  return std::string();
}

[[nodiscard]] inline bool ExtractMessageTokens(const std::string& message, const std::string& token, bool& matchPadding, std::string& matchCmd, std::string& matchPayload)
{
  matchPayload.clear();
  if (message.empty()) return false;
  std::string::size_type tokenSize = token.length();
  if (message.length() <= tokenSize || (tokenSize > 0 && message.substr(0, tokenSize) != token)) {
    return false;
  }
  std::string::size_type cmdStart = message.find_first_not_of(' ', tokenSize);
  matchPadding = cmdStart > tokenSize;
  if (cmdStart == std::string::npos) {
    return false;
  }
  std::string::size_type cmdEnd = message.find_first_of(' ', cmdStart);
  if (cmdEnd == std::string::npos) {
    matchCmd = message.substr(cmdStart);
  } else {
    matchCmd = message.substr(cmdStart, cmdEnd - cmdStart);
    std::string::size_type payloadStart = message.find_first_not_of(' ', cmdEnd);
    if (payloadStart != std::string::npos) {
      std::string::size_type payloadEnd = message.find_last_not_of(' ');
      if (payloadEnd != std::string::npos) {
        matchPayload = message.substr(payloadStart, payloadEnd + 1 - payloadStart);
      }
    }
  }
  return true;
}

[[nodiscard]] inline uint8_t ExtractMessageTokensAny(const std::string& message, const std::string& privateToken, const std::string& broadcastToken, std::string& matchToken, std::string& matchCmd, std::string& matchPayload)
{
  uint8_t result = COMMAND_TOKEN_MATCH_NONE;
  if (message.empty()) return result;
  if (!privateToken.empty()) {
    bool matchPadding = false;
    if (ExtractMessageTokens(message, privateToken, matchPadding, matchCmd, matchPayload)) {
      result = COMMAND_TOKEN_MATCH_PRIVATE;
      if (matchPadding) {
        matchToken = privateToken + " ";
      } else {
        matchToken = privateToken;
      }
    }
  }    
  if (result == COMMAND_TOKEN_MATCH_NONE && !broadcastToken.empty()) {
    bool matchPadding = false;
    if (ExtractMessageTokens(message, broadcastToken, matchPadding, matchCmd, matchPayload)) {
      result = COMMAND_TOKEN_MATCH_BROADCAST;
      if (matchPadding) {
        matchToken = broadcastToken + " ";
      } else {
        matchToken = broadcastToken;
      }
    }
  }

  if (result != COMMAND_TOKEN_MATCH_NONE) {
    std::transform(std::begin(matchCmd), std::end(matchCmd), std::begin(matchCmd), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  }

  return result;
}

[[nodiscard]] inline bool ParseBoolean(const std::string& payload, std::optional<bool>& result)
{
  if (payload.empty()) return true;
  std::string inputLower = payload;
  std::transform(std::begin(inputLower), std::end(inputLower), std::begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });
  if (inputLower == "enable" || inputLower == "on" || inputLower == "yes") {
    result = true;
    return true;
  } else if (inputLower == "disable" || inputLower == "off" || inputLower == "no") {
    result = false;
    return true;
  }
  return false;
}

#endif
