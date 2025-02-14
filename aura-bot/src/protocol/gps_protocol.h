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

 */

#ifndef AURA_GPSPROTOCOL_H_
#define AURA_GPSPROTOCOL_H_

#include "../includes.h"

#define REJECTGPS_INVALID 1
#define REJECTGPS_NOTFOUND 2

#define GPS_ACK_PERIOD 10000u

namespace GPSProtocol
{
  namespace Magic
  {
    constexpr uint8_t INIT              = 1u;   // 0x1
    constexpr uint8_t RECONNECT         = 2u;   // 0x2
    constexpr uint8_t ACK               = 3u;   // 0x3
    constexpr uint8_t REJECT            = 4u;   // 0x4
    constexpr uint8_t UDPSYN            = 11u;  // 0xB
    constexpr uint8_t UDPACK            = 12u;  // 0xC
    constexpr uint8_t UDPFIN            = 13u;  // 0xD
    constexpr uint8_t SUPPORT_EXTENDED	= 50u;  // 0x32
    constexpr uint8_t CHANGEKEY         = 51u;  // 0x33

    constexpr uint8_t GPS_HEADER        = 248u; // 0xF8
  };

  // receive functions

  // send functions

  [[nodiscard]] std::vector<uint8_t> SEND_GPSC_INIT(const uint32_t version);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSC_RECONNECT(uint8_t UID, const uint32_t reconnectKey, const uint32_t lastPacket);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSC_ACK(const uint32_t lastPacket);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSS_INIT(const uint16_t reconnectPort, const uint8_t UID, const uint32_t reconnectKey, const uint8_t numEmptyActions);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSS_RECONNECT(const uint32_t lastPacket);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSS_ACK(const uint32_t lastPacket);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSS_REJECT(const uint32_t reason);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSS_SUPPORT_EXTENDED(const int64_t ticks, const uint32_t gameID);
  [[nodiscard]] std::vector<uint8_t> SEND_GPSS_CHANGE_KEY( uint32_t reconnectKey );
  [[nodiscard]] std::array<uint8_t, 2> SEND_GPSS_DIMENSIONS();
};

#endif // AURA_GPSPROTOCOL_H_
