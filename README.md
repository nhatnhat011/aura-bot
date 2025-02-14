# Aura

![Build Status](https://gitlab.com/ivojulca/aura-bot/badges/master/pipeline.svg?ignore_skipped=true)

## Overview

[Aura][1] is a modern cross-platform hosting bot for Warcraft III The Frozen Throne Ⓡ.
It's based on [GHost++][2] by Trevor Hogan. After several overhauls across 
the entire code base, it now has excellent performance, and extensive networking, 
configuration, and input capabilities, making it very easy to use not only by players, 
but also bot owners, even behind the most strict [NATs][3].

Aura runs on little-endian Linux (32-bit and 64-bit), Windows (32-bit and 64-bit), 
and OS X (64-bit Intel CPU) machines.

## Getting started

First, get an Aura release. They are already provided for Windows 32-bit and 64-bit. 
For Linux, or OS X, you will need to build it yourself. See [BUILDING.md][4] for instructions.

To run the executable, you will need to use a command line. In Windows, open Command Prompt 
(type cmd into the Start menu and it should be the first result). In Mac OS X, open Terminal 
(it's in Utilities).

Type this into the command line:

```
cd LOCATION
```

Replace ``LOCATION`` with the location Aura is in (ending up with, for instance, ``cd "~/Downloads/aura-bot"`` 
or ``cd "C:\Users\Jon\Downloads\aura-bot\")``.

This will set your command line's location to Aura's folder. You'll have to do this each time you open 
a command line to run Aura.

Copy ``config-example.ini`` into ``config.ini``, and edit as you please. Aura is capable of autoconfiguration to some
extent, but you will need to provide it with ``bot.maps_path`` to let it find your maps folder. This should 
be in the same format as ``LOCATION`` above. See [CONFIG.md][5] for documentation regarding config options.

Now, to test your setup, run the command:

```
aura wormwar.ini "my first hosted game" -s config
```

Open your game client, and go to **Local Area Network**. You should see your Worm War game hosted there. 
Join it and type to the chat ``!owner``. This is a bot command. You may use them from many channels, 
including Battle.net, PvPGN public chats, whispers, IRC networks, and your system command line.

You may run ``!unhost`` to cancel this game, and proceed with the next step.

Run the commmand:

```
aura "Lost Temple" "testing config"
```

Open your game client, and go to ``Local Area Network``. If you have properly configured ``<bot.maps_path>``, 
you should see your Lost Temple game hosted there.

Join it, and take ownership by running the command ``!owner``. Afterwards, diagnostic your network 
connectivity with ``!checknetwork *``. See [NETWORKING.md][6] for additional information and troubleshooting.

Once you are satisfied, you may add fake players or computers (``!fp``, or ``!fill``) before starting 
the game (it cannot start with you alone.) Games may be started with ``!start`` or a few alternative 
commands, which you may find at [COMMANDS.md][7].

In both CLI examples so far, we have seen how to:
a. Host a known game from a config file at ``<bot.map_configs_path>``.
b. Host a known game from a map file at ``<bot.maps_path>``.

There are more CLI features available at [CLI.md][8].

However, you likely want to start Aura and let authorized people host any number of games. 
For that, setup your ``config.ini`` to connect to one or more realms, or to an IRC network. Then, 
just run the command:

```
aura
```

Now, you may join the same server as your bot, and send commands to it through public chat or whispers. 

To send commands to it through whispers, right click its name on the user list, then click on ``Whisper``. 
Afterwards, you will see ``/w BOTNAME`` show up in your chatbox. You may add any desired bot command afterwards. 
Note that, in contrast to previous CLI examples, bot commands only use commas to separate parameters.

```
/w aurabot !host lost temple, 2v2 fight
```

## Features

Find a non-extensive list of Aura features at [FEATURES.md][9].

## Contributing

This project is open-source, and hosted at [Gitlab][1]. You will need [Git][9] in 
order to contribute to this and other open-source projects. Then proceed with the 
following:

1. Fork the [aura-bot][1] repository.
2. Create a branch (`git checkout -b my_aura`)
3. Commit your changes (`git commit -am "Fixed a crash when using GProxy++"`)
4. Push to the branch (`git push origin my_aura`)
5. Create an [Issue][10] with a link to your branch or make a [Pull Request][11]

## License

### Aura
This work is licensed under the MIT license, reproduced below.

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

### Other authors

This work contains files originally developed by:
* Josko Nikolic -- [Aura v1.32][12] author (Apache License, Version 2.0)
* Trevor "Varlock" Hogan -- GHost++ author (Apache License, Version 2.0)

### Prior art

This work is inspired by prior work by MrJag, author of [Ghost][13].

### Libraries

Included libraries are licensed as follows.

#### [CLI11][14]

CLI11 2.2 Copyright (c) 2017-2024 University of Cincinnati, developed by Henry
Schreiner under NSF AWARD 1414736. All rights reserved.

Redistribution and use in source and binary forms of CLI11, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#### [UTF8-CPP][15]

Copyright 2006-2016 Nemanja Trifunovic

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

#### [CRC32][16]

This library has been modified for inclusion in Aura.

zlib License

Copyright (c) 2011-2016 Stephan Brumme

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
   If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

#### [CSVParser][17]

Copyright (c) 2001, Mayukh Bose
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  

 * Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

 * Neither the name of Mayukh Bose nor the names of other
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#### [StormLib][18]

The MIT License (MIT)

Copyright (c) 1999-2013 Ladislav Zezula

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

#### [BNCSutil][19]

Battle.Net Utility Library

Copyright (C) 2004-2006 Eric Naeseth

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

A copy of the GNU Lesser General Public License is included in the BNCSutil
distribution in the file COPYING.  If you did not receive this copy,
write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA  02111-1307  USA

#### [MiniUPnP Project][20]

Copyright (c) 2005-2017, Thomas BERNARD
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * The name of the author may not be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

#### [C++ Requests][21]

This license applies to everything except the contents of the "test"
directory and its subdirectories.

MIT License

Copyright (c) 2017-2021 Huu Nguyen
Copyright (c) 2022 libcpr and many other contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

#### [cURL][22]

Copyright (C) Daniel Stenberg, <daniel@haxx.se>, and many
contributors, see the THANKS file.

All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

#### [zlib][23]

version 1.3, August 18th, 2023

Copyright (C) 1995-2023 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Jean-loup Gailly        Mark Adler
jloup@gzip.org          madler@alumni.caltech.edu


The data format used by the zlib library is described by RFCs (Request for
Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
(zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).

Except as contained in this notice, the name of a copyright holder shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization of the copyright holder.

#### [D++][24]

 D++, A Lightweight C++ library for Discord
 
 SPDX-License-Identifier: Apache-2.0
 Copyright 2021 Craig Edwards and D++ contributors 
 (https://github.com/brainboxdotcc/DPP/graphs/contributors)
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

#### [JSON for Modern C++][25]

MIT License 

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

[1]: https://gitlab.com/ivojulca/aura-bot
[2]: https://github.com/uakfdotb/ghostpp
[3]: https://en.wikipedia.org/wiki/Network_address_translation
[4]: https://gitlab.com/ivojulca/aura-bot/BUILDING.md
[5]: https://gitlab.com/ivojulca/aura-bot/CONFIG.md
[6]: https://gitlab.com/ivojulca/aura-bot/NETWORKING.md
[7]: https://gitlab.com/ivojulca/aura-bot/COMMANDS.md
[8]: https://gitlab.com/ivojulca/aura-bot/CLI.md
[9]: https://git-scm.com/book/en/v2/Getting-Started-Installing-Git
[10]: https://gitlab.com/ivojulca/aura-bot/-/issues
[11]: https://gitlab.com/ivojulca/aura-bot/-/pulls
[12]: https://github.com/Josko/aura-bot
[13]: https://github.com/MrJag/ghost
[14]: https://github.com/CLIUtils/CLI11
[15]: https://github.com/nemtrif/utfcpp
[16]: https://github.com/stbrumme/crc32
[17]: https://gitlab.com/ivojulca/aura-bot/-/blob/master/src/csvparser.cpp
[18]: https://github.com/ladislav-zezula/StormLib
[19]: https://github.com/BNETDocs/bncsutil
[20]: https://github.com/miniupnp/miniupnp
[21]: https://github.com/libcpr/cpr
[22]: https://github.com/curl/curl
[23]: https://github.com/madler/zlib
[24]: https://github.com/brainboxdotcc/DPP
[25]: https://github.com/nlohmann/json/
