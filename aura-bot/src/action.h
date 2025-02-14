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

#ifndef AURA_ACTION_H_
#define AURA_ACTION_H_

#include "includes.h"

enum class CommandAuth : uint8_t
{
  kAuto = 0u,
  kSpoofed = 1u,
  kVerified = 2u,
  kAdmin = 3u,
  kRootAdmin = 4u,
  kSudo = 5u,
};

struct AppAction
{
  uint8_t type;
  uint8_t mode;
  uint32_t value_1;
  uint32_t value_2;
  int64_t queuedTime;

  AppAction(uint8_t nType, uint8_t nMode = 0, uint32_t nValue1 = 0, uint32_t nValue2 = 0)
   : type(nType),
     mode(nMode),
     value_1(nValue1),
     value_2(nValue2),
     queuedTime(GetTicks())
  {
  };

  ~AppAction() = default;
};

struct LazyCommandContext
{
  bool broadcast;
  int64_t queuedTime;
  std::string command;
  std::string payload;
  std::string targetGame;
  std::string identityName;
  std::string identityLoc;
  CommandAuth auth;

  LazyCommandContext(bool nBroadcast, const std::string& nCommand, const std::string& nPayload, const std::string& nTargetGame, const std::string& nIdentityName, const std::string& nIdentityLoc, const CommandAuth nAuth)
   : broadcast(nBroadcast),
     queuedTime(GetTicks()),
     command(nCommand),
     payload(nPayload),
     targetGame(nTargetGame),
     identityName(nIdentityName),
     identityLoc(nIdentityLoc),
     auth(nAuth)
  {
  };

  ~LazyCommandContext() = default;
};

#endif // AURA_ACTION_H_
